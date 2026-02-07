#include <battery_graphics.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>
#include <Ps3Controller.h>
#include <Ramp.h>

/*
  LED VARIABLES
*/
#define R 5
#define G 18
#define B 19

enum Led_State {
  IDLE,
  CLOSED,
  ATK,
  BLUE,
  RED,
  ALL,
  TURQUOISE
};

enum Led_State led_state = IDLE;

/*
  SERVO VARIABLES
*/

#define rs 0
#define rb 1

#define ls 2
#define lb 3

#define w 4

#define rh 5
#define rf 6

#define lh 7
#define lf 8

int servo_pins[] = { 13, 12, 14, 27, 26, 25, 33, 15, 2 };

Servo s_rs;
Servo s_rb;

Servo s_ls;
Servo s_lb;

Servo s_w;

Servo s_rh;
Servo s_rf;

Servo s_lh;
Servo s_lf;

Servo joints[] = {
    s_rs, s_rb,
    s_ls, s_lb,
    s_w,
    s_rh, s_rf,
    s_lh, s_lf
};

int std_pos[] = { 20, 145, 160, 35, 95, 60, 40, 130, 130 };
int gaucho_pos[] = { 20, 145, 160, 35, 95, 80, 60, 100, 100 };
int crouch_pos[] = { 20, 145, 160, 35, 95, 135, 115, 45, 45 };

/*
  BATTERY MONITORING VARIABLES
*/

#define battery 35
#define K 100

float readings[K];
int reading_idx = 0;

Adafruit_SSD1306 lcd(128, 64, &Wire, -1);

/*
  BATTERY DISPLAY FUNCTIONS
*/

/**
 * @brief Displays current voltage on connected OLED display.
 * 
 * Reads the current voltage of the battery
 * and depending on which bin it lands in, displays a graphic
 * corresponding to charge on the OLED display.
 * [3050, 4095]: full charge graphic,
 * [2800, 3050): two bar charge graphic,
 * [2550, 2800): one bar charge graphic (blinking),
 * [0, 2550): empty charge graphic (blinking).
 * Filtering of size K is used, such that readings are 
 * stored on an array of size K and the average of the
 * array is taken as the output value.
*/
void Display_Voltage() {
  readings[reading_idx] = analogRead(battery);
  float voltage = 0;
  for (int i = 0; i < K; i++) voltage += readings[i];
  voltage /= K;

  lcd.clearDisplay();

  if (voltage >= 3050) lcd.drawBitmap(0, 0, full_charge, 128, 64, 1);
  else if (voltage >= 2800) lcd.drawBitmap(0, 0, two_bar_charge, 128, 64, 1);
  else if (voltage >= 2550) {
    if (millis() % 2000 < 1000) lcd.drawBitmap(0, 0, one_bar_charge, 128, 64, 1);
  }
  else {
    if (millis() % 2000 < 1000) lcd.drawBitmap(0, 0, empty_charge, 128, 64, 1);
  }

  lcd.display();

  reading_idx = (reading_idx + 1) % K;
}

/**
 * @brief Animate pairing message on connected OLED display.
 * 
 * Prints to the OLED display "Waiting to pair"
 * followed by an amount of periods equal to the passed in 
 * phase value.
 * 
 * @param phase The input phase (0-3)
*/
void Waiting_To_Pair(int phase) {
	lcd.clearDisplay();
	lcd.setCursor(0, 54);
	switch (phase) {
		case 0:
			lcd.print("Waiting to pair");
			break;
		case 1:
			lcd.print("Waiting to pair.");
			break;
		case 2:
			lcd.print("Waiting to pair..");
			break;
		case 3:
			lcd.print("Waiting to pair...");
			break;
	}
	lcd.display();
}

/*
  ANIMATIONS FUNCTIONS
*/

// Movement States
bool crouched = false;

// Locomotion Functions

