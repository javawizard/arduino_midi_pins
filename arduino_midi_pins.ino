#include "MIDIUSB.h"

// ADJUST THIS AS NEEDED: board number. for javawizard's organ console, use:
//   10 for the CH/GT piston board
//   11 for the SW/SO piston board
//   12 for the PED toe stud board
#define MIDI_CHANNEL_NUMBER 10

typedef struct {
  bool lastState;
  bool isChanging;
  unsigned long startOfChange;
} buttonState_t;

const byte buttonPins[] = {
  // PWM pins, except pin 13 because the board LED is hooked up to that
  2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
  // Pins on the 2x wide header
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
  // Analog input pins
  A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11
};

// pin to which the status LED is attached. use the one built into the board for now.
#define LED_PIN 13
// length of buttonPins, i.e. number of pins to use as button inputs
#define BUTTON_PIN_COUNT (sizeof(buttonPins) / sizeof(byte))
// number of microseconds to wait after a button starts transitioning to consider the transition complete
#define BUTTON_CHANGE_THRESHOLD_MICROS ((unsigned long) 5000)
// number of microseconds for the LED to remain on after an event occurs
#define LED_ON_MICROS ((unsigned long) 70000)

buttonState_t *buttonState;

bool ledOn = false;
unsigned long ledOnAt;

bool readButtonState(int buttonNumber) {
  return digitalRead(buttonPins[buttonNumber]) == LOW;
}

void turnLedOn() {
  ledOn = true;
  ledOnAt = micros();
  digitalWrite(LED_PIN, HIGH);
}

void handleButtonStateChanged(int buttonNumber, bool newState) {
  // ugh how the heck do we get rid of the narrowing type conversion warnings without all the casts?
  MidiUSB.sendMIDI({newState ? (uint8_t) 0x09 : (uint8_t) 0x08, newState ? (uint8_t) (0x90 | MIDI_CHANNEL_NUMBER) : (uint8_t) (0x80 | MIDI_CHANNEL_NUMBER), (uint8_t) buttonNumber, 64});
  MidiUSB.flush();
  turnLedOn();
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  buttonState = (buttonState_t*) calloc(BUTTON_PIN_COUNT, sizeof(buttonState_t));

  for (int i = 0; i < BUTTON_PIN_COUNT; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
}

void loop() {
  for (int i = 0; i < BUTTON_PIN_COUNT; i++) {
    if (buttonState[i].isChanging) {
      // Button is in the middle of changing. Check its state to make sure it's still different from its previous state.
      bool currentState = readButtonState(i);
      if (currentState == buttonState[i].lastState) {
        // This button's state changed back to what it was before. Transition did not complete successfully, so cancel it.
        buttonState[i].isChanging = false;
      } else if (micros() - buttonState[i].startOfChange >= BUTTON_CHANGE_THRESHOLD_MICROS) {
        // This button's state is still different and it's been long enough. Consider the change succcessful.
        buttonState[i].isChanging = false;
        buttonState[i].lastState = currentState;
        handleButtonStateChanged(i, currentState);
      }
      // Otherwise, this button is changing and is different than it used to be but not enough time has elapsed yet. Keep waiting.
    } else {
      // Button is not in the middle of changing. Check to see if its state is different than the previous state we have written
      // down for it, and if so, log that it's now changing.
      if (readButtonState(i) != buttonState[i].lastState) {
        // Button just changed state. Log that the button is starting to change state.
        buttonState[i].isChanging = true;
        buttonState[i].startOfChange = micros();
      }
    }
  }

  // turn LED off if it's been LED_ON_MICROS microseconds since the last request to turn it on
  if (ledOn && micros() - ledOnAt >= LED_ON_MICROS) {
    ledOn = false;
    digitalWrite(LED_PIN, LOW);
  }
}
