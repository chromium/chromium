// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <iterator>

#include "base/ranges/algorithm.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_standard_mappings.h"

namespace device {

namespace {
// The Linux kernel has been updated to improve the mapping exposed by Sony
// Playstation controllers. If the high bit of the bcdHID value is set it
// indicates that an improved mapping is used, otherwise the default mapping
// is used.
// Dualshock 4 devices are patched in 4.10:
// https://github.com/torvalds/linux/commit/9131f8cc2b4eaf7c08d402243429e0bfba9aa0d6
// Dualshock 3 and SIXAXIS devices are patched in 4.12:
// https://github.com/torvalds/linux/commit/e19a267b9987135c00155a51e683e434b9abb56b
// Dualsense devices are patched in 5.12:
// https://github.com/torvalds/linux/commit/bc2e15a9a0228b10fece576d4f6a974c002ff07b
const uint16_t kDualshockPatchedBcdHidMask = 0x8000;

// Older versions of the Stadia Controller firmware use an alternate mapping
// function.
const uint16_t kStadiaControllerOldFirmwareVersion = 0x0001;

enum StadiaGamepadButtons {
  STADIA_GAMEPAD_BUTTON_EXTRA = BUTTON_INDEX_COUNT,
  STADIA_GAMEPAD_BUTTON_EXTRA2,
  STADIA_GAMEPAD_BUTTON_COUNT
};

enum XboxSeriesXGamepadButtons {
  kSeriesXGamepadButtonShare = BUTTON_INDEX_COUNT,
  kSeriesXGamepadButtonCount
};

void MapperXInputStyleGamepad(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[8];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];
  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperXboxBluetooth(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperXboxSeriesXBluetooth(const Gamepad& input, Gamepad* mapped) {
  MapperXboxBluetooth(input, mapped);
  // Xbox Wireless Controller Model 1914 has an extra Share button not present
  // on other Xbox controllers. Map Share to the next button index after Meta.
  mapped->buttons[kSeriesXGamepadButtonShare] = input.buttons[15];
  mapped->buttons_length = kSeriesXGamepadButtonCount;
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
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[10];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];
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
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[16];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);

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

  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[3];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperXboxElite2Bluetooth(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;

  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  // On some systems, the View (back/select) button is interpreted as a media
  // key instead of a gamepad button. When it behaves as a media key, pressing
  // the button causes a back-navigation in the browser. The below mapping is
  // correct when this behavior is not present.
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[16];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  // The modern Xbox Elite Series 2 firmware reports less axes than prior
  // versions. However, the only way to distinguish between the versions is
  // to check the length of the axes.
  //
  // In the older firmware, axes 4 and 9 are redundancies, so after axis 3
  // the mappings are shifted by 1
  int axis_shift = mapped->axes_length > 8 ? 1 : 0;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] =
      AxisToButton(input.axes[5 + axis_shift]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] =
      AxisToButton(input.axes[4 + axis_shift]);
  mapped->buttons[BUTTON_INDEX_DPAD_UP] =
      AxisNegativeAsButton(input.axes[7 + axis_shift]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] =
      AxisPositiveAsButton(input.axes[7 + axis_shift]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] =
      AxisNegativeAsButton(input.axes[6 + axis_shift]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6 + axis_shift]);

  // The Xbox (meta) button does not generate an input event for this device.
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperLakeviewResearch(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[4]);
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;  // no Meta on this device
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperDualshock3SixAxis(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[15];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[12]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[13]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisToButton(input.axes[8]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisToButton(input.axes[10]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = AxisToButton(input.axes[9]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[16];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperDualshock3SixAxisNew(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[15];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[16];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[10];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];

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
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->buttons[DUALSHOCK_BUTTON_TOUCHPAD] = input.buttons[13];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];

  mapped->buttons_length = DUALSHOCK_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

// This mapping function is intended for Playstation 4 and 5 gamepads handled
// by hid-sony (kernel 4.10+) and hid-playstation (kernel 5.12+).
void MapperPs4Ps5(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[10];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperDualSense(const Gamepad& input, Gamepad* mapped) {
  enum DualSenseButtons {
    DUAL_SENSE_BUTTON_TOUCHPAD = BUTTON_INDEX_COUNT,
    DUAL_SENSE_BUTTON_COUNT
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
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->buttons[DUAL_SENSE_BUTTON_TOUCHPAD] = input.buttons[13];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[2];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];

  mapped->buttons_length = DUAL_SENSE_BUTTON_COUNT;
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

void MapperXGEAR(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[4]);
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[2];
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;  // no Meta on this device
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperDragonRiseGeneric(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[5]);
  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;  // no Meta on this device
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperOnLiveWireless(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[2]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[8];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperADT1(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = NullButton();
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[6];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperNvShield(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[6];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperNvShield2017(const Gamepad& input, Gamepad* mapped) {
  enum Shield2017Buttons {
    SHIELD2017_BUTTON_PLAYPAUSE = BUTTON_INDEX_COUNT,
    SHIELD2017_BUTTON_COUNT
  };
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->buttons[SHIELD2017_BUTTON_PLAYPAUSE] = input.buttons[6];

  mapped->buttons_length = SHIELD2017_BUTTON_COUNT;
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
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);

  mapped->buttons_length = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperMoga(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = NullButton();
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);

  mapped->buttons_length = BUTTON_INDEX_COUNT - 1; /* no meta */
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSamsung_EI_GP20(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = NullButton();
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = NullButton();
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = NullButton();
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = NullButton();

  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[15];

  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[2];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[3];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSteelSeriesZeemote(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = NullButton();
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = NullButton();
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = NullButton();
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = NullButton();
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_META] = NullButton();
  mapped->buttons_length = BUTTON_INDEX_META;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSteelSeriesStratusXLUsb(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[3]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[18];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[12];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] = input.buttons[15];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[19];

  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[2];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[5];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSteelSeriesStratusBt(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = NullButton();
  // The META button currently isn't mappable since it's handled separately as
  // key events, causing a browser HOME action. If this is fixed, it should be
  // added here.

  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[2];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[3];

  mapped->buttons_length = BUTTON_INDEX_META;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSteelSeriesStratusPlusBt(const Gamepad& input, Gamepad* mapped) {
  MapperSteelSeriesStratusBt(input, mapped);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->buttons_length = BUTTON_INDEX_COUNT;
}

void MapperLogitechDInput(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[4]);

  // The Logitech button (BUTTON_INDEX_META) is not accessible through the
  // device's D-mode.
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperStadiaControllerOldFirmware(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[7];
  mapped->buttons[STADIA_GAMEPAD_BUTTON_EXTRA] = input.buttons[11];
  mapped->buttons[STADIA_GAMEPAD_BUTTON_EXTRA2] = input.buttons[12];
  mapped->buttons_length = STADIA_GAMEPAD_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperStadiaController(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = AxisToButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = AxisToButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[8];
  mapped->buttons[STADIA_GAMEPAD_BUTTON_EXTRA] = input.buttons[11];
  mapped->buttons[STADIA_GAMEPAD_BUTTON_EXTRA2] = input.buttons[12];
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
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = -input.axes[1];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[2];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = -input.axes[3];
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;  // no meta
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSnakebyteIDroidCon(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[5];
  mapped->buttons[BUTTON_INDEX_META] = NullButton();

  if ((input.axes_used & 0b1000000) == 0) {
    // "Game controller 1" mode: digital triggers.
    mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[8];
    mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[9];
    mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[5]);
    mapped->buttons[BUTTON_INDEX_DPAD_DOWN] =
        AxisPositiveAsButton(input.axes[5]);
    mapped->buttons[BUTTON_INDEX_DPAD_LEFT] =
        AxisNegativeAsButton(input.axes[4]);
    mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
        AxisPositiveAsButton(input.axes[4]);
  } else {
    // "Game controller 2" mode: analog triggers.
    mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] =
        AxisPositiveAsButton(input.axes[2]);
    mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] =
        AxisNegativeAsButton(input.axes[2]);
    mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[6]);
    mapped->buttons[BUTTON_INDEX_DPAD_DOWN] =
        AxisPositiveAsButton(input.axes[6]);
    mapped->buttons[BUTTON_INDEX_DPAD_LEFT] =
        AxisNegativeAsButton(input.axes[5]);
    mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
        AxisPositiveAsButton(input.axes[5]);
    mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
    mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[4];
  }

  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;  // no meta
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperHoripadSwitch(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->buttons[SWITCH_PRO_BUTTON_CAPTURE] = input.buttons[13];
  mapped->buttons_length = SWITCH_PRO_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperElecomWiredDirectInput(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[2];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[5]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[4]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[12];
  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperElecomWirelessDirectInput(const Gamepad& input, Gamepad* mapped) {
  MapperElecomWiredDirectInput(input, mapped);

  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[2];
}

void MapperDjiFpv(const Gamepad& input, Gamepad* mapped) {
  enum DjiFpvAxis {
    kDjiFpvAxisGimbalDial = AXIS_INDEX_COUNT,
    kDjiFpvAxisFlightModeSwitch,
    kDjiFpvAxisC2Switch,
    kDjiFpvAxisCount,
  };

  double flight_mode_axis;
  if (input.buttons[6].pressed)
    flight_mode_axis = -1.0;
  else if (input.buttons[7].pressed)
    flight_mode_axis = 1.0;
  else
    flight_mode_axis = 0.0;

  double c2_axis;
  if (input.buttons[4].pressed)
    c2_axis = 0.0;
  else if (input.buttons[5].pressed)
    c2_axis = -1.0;
  else
    c2_axis = 1.0;

  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = NullButton();
  mapped->buttons[BUTTON_INDEX_SECONDARY] = NullButton();
  mapped->buttons[BUTTON_INDEX_TERTIARY] = NullButton();
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = NullButton();
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] =
      input.buttons[2];  // Flight Pause/RTH
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] =
      input.buttons[3];  // Shutter/Record
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = NullButton();
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[1];  // Start/Stop
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[0];    // C1
  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[3];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = -input.axes[2];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[0];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = -input.axes[1];
  mapped->axes[kDjiFpvAxisGimbalDial] = input.axes[4];
  mapped->axes[kDjiFpvAxisFlightModeSwitch] = flight_mode_axis;
  mapped->axes[kDjiFpvAxisC2Switch] = c2_axis;

  mapped->buttons_length = 9;
  mapped->axes_length = kDjiFpvAxisCount;
}

void MapperAcer(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;  // no meta
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperAcerAppMode(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[0];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[1];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[3];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[4];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] = input.buttons[6];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] = input.buttons[7];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] = input.buttons[8];
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] = input.buttons[9];
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] = input.buttons[10];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[11];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] = input.buttons[13];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] = input.buttons[14];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = AxisNegativeAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] = AxisPositiveAsButton(input.axes[7]);
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] = AxisNegativeAsButton(input.axes[6]);
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      AxisPositiveAsButton(input.axes[6]);
  mapped->buttons_length = BUTTON_INDEX_COUNT - 1;  // no meta
  mapped->axes_length = AXIS_INDEX_COUNT;
}

constexpr struct MappingData {
  GamepadId gamepad_id;
  GamepadStandardMappingFunction function;
} kAvailableMappings[] = {
    // PowerA Wireless Controller - Nintendo GameCube style
    {GamepadId::kPowerALicPro, MapperSwitchPro},
    // DragonRise Generic USB
    {GamepadId::kDragonRiseProduct0006, MapperDragonRiseGeneric},
    // HORIPAD for Nintendo Switch
    {GamepadId::kHoriProduct00c1, MapperHoripadSwitch},
    // Xbox One S (Bluetooth)
    {GamepadId::kMicrosoftProduct02e0, MapperXboxOneS},
    // Xbox One S (Bluetooth)
    {GamepadId::kMicrosoftProduct02fd, MapperXboxOneS2016Firmware},
    // Xbox One Elite 2 (Bluetooth)
    {GamepadId::kMicrosoftProduct0b05, MapperXboxElite2Bluetooth},
    // Xbox Series X (Bluetooth)
    {GamepadId::kMicrosoftProduct0b13, MapperXboxSeriesXBluetooth},
    // Xbox One S (Bluetooth)
    {GamepadId::kMicrosoftProduct0b20, MapperXboxBluetooth},
    // Xbox Adaptive (Bluetooth)
    {GamepadId::kMicrosoftProduct0b21, MapperXboxBluetooth},
    // Xbox Elite Series 2 (Bluetooth)
    {GamepadId::kMicrosoftProduct0b22, MapperXboxBluetooth},
    // Logitech F310 D-mode
    {GamepadId::kLogitechProductc216, MapperLogitechDInput},
    // Logitech F510 D-mode
    {GamepadId::kLogitechProductc218, MapperLogitechDInput},
    // Logitech F710 D-mode
    {GamepadId::kLogitechProductc219, MapperLogitechDInput},
    // Samsung Gamepad EI-GP20
    {GamepadId::kSamsungElectronicsProducta000, MapperSamsung_EI_GP20},
    // Acer GC501 X-INPUT mode
    {GamepadId::kAcerProduct1304, MapperAcer},
    // Acer Gaming Controller Nitro X-INPUT mode
    {GamepadId::kAcerProduct1305, MapperAcer},
    // Acer GC501 APP mode
    {GamepadId::kAcerProduct1316, MapperAcerAppMode},
    // Acer Gaming Controller Nitro APP mode
    {GamepadId::kAcerProduct1317, MapperAcerAppMode},
    // Dualshock 3 / SIXAXIS
    {GamepadId::kSonyProduct0268, MapperDualshock3SixAxis},
    // Playstation Dualshock 4
    {GamepadId::kSonyProduct05c4, MapperDualshock4},
    // Dualshock 4 (PS4 Slim)
    {GamepadId::kSonyProduct09cc, MapperDualshock4},
    // Dualshock 4 USB receiver
    {GamepadId::kSonyProduct0ba0, MapperDualshock4},
    // DualSense
    {GamepadId::kSonyProduct0ce6, MapperDualSense},
    // DualSense Edge
    {GamepadId::kSonyProduct0df2, MapperDualSense},
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
    {GamepadId::kLakeviewResearchProduct0005, MapperLakeviewResearch},
    // WiseGroup MP-8866
    {GamepadId::kLakeviewResearchProduct8866, MapperLakeviewResearch},
    // Nvidia Shield gamepad (2015)
    {GamepadId::kNvidiaProduct7210, MapperNvShield},
    // Nvidia Shield gamepad (2017)
    {GamepadId::kNvidiaProduct7214, MapperNvShield2017},
    // Nexus Player Controller
    {GamepadId::kAsusTekProduct4500, MapperADT1},
    // XSkills Gamecube USB adapter
    {GamepadId::kPlayComProduct0005, MapperXSkills},
    // XFXforce XGEAR PS2 Controller
    {GamepadId::kGreenAsiaProduct0003, MapperXGEAR},
    // Zeemote: SteelSeries FREE
    {GamepadId::kSteelSeriesProduct1412, MapperSteelSeriesZeemote},
    // SteelSeries Stratus XL USB
    {GamepadId::kSteelSeriesProduct1418, MapperSteelSeriesStratusXLUsb},
    // SteelSeries Stratus XL Bluetooth
    {GamepadId::kSteelSeriesBtProduct1419, MapperSteelSeriesStratusBt},
    // SteelSeries Stratus Duo Bluetooth
    {GamepadId::kSteelSeriesBtProduct1431, MapperSteelSeriesStratusBt},
    // SteelSeries Stratus+ Bluetooth
    {GamepadId::kSteelSeriesBtProduct1434, MapperSteelSeriesStratusPlusBt},
    // Razer Serval Controller
    {GamepadId::kRazer1532Product0900, MapperRazerServal},
    // ADT-1 Controller
    {GamepadId::kGoogleProduct2c40, MapperADT1},
    // Stadia Controller
    {GamepadId::kGoogleProduct9400, MapperStadiaController},
    // Moga Pro Controller (HID mode)
    {GamepadId::kBdaProduct6271, MapperMoga},
    // Moga 2 HID
    {GamepadId::kBdaProduct89e5, MapperMoga},
    // OnLive Controller (Bluetooth)
    {GamepadId::kOnLiveProduct1008, MapperOnLiveWireless},
    // OnLive Controller (Wired)
    {GamepadId::kOnLiveProduct100a, MapperOnLiveWireless},
    // OUYA Controller
    {GamepadId::kOuyaProduct0001, MapperOUYA},
    // DJI FPV Remote Controller 2
    {GamepadId::kDjiProduct1020, MapperDjiFpv},
    // SCUF Vantage, SCUF Vantage 2
    {GamepadId::kScufProduct7725, MapperDualshock4},
    // boom PSX+N64 USB Converter
    {GamepadId::kPrototypeVendorProduct0667, MapperBoomN64Psx},
    // Stadia Controller prototype
    {GamepadId::kPrototypeVendorProduct9401, MapperStadiaControllerOldFirmware},
    // Snakebyte iDroid:con
    {GamepadId::kBroadcomProduct8502, MapperSnakebyteIDroidCon},
    // Elecom JC-U4013SBK (DirectInput mode)
    {GamepadId::kElecomProduct200f, MapperElecomWiredDirectInput},
    // Elecom JC-U4113SBK (DirectInput mode)
    {GamepadId::kElecomProduct2010, MapperElecomWirelessDirectInput},
};

}  // namespace

GamepadStandardMappingFunction GetGamepadStandardMappingFunction(
    std::string_view product_name,
    const uint16_t vendor_id,
    const uint16_t product_id,
    const uint16_t hid_specification_version,
    const uint16_t version_number,
    GamepadBusType bus_type) {
  GamepadId gamepad_id =
      GamepadIdList::Get().GetGamepadId(product_name, vendor_id, product_id);
  const auto* find_it = base::ranges::find(kAvailableMappings, gamepad_id,
                                           &MappingData::gamepad_id);
  GamepadStandardMappingFunction mapper =
      (find_it == std::end(kAvailableMappings)) ? nullptr : find_it->function;

  // The Linux kernel was updated in version 4.10 to better support Dualshock 4
  // and Dualshock 3/SIXAXIS gamepads. The driver patches the bcdHID value when
  // using the new mapping to allow downstream users to distinguish them.
  if (mapper == MapperDualshock4 &&
      (hid_specification_version & kDualshockPatchedBcdHidMask)) {
    mapper = MapperPs4Ps5;
  } else if (mapper == MapperDualshock3SixAxis &&
             (hid_specification_version & kDualshockPatchedBcdHidMask)) {
    mapper = MapperDualshock3SixAxisNew;
  } else if (mapper == MapperDualSense &&
             (hid_specification_version & kDualshockPatchedBcdHidMask)) {
    mapper = MapperPs4Ps5;
  }

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

  // If no mapper was found, check if the device is a known XInput gamepad.
  if (mapper == nullptr) {
    XInputType xtype =
        GamepadIdList::Get().GetXInputType(vendor_id, product_id);
    if (xtype == kXInputTypeXbox360 || xtype == kXInputTypeXboxOne)
      mapper = MapperXInputStyleGamepad;
  }

  return mapper;
}

}  // namespace device