/**
 * @brief Turn left via legs.
 * 
 * Turn in place left via a two beat pattern. 
 * 
 * First beat raises the body via the feet 
 * and returns the waist to the original 
 * position. 
 * 
 * Second beat lowers the body to the default
 * position while turning the waist left.
 * 
 * @param spd The speed in milliseconds to cycle 
 * through all the phases.
*/
void Left(int spd) {
  unsigned long delta = millis() % spd;
  if (delta < spd/2) {
    // Raise Body and Reorient
    s_lf.write(gaucho_pos[lf]-20);
    s_rf.write(gaucho_pos[rf]+20);
    s_w.write(gaucho_pos[w]);
  }
  else {
    // Lower Body and Turn Left
    s_lf.write(gaucho_pos[lf]);
    s_rf.write(gaucho_pos[rf]);
    s_w.write(gaucho_pos[w]+80);
  }

  // Fix Rest of Body
  for (int i = 0; i < 9; i++) {
    if (i != lf && i != rf && i != w) joints[i].write(gaucho_pos[i]);
  }
}

/**
 * @brief Turn right via legs.
 * 
 * Turn in place right via a two beat pattern. 
 * 
 * First beat raises the body via the feet 
 * and returns the waist to the original 
 * position. 
 * 
 * Second beat lowers the body to the default
 * position while turning the waist right.
 * 
 * @param spd The speed in milliseconds to cycle 
 * through all the phases.
*/
void Right(int spd) {
  unsigned long delta = millis() % spd;
  if (delta < spd/2) {
    // Raise Body and Reorient
    s_lf.write(gaucho_pos[lf]-20);
    s_rf.write(gaucho_pos[rf]+20);
    s_w.write(gaucho_pos[w]);
  }
  else {
    // Lower Body and Turn Right
    s_lf.write(gaucho_pos[lf]);
    s_rf.write(gaucho_pos[rf]);
    s_w.write(gaucho_pos[w]-80);
  }

  // Fix Rest of Body
  for (int i = 0; i < 9; i++) {
    if (i != lf && i != rf && i != w) joints[i].write(gaucho_pos[i]);
  }
}

/**
 * @brief Move left via legs.
 * 
 * Move left via a two beat pattern.
 * 
 * First beat moves the right hip and foot
 * outward and upward, thrusting the body in
 * the left direction. The left hip and foot
 * are extended outward as well but to stabilize
 * the body.
 * 
 * Second beat returns the body to default position.
 * 
 * @param spd The speed in milliseconds to cycle 
 * through all the phases.
*/
void Sidestep_Left(int spd) {
  unsigned long delta = millis() % spd;
  if (delta < spd/2) {
    // Thrust
    s_rh.write(std_pos[rh]+20);
    s_rf.write(std_pos[rf]-20);
    // Catch
    s_lh.write(std_pos[lh]-20);
    s_lf.write(std_pos[lf]-20);
  }
  else {
    // Reset
    s_lh.write(std_pos[lh]);
    s_lf.write(std_pos[lf]);
    s_rh.write(std_pos[rh]);
    s_rf.write(std_pos[rf]);
  }

  // Fix Rest of Body
  for (int i = 0; i < 9; i++) {
    if (i != rh && i != rf && i != lh && i != lf) joints[i].write(std_pos[i]);
  }
}

/**
 * @brief Move right via legs.
 * 
 * Move right via a two beat pattern.
 * 
 * First beat moves the left hip and foot
 * outward and upward, thrusting the body in
 * the right direction. The right hip and foot
 * are extended outward as well but to stabilize
 * the body.
 * 
 * Second beat returns the body to default position.
 * 
 * @param spd The speed in milliseconds to cycle 
 * through all the phases.
*/
void Sidestep_Right(int spd) {
  unsigned long delta = millis() % spd;
  if (delta < spd/2) {
    // Thrust
    s_lh.write(std_pos[lh]-20);
    s_lf.write(std_pos[lf]+20);
    // Catch
    s_rh.write(std_pos[rh]+20);
    s_rf.write(std_pos[rf]+20);
  }
  else {
    // Reset
    s_lh.write(std_pos[lh]);
    s_lf.write(std_pos[lf]);
    s_rh.write(std_pos[rh]);
    s_rf.write(std_pos[rf]);
  }

  // Fix Rest of Body
  for (int i = 0; i < 9; i++) {
    if (i != rh && i != rf && i != lh && i != lf) joints[i].write(std_pos[i]);
  }
}

