// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <iterator>

#include "base/macros.h"
#include "base/stl_util.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_standard_mappings.h"

namespace device {

namespace {

// Older versions of the Stadia Controller firmware use an alternate mapping
// function.
const uint16_t kStadiaControllerOldFirmwareVersion = 0x0001;

enum StadiaGamepadButtons {
  STADIA_GAMEPAD_BUTTON_EXTRA = BUTTON_INDEX_COUNT,
  STADIA_GAMEPAD_BUTTON_EXTRA2,
  STADIA_GAMEPAD_BUTTON_COUNT
};

void MapperXbox360Gamepad(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[10];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];
  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperXboxOneS(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[10];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperXboxOneS2016Firmware(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;

  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[16];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];

  // Xbox Wireless Controller (045e:02fd) received a firmware update in 2019
  // that changed which field is populated with the Xbox button state. Check
  // both fields and combine the results.
  auto& xbox_old = input.buttons[15];
  auto& xbox_new = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_META].pressed =
      (xbox_old.pressed || xbox_new.pressed);
  mapped->buttons[BUTTON_INDEX_META].touched =
      (xbox_old.touched || xbox_new.touched);
  mapped->buttons[BUTTON_INDEX_META].value =
      std::max(xbox_old.value, xbox_new.value);

  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperPlaystationSixAxis(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[15];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[11];

  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] =
      ButtonFromButtonAndAxis(input.buttons[8], input.axes[14]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] =
      ButtonFromButtonAndAxis(input.buttons[9], input.axes[15]);

  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[2];

  // The SixAxis Dpad is pressure sensitive.
  mapped->buttons[BUTTON_INDEX_DPAD_UP] =
      ButtonFromButtonAndAxis(input.buttons[4], input.axes[10]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] =
      ButtonFromButtonAndAxis(input.buttons[6], input.axes[12]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] =
      ButtonFromButtonAndAxis(input.buttons[7], input.axes[13]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      ButtonFromButtonAndAxis(input.buttons[5], input.axes[11]);

  mapped->buttons[BUTTON_INDEX_META] = input.buttons[16];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperDualshock4(const Gamepad& input, Gamepad* mapped) {
  enum Dualshock4Buttons {
    DUALSHOCK_BUTTON_TOUCHPAD = BUTTON_INDEX_COUNT,
    DUALSHOCK_BUTTON_COUNT
  };

  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->buttons[DUALSHOCK_BUTTON_TOUCHPAD] = input.buttons[13];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttons_length = DUALSHOCK_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperIBuffalo(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[1]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[1]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[0]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[0]);
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axes_length = 2;
}

void MapperDirectInputStyle(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[0];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperMacallyIShock(const Gamepad& input, Gamepad* mapped) {
  enum IShockButtons {
    ISHOCK_BUTTON_C = BUTTON_INDEX_COUNT,
    ISHOCK_BUTTON_D,
    ISHOCK_BUTTON_E,
    ISHOCK_BUTTON_COUNT,
  };

  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[15];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[16];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[17];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[11];
  mapped->buttons[ISHOCK_BUTTON_C] = input.buttons[8];
  mapped->buttons[ISHOCK_BUTTON_D] = input.buttons[18];
  mapped->buttons[ISHOCK_BUTTON_E] = input.buttons[19];
  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = -input.axes[5];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[6];

  mapped->buttons_length = ISHOCK_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperXGEAR(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[5];
  DpadFromAxis(mapped, input.axes[9]);
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[5];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[2];
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSmartJoyPLUS(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[5];
  DpadFromAxis(mapped, input.axes[9]);
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperDragonRiseGeneric(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  DpadFromAxis(mapped, input.axes[9]);
  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[2];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperOnLiveWireless(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperADT1(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = NullButton();
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperNvShield(const Gamepad& input, Gamepad* mapped) {
  enum ShieldButtons {
    SHIELD_BUTTON_CIRCLE = BUTTON_INDEX_COUNT,
    SHIELD_BUTTON_COUNT
  };
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[2];
  mapped->buttons[SHIELD_BUTTON_CIRCLE] = input.buttons[5];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttons_length = SHIELD_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperOUYA(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = NullButton();
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[15];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperRazerServal(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttons_length = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperMogaPro(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttons_length = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperStadiaControllerOldFirmware(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[11];
  mapped->buttons[STADIA_GAMEPAD_BUTTON_EXTRA] = input.buttons[16];
  mapped->buttons[STADIA_GAMEPAD_BUTTON_EXTRA2] = input.buttons[17];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttons_length = STADIA_GAMEPAD_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperStadiaController(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->buttons[STADIA_GAMEPAD_BUTTON_EXTRA] = input.buttons[16];
  mapped->buttons[STADIA_GAMEPAD_BUTTON_EXTRA2] = input.buttons[17];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  DpadFromAxis(mapped, input.axes[9]);

  mapped->buttons_length = STADIA_GAMEPAD_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperXSkills(const Gamepad& input, Gamepad* mapped) {
  enum GamecubeButtons {
    GAMECUBE_BUTTON_LEFT_TRIGGER_CLICK = BUTTON_INDEX_COUNT,
    GAMECUBE_BUTTON_RIGHT_TRIGGER_CLICK,
    GAMECUBE_BUTTON_COUNT
  };
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];     // A
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[2];   // X
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[1];    // B
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[3];  // Y
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = NullButton();
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[6];  // Z
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = NullButton();
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = NullButton();
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_META] = NullButton();
  mapped->buttons[GAMECUBE_BUTTON_LEFT_TRIGGER_CLICK] = input.buttons[4];
  mapped->buttons[GAMECUBE_BUTTON_RIGHT_TRIGGER_CLICK] = input.buttons[5];
  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[5];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[2];
  mapped->buttons_length = GAMECUBE_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperBoomN64Psx(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  // Mapped for a PSX device with Analog mode enabled.
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[15];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_META] = NullButton();
  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[2];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;  // no meta
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSwitchJoyCon(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = 2;
}

void MapperSwitchPro(const Gamepad& input, Gamepad* mapped) {
  // The Switch Pro controller has a Capture button that has no equivalent in
  // the Standard Gamepad.
  const size_t kSwitchProExtraButtonCount = 1;
  *mapped = input;
  mapped->buttons_length = BUTTON_INDEX_COUNT + kSwitchProExtraButtonCount;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSwitchComposite(const Gamepad& input, Gamepad* mapped) {
  // In composite mode, the inputs from two Joy-Cons are combined to form one
  // virtual gamepad. Some buttons do not have equivalents in the Standard
  // Gamepad and are exposed as extra buttons:
  // * Capture button (Joy-Con L):  BUTTON_INDEX_COUNT
  // * SL (Joy-Con L):              BUTTON_INDEX_COUNT + 1
  // * SR (Joy-Con L):              BUTTON_INDEX_COUNT + 2
  // * SL (Joy-Con R):              BUTTON_INDEX_COUNT + 3
  // * SR (Joy-Con R):              BUTTON_INDEX_COUNT + 4
  const size_t kSwitchCompositeExtraButtonCount = 5;
  *mapped = input;
  mapped->buttons_length =
      BUTTON_INDEX_COUNT + kSwitchCompositeExtraButtonCount;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperXboxAdaptiveControllerBluetooth(const Gamepad& input,
                                           Gamepad* mapped) {
  *mapped = input;

  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[10]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[11]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[31];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  DpadFromAxis(mapped, input.axes[9]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[30];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

constexpr struct MappingData {
  GamepadId gamepad_id;
  GamepadStandardMappingFunction function;
} AvailableMappings[] = {
    // DragonRise Generic USB
    {GamepadId::kDragonRiseProduct0006, MapperDragonRiseGeneric},
    // Xbox 360 Wired
    {GamepadId::kMicrosoftProduct028e, MapperXbox360Gamepad},
    // Xbox 360 Wireless
    {GamepadId::kMicrosoftProduct028f, MapperXbox360Gamepad},
    // Xbox One Wired
    {GamepadId::kMicrosoftProduct02d1, MapperXbox360Gamepad},
    // Xbox One Wired (2015 FW)
    {GamepadId::kMicrosoftProduct02dd, MapperXbox360Gamepad},
    // Xbox One S (Bluetooth)
    {GamepadId::kMicrosoftProduct02e0, MapperXboxOneS},
    // Xbox One Elite Wired
    {GamepadId::kMicrosoftProduct02e3, MapperXbox360Gamepad},
    // Xbox One S (USB)
    {GamepadId::kMicrosoftProduct02ea, MapperXbox360Gamepad},
    // Xbox One S (Bluetooth)
    {GamepadId::kMicrosoftProduct02fd, MapperXboxOneS2016Firmware},
    // Xbox 360 Wireless
    {GamepadId::kMicrosoftProduct0719, MapperXbox360Gamepad},
    // Xbox Adaptive Controller (USB)
    {GamepadId::kMicrosoftProduct0b0a, MapperXbox360Gamepad},
    // Xbox Adaptive Controller (Bluetooth)
    {GamepadId::kMicrosoftProduct0b0c, MapperXboxAdaptiveControllerBluetooth},
    // Logitech F310, D mode
    {GamepadId::kLogitechProductc216, MapperDirectInputStyle},
    // Logitech F510, D mode
    {GamepadId::kLogitechProductc218, MapperDirectInputStyle},
    // Logitech F710, D mode
    {GamepadId::kLogitechProductc219, MapperDirectInputStyle},
    // Playstation SIXAXIS
    {GamepadId::kSonyProduct0268, MapperPlaystationSixAxis},
    // Playstation Dualshock 4
    {GamepadId::kSonyProduct05c4, MapperDualshock4},
    // Dualshock 4 (PS4 Slim)
    {GamepadId::kSonyProduct09cc, MapperDualshock4},
    // Dualshock 4 USB receiver
    {GamepadId::kSonyProduct0ba0, MapperDualshock4},
    // Switch Joy-Con L
    {GamepadId::kNintendoProduct2006, MapperSwitchJoyCon},
    // Switch Joy-Con R
    {GamepadId::kNintendoProduct2007, MapperSwitchJoyCon},
    // Switch Pro Controller
    {GamepadId::kNintendoProduct2009, MapperSwitchPro},
    // Switch Charging Grip
    {GamepadId::kNintendoProduct200e, MapperSwitchPro},
    // iBuffalo Classic
    {GamepadId::kPadixProduct2060, MapperIBuffalo},
    // SmartJoy PLUS Adapter
    {GamepadId::kLakeviewResearchProduct0005, MapperSmartJoyPLUS},
    // Nvidia Shield gamepad (2015)
    {GamepadId::kNvidiaProduct7210, MapperNvShield},
    // XSkills Gamecube USB adapter
    {GamepadId::kPlayComProduct0005, MapperXSkills},
    // Nexus Player Controller
    {GamepadId::kAsusTekProduct4500, MapperADT1},
    // XFXforce XGEAR PS2 Controller
    {GamepadId::kPdpProduct0003, MapperXGEAR},
    // Razer Serval Controller
    {GamepadId::kRazer1532Product0900, MapperRazerServal},
    // ADT-1 Controller
    {GamepadId::kGoogleProduct2c40, MapperADT1},
    // Stadia Controller
    {GamepadId::kGoogleProduct9400, MapperStadiaController},
    // Moga Pro Controller (HID mode)
    {GamepadId::kVendor20d6Product6271, MapperMogaPro},
    // Macally iShockX, analog mode
    {GamepadId::kMacAllyProduct0060, MapperDirectInputStyle},
    // Macally iShock
    {GamepadId::kMacAllyProduct4010, MapperMacallyIShock},
    // OnLive Controller (Bluetooth)
    {GamepadId::kVendor2378Product1008, MapperOnLiveWireless},
    // OnLive Controller (Wired)
    {GamepadId::kVendor2378Product100a, MapperOnLiveWireless},
    // OUYA Controller
    {GamepadId::kVendor2836Product0001, MapperOUYA},
    // SCUF Vantage, SCUF Vantage 2
    {GamepadId::kVendor2e95Product7725, MapperDualshock4},
    // boom PSX+N64 USB Converter
    {GamepadId::kPrototypeVendorProduct0667, MapperBoomN64Psx},
    // Stadia Controller prototype
    {GamepadId::kPrototypeVendorProduct9401, MapperStadiaControllerOldFirmware},
};

}  // namespace

GamepadStandardMappingFunction GetGamepadStandardMappingFunction(
    const uint16_t vendor_id,
    const uint16_t product_id,
    const uint16_t hid_specification_version,
    const uint16_t version_number,
    GamepadBusType bus_type) {
  GamepadId gamepad_id =
      GamepadIdList::Get().GetGamepadId(vendor_id, product_id);
  const MappingData* begin = std::begin(AvailableMappings);
  const MappingData* end = std::end(AvailableMappings);
  const auto* find_it = std::find_if(begin, end, [=](const MappingData& item) {
    return gamepad_id == item.gamepad_id;
  });
  GamepadStandardMappingFunction mapper =
      (find_it == end) ? nullptr : find_it->function;

  // The Switch Joy-Con Charging Grip allows a pair of Joy-Cons to be docked
  // with the grip and used over USB as a single composite gamepad. The Nintendo
  // data fetcher also allows a pair of Bluetooth-connected Joy-Cons to be used
  // as a composite device and sets the same product ID as the Charging Grip.
  //
  // In both configurations, we remap the Joy-Con buttons to align with the
  // Standard Gamepad mapping. Docking a Joy-Con in the Charging Grip makes the
  // SL and SR buttons inaccessible.
  //
  // If the Joy-Cons are not docked, the SL and SR buttons are still accessible.
  // Inspect the |bus_type| of the composite device to detect this case and use
  // an alternate mapping function that exposes the extra buttons.
  if (gamepad_id == GamepadId::kNintendoProduct200e &&
      mapper == MapperSwitchPro && bus_type != GAMEPAD_BUS_USB) {
    mapper = MapperSwitchComposite;
  }

  // Use an alternate mapping function if the Stadia controller is using an old
  // firmware version.
  if (gamepad_id == GamepadId::kGoogleProduct9400 &&
      mapper == MapperStadiaController &&
      version_number == kStadiaControllerOldFirmwareVersion) {
    mapper = MapperStadiaControllerOldFirmware;
  }

  return mapper;
}

}  // namespace device
