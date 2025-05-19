// Original code: Viral Science www.viralsciencecreativity.com www.youtube.com/c/viralscience
// Arduino Matrix 8x8 Display Hour Glass Project
// Edited by Yaser Ali Husen

#include "Arduino.h"
#include <MPU6050_tockn.h>
#include "LedControl.h"
#include "Delay.h"

#define  MATRIX_A  1
#define MATRIX_B  0

//------------------For display-------------------
#include <TM1637Display.h>
// Define the connections pins
#define CLK 2
#define DIO 3
// Create a display object of type TM1637Display
TM1637Display display = TM1637Display(CLK, DIO);
// Create an array that turns all segments ON
const uint8_t allON[] = {0xff, 0xff, 0xff, 0xff};
// Create an array that turns all segments OFF
const uint8_t allOFF[] = {0x00, 0x00, 0x00, 0x00};
// Create array for Stop
const uint8_t Stop[] = {
  SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,            // S
  SEG_F | SEG_G | SEG_E | SEG_D,                    // t
  SEG_G | SEG_E | SEG_D | SEG_C,                    // o
  SEG_A | SEG_B | SEG_F | SEG_G | SEG_E             // p
};
// Create array for test
const uint8_t test[] = {
  SEG_F | SEG_G | SEG_E | SEG_D,                    // t
  SEG_A | SEG_B | SEG_G | SEG_F | SEG_E | SEG_D,    // e
  SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,            // s
  SEG_F | SEG_G | SEG_E | SEG_D                     // t
};
// Create array for Hold
const uint8_t Hold[] = {
  SEG_F | SEG_G | SEG_B | SEG_E | SEG_C,            // H
  SEG_G | SEG_E | SEG_D | SEG_C,                    // o
  SEG_F | SEG_E | SEG_D,                            // l
  SEG_B | SEG_G | SEG_E | SEG_D  | SEG_C            // d
};
// Create array for drop
const uint8_t drop[] = {
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,            // P
  SEG_A | SEG_B | SEG_G | SEG_F,                    // o
  SEG_B | SEG_G,                                    // r
  SEG_A | SEG_B | SEG_G | SEG_F  | SEG_E            // d
};
int no;
bool drop_stat;
//------------------For display-------------------

MPU6050 mpu6050(Wire);

// Values are 260/330/400
#define ACC_THRESHOLD_LOW -25
#define ACC_THRESHOLD_HIGH 25

// Matrix
#define PIN_DATAIN 5
#define PIN_CLK 4
#define PIN_LOAD 6

// Accelerometer
#define PIN_X  mpu6050.getAngleX()
#define PIN_Y  mpu6050.getAngleY()

// Rotary Encoder
#define PIN_ENC_1 3
#define PIN_ENC_2 2
#define PIN_ENC_BUTTON 7

#define PIN_BUZZER 14

// This takes into account how the matrixes are mounted
#define ROTATION_OFFSET 90

// in milliseconds
#define DEBOUNCE_THRESHOLD 500

#define DELAY_FRAME 100

#define DEBUG_OUTPUT 1

#define MODE_HOURGLASS 0
#define MODE_SETMINUTES 1
#define MODE_SETHOURS 2

byte delayHours = 0;
byte delayMinutes = 1;
int mode = MODE_HOURGLASS;
int gravity;
LedControl lc = LedControl(PIN_DATAIN, PIN_CLK, PIN_LOAD, 2);
NonBlockDelay d;
int resetCounter = 0;
bool alarmWentOff = false;


/**
   Get delay between particle drops (in seconds)
*/
long getDelayDrop() {
  // since we have exactly 60 particles we don't have to multiply by 60 and then divide by the number of particles again :)
  return delayMinutes + delayHours * 60;
}


#if DEBUG_OUTPUT
void printmatrix() {
  Serial.println(" 0123-4567 ");
  for (int y = 0; y < 8; y++) {
    if (y == 4) {
      Serial.println("|----|----|");
    }
    Serial.print(y);
    for (int x = 0; x < 8; x++) {
      if (x == 4) {
        Serial.print("|");
      }
      Serial.print(lc.getXY(0, x, y) ? "X" : " ");
    }
    Serial.println("|");
  }
  Serial.println("-----------");
}
#endif



coord getDown(int x, int y) {
  coord xy;
  xy.x = x - 1;
  xy.y = y + 1;
  return xy;
}
coord getLeft(int x, int y) {
  coord xy;
  xy.x = x - 1;
  xy.y = y;
  return xy;
}
coord getRight(int x, int y) {
  coord xy;
  xy.x = x;
  xy.y = y + 1;
  return xy;
}