/**
 * @brief Move forward via legs.
 * 
 * Move forward via a two beat pattern.
 * 
 * First beat shifts the center of mass towards the left
 * via the feet, then rotates the waist left.
 * 
 * Second beat shifts the center of mass towards the right
 * via the feet,  then rotates the waist right.
 * 
 * Thus forward movement is achieved by turning the waist
 * towards where the center of mass is currently shifted to.
 * 
 * @param spd The speed in milliseconds to cycle 
 * through all the phases.
*/
void Forward(int spd) {
  unsigned long delta = millis() % spd;

  if (delta < spd/2) {
    // Shift Mass Left and Rotate Left
    s_lf.write(std_pos[lf]+25);
    s_rf.write(std_pos[rf]+25);
    s_w.write(std_pos[w]+45);
  }
  else {
    // Shift Mass Right and Rotate Right
    s_lf.write(std_pos[lf]-25);
    s_rf.write(std_pos[rf]-25);
    s_w.write(std_pos[w]-45);
  }

  // Fix Rest of Body
  for (int i = 0; i < 9; i++) {
    if (i != rf && i != lf && i != w) joints[i].write(std_pos[i]);
  }
}

/**
 * @brief Move backward via legs.
 * 
 * Move backward via a two beat pattern.
 * 
 * First beat shifts the center of mass towards the left
 * via the feet, then rotates the waist right.
 * 
 * Second beat shifts the center of mass towards the right
 * via the feet,  then rotates the waist left.
 * 
 * Thus backward movement is achieved by turning the waist
 * away from where the center of mass is currently shifted to.
 * 
 * @param spd The speed in milliseconds to cycle 
 * through all the phases.
*/
void Backward(int spd) {
  unsigned long delta = millis() % spd;

  if (delta < spd/2) {
    // Shift Mass Left and Rotate Right
    s_lf.write(std_pos[lf]+25);
    s_rf.write(std_pos[rf]+25);
    s_w.write(std_pos[w]-45);
  }
  else {
    // Shift Mass Right and Rotate Left
    s_lf.write(std_pos[lf]-25);
    s_rf.write(std_pos[rf]-25);
    s_w.write(std_pos[w]+45);
  }

  // Fix Rest of Body
  for (int i = 0; i < 9; i++) {
    if (i != rf && i != lf && i != w) joints[i].write(std_pos[i]);
  }
}

/**
 * @brief Fix the rest of body excluding used servos.
 * 
 * Takes a In_Use function, which checks if the currently
 * accessed servo is a used servo, if so returns true.
 * 
 * @param In_Use function to check if servo is in use.
*/
void Fix_Rest(bool (*In_Use)(int)) {
  if (crouched) {
			for (int i = 0; i < 9; i++) { 
        if (!In_Use(i)) joints[i].write(crouch_pos[i]);
      }
  }
  else {
    for (int i = 0; i < 9; i++) {
      if (!In_Use(i)) joints[i].write(gaucho_pos[i]);
    }
  }
}

bool In_Use_Idle(int i) { return false; }
/**
 * @brief Idle body
 * 
 * Set joints to resting position (depends on if crouched or not).
*/
void Idle() { Fix_Rest(In_Use_Idle); }

