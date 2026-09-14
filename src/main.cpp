#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSerifItalic9pt7b.h>
#include <Fonts/Picopixel.h>
// ============================================================
// OLED
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);


// ============================================================
// PINS
// ============================================================

#define DASHBOARD_BUTTON 4
#define PET_BUTTON 5

#define ENCODER_CLK 6
#define ENCODER_DT 7
#define ENCODER_SW 8


// ============================================================
// CONSTANTS
// ============================================================

#define MAX_QUESTS 5

#define EASY_XP 30
#define MEDIUM_XP 50
#define HARD_XP 70

#define DEBOUNCE_MS 120


// ============================================================
// SCREEN SYSTEM
// ============================================================

enum Screen : uint8_t
{
  WELCOME_SCREEN,
  DASHBOARD_SCREEN,
  PET_SCREEN,
  ADD_QUEST_SCREEN,
  DIFFICULTY_SCREEN
};

Screen currentScreen = WELCOME_SCREEN;

static const unsigned char PROGMEM image_hour_glass_75_bits[] = {0xff,0xe0,0x40,0x40,0x40,0x40,0x51,0x40,0x5f,0x40,0x2e,0x80,0x15,0x00,0x0a,0x00,0x0a,0x00,0x11,0x00,0x24,0x80,0x44,0x40,0x4e,0x40,0x5f,0x40,0x7f,0xc0,0xff,0xe0};

// ============================================================
// QUEST DATA
// ============================================================

struct Quest
{
  char name[18];

  uint8_t type;
  uint8_t difficulty;
  uint8_t xp;

  bool completed;
};

Quest quests[MAX_QUESTS];

uint8_t questCount = 0;


// ============================================================
// PROGRESSION DATA
// ============================================================

uint16_t currentXP = 0;

uint16_t developmentCount = 0;

uint8_t currentLevel = 1;


// ============================================================
// NEW QUEST DATA
// ============================================================

uint8_t newQuestType = 0;
uint8_t newQuestDifficulty = 1;


// ============================================================
// MENU SELECTION
// ============================================================

uint8_t selectedItem = 0;


// ============================================================
// ENCODER
// ============================================================

uint8_t lastEncoderState = 0;

int8_t encoderCounter = 0;

static const int8_t transitions[16] =
{
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};


// ============================================================
// BUTTON STATES
// ============================================================

bool lastDashboardButton = HIGH;
bool lastPetButton = HIGH;
bool lastEncoderButton = HIGH;

unsigned long dashboardButtonTime = 0;
unsigned long petButtonTime = 0;
unsigned long encoderButtonTime = 0;


// ============================================================
// DISPLAY CONTROL
// ============================================================

bool displayNeedsUpdate = true;


// ============================================================
// FUNCTION DECLARATIONS
// ============================================================


// ---------- SYSTEM ----------

void initializeSystem();
void runSystem();


// ---------- INPUT ----------

void readEncoder();
void readEncoderButton();
void readDashboardButton();
void readPetButton();


// ---------- SCREEN LOGIC ----------

void handleEncoderMovement();
void handleEncoderClick();


// ---------- QUEST LOGIC ----------

void toggleQuest();
void addQuest();
void clearQuestBuffer();

void checkAllQuestsComplete();

uint8_t calculateXP(uint8_t difficulty);


// ---------- SELECTION ----------

void limitSelection();


// ============================================================
// UI SYSTEM
// ============================================================

// Basic UI components

void drawHeader(const __FlashStringHelper* title);

void drawText(
  const __FlashStringHelper* text,
  uint8_t x,
  uint8_t y
);

void drawCheckbox(
  uint8_t x,
  uint8_t y,
  bool checked
);

void drawSelector(
  uint8_t x,
  uint8_t y,
  bool selected
);

void drawQuest(
  uint8_t index,
  uint8_t y,
  bool selected
);


// ---------- SCREEN UI ----------

void drawScreen();

void drawWelcomeScreen();
void drawDashboard();
void drawPetScreen();
void drawAddQuestScreen();
void drawDifficultyScreen();


// ============================================================
// SETUP
// ============================================================

void setup()
{
  initializeSystem();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  runSystem();
}


// ============================================================
// SYSTEM INITIALIZATION
// ============================================================

