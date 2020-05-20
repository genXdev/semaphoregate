/*

Author: René Vaessen
Email: rene.vaessen@gmail.com

Title: SemaphoreGate - Arduino firmware for a simple device that keeps track and controls number of people inside a room/building

Description:
  Arduino firmware for a small device that is capable of counting visitors and limiting access
  to a particular room or building using two infrared sensors, a traffic light (red, green),
  and a buzzer at the entrance.

  For changing the two parameters (current, and maximum) there are three buttons and a lcd display.
  Namely, "R", "+" and "-"

  Holding "R" for 5 seconds resets device to default settings

  Pressing "-" or "+" changes the "maximum" parameter.

  Holding "R" and then pressing "-" or "+" changes the "current" parameter.

Used hardware:

   - Arduino Uno
   - HD44780 1602 LCD Module Display with I2C interface 2x16 characters

License: MIT License

Copyright (c) 2020 René Vaessen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,2,1,0,4,5,6,7,3,POSITIVE);

bool LOG_TO_SERIAL = false;

unsigned long SENSOR_DELAY_MS        = 1000;
unsigned long BUZZER_DURATION_MS     = 500;
unsigned long BUTTON_REPEAT_DELAY_MS = 500;
unsigned long BUTTON_LONG_DELAY_MS   = 5000;
unsigned long BUTTON_ENTRANCE_MIN_MS = 2000;

int PORT_A0_SENSOR_IN           = 8;
int PORT_A1_SENSOR_OUT          = 9;
int PORT_A2_BUZZER              = 4;
int PORT_A3_RED_LIGHT           = 3;
int PORT_A4_GREEN_LIGHT         = 2;
int PORT_A5_BUTTON_RESET        = 7;
int PORT_A8_BUTTON_UP           = 6;
int PORT_A9_BUTTON_DOWN         = 5;

void setup() {

  // setup pins
  pinMode(PORT_A0_SENSOR_IN, INPUT);
  digitalWrite(PORT_A0_SENSOR_IN, HIGH);
  pinMode(PORT_A1_SENSOR_OUT, INPUT);
  digitalWrite(PORT_A1_SENSOR_OUT, HIGH);
  pinMode(PORT_A3_RED_LIGHT, OUTPUT);
  pinMode(PORT_A4_GREEN_LIGHT, OUTPUT);
  pinMode(PORT_A2_BUZZER, OUTPUT);
  pinMode(PORT_A5_BUTTON_RESET, INPUT);
  digitalWrite(PORT_A5_BUTTON_RESET, HIGH);
  pinMode(PORT_A8_BUTTON_UP, INPUT);
  digitalWrite(PORT_A8_BUTTON_UP, HIGH);
  pinMode(PORT_A9_BUTTON_DOWN, INPUT);
  digitalWrite(PORT_A9_BUTTON_DOWN, HIGH);

  handleReset();
  initializeDisplay();
}

unsigned long currentMillis = millis();
unsigned long previousMillis = millis();
unsigned long buttonResetTimestamp = 0;
unsigned long buttonUpTimestamp = 0;
unsigned long buttonDownTimestamp = 0;
unsigned long sensorInTimestamp = 0;
unsigned long sensorOutTimestamp = 0;
unsigned long buzzerTimestamp = 0;
unsigned long displayTimestamp = 0;
unsigned long keepRedTimestamp = millis();

bool buttonResetBlocked = false;
bool buttonResetWasPressed = false;
bool buttonUpWasPressed = false;
bool buttonDownWasPressed = false;
bool sensorInWasActive = false;
bool sensorOutWasActive = false;
bool buzzerIsActive = false;
bool displayUpdated = false;

bool buttonResetNowPressed = false;
bool buttonUpNowPressed = false;
bool buttonDownNowPressed= false;
bool sensorInNowActive = false;
bool sensorOutNowActive = false;

unsigned long visitorCount = 0;
unsigned long maxVisitorCount = 5;

void loop() {

  // read inputs
  buttonResetNowPressed = digitalRead(PORT_A5_BUTTON_RESET) == LOW;
  buttonUpNowPressed = digitalRead(PORT_A8_BUTTON_UP) == LOW;
  buttonDownNowPressed= digitalRead(PORT_A9_BUTTON_DOWN) == LOW;
  sensorInNowActive = digitalRead(PORT_A0_SENSOR_IN) == LOW;
  sensorOutNowActive = digitalRead(PORT_A1_SENSOR_OUT) == LOW;

  // perform tasks
  updateTimers();
  checkBuzzer();
  checkSensorIn();
  checkSensorOut();
  checkButtons();
  updateDisplay();

  // update state
  buttonResetWasPressed = buttonResetNowPressed;
  buttonUpWasPressed = buttonUpNowPressed;
  buttonDownWasPressed = buttonDownNowPressed;
  sensorInWasActive = sensorInNowActive;
  sensorOutWasActive = sensorOutNowActive;
}

void updateTimers() {

  // determine current time
  currentMillis = millis();

  // once every 49 days, this counter will reset
  // prevent strange behavior
  if (currentMillis < previousMillis) {

      if (LOG_TO_SERIAL) Serial.println("timers reset");

      buttonResetTimestamp = 0;
      buttonUpTimestamp = 0;
      buttonDownTimestamp = 0;
      sensorInTimestamp = 0;
      sensorOutTimestamp = 0;
      buzzerTimestamp = 0;
      displayTimestamp = 0;
      keepRedTimestamp = 0;
  }

  previousMillis = currentMillis;
}

void checkBuzzer() {

  if (buzzerIsActive && currentMillis - buzzerTimestamp > BUZZER_DURATION_MS) {

     if (LOG_TO_SERIAL) Serial.println("buzzing stopped.");

    buzzerIsActive = false;
    buzzerTimestamp = currentMillis;
  }

  digitalWrite(PORT_A2_BUZZER, buzzerIsActive ? LOW : HIGH);
}

void buzz() {

  if (buzzerIsActive) return;

  if (LOG_TO_SERIAL) Serial.println("buzzing started.");

  buzzerIsActive = true;
  buzzerTimestamp = currentMillis;
  digitalWrite(PORT_A2_BUZZER, HIGH);
}

void checkSensorIn() {

  if (sensorInWasActive && !sensorInNowActive) {

    if (currentMillis - sensorInTimestamp > SENSOR_DELAY_MS) {

       handleVisitorEntered();
       sensorInTimestamp = currentMillis;
    }
    else {

      if (LOG_TO_SERIAL) Serial.println("ignoring sensor IN");
    }
  }
}

void checkSensorOut() {

  if (sensorOutWasActive && !sensorOutNowActive) {

    if (currentMillis - sensorOutTimestamp > SENSOR_DELAY_MS) {

       handleVisitorLeft();
       sensorOutTimestamp = currentMillis;
    }
    else {

       if (LOG_TO_SERIAL) Serial.println("ignoring sensor OUT");
    }
  }
}

void checkButtons() {

  // unblock reset long press?
  if (!buttonResetNowPressed) {

     buttonResetBlocked = false;
  }
  else if (!buttonResetWasPressed) {

     buttonResetTimestamp = currentMillis;
  }

  // long pressed reset button?
  if (!buttonResetBlocked && buttonResetWasPressed && buttonResetNowPressed &&
     currentMillis - buttonResetTimestamp > BUTTON_LONG_DELAY_MS) {

      if (LOG_TO_SERIAL) Serial.println("Long press RESETING!");

      buttonResetBlocked = true;
      handleReset();

      return;
  }

  // pressed up?
  if (buttonUpNowPressed && currentMillis - buttonUpTimestamp > BUTTON_REPEAT_DELAY_MS) {

     // disable long press-to-reset when using up/down
     buttonResetBlocked = buttonResetNowPressed;

     if (LOG_TO_SERIAL) Serial.println("Up pressed !");

     handleButtonUp();

     return;
  }

  // pressed down?
  if (buttonDownNowPressed && currentMillis - buttonDownTimestamp > BUTTON_REPEAT_DELAY_MS) {

     // disable long press-to-reset when using up/down
     buttonResetBlocked = buttonResetNowPressed;

     if (LOG_TO_SERIAL) Serial.println("Down pressed !");

     handleButtonDown();
  }
}

void updateDisplay() {

  // reinitialize every 60 seconds, just in case
  if (currentMillis - displayTimestamp > 60000) {

    displayUpdated = false;
    initializeDisplay();
  }

  updateLights();

  if (displayUpdated) return;

  displayTimestamp = currentMillis;
  displayUpdated = true;
  setDisplayText();

  if (LOG_TO_SERIAL) Serial.println("Current: "+String(visitorCount));
  if (LOG_TO_SERIAL) Serial.println("Max: "+String(maxVisitorCount));
}

void handleVisitorEntered() {

  if (LOG_TO_SERIAL) Serial.println("Visitor Entered!");

  visitorCount++;
  displayUpdated = false;
  buzz();

  keepRedTimestamp = currentMillis;
}

void handleVisitorLeft() {

  if (visitorCount == maxVisitorCount) {

    buzz();
  }

  if (visitorCount > 0) {

      if (LOG_TO_SERIAL) Serial.println("Visitor Left!");

      visitorCount--;
  }

  displayUpdated = false;
}

void updateLights() {

  bool allow = visitorCount < maxVisitorCount;

  if (keepRedTimestamp > 0) {

    // minimum time expired?
    if (currentMillis - keepRedTimestamp > BUTTON_ENTRANCE_MIN_MS) {

        keepRedTimestamp = 0;

        if (allow) buzz();
    }
    else
      allow = false;
  }

  digitalWrite(PORT_A3_RED_LIGHT, allow ? LOW : HIGH);
  digitalWrite(PORT_A4_GREEN_LIGHT, allow ? HIGH : LOW);
}

void handleReset() {

  visitorCount = 0;
  maxVisitorCount = 20;
  displayUpdated = false;
  buzz();
}

void handleButtonUp() {

  buttonUpTimestamp = currentMillis;

  if (buttonResetNowPressed) {

    visitorCount++;
  }
  else {

    maxVisitorCount++;
  }

  displayUpdated = false;
}

void handleButtonDown() {

  buttonDownTimestamp = currentMillis;

  if (buttonResetNowPressed) {

    if (visitorCount > 0) visitorCount--;
  }
  else {

    if (maxVisitorCount > 0) maxVisitorCount--;
  }

  displayUpdated = false;
}

void initializeDisplay() {

  if (LOG_TO_SERIAL) Serial.begin(9600);

  lcd.begin(16,2);
}

void setDisplayText() {

  if (LOG_TO_SERIAL) Serial.println("updating display ");

  lcd.home ();
  lcd.print("Current : "+String(visitorCount)+"      ");
  lcd.setCursor ( 0, 1 );
  lcd.print("Maximum : "+String(maxVisitorCount)+"      ");
}