rampInt br_rs;
rampInt br_ls;
rampInt br_rb;
rampInt br_lb;
rampInt br_rh;
rampInt br_lh;
rampInt br_rf;
rampInt br_lf;
unsigned long back_recovery_start = 0;
/**
 * @brief Stand back up from lying on back
 * 
 * Four beat motion to stand up from.
 * 
 * First beat orients legs in a split.
 * 
 * Second beat swings biceps backward to push body forward.
 * 
 * Third beat brings arms down to further push body forward.
 * 
 * Final beat resets body to idle position.
 * 
 * @param spd The speed in milliseconds to complete the motion.
*/
void Back_Recovery(int spd) {
  unsigned long curr_time = millis();
  // Orient
  if (curr_time < back_recovery_start + (1*spd/4)) {
    // Reset ramps
    br_rs.go(gaucho_pos[rs]+30);
    br_ls.go(gaucho_pos[ls]-30);
    br_rb.go(gaucho_pos[rb]-145);
    br_lb.go(gaucho_pos[lb]+145);
    br_rh.go(gaucho_pos[rh]+50);
    br_lh.go(gaucho_pos[lh]-50);
    br_rf.go(gaucho_pos[rf]-80);
    br_lf.go(gaucho_pos[lf]+80);

    s_rs.write(gaucho_pos[rs]+105);
    s_ls.write(gaucho_pos[ls]-105);
    s_rf.write(gaucho_pos[rf]-80);
    s_lf.write(gaucho_pos[lf]+80);
    s_rh.write(gaucho_pos[rh]+50);
    s_lh.write(gaucho_pos[lh]-50);
    for (int i = 0; i < 9; i++) {
      if (i != rs && i != ls && i != rf && i != lf && i != rh && i != lh) joints[i].write(gaucho_pos[i]);
    }
  }
  // Swing biceps back
  else if (curr_time < back_recovery_start + (2*spd/4)) {
    s_rs.write(gaucho_pos[rs]+105);
    s_ls.write(gaucho_pos[ls]-105);
    s_rb.write(gaucho_pos[rb]-145);
    s_lb.write(gaucho_pos[lb]+145);

    s_rs.write(gaucho_pos[rs]+105);
    s_ls.write(gaucho_pos[ls]-105);
    s_rf.write(gaucho_pos[rf]-80);
    s_lf.write(gaucho_pos[lf]+80);
    s_rh.write(gaucho_pos[rh]+50);
    s_lh.write(gaucho_pos[lh]-50);
    
    s_w.write(gaucho_pos[w]);
  }
  // Swing shoulders down
  else if (curr_time < back_recovery_start + (3*spd/4)) {
    // Set ramp targets
    br_rs.go(gaucho_pos[rs], 1000, LINEAR);
    br_ls.go(gaucho_pos[ls], 1000, LINEAR);
    br_rb.go(gaucho_pos[rb], 1000, LINEAR);
    br_lb.go(gaucho_pos[lb], 1000, LINEAR);
    br_rh.go(gaucho_pos[rh], 1000, LINEAR);
    br_lh.go(gaucho_pos[lh], 1000, LINEAR);
    br_rf.go(gaucho_pos[rf], 1000, LINEAR);
    br_lf.go(gaucho_pos[lf], 1000, LINEAR);

    s_rs.write(gaucho_pos[rs]+30);
    s_ls.write(gaucho_pos[ls]-30);
    s_rb.write(gaucho_pos[rb]-145);
    s_lb.write(gaucho_pos[lb]+145);
    
    s_rs.write(gaucho_pos[rs]+105);
    s_ls.write(gaucho_pos[ls]-105);
    s_rf.write(gaucho_pos[rf]-80);
    s_lf.write(gaucho_pos[lf]+80);
    s_rh.write(gaucho_pos[rh]+50);
    s_lh.write(gaucho_pos[lh]-50);
    
    s_w.write(gaucho_pos[w]);
  }
  // Ease into default stance
  else {
    s_rs.write(br_rs.update());
    s_ls.write(br_ls.update());
    s_rb.write(br_rb.update());
    s_lb.write(br_lb.update());


    s_rf.write(br_rf.update());
    s_lf.write(br_lf.update());
    s_rh.write(br_rh.update());
    s_lh.write(br_lh.update());
    
    s_w.write(gaucho_pos[w]);
  }
}