bool canGoLeft(int addr, int x, int y) {
  if (x == 0) return false; // not available
  return !lc.getXY(addr, getLeft(x, y)); // you can go there if this is empty
}
bool canGoRight(int addr, int x, int y) {
  if (y == 7) return false; // not available
  return !lc.getXY(addr, getRight(x, y)); // you can go there if this is empty
}
bool canGoDown(int addr, int x, int y) {
  if (y == 7) return false; // not available
  if (x == 0) return false; // not available
  if (!canGoLeft(addr, x, y)) return false;
  if (!canGoRight(addr, x, y)) return false;
  return !lc.getXY(addr, getDown(x, y)); // you can go there if this is empty
}



void goDown(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getDown(x, y), true);
}
void goLeft(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getLeft(x, y), true);
}
void goRight(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getRight(x, y), true);
}


int countParticles(int addr) {
  int c = 0;
  for (byte y = 0; y < 8; y++) {
    for (byte x = 0; x < 8; x++) {
      if (lc.getXY(addr, x, y)) {
        c++;
      }
    }
  }
  return c;
}


bool moveParticle(int addr, int x, int y) {
  if (!lc.getXY(addr, x, y)) {
    return false;
  }

  bool can_GoLeft = canGoLeft(addr, x, y);
  bool can_GoRight = canGoRight(addr, x, y);

  if (!can_GoLeft && !can_GoRight) {
    return false; // we're stuck
  }

  bool can_GoDown = canGoDown(addr, x, y);

  if (can_GoDown) {
    goDown(addr, x, y);
  } else if (can_GoLeft && !can_GoRight) {
    goLeft(addr, x, y);
  } else if (can_GoRight && !can_GoLeft) {
    goRight(addr, x, y);
  } else if (random(2) == 1) { // we can go left and right, but not down
    goLeft(addr, x, y);
  } else {
    goRight(addr, x, y);
  }
  return true;
}



void fill(int addr, int maxcount) {
  int n = 8;
  byte x, y;
  int count = 0;
  for (byte slice = 0; slice < 2 * n - 1; ++slice) {
    byte z = slice < n ? 0 : slice - n + 1;
    for (byte j = z; j <= slice - z; ++j) {
      y = 7 - j;
      x = (slice - j);
      lc.setXY(addr, x, y, (++count <= maxcount));
    }
  }
}



/**
   Detect orientation using the accelerometer

       | up | right | left | down |
   --------------------------------
   400 |    |       | y    | x    |
   330 | y  | x     | x    | y    |
   260 | x  | y     |      |      |
*/
int getGravity() {
  int x = mpu6050.getAngleX();
  int y = mpu6050.getAngleY();
  if (y < ACC_THRESHOLD_LOW)  {
    return 90;
  }
  if (x > ACC_THRESHOLD_HIGH) {
    return 0;
  }
  if (y > ACC_THRESHOLD_HIGH) {
    return 270;
  }
  if (x < ACC_THRESHOLD_LOW)  {
    return 180;
  }
}


int getTopMatrix() {
  return (getGravity() == 90) ? MATRIX_A : MATRIX_B;
}
int getBottomMatrix() {
  return (getGravity() != 90) ? MATRIX_A : MATRIX_B;
}



void resetTime() {
  for (byte i = 0; i < 2; i++) {
    lc.clearDisplay(i);
  }
  fill(getTopMatrix(), 60);
  d.Delay(getDelayDrop() * 1000);
}



/**
   Traverse matrix and check if particles need to be moved
*/
bool updateMatrix() {
  int n = 8;
  bool somethingMoved = false;
  byte x, y;
  bool direction;
  for (byte slice = 0; slice < 2 * n - 1; ++slice) {
    direction = (random(2) == 1); // randomize if we scan from left to right or from right to left, so the grain doesn't always fall the same direction
    byte z = slice < n ? 0 : slice - n + 1;
    for (byte j = z; j <= slice - z; ++j) {
      y = direction ? (7 - j) : (7 - (slice - j));
      x = direction ? (slice - j) : j;
      // for (byte d=0; d<2; d++) { lc.invertXY(0, x, y); delay(50); }
      if (moveParticle(MATRIX_B, x, y)) {
        somethingMoved = true;
      };
      if (moveParticle(MATRIX_A, x, y)) {
        somethingMoved = true;
      }
    }
  }
  return somethingMoved;
}



