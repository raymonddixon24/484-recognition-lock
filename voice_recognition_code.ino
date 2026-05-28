#include <Wire.h>
#include <avr/wdt.h>
#include "Adafruit_VL53L0X.h"
#include "DFRobot_DF2301Q.h"

Adafruit_VL53L0X lox = Adafruit_VL53L0X();
DFRobot_DF2301Q_I2C voice;

#define ALERT_PIN 6
#define TRIGGER_PIN 4
#define THRESHOLD_MM 1000

unsigned long triggerStart = 0;
bool triggerActive = false;
unsigned long badReadingStart = 0;
bool badReading = false;

void softReset() {
  wdt_enable(WDTO_15MS);
  while (1);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(1); }

  pinMode(ALERT_PIN, OUTPUT);
  digitalWrite(ALERT_PIN, LOW);

  pinMode(TRIGGER_PIN, OUTPUT);
  digitalWrite(TRIGGER_PIN, LOW);

  Wire.begin();

  if (!voice.begin()) {
    Serial.println("Failed to init DF2301Q");
    while (1);
  }
  voice.setVolume(20);
  voice.setWakeTime(20);
  Serial.println("DF2301Q ready");

  if (!lox.begin()) {
    Serial.println("Failed to boot VL53L0X");
    while (1);
  }
  lox.startRangeContinuous();
  Serial.println("VL53L0X ready");
}

void loop() {
  // --- Continuous distance reading ---
  int distance = -1;
  if (lox.isRangeComplete()) {
    distance = lox.readRange();

    if (distance > 0 && distance <= 1200) {
      badReading = false;  // good reading, reset bad timer
      Serial.print("Distance in mm: ");
      Serial.println(distance);
    } else {
      // Bad reading, start or check timer
      if (!badReading) {
        badReading = true;
        badReadingStart = millis();
      }
      if (millis() - badReadingStart >= 3000) {
        Serial.println("Sensor stuck, resetting board...");
        softReset();
      }
    }
  }

  // --- Voice commands ---
  uint8_t cmd = voice.getCMDID();
  if (cmd != 0) {
    Serial.print("Voice CMD ID: ");
    Serial.println(cmd);
  }

  switch (cmd) {
    case 5:
      if (distance > 0 && distance <= THRESHOLD_MM) {
        Serial.println("Unlock triggered!");
        digitalWrite(TRIGGER_PIN, HIGH);
        triggerStart = millis();
        triggerActive = true;
      } else {
        Serial.println("Out of range, unlock denied");
      }
      break;
  }

  // --- Turn off trigger pin after 5 seconds non-blocking ---
  if (triggerActive && millis() - triggerStart >= 5000) {
    digitalWrite(TRIGGER_PIN, LOW);
    triggerActive = false;
    Serial.println("Trigger reset");
  }
}