unsigned long front_recovery_start = 0;
/**
 * @brief Stand back up from lying on front
 * 
 * Four beat motion to stand up from.
 * 
 * First beat orients legs in a split.
 * 
 * Second beat swings biceps backward to push body backward.
 * 
 * Third beat brings arms down to further push body backward.
 * 
 * Final beat resets body to crouch position.
 * 
 * @param spd The speed in milliseconds to complete the motion.
*/
void Front_Recovery(int spd) {
  unsigned long curr_time = millis();
  // Orient
  if (curr_time < front_recovery_start + (1*spd/4)) {
    s_rs.write(gaucho_pos[rs]+105);
    s_ls.write(gaucho_pos[ls]-105);
    s_rf.write(gaucho_pos[rf]-80);
    s_lf.write(gaucho_pos[lf]+80);
    for (int i = 0; i < 9; i++) {
      if (i != rs && i != ls && i != rf && i != lf) joints[i].write(gaucho_pos[i]);
    }
  }
  // Swing biceps forward
  else if (curr_time < front_recovery_start + (2*spd/4)) {
    s_rs.write(gaucho_pos[rs]+105);
    s_ls.write(gaucho_pos[ls]-105);
    s_rb.write(gaucho_pos[rb]+35);
    s_lb.write(gaucho_pos[lb]-35);
    for (int i = 0; i < 9; i++) {
      if (i != rs && i != ls && i != rb && i != lb) joints[i].write(gaucho_pos[i]);
    }
  }
  // Swing shoulders down
  else if (curr_time < front_recovery_start + (3*spd/4)) {
    s_rs.write(gaucho_pos[rs]+30);
    s_ls.write(gaucho_pos[ls]-30);
    s_rb.write(gaucho_pos[rb]+35);
    s_lb.write(gaucho_pos[lb]-35);
    for (int i = 0; i < 9; i++) {
      if (i != rs && i != ls && i != rb && i != lb) joints[i].write(gaucho_pos[i]);
    }
  }
  // Return to crouch stance
  else {
    for (int i = 0; i < 9; i++) joints[i].write(crouch_pos[i]);
  }
}

// Attacks

bool In_Use_Right_Atk(int i) { return (i == rs || i == rb || i == w); }
bool In_Use_Left_Atk(int i) { return (i == ls || i == lb || i == w); }

/**
 * @brief Wide right attack.
 * 
 * Extends right arm out and swing it.
*/
void Right_Sweep() {
  s_rs.write(gaucho_pos[rs]+70);
  s_rb.write(gaucho_pos[rb]-55);
  s_w.write(gaucho_pos[w]+85);
  Fix_Rest(In_Use_Right_Atk);
}

/**
 * @brief Wide left attack.
 * 
 * Extends left arm out and swing it.
*/
void Left_Sweep() {
  s_ls.write(gaucho_pos[ls]-70);
  s_lb.write(gaucho_pos[lb]+55);
  s_w.write(gaucho_pos[w]-95);
  Fix_Rest(In_Use_Left_Atk);
}

/**
 * @brief Low right attack.
 * 
 * Swing arm outward (half of sweep) and arc it in a 90 degree angle.
*/
void Right_Hook() {
  s_rs.write(gaucho_pos[rs]+30);
  s_rb.write(gaucho_pos[rb]+35);
  s_w.write(gaucho_pos[w]+90);
  Fix_Rest(In_Use_Right_Atk);
}

/**
 * @brief Low left attack.
 * 
 * Swing arm outward (half of sweep) and arc it in a 90 degree angle.
*/
void Left_Hook() {
  s_ls.write(gaucho_pos[ls]-30);
  s_lb.write(gaucho_pos[lb]-35);
  s_w.write(gaucho_pos[w]-90);
  Fix_Rest(In_Use_Left_Atk);
}

bool In_Use_Right_Shot(int i) { return (i == rs || i == rb || i == lb); }
/**
 * @brief Right side attack.
 * 
 * Extend arm out and swing it to the right.
*/
void Right_Shot() {
  s_rs.write(gaucho_pos[rs]+70);
  s_rb.write(gaucho_pos[rb]-55);
  s_lb.write(gaucho_pos[lb]-35);
  Fix_Rest(In_Use_Right_Shot);
}

bool In_Use_Left_Shot(int i) { return (i == ls || i == lb || i == rb); }
/**
 * @brief Left side attack.
 * 
 * Extend arm out and swing it to the left.
*/
void Left_Shot() {
  s_ls.write(gaucho_pos[ls]-70);
  s_lb.write(gaucho_pos[lb]+55);
  s_rb.write(gaucho_pos[rb]+35);
  Fix_Rest(In_Use_Left_Shot);
}

