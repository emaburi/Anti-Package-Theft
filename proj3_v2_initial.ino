#include <Servo.h>

// copied from https://stackoverflow.com/questions/3437404/min-and-max-in-c
#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

// Record pressure pins
const int PRESSURE_PINS[1] = {A1};

// Record Servo Pin
const int SERVO_PIN = 10;
// Record Key Switch Pin
const int KEY_PIN = 9;
// Record Roller Switch Pin
const int ROLLER_PIN = 8;
// Record Sample Button Pin
const int SAMPLE_PIN = 7;
// Initialize motor
Servo motor;
// Initialize device state variables
// Record whether a package is place in the bin
bool package_exists = false;
// Record if the lid is closed
bool lid_is_closed = false;
// Record if the bin is locked
bool bin_is_locked = false;
// Record the state of the key switch. 
// Note: 
//  - HIGH: Key in the 'lock' position
//  - LOW: Key in the 'unlock' position
int key_locked = HIGH;
// Record whether to take a sample of pressure sensors.
// This is used to set a new threshold for package detection.
//  - HIGH: Do not take sample
//  - LOW: Take sample
int take_sample = HIGH;

// Instantiate initial thresholds
// Unsigned to: (i) Increase INT_MAX, (ii) pressure should never be < 0
unsigned int pressure_threshold = 0;
// Amount of history to record
unsigned int history_amount = 10;
// Initialize pressure history sequence
unsigned int pressure_amounts[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
// Record current iteration in the device state. Also is used to update pressure amounts list
unsigned int iteration_num = 0;
// Hyper parameter indicating minimum proportion of times the pressure reading is above the 
// pressure threshold for a package to be deemed to exist.
float proportion_threshold = .7;

void setup() {
  // Dictate Modes of all buttons, motors, and switches
  Serial.begin(9600);
  motor.attach(SERVO_PIN);
  pinMode(ROLLER_PIN, INPUT_PULLUP);
  pinMode(KEY_PIN, INPUT_PULLUP);
  pinMode(SAMPLE_PIN, INPUT_PULLUP);
  motor.write(90);
}

void loop() {
  delay(200);
  // If the iteration is larger than the history amount, roll it over to the beginning.
  // This allows us to continuously update the pressure history.
  if (iteration_num >= history_amount) {
    iteration_num = 0;
  }
  //  Debugging Code: Remove for final documentation
  for (int i = 0; i < 10; i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.print(pressure_amounts[i]);
    Serial.print(" ");
    
  }
  Serial.println("");
   // Read the current pressure
  // int pressure_reading1 = analogRead(PRESSURE_PIN);
  unsigned int max_pressure_read = 0;
  for (int i = 0; i < 1; i++) {
    max_pressure_read = max(max_pressure_read, analogRead(PRESSURE_PINS[i]));
  }
  // Update pressure history with current pressure reading
  pressure_amounts[iteration_num] = max_pressure_read;
  // Read if lid is closed, lock is in closed position, and whether to take a sample
  lid_is_closed = digitalRead(ROLLER_PIN);
  key_locked = digitalRead(KEY_PIN);
  take_sample = digitalRead(SAMPLE_PIN);
  // Debugging Code: Remove for final documentation
  Serial.print("Pressure reading: ");
  Serial.println(max_pressure_read);
  Serial.print("Lid is: ");
  Serial.println(lid_is_closed);
  Serial.print("Key_locked: ");
  Serial.println(key_locked);
  Serial.print("sample pressed: ");
  Serial.println(take_sample);
  
  // Key is in the unlocked position
  if (key_locked == LOW) {
    // If the box is locked, unlock it.
    if (bin_is_locked)  {
      Serial.println("unlocking the box");
      // Indicate that the box is now unlocked. This ensures that we do not redundantly 
      // make the motor unlock an already unlocked box.       
      bin_is_locked = false;
      // Unlock box
      motor.write(90);
      // Delay to give motor time to unlock the box
      delay(1000);
    }
    // Take sample if desired. Assumes that the package is already outside the box.
    // Note that a sample can only be taken if: (i) key is in unlocked position 
    // (box is unlocked), (ii) the box lid is open (gives us an indication that
    // that the person has removed the package, (iii) person requests a sample to be taken.
    if ((lid_is_closed == HIGH) && (take_sample == LOW)) {
      // Reset pressure thresholds
      pressure_threshold = 0;
      // Debugging Code: Remove for final documentation
      Serial.print("sample: ");
      Serial.println(take_sample);
      // Compute new pressure threshold. We take the max of all pressure history to
      // try and account for 'worst case' abnormal pressure readings.
      for (int i = 0; i < history_amount; i++) {
        pressure_threshold = max(pressure_threshold, pressure_amounts[i]);
        // Debugging Code: Remove for final documentation
        Serial.print("new threshold: ");
        Serial.println(pressure_threshold);
      }
      pressure_threshold+=5;
    }
    
  }
  // box is in package detection mode
  else {
    // Possible package has been detected. 
    Serial.print("Pressure threshold: ");
    Serial.println(pressure_threshold);
    if (pressure_amounts[iteration_num] > pressure_threshold && !bin_is_locked) {
      // initial readings above threshold proportion variable to record
      // number of times pressure exceeds threshold and calculate proportion.
      float proportion_readings = 0;
      // Check pressure history to determine whether an abnormality exists.
      Serial.print("Pressure reading: ");
      for (int i = 0; i < 10; i++) {
        int pressure_reading = pressure_amounts[i];
        // Reading is above threshold
        Serial.print(pressure_reading);
        Serial.print(" , ");
        if (pressure_reading > pressure_threshold) {
          // Record observation
          Serial.println("Recording observation ");
          proportion_readings++;
        }
      }
      Serial.println();
      // Compute the average amount of time reading is above the threshold in 
      // the current history.
      proportion_readings = proportion_readings / history_amount;
      // Proportion calculated lies above the minimum propotion needed to classify
      // the readings as detecting a package.
      if (proportion_readings > proportion_threshold) {
        package_exists = true;
      }
      // Proportion lies below the thereshold, package is deemed to not exist.
      else {
        package_exists = false;
      }
    }
    // No package has been detected
    else {
      package_exists = false;
    }
    // Lock box if a package exists, the lid is closed, and the box is not already locked.
    // Note that by tracking if a box is already locked, we eliminate redundant commands to 
    // lock the box to the motor.
    Serial.print("Package exists: ");
    Serial.print(package_exists);
    Serial.print(" | lid is closed: ");
    Serial.print(lid_is_closed);
    Serial.print(" | Bin is closed: ");
    Serial.println(bin_is_locked);
    if (package_exists && (lid_is_closed == LOW) && !bin_is_locked) {
      Serial.println("locking the box");
      // Lock the box
      motor.write(15);
      // Inidicate that the box is now locked
      bin_is_locked = true;
      // Delay to give motor enough time to lock the box
      delay(1000);
    }
    // keep box unlocked
    //else {
      //bin_is_locked = false;
    //}
  }
  // Increase iteration number to next step
  iteration_num++;
  

}