/**
   Let a particle go from one matrix to the other
*/
boolean dropParticle() {
  if (d.Timeout()) {
    d.Delay(getDelayDrop() * 1000);
    if (gravity == 0 || gravity == 180) {
      if ((lc.getRawXY(MATRIX_A, 0, 0) && !lc.getRawXY(MATRIX_B, 7, 7)) ||
          (!lc.getRawXY(MATRIX_A, 0, 0) && lc.getRawXY(MATRIX_B, 7, 7))
         ) {
        // for (byte d=0; d<8; d++) { lc.invertXY(0, 0, 7); delay(50); }
        lc.invertRawXY(MATRIX_A, 0, 0);
        lc.invertRawXY(MATRIX_B, 7, 7);
        tone(PIN_BUZZER, 440, 10);
        return true;
      }
    }
  }
  return false;
}



void alarm() {
  for (int i = 0; i < 5; i++) {
    tone(PIN_BUZZER, 440, 200);
    delay(1000);
  }
}



void resetCheck() {
  int z = analogRead(A3);
  if (z > ACC_THRESHOLD_HIGH || z < ACC_THRESHOLD_LOW) {
    resetCounter++;
    Serial.println(resetCounter);
  } else {
    resetCounter = 0;
  }
  if (resetCounter > 20) {
    Serial.println("RESET!");
    resetTime();
    resetCounter = 0;
  }
}



void displayLetter(char letter, int matrix) {
  // Serial.print("Letter: ");
  // Serial.println(letter);
  lc.clearDisplay(matrix);
  lc.setXY(matrix, 1, 4, true);
  lc.setXY(matrix, 2, 3, true);
  lc.setXY(matrix, 3, 2, true);
  lc.setXY(matrix, 4, 1, true);

  lc.setXY(matrix, 3, 6, true);
  lc.setXY(matrix, 4, 5, true);
  lc.setXY(matrix, 5, 4, true);
  lc.setXY(matrix, 6, 3, true);

  if (letter == 'M') {
    lc.setXY(matrix, 4, 2, true);
    lc.setXY(matrix, 4, 3, true);
    lc.setXY(matrix, 5, 3, true);
  }
  if (letter == 'H') {
    lc.setXY(matrix, 3, 3, true);
    lc.setXY(matrix, 4, 4, true);
  }
}



void renderSetMinutes() {
  fill(getTopMatrix(), delayMinutes);
  displayLetter('M', getBottomMatrix());
}
void renderSetHours() {
  fill(getTopMatrix(), delayHours);
  displayLetter('H', getBottomMatrix());
}




void knobClockwise() {
  Serial.println("Clockwise");
  if (mode == MODE_SETHOURS) {
    delayHours = constrain(delayHours + 1, 0, 64);
    renderSetHours();
  } else if (mode == MODE_SETMINUTES) {
    delayMinutes = constrain(delayMinutes + 1, 0, 64);
    renderSetMinutes();
  }
  Serial.print("Delay: ");
  Serial.println(getDelayDrop());
}
void knobCounterClockwise() {
  Serial.println("Counterclockwise");
  if (mode == MODE_SETHOURS) {
    delayHours = constrain(delayHours - 1, 0, 64);
    renderSetHours();
  } else if (mode == MODE_SETMINUTES) {
    delayMinutes = constrain(delayMinutes - 1, 0, 64);
    renderSetMinutes();
  }
  Serial.print("Delay: ");
  Serial.println(getDelayDrop());
}



volatile int lastEncoded = 0;
volatile long encoderValue = 0;
long lastencoderValue = 0;
long lastValue = 0;
void updateEncoder() {
  int MSB = digitalRead(PIN_ENC_1); //MSB = most significant bit
  int LSB = digitalRead(PIN_ENC_2); //LSB = least significant bit

  int encoded = (MSB << 1) | LSB; //converting the 2 pin value to single number
  int sum  = (lastEncoded << 2) | encoded; //adding it to the previous encoded value

  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue--;
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue++;

  // Serial.print("Value: ");
  // Serial.println(encoderValue);
  if ((encoderValue % 4) == 0) {
    int value = encoderValue / 4;
    if (value > lastValue) knobClockwise();
    if (value < lastValue) knobCounterClockwise();
    lastValue = value;
  }
  lastEncoded = encoded; //store this value for next time
}