// Taunts
rampInt t1_rs;
rampInt t1_rb;
rampInt t1_ls;
rampInt t1_lb;
bool In_Use_t1(int i) { return (i == rs || i == rb || i == ls || i == lb); }
/**
 * @brief Taunt 1
 * 
 * Stretch arms, warming up for a battle.
 * Blinks led blue.
 * Toggles off crouch.
*/
void WARMING_UP() {
  crouched = false;
  led_state = BLUE;
  s_rs.write(t1_rs.update());
  s_rb.write(t1_rb.update());
  s_ls.write(t1_ls.update());
  s_lb.write(t1_lb.update());
  Fix_Rest(In_Use_t1);
}

rampInt t2_w;
unsigned long t2_timeout = 0;
bool In_Use_t2_p1(int i) { return (i == rf || i == lf || i == rs || i == ls || i == rb || i == lb); }
bool In_Use_t2_p2(int i) { return (i == rs || i == ls || i == w); }
/**
 * @brief Taunt 2
 * 
 * Raises body in the air before crashing down
 * and stretching out arms while rotating waist 
 * back and forth.
 * Blinks led red
 * Toggles off crouch.
*/
void BEHOLD() {
  crouched = false;
  led_state = RED;
  if (millis() < t2_timeout + 350) {
    s_rf.write(gaucho_pos[rf]+20);
    s_lf.write(gaucho_pos[lf]-20);
    s_rs.write(gaucho_pos[rs]+70);
    s_ls.write(gaucho_pos[ls]-70);
    s_rb.write(gaucho_pos[rb]-55);
    s_lb.write(gaucho_pos[lb]+55);
    Fix_Rest(In_Use_t2_p1);
  }
  else {
    s_rs.write(gaucho_pos[rs]+50);
    s_ls.write(gaucho_pos[ls]-50);
    s_w.write(t2_w.update());
    Fix_Rest(In_Use_t2_p2);
  }
}

rampInt t3_rb;
rampInt t3_lb;
unsigned long t3_timeout;
bool In_Use_t3(int i) { return (i == rb || i == lb); }
/**
 * @brief Taunt 3
 * 
 * Raise fists up slowly then quickly shift them down.
 * Turns led orange.
 * Toggles off crouch.
*/
void DUST_OFF() {
  crouched = false;
  led_state = ALL;
  if (millis() < t3_timeout + 500) {
    s_rb.write(t3_rb.update());
    s_lb.write(t3_lb.update());
  }
  else {
    s_rb.write(gaucho_pos[rb]);
    s_lb.write(gaucho_pos[lb]);
  }
  Fix_Rest(In_Use_t3);
}

bool In_Use_t4(int i) { return (i == rs || i == ls || i == rb || i == lb || i == w); }
/**
 * @brief Taunt 4
 * 
 * Beckon the opponent forward for a fight.
 * Turns led turquoise.
*/
void GIVE_IT_YOUR_ALL() {
  led_state = TURQUOISE;
  s_rs.write(gaucho_pos[rs]+70);
  s_ls.write(gaucho_pos[ls]-70);
  s_rb.write(gaucho_pos[rb]-55);
  s_lb.write(gaucho_pos[lb]-35);
  s_w.write(gaucho_pos[w]+85);
  Fix_Rest(In_Use_t4);
}

// LED Animations
/**
 * @brief Set led to purple.
*/
void Idle_Led() {
  analogWrite(R, 255);
  analogWrite(G, 0);
  analogWrite(B, 255);
}

/**
 * @brief Turn off led
*/
void Close_Led() {
  analogWrite(R, 0);
  analogWrite(G, 0);
  analogWrite(B, 0);
}

/**
 * @brief Glows led a certain value.
 * 
 * @param r_val pointer to R led value.
 * @param g_val pointer to G led value.
 * @param b_val pointer to B led value.
 * @param timeout pointer to timeout.
 * @param r_max max value for R led [0,256]
 * @param g_max max value for G led [0,256]
 * @param b_max max value for B led [0,256]
*/
void Glow_Led(unsigned* r_val, unsigned* g_val, unsigned* b_val, unsigned long* timeout, unsigned r_max, unsigned g_max, unsigned b_max) {
  unsigned long ms = millis();
  if (ms > *timeout + 1) {
    if (r_val != NULL) (*r_val) = ((*r_val)+1)%((r_max*2)+1);
    if (g_val != NULL) (*g_val) = ((*g_val)+1)%((g_max*2)+1);
    if (b_val != NULL) (*b_val) = ((*b_val)+1)%((b_max*2)+1);
  }
  analogWrite(R, (r_val != NULL) ? ( (*r_val < r_max) ? *r_val : ((r_max-1)*2)-(*r_val) ) : 0);
  analogWrite(G, (g_val != NULL) ? ( (*g_val < g_max) ? *g_val : ((g_max-1)*2)-(*g_val) ) : 0);
  analogWrite(B, (b_val != NULL) ? ( (*b_val < b_max) ? *b_val : ((b_max-1)*2)-(*b_val) ) : 0);
}