void initializeSystem()
{
  // OLED
  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
      ))
  {
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.display();


  // Buttons
  pinMode(DASHBOARD_BUTTON, INPUT_PULLUP);
  pinMode(PET_BUTTON, INPUT_PULLUP);


  // Encoder
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);


  // Initial encoder state
  lastEncoderState =
      (digitalRead(ENCODER_CLK) << 1)
    |  digitalRead(ENCODER_DT);


  clearQuestBuffer();

  currentScreen = WELCOME_SCREEN;

  selectedItem = 0;

  displayNeedsUpdate = true;
}


// ============================================================
// MAIN SYSTEM LOOP
// ============================================================

void runSystem()
{
  readEncoder();

  readEncoderButton();

  readDashboardButton();

  readPetButton();


  if (displayNeedsUpdate)
  {
    drawScreen();

    displayNeedsUpdate = false;
  }
}


// ============================================================
// ENCODER READING
// ============================================================

void readEncoder()
{
  uint8_t currentState =
      (digitalRead(ENCODER_CLK) << 1)
    |  digitalRead(ENCODER_DT);


  uint8_t index =
      (lastEncoderState << 2)
    | currentState;


  int8_t movement =
      transitions[index];


  encoderCounter += movement;

  lastEncoderState = currentState;


  // One complete encoder detent
  if (encoderCounter >= 4)
  {
    encoderCounter = 0;

    selectedItem++;

    limitSelection();

    displayNeedsUpdate = true;
  }


  else if (encoderCounter <= -4)
  {
    encoderCounter = 0;

    if (selectedItem > 0)
      selectedItem--;

    displayNeedsUpdate = true;
  }
}


// ============================================================
// ENCODER BUTTON
// ============================================================

void readEncoderButton()
{
  bool currentButton =
      digitalRead(ENCODER_SW);


  if (currentButton == LOW &&
      lastEncoderButton == HIGH)
  {
    if (millis() - encoderButtonTime > DEBOUNCE_MS)
    {
      encoderButtonTime = millis();

      handleEncoderClick();
    }
  }


  lastEncoderButton = currentButton;
}


// ============================================================
// DASHBOARD BUTTON
// ============================================================

void readDashboardButton()
{
  bool currentButton =
      digitalRead(DASHBOARD_BUTTON);


  if (currentButton == LOW &&
      lastDashboardButton == HIGH)
  {
    if (millis() - dashboardButtonTime > DEBOUNCE_MS)
    {
      dashboardButtonTime = millis();

      currentScreen = DASHBOARD_SCREEN;

      selectedItem = 0;

      displayNeedsUpdate = true;
    }
  }


  lastDashboardButton = currentButton;
}


// ============================================================
// PET BUTTON
// ============================================================

void readPetButton()
{
  bool currentButton =
      digitalRead(PET_BUTTON);


  if (currentButton == LOW &&
      lastPetButton == HIGH)
  {
    if (millis() - petButtonTime > DEBOUNCE_MS)
    {
      petButtonTime = millis();

      currentScreen = PET_SCREEN;

      selectedItem = 0;

      displayNeedsUpdate = true;
    }
  }


  lastPetButton = currentButton;
}


// ============================================================
// ENCODER CLICK LOGIC
// ============================================================

void handleEncoderClick()
{
  switch (currentScreen)
  {
    // --------------------------------------------------------
    // DASHBOARD
    // --------------------------------------------------------

    case DASHBOARD_SCREEN:

      if (selectedItem < questCount)
      {
        toggleQuest();
      }

      else if (
        questCount < MAX_QUESTS &&
        selectedItem == questCount
      )
      {
        currentScreen = ADD_QUEST_SCREEN;

        selectedItem = 0;
      }

      displayNeedsUpdate = true;

      break;


    // --------------------------------------------------------
    // ADD QUEST
    // --------------------------------------------------------

    case ADD_QUEST_SCREEN:

      newQuestType = selectedItem;

      selectedItem = 0;

      currentScreen = DIFFICULTY_SCREEN;

      displayNeedsUpdate = true;

      break;


    // --------------------------------------------------------
    // DIFFICULTY
    // --------------------------------------------------------

    case DIFFICULTY_SCREEN:

      newQuestDifficulty =
          selectedItem + 1;

      addQuest();

      currentScreen = DASHBOARD_SCREEN;

      selectedItem = questCount - 1;

      displayNeedsUpdate = true;

      break;


    // --------------------------------------------------------
    // PET
    // --------------------------------------------------------

    case PET_SCREEN:

      currentScreen = DASHBOARD_SCREEN;

      selectedItem = 0;

      displayNeedsUpdate = true;

      break;


    default:
      break;
  }
}