/**
   Button callback (incl. software debouncer)
   This switches between the modes (normal, set minutes, set hours)
*/
volatile unsigned long lastButtonPushMillis;
void buttonPush() {
  if ((long)(millis() - lastButtonPushMillis) >= DEBOUNCE_THRESHOLD) {
    mode = (mode + 1) % 3;
    Serial.print("Switched mode to: ");
    Serial.println(mode);
    lastButtonPushMillis = millis();

    if (mode == MODE_SETMINUTES) {
      lc.backup(); // we only need to back when switching from MODE_HOURGLASS->MODE_SETMINUTES
      renderSetMinutes();
    }
    if (mode == MODE_SETHOURS) {
      renderSetHours();
    }
    if (mode == MODE_HOURGLASS) {
      lc.clearDisplay(0);
      lc.clearDisplay(1);
      lc.restore();
      resetTime();
    }
  }
}



/**
   Setup
*/
void setup() {

  // Set the brightness to 5 (0=dimmest 7=brightest)
  display.setBrightness(5);
  // Set all segments ON
  display.setSegments(allON);
  delay(1000);
  display.setSegments(allOFF);
  delay(1000);
  // Test Hold
  display.setSegments(Hold);
  delay(1000);
  // Test Stop
  display.setSegments(Stop);
  delay(1000);
  display.setSegments(test);
  delay(1000);
  //Counting 60
  int i;
  no = 60;
  for (i = 0; i < 60; i++) {
    no = no - 1;
    if ( (no % 2) == 0) {
      // Display the current time in 24 hour format with leading zeros enabled and a center colon:
      display.showNumberDecEx(no, 0b11100000, true, 4, 0);
      //display.showNumberDec(no, 0b11100000, true);
    }
    else {
      display.showNumberDec(no, true); // Prints displaytime without center colon.
    }
    delay(100);
  }


  mpu6050.calcGyroOffsets(true);
  // Serial.begin(9600);
  mpu6050.begin();

  // while (!Serial) {
  //   ; // wait for serial port to connect. Needed for native USB
  // }

  // setup rotary encoder
  pinMode(PIN_ENC_1, INPUT);
  pinMode(PIN_ENC_2, INPUT);
  pinMode(PIN_ENC_BUTTON, INPUT);
  digitalWrite(PIN_ENC_1, HIGH); //turn pullup resistor on
  digitalWrite(PIN_ENC_2, HIGH); //turn pullup resistor on
  digitalWrite(PIN_ENC_BUTTON, HIGH); //turn pullup resistor on
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_1), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_2), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_BUTTON), buttonPush, RISING);

  // Serial.println(digitalPinToInterrupt(PIN_ENC_1));
  // Serial.println(digitalPinToInterrupt(PIN_ENC_2));
  // Serial.println(digitalPinToInterrupt(PIN_ENC_BUTTON));

  randomSeed(analogRead(A0));

  // init displays
  for (byte i = 0; i < 2; i++) {
    lc.shutdown(i, false);
    lc.setIntensity(i, 0);
  }

  resetTime();
}



/**
   Main loop
*/
void loop() {
  mpu6050.update();
  Serial.println("angleX : ");
  Serial.println(mpu6050.getAngleX());
  Serial.println("\tangleY : ");
  Serial.println(mpu6050.getAngleY());

  delay(DELAY_FRAME);


  // update the driver's rotation setting. For the rest of the code we pretend "down" is still 0,0 and "up" is 7,7
  gravity = getGravity();
  lc.setRotation((ROTATION_OFFSET + gravity) % 360);

  // handle special modes
  if (mode == MODE_SETMINUTES) {
    renderSetMinutes(); return;
  } else if (mode == MODE_SETHOURS) {
    renderSetHours(); return;
  }

  // resetCheck(); // reset now happens when pushing a button
  bool moved = updateMatrix();
  bool dropped = dropParticle();

  // alarm when everything is in the bottom part
  if (!moved && !dropped && !alarmWentOff && (countParticles(getTopMatrix()) == 0)) {
    alarmWentOff = true;
    alarm();
  }
  // reset alarm flag next time a particle was dropped
  if (dropped) {
    alarmWentOff = false;
  }

  // for display--------------
  no = countParticles(getTopMatrix());
  if (no == 60) {
    no = 100;
  }
  else {
    no = countParticles(getTopMatrix());
  }

  if (no == 100) {
    drop_stat = true;
  }

  if (no == 0) {
    drop_stat = false;
  }

  //if drop show drop text
  if (drop_stat == true and no < 100) {
    display.setSegments(drop);
  }
  else {
    if ( (no % 2) == 0) {
      // Display the current time in 24 hour format with leading zeros enabled and a center colon:
      display.showNumberDecEx(no, 0b11100000, true, 4, 0);
    }
    else {
      display.showNumberDec(no, true); // Prints displaytime without center colon.
    }
  }
}