unsigned long atk_led_timeout = 0;
unsigned atk_r_val = 0;
unsigned atk_b_val = 0;
/**
 * @brief Glow led purple.
*/
void Atk_Led() { Glow_Led(&atk_r_val, NULL, &atk_b_val, &atk_led_timeout, 256, 0, 256); }

unsigned long blue_led_timeout = 0;
unsigned blue_led_val = 0;
/**
 * @brief Glow led blue.
*/
void Blue_Led() { Glow_Led(NULL, NULL, &blue_led_val, &blue_led_timeout, 0, 0, 256); }

unsigned long red_led_timeout = 0;
unsigned r_val = 255;
/**
 * @brief Blink led red.
*/
void Red_Led() {
  unsigned long ms = millis();
  if (ms > red_led_timeout + 50) {
    r_val = 0;
    red_led_timeout = ms;
  }
  else r_val = 255;
  analogWrite(R, r_val);
  analogWrite(G, 0);
  analogWrite(B, 0);
}

unsigned long all_led_timeout = 0;
unsigned all_led_r_val = 0;
unsigned all_led_g_val = 0;
unsigned all_led_b_val = 0;
/**
 * @brief Glow all colors of led.
*/
void All_Led() { Glow_Led(&all_led_r_val, &all_led_g_val, &all_led_b_val, &all_led_timeout, 256, 256, 256); }

unsigned long turquoise_led_timeout = 0;
unsigned turquoise_led_g_val = 0;
unsigned turquoise_led_b_val = 0;
/**
 * @brief Glow led turquoise.
*/
void Turquoise_Led() { Glow_Led(NULL, &turquoise_led_g_val, &turquoise_led_b_val, &turquoise_led_timeout, 256, 256, 0); }

/*
  PS3 CALLBACKS
*/
void notify() {
  ps3_button_t btn_down = Ps3.data.button;
  ps3_analog_stick_t stick_data = Ps3.data.analog.stick;
  int lx = stick_data.lx;
  int ly = stick_data.ly;
  int rx = stick_data.rx;
  int ry = stick_data.ry;

  // Toggle states according to their respective buttons
  if (Ps3.event.button_down.cross) crouched = !crouched;

  // Check if battery low
  float voltage = 0;
  for (int i = 0; i < K; i++) voltage += readings[i];
  voltage /= K;
  if (voltage < 2550) { 
    led_state = CLOSED;
    Idle(); 
  }
  else {
    // Adjust timeouts
    ps3_button_t btn_pressed = Ps3.event.button_down;
    if (btn_pressed.right) t2_timeout = millis();
    if (btn_pressed.down) {
      t3_timeout = millis();
      t3_rb.go(gaucho_pos[rb]);
      t3_rb.go(gaucho_pos[rb]+35, 500, LINEAR);
      t3_lb.go(gaucho_pos[lb]);
      t3_lb.go(gaucho_pos[lb]-35, 500, LINEAR);
    }
    if (btn_pressed.select) back_recovery_start = millis();
    if (btn_pressed.start) front_recovery_start = millis();
    // Check if any buttons are pressed
    if (
      btn_down.l1 || btn_down.l2 || btn_down.r1 || 
      btn_down.r2 || btn_down.up || btn_down.right ||
      btn_down.down || btn_down.left || btn_down.square ||
      btn_down.circle || btn_down.cross || btn_down.select || btn_down.start
    ) {
      led_state = ATK;

      if (btn_down.up) WARMING_UP();
      if (btn_down.right) BEHOLD();
      if (btn_down.down) DUST_OFF();
      if (btn_down.left) GIVE_IT_YOUR_ALL();
      if (btn_down.r1) Right_Hook();
      if (btn_down.l1) Left_Hook();
      if (btn_down.r2) Right_Sweep();
      if (btn_down.l2) Left_Sweep();
      if (btn_down.circle) Right_Shot();
      if (btn_down.square) Left_Shot();
      if (btn_down.select) Back_Recovery(2100);
      if (btn_down.start) Front_Recovery(2100);
    }
    // Else check if the stick movement is above a certain threshold
		else if (abs(lx) > 10 || abs(ly) > 10 || abs(rx) > 10 || abs(ry) > 10) {
      led_state = ATK;

      // Check which stick received the stronger signal
    	if (abs(ry) + abs(rx) < abs(ly) + abs(lx)) {
      	if (abs(ly) > abs(lx)) {
					if (ly < 0) Forward(350);
        	else Backward(350);
      	}
      	else {
					if (lx < 0) Right(350);
        	else Left(350);
      	}
    	}
    	else {
      		if (rx < 0) Sidestep_Left(250);
      		else Sidestep_Right(250);
    	}
  	}
		// No input, idle
		else {
      led_state = IDLE;
      Idle(); 
    }
  }
}

