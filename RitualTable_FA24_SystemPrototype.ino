/* 
Ritual Table System Prototype by Yinglian Liu

- each reader corrsponding one LED
- corrsponding tag is detected will light up a LED and play sound effect
- mismatching target tag is detected, all leds will be flashed 3 times and play sound effect
- all taeget tags aer detected, will play a led animation and sound effect, 
  after animation the Electromagnet lock will be unlocked
- a reset card is detected, the program will be reset 

1 Mega 2560
5 RFID Readers
1 Serial MP3 Player
1 Relay
1 Electromagnet lock
1 Multiplexer
Many LEDs

*/

#include <SPI.h>
#include <MFRC522.h> // RFID library
#include <SoftwareSerial.h> // For MP3 module

// RFID reader
const uint8_t Num_Readers = 5;
// RFID reader SS pins
//const uint8_t ssPins[Num_Readers] = {23,24,25,26,28};
const uint8_t ssPins[Num_Readers] = {26, 28, 23, 24, 25};  // The pin order must be the same as the targetUIDs order
const uint8_t rstPin = 22; // Common RST pin and share the same pin

MFRC522 rfid[Num_Readers];

// Target UIDs for each reader
byte targetUIDs[Num_Readers][4] = {
  {0x81, 0x46, 0x21, 0x03},   // Reader 0 81 46 21 03
  {0x23, 0xB7, 0xE1, 0x0C},   // Reader 1 23 B7 E1 0C
  {0x93, 0x64, 0xE0, 0x0C},   // Reader 2 93 64 E0 0C
  {0xE3, 0x15, 0xD8, 0x0C},   // Reader 3 E3 15 D8 0C
  {0x43, 0x34, 0xE7, 0x0C}    // Reader 4 43 34 E7 0C  

};

byte resetUID[4] = {0xF3, 0x9A, 0x2B, 0xAB} ;// F3 9A 2B AB, this card is used for reset the program

// byte targetUIDs[Num_Readers][4] = {
//   {0x93, 0x64, 0xE0, 0x0C},   // Reader 0 93 64 E0 0C
//   {0xE3, 0x15, 0xD8, 0x0C},   // Reader 1 E3 15 D8 0C
//   {0x43, 0x34, 0xE7, 0x0C},   // Reader 2 43 34 E7 0C  
//   {0x81, 0x46, 0x21, 0x03},   // Reader 3 81 46 21 03
//   {0x23, 0xB7, 0xE1, 0x0C},   // Reader 4 23 B7 E1 0C
// };

// Define the LED pins
const uint8_t ledPins[] = {10, 9, 8, 7, 6}; // LEDs for each reader
const uint8_t ledResetPin = 5;     

// Array to track if the correct card has been detected for each reader
bool cardDetected[] = {false, false, false, false, false};

//Setting up the multiplexer to control 16 LEDs, the pins as follow//
const uint8_t S0 =  4;
const uint8_t S1 =  3;
const uint8_t S2 =  2;
const uint8_t S3 =  11;
const uint8_t sig = 12;

bool playLEDsPlayed = false; //Used to control the led animation play once

//Serial mp3 player
#define ARDUINO_RX 15 // Should connect to TX of the Serial MP3 Player module
#define ARDUINO_TX 14 // Connect to RX of the module
SoftwareSerial mySerial(ARDUINO_RX, ARDUINO_TX);
static int8_t Send_buf[8] = {0};

#define CMD_PLAY_W_INDEX 0X03
#define CMD_SET_VOLUME 0X06
#define CMD_SEL_DEV 0X09
#define DEV_TF 0X02
#define CMD_PLAY_W_VOL 0X22

//bool individualTagMatched = false; 

const uint8_t relayPin = 13;

