/*
   THIS CODE REQUIRES "IR REMOTE" LIBRARY TO WORK!!!!!!!!!!!!
   GET IT HERE: https://github.com/z3t0/Arduino-IRremote.git
   Instructions:

    Normal Mode:
      1. Receiver is always "sniffing" signals
      2. Press button to send out a captured code
    Autonomous Mode:
      1. Press and hold button until bright solid light.
      2. Set the time interval by pressing the button.
         Each press adds five seconds.
      3. After done with interval, press and hold button
         until link blinks brightly twice.
      4. Autonomous mode is now active.
*/

#include <IRremote.h>

int RECV_PIN = 11;
int BUTTON_PIN = 12;
int STATUS_PIN = 13;

IRrecv irrecv(RECV_PIN);
IRsend irsend;

decode_results results;

void setup()
{
  Serial.begin(9600);
  irrecv.enableIRIn(); // Start the receiver
  pinMode(BUTTON_PIN, INPUT);
  pinMode(STATUS_PIN, OUTPUT);
}

// Storage for the recorded code
int codeType = -1; // The type of code
unsigned long codeValue; // The code value if not raw
unsigned int rawCodes[RAWBUF]; // The durations if raw
int codeLen; // The length of the code
int toggle = 0; // The RC5/6 toggle state

// Stores the code for later playback
// Most of this code is just logging
void storeCode(decode_results *results) {
  codeType = results->decode_type;
  int count = results->rawlen;
  if (codeType == UNKNOWN) {
    Serial.println("Received unknown code, saving as raw");
    codeLen = results->rawlen - 1;
    // To store raw codes:
    // Drop first value (gap)
    // Convert from ticks to microseconds
    // Tweak marks shorter, and spaces longer to cancel out IR receiver distortion
    for (int i = 1; i <= codeLen; i++) {
      if (i % 2) {
        // Mark
        rawCodes[i - 1] = results->rawbuf[i] * USECPERTICK - MARK_EXCESS;
        Serial.print(" m");
      }
      else {
        // Space
        rawCodes[i - 1] = results->rawbuf[i] * USECPERTICK + MARK_EXCESS;
        Serial.print(" s");
      }
      Serial.print(rawCodes[i - 1], DEC);
    }
    Serial.println("");
  }
  else {
    if (codeType == NEC) {
      Serial.print("Received NEC: ");
      if (results->value == REPEAT) {
        // Don't record a NEC repeat value as that's useless.
        Serial.println("repeat; ignoring.");
        return;
      }
    }
    else if (codeType == SONY) {
      Serial.print("Received SONY: ");
    }
    else if (codeType == PANASONIC) {
      Serial.print("Received PANASONIC: ");
    }
    else if (codeType == JVC) {
      Serial.print("Received JVC: ");
    }
    else if (codeType == RC5) {
      Serial.print("Received RC5: ");
    }
    else if (codeType == RC6) {
      Serial.print("Received RC6: ");
    }
    else {
      Serial.print("Unexpected codeType ");
      Serial.print(codeType, DEC);
      Serial.println("");
    }
    Serial.println(results->value, HEX);
    codeValue = results->value;
    codeLen = results->bits;
  }
}

void sendCode(int repeat) {
  if (codeType == NEC) {
    if (repeat) {
      irsend.sendNEC(REPEAT, codeLen);
      Serial.println("Sent NEC repeat");
    }
    else {
      irsend.sendNEC(codeValue, codeLen);
      Serial.print("Sent NEC ");
      Serial.println(codeValue, HEX);
    }
  }
  else if (codeType == SONY) {
    irsend.sendSony(codeValue, codeLen);
    Serial.print("Sent Sony ");
    Serial.println(codeValue, HEX);
  }
  else if (codeType == PANASONIC) {
    irsend.sendPanasonic(codeValue, codeLen);
    Serial.print("Sent Panasonic");
    Serial.println(codeValue, HEX);
  }
  else if (codeType == JVC) {
    irsend.sendJVC(codeValue, codeLen, false);
    Serial.print("Sent JVC");
    Serial.println(codeValue, HEX);
  }
  else if (codeType == RC5 || codeType == RC6) {
    if (!repeat) {
      // Flip the toggle bit for a new button press
      toggle = 1 - toggle;
    }
    // Put the toggle bit into the code to send
    codeValue = codeValue & ~(1 << (codeLen - 1));
    codeValue = codeValue | (toggle << (codeLen - 1));
    if (codeType == RC5) {
      Serial.print("Sent RC5 ");
      Serial.println(codeValue, HEX);
      irsend.sendRC5(codeValue, codeLen);
    }
    else {
      irsend.sendRC6(codeValue, codeLen);
      Serial.print("Sent RC6 ");
      Serial.println(codeValue, HEX);
    }
  }
  else if (codeType == UNKNOWN /* i.e. raw */) {
    // Assume 38 KHz
    irsend.sendRaw(rawCodes, codeLen, 38);
    Serial.println("Sent raw");
  }
}

int lastButtonState = LOW;
unsigned long startTime = 0;
int interVal = 0;
unsigned long lastUn = 0;

void loop() {
  // If button pressed, send the code.
  int buttonState = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && buttonState == LOW) {
    Serial.println("Released");
    irrecv.enableIRIn(); // Re-enable receiver
  }

  if (buttonState) {
    Serial.println("Pressed, sending");
    digitalWrite(STATUS_PIN, HIGH);
    sendCode(lastButtonState == buttonState);
    digitalWrite(STATUS_PIN, LOW);
    delay(50); // Wait a bit between retransmissions

    if (lastButtonState == LOW) {
      startTime = millis();
    }
    if (millis() - startTime >= 5000) {
      digitalWrite(STATUS_PIN, HIGH);
      delay(3000);
      digitalWrite(STATUS_PIN, LOW);
      boolean gotIt = false;
      lastButtonState == LOW;
      while (gotIt == false) {
        buttonState = digitalRead(BUTTON_PIN);
        if (lastButtonState == HIGH && buttonState == LOW) {
          digitalWrite(STATUS_PIN, HIGH);
          interVal++;
          delay(500);
          digitalWrite(STATUS_PIN, LOW);
        }
        if (buttonState) {
          if (lastButtonState == LOW) {
            startTime = millis();
          }
          if (millis() - startTime >= 5000) {
            digitalWrite(STATUS_PIN, HIGH);
            delay(500);
            digitalWrite(STATUS_PIN, LOW);
            delay(500);
            digitalWrite(STATUS_PIN, HIGH);
            delay(500);
            digitalWrite(STATUS_PIN, LOW);
            gotIt = true;
          }
        }
        lastButtonState = buttonState;
      }
      gotIt = false;
      lastButtonState = LOW;
      lastUn = millis();
      irrecv.enableIRIn(); // Re-enable receiver
      Serial.println("NOW AUTONOMOUS");
      while (gotIt == false) {

        buttonState = digitalRead(BUTTON_PIN);
        if (buttonState) {
          if (lastButtonState == LOW) {
            startTime = millis();
            Serial.println("Reset millis:");
            Serial.println(millis());
          }
          if (millis() - startTime >= 5000) {
            digitalWrite(STATUS_PIN, HIGH);
            delay(500);
            digitalWrite(STATUS_PIN, LOW);
            delay(500);
            digitalWrite(STATUS_PIN, HIGH);
            delay(500);
            digitalWrite(STATUS_PIN, LOW);
            gotIt = true;
          }
        }
        if ((millis() - lastUn) >= (abs(interVal) * 5000)) {
          lastUn = millis();
          Serial.println(millis());
          Serial.println("Pressed, sending");
          digitalWrite(STATUS_PIN, HIGH);
          sendCode(false);
          digitalWrite(STATUS_PIN, LOW);
          Serial.println("Released");
          irrecv.enableIRIn(); // Re-enable receiver
        }

        if (irrecv.decode(&results)) {
          digitalWrite(STATUS_PIN, HIGH);
          storeCode(&results);
          irrecv.resume(); // resume receiver
          digitalWrite(STATUS_PIN, LOW);
        }
        lastButtonState = buttonState;





      }
    }
  }
  else if (irrecv.decode(&results)) {
    digitalWrite(STATUS_PIN, HIGH);
    storeCode(&results);
    irrecv.resume(); // resume receiver
    digitalWrite(STATUS_PIN, LOW);
  }
  lastButtonState = buttonState;
}