void On_Connect() {
	lcd.setRotation(0);
	Ps3.setPlayer(1);
  led_state = IDLE;
	Idle();
}

void setup() {
  // LED Initialization
  pinMode(R, OUTPUT);
  pinMode(G, OUTPUT);
  pinMode(B, OUTPUT);

  // Servo Initialization
	for (int i = 0; i < 9; i++) joints[i].attach(servo_pins[i]);

  // Battery Monitoring Initialization
	lcd.begin(SSD1306_SWITCHCAPVCC, 0x3C);
	lcd.clearDisplay();
	pinMode(battery, INPUT);

	lcd.setRotation(1);
	lcd.setTextSize(1);
	lcd.setTextColor(SSD1306_WHITE);

	// Ps3 Initialization
	Ps3.attach(notify);
	Ps3.attachOnConnect(On_Connect);
	Ps3.begin("2c:81:58:3a:93:f7");

  // Animation Ramps Initialization
  t1_rs.go(gaucho_pos[rs]);
  t1_rs.go(gaucho_pos[rs]+70, 1000, LINEAR, FORTHANDBACK);
  t1_rb.go(gaucho_pos[rb]);
  t1_rb.go(gaucho_pos[rb]-55, 1000, LINEAR, FORTHANDBACK);
  t1_ls.go(gaucho_pos[ls]);
  t1_ls.go(gaucho_pos[ls]-70, 1000, LINEAR, FORTHANDBACK);
  t1_lb.go(gaucho_pos[lb]);
  t1_lb.go(gaucho_pos[lb]+55, 1000, LINEAR, FORTHANDBACK);
  
  t2_w.go(gaucho_pos[w]-95);
  t2_w.go(gaucho_pos[w]+85, 1000, LINEAR, FORTHANDBACK);
}

unsigned long init_timeout = 0;
unsigned init_led_val = 0;
void loop() {
  while (!Ps3.isConnected()) {
    unsigned long ms = millis();

    if (ms > init_timeout + 2) {
      init_led_val = (init_led_val+1)%511;
      init_timeout = ms;
    }

    Waiting_To_Pair( ( ((init_led_val < 256) ? init_led_val : 510 - init_led_val)/17 ) % 4 );
    analogWrite(R, (init_led_val < 256) ? init_led_val : 510 - init_led_val);
    analogWrite(B, (init_led_val < 256) ? init_led_val : 510 - init_led_val);
	}

  switch (led_state) {
		case IDLE:
			Idle_Led();
			break;
		case CLOSED:
			Close_Led();
			break;
		case ATK:
			Atk_Led();
			break;
    case BLUE:
      Blue_Led();
      break;
    case RED:
      Red_Led();
      break;
    case ALL:
      All_Led();
      break;
    case TURQUOISE:
      Turquoise_Led();
      break;
	}

	Display_Voltage();
}