void setup() {
  Serial.begin(115200);
  //delay(1000);
  while (!Serial);

  //Set multiplexer pins
  pinMode(sig, OUTPUT);
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);

  //relay
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  //Initialize different LED pins as output and off
  for (uint8_t i = 0; i < Num_Readers; i++) {
    pinMode(ledPins[i], OUTPUT); 
    digitalWrite(ledPins[i], LOW); 
  }
    pinMode(ledResetPin, OUTPUT); 
    digitalWrite(ledResetPin, LOW); 

  //Initialize the SDA pin aka SS pin as output mode and set the state as inactive (HIGH as inactive, LOW as active)
  for (uint8_t i=0; i < Num_Readers; i++) {
		pinMode(ssPins[i], OUTPUT);
		digitalWrite(ssPins[i], HIGH);
	}

  SPI.begin(); // Initialize SPI bus

  // Initialize RFID readers ( References: library: MFRC522 ->DumpInfo )
  for (uint8_t reader = 0; reader < Num_Readers; reader++) {
    rfid[reader].PCD_Init(ssPins[reader],rstPin);
    delay(4);
    Serial.print("RFID reader ");
    Serial.print(reader);
    Serial.println(" initialized");
    rfid[reader].PCD_DumpVersionToSerial(); /*Add this to be clear if all the readers are communicated to the microcontroller successfully
    (If it cann't be printed the infor into the serial monitor, it means the reader cannot comunicate with the microcontroller)*/
  }

  // Initialize MP3 module
  mySerial.begin(9600);
  delay(500);
  sendCommand(CMD_SEL_DEV, DEV_TF);
  delay(200);
  sendCommand(CMD_SET_VOLUME, 0x1E); 
  /*
        0	0x00
          1	0x01
          2	0x02
          3	0x03
          4	0x04
          5	0x05
          6	0x06
          7	0x07
          8	0x08
          9	0x09
          10	0x0A
          11	0x0B
          12	0x0C
          13	0x0D
          14	0x0E
          15	0x0F
          16	0x10
          17	0x11
          18	0x12
          19	0x13
          20	0x14
          21	0x15
          22	0x16
          23	0x17
          24	0x18
          25	0x19
          26	0x1A
          27	0x1B
          28	0x1C
          29	0x1D
          30	0x1E
  */
  delay(100);


}



void loop() {
  bool allCorrectCardsDetected = true;

  for (uint8_t reader = 0; reader < Num_Readers; reader++) {
    // Deactivate all readers
    for (uint8_t i = 0; i < Num_Readers; i++) {
      digitalWrite(ssPins[i], HIGH);
    }
    // Activate the current reader
    digitalWrite(ssPins[reader], LOW);

    // Small delay to allow the SPI bus to stabilize
    delay(10);

    // Check for new cards
    if (rfid[reader].PICC_IsNewCardPresent() && rfid[reader].PICC_ReadCardSerial()) {
      // Read UID
      byte* uid = rfid[reader].uid.uidByte;
      byte uidSize = rfid[reader].uid.size;

      // Print UID
      Serial.print("Reader ");
      Serial.print(reader);
      Serial.print(" detected tag UID:");
      for (byte i = 0; i < uidSize; i++) {
        Serial.print(uid[i] < 0x10 ? " 0" : " ");
        Serial.print(uid[i], HEX);
      }
        Serial.println();

      // Compare with target UID
      if (compareUID(uid, uidSize, targetUIDs[reader], 4)) {
        Serial.print("Reader ");
        Serial.print(reader);
        Serial.println(": Target tag detected!");

        //play the matched sound effect:001.mp3
        sendCommand(CMD_PLAY_W_INDEX,1);
        delay(100);

        cardDetected[reader] = true;
        digitalWrite(ledPins[reader], HIGH);

        // if(!individualTagMatched) {
        //   sendCommand(CMD_PLAY_W_INDEX,3);
        //   delay(100);
        //   individualTagMatched = true;
        // }

      } else if(compareUID(uid, uidSize, resetUID, 4)) {
        Serial.println("Reset card detected! Reseting...");

        //Run reset function
        resetToInitialState();
        playLEDsPlayed = false;
        //individualTagMatched = false;
        digitalWrite(relayPin, LOW); //Lock the MagLock after reset

      } else {
        Serial.print("Reader ");
        Serial.print(reader);
        Serial.println(": Tag does not match target.");

       //play the mismatched sound effect:002.mp3
        sendCommand(CMD_PLAY_W_INDEX, 2);
        delay(100);

        // Flash all LEDs
        flashAllLEDs(3, 100); // Flash all LEDs for 100ms intervals for 3 times
        cardDetected[reader] = false;
        //delay(100);
      }

      rfid[reader].PICC_HaltA(); // Halt the current card
      rfid[reader].PCD_StopCrypto1();
    }

    // Deactivate the current reader
    digitalWrite(ssPins[reader], HIGH);

    //If not all the reader detected the corrsponding tag, change the "allCorrectCardsDetected" to false
    if (!cardDetected[reader]) {
      allCorrectCardsDetected = false;
      //individualTagMatched = true;
    }
    
    // Keep the LED on if the correct card was previously detected
    if (cardDetected[reader]) {
      digitalWrite(ledPins[reader], HIGH);
    } else {
      digitalWrite(ledPins[reader], LOW);
    }
  }
  
  if (allCorrectCardsDetected && !playLEDsPlayed) {
    Serial.println("All correct tags detected!");
    delay(2000);

    Serial.println("Music and LED animation will be played!");
    //play the all tags matched sound effect:003.mp3
    sendCommand(CMD_PLAY_W_INDEX,3);
    delay(500);

    playLEDs();
    playLEDsPlayed = true;
    //allCorrectCardsDetected = false;
    //delay(1000);

   // individualTagMatched = false;
   digitalWrite(relayPin, HIGH);
   Serial.println("UNLOCK!");

   delay(1000);
   Serial.println("Please use the reset card to reset the program!");
    

} else {
    digitalWrite(ledResetPin, LOW);
    //digitalWrite(relayPin, LOW);
  }
}