// ============================================================
// LIMIT MENU SELECTION
// ============================================================

void limitSelection()
{
  uint8_t maximum = 0;


  switch (currentScreen)
  {
    case DASHBOARD_SCREEN:

      if (questCount < MAX_QUESTS)
        maximum = questCount;
      else
        maximum = questCount - 1;

      break;


    case ADD_QUEST_SCREEN:

      maximum = 2;

      break;


    case DIFFICULTY_SCREEN:

      maximum = 2;

      break;


    default:

      maximum = 0;

      break;
  }


  if (selectedItem > maximum)
    selectedItem = maximum;
}


// ============================================================
// QUEST LOGIC
// ============================================================

void toggleQuest()
{
  Quest &q = quests[selectedItem];


  if (!q.completed)
  {
    q.completed = true;

    currentXP += q.xp;
  }

  else
  {
    q.completed = false;

    if (currentXP >= q.xp)
      currentXP -= q.xp;
    else
      currentXP = 0;
  }


  checkAllQuestsComplete();
}


// ============================================================
// ADD QUEST
// ============================================================

void addQuest()
{
  if (questCount >= MAX_QUESTS)
    return;


  Quest &q =
      quests[questCount];


  q.completed = false;

  q.difficulty =
      newQuestDifficulty;


  q.xp =
      calculateXP(
        newQuestDifficulty
      );


  q.type =
      newQuestType;


  // Temporary names
  // We will replace this later
  // with your custom quest UI.

  if (newQuestType == 0)
  {
    strcpy(q.name, "Session Quest");
  }

  else if (newQuestType == 1)
  {
    strcpy(q.name, "Practice Quest");
  }

  else
  {
    strcpy(q.name, "Assignment");
  }


  questCount++;
}


// ============================================================
// XP CALCULATION
// ============================================================

uint8_t calculateXP(
    uint8_t difficulty
)
{
  if (difficulty == 1)
    return EASY_XP;

  if (difficulty == 2)
    return MEDIUM_XP;

  return HARD_XP;
}


// ============================================================
// CHECK ALL QUESTS
// ============================================================

void checkAllQuestsComplete()
{
  if (questCount != MAX_QUESTS)
    return;


  for (uint8_t i = 0; i < MAX_QUESTS; i++)
  {
    if (!quests[i].completed)
      return;
  }


  // Five quests completed

  developmentCount++;

  currentXP = 0;


  clearQuestBuffer();


  currentScreen = DASHBOARD_SCREEN;

  selectedItem = 0;

  displayNeedsUpdate = true;
}


// ============================================================
// CLEAR QUEST BUFFER
// ============================================================

void clearQuestBuffer()
{
  questCount = 0;


  for (uint8_t i = 0; i < MAX_QUESTS; i++)
  {
    quests[i].name[0] = '\0';

    quests[i].type = 0;

    quests[i].difficulty = 0;

    quests[i].xp = 0;

    quests[i].completed = false;
  }


  selectedItem = 0;
}


// ============================================================
// UI SYSTEM
// ============================================================


// ------------------------------------------------------------
// HEADER
// ------------------------------------------------------------

void drawHeader(
    const __FlashStringHelper* title
)
{
  display.setTextSize(1);

  display.setCursor(2, 2);

  display.print(title);

  display.drawLine(
    0, 11,
    127, 11,
    SSD1306_WHITE
  );
}


// ------------------------------------------------------------
// TEXT
// ------------------------------------------------------------

void drawText(
    const __FlashStringHelper* text,
    uint8_t x,
    uint8_t y
)
{
  display.setTextSize(1);

  display.setCursor(x, y);

  display.print(text);
}


// ------------------------------------------------------------
// CHECKBOX
// ------------------------------------------------------------

void drawCheckbox(
    uint8_t x,
    uint8_t y,
    bool checked
)
{
  display.drawRect(
    x,
    y,
    7,
    7,
    SSD1306_WHITE
  );


  if (checked)
  {
    display.drawLine(
      x + 1,
      y + 3,
      x + 3,
      y + 5,
      SSD1306_WHITE
    );

    display.drawLine(
      x + 3,
      y + 5,
      x + 6,
      y + 1,
      SSD1306_WHITE
    );
  }
}


// ------------------------------------------------------------
// SELECTION ARROW
// ------------------------------------------------------------

void drawSelector(
    uint8_t x,
    uint8_t y,
    bool selected
)
{
  if (selected)
  {
    display.print(F(">"));
  }
  else
  {
    display.print(F(" "));
  }
}


// ------------------------------------------------------------
// QUEST
// ------------------------------------------------------------

void drawQuest(
    uint8_t index,
    uint8_t y,
    bool selected
)
{
  Quest &q = quests[index];


  // Selection arrow

  display.setCursor(2, y);

  drawSelector(
    2,
    y,
    selected
  );


  // Checkbox

  drawCheckbox(
    12,
    y,
    q.completed
  );


  // Quest name

  display.setCursor(23, y);

  display.print(q.name);


  // XP

  display.setCursor(105, y);

  display.print(F("+"));

  display.print(q.xp);
}


// ============================================================
// SCREEN MANAGER
// ============================================================

void drawScreen()
{
  display.clearDisplay();


  switch (currentScreen)
  {
    case WELCOME_SCREEN:
      drawWelcomeScreen();
      break;


    case DASHBOARD_SCREEN:
      drawDashboard();
      break;


    case PET_SCREEN:
      drawPetScreen();
      break;


    case ADD_QUEST_SCREEN:
      drawAddQuestScreen();
      break;


    case DIFFICULTY_SCREEN:
      drawDifficultyScreen();
      break;
  }


  display.display();
}


// ============================================================
// WELCOME UI
// ============================================================

void drawWelcomeScreen()
{
  display.clearDisplay();
    // string 1
    display.setTextColor(1);
    display.setTextWrap(false);
    display.setFont(&FreeSerifItalic9pt7b);
    display.setCursor(20, 32);
    display.print("Hey There !!!");
    // string 2
    display.setFont(&Picopixel);
    display.setCursor(2, 61);
    display.print("sit with me");
    // string 3
    display.setFont();
    display.setCursor(78, 1);
    display.print("LEVEL:");
    // string 4
    display.setFont(&Picopixel);
    display.setCursor(92, 61);
    display.print("dashnoard");
    // string 5
    display.setFont();
    display.setCursor(3, 1);
    display.print("XP:");
    // hour_glass_75
    display.drawBitmap(58, 38, image_hour_glass_75_bits, 11, 16, 1);
    display.display();
}


// ============================================================
// DASHBOARD UI
// ============================================================

void drawDashboard()
{
  drawHeader(F("QUESTS"));


  // XP

  display.setCursor(92, 2);

  display.print(F("XP:"));

  display.print(currentXP);


  // Quest list

  for (uint8_t i = 0; i < questCount; i++)
  {
    drawQuest(
      i,
      15 + i * 10,
      selectedItem == i
    );
  }


  // Add Quest

  if (questCount < MAX_QUESTS)
  {
    uint8_t y =
        15 + questCount * 10;


    display.setCursor(2, y);

    if (selectedItem == questCount)
      display.print(F("> "));
    else
      display.print(F("  "));


    display.print(F("+ Add Quest"));
  }


  // Quest count

  display.setCursor(103, 55);

  display.print(questCount);

  display.print(F("/5"));
}


// ============================================================
// PET UI
// ============================================================

void drawPetScreen()
{
  drawHeader(F("MY PET"));


  display.setCursor(45, 25);

  display.print(F("(^_^)"));


  display.setCursor(35, 45);

  display.print(F("Hello!"));
}


// ============================================================
// ADD QUEST UI
// ============================================================

void drawAddQuestScreen()
{
  drawHeader(F("ADD QUEST"));


  const char* types[] =
  {
    "Session",
    "Practice",
    "Assignment"
  };


  for (uint8_t i = 0; i < 3; i++)
  {
    display.setCursor(15, 18 + i * 12);


    if (selectedItem == i)
      display.print(F("> "));
    else
      display.print(F("  "));


    display.print(types[i]);
  }
}


// ============================================================
// DIFFICULTY UI
// ============================================================

void drawDifficultyScreen()
{
  drawHeader(F("DIFFICULTY"));


  const char* difficulty[] =
  {
    "Easy",
    "Medium",
    "Hard"
  };


  for (uint8_t i = 0; i < 3; i++)
  {
    display.setCursor(15, 18 + i * 12);


    if (selectedItem == i)
      display.print(F("> "));
    else
      display.print(F("  "));


    display.print(difficulty[i]);
  }
}