//////////////////////////////////////////////Defind Functions///////////////////////////////////////////////////////////////////////////////////

/* -----------------Set Multiplexer Channel-----------------------*/
int SetMuxChannel(byte channel) {

  digitalWrite(S0, bitRead(channel, 0));
  digitalWrite(S1, bitRead(channel, 1));
  digitalWrite(S2, bitRead(channel, 2));
  digitalWrite(S3, bitRead(channel, 3));

}

/* -----------------Compare two UIDs-----------------------*/
bool compareUID(byte* uid1, byte uid1Size, byte* uid2, byte uid2Size) {
  if (uid1Size != uid2Size) return false;
    for (byte i = 0; i < uid1Size; i++) {
      if (uid1[i] != uid2[i]) {
         return false;
    }
  }
  return true;
}

/* ------------------Flash all LEDs---------------------*/
void flashAllLEDs(uint8_t times, unsigned long duration) {
  for (uint8_t t = 0; t < times; t++) {
    for (uint8_t i = 0; i < Num_Readers; i++) {
      digitalWrite(ledPins[i], HIGH);
    }
      delay(duration);
    for (uint8_t i = 0; i < Num_Readers; i++) {
      digitalWrite(ledPins[i], LOW);
    }
      delay(duration);
  }
}

/* ------------------Reset to Initial State---------------------*/
void resetToInitialState() {
  // reset all detected tags and cards to false
  for (uint8_t i = 0; i < Num_Readers; i++) {

    cardDetected[i] = false;
    digitalWrite(ledPins[i], LOW);

  }

    digitalWrite(ledResetPin, HIGH);
    delay(1500);
    digitalWrite(ledResetPin, LOW);
    delay(500);
    Serial.println("Program has been reset...");

}


/* ------------------------Play LED Animation---------------------------*/
void playLEDs () {

  //control the row of leds play forword and backword for 4 times when all the tags are placed correlly
    for( uint8_t repeat = 0; repeat < 18; repeat++){
        for (byte i = 0; i< 16; i++){
          //Serial.println(i);
          SetMuxChannel(i);
          digitalWrite(sig, HIGH);
          delay(30);
          digitalWrite(sig, LOW);
          delay(30);

        }

        for (uint8_t i = 15; i > 0; i--) {
        // Serial.println(i);
          SetMuxChannel(i);
          digitalWrite(sig, HIGH);
          delay(25);
          digitalWrite(sig, LOW);
          delay(25);
         }

    } 
}


/* ------------------Sending MP3 Command---------------------*/
void sendCommand(int8_t command, int16_t dat) {
  delay(20);
  Send_buf[0] = 0x7e; // Starting byte
  Send_buf[1] = 0xff; // Version
  Send_buf[2] = 0x06; // The number of bytes of the command without starting and ending bytes
  Send_buf[3] = command;
  Send_buf[4] = 0x00; // 0x00 = no feedback, 0x01 = feedback
  Send_buf[5] = (int8_t)(dat >> 8); // Data high byte
  Send_buf[6] = (int8_t)(dat); // Data low byte
  Send_buf[7] = 0xef; // Ending byte
  for (uint8_t i = 0; i < 8; i++) {
    mySerial.write(Send_buf[i]);
  }
}
