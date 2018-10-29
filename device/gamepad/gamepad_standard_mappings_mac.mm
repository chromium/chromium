// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/stl_util.h"
#include "device/gamepad/gamepad_standard_mappings.h"

namespace device {

namespace {

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
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[15];
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

struct MappingData {
  const uint16_t vendor_id;
  const uint16_t product_id;
  GamepadStandardMappingFunction function;
} AvailableMappings[] = {
    // http://www.linux-usb.org/usb.ids
    {0x0079, 0x0006, MapperDragonRiseGeneric},     // DragonRise Generic USB
    {0x045e, 0x028e, MapperXbox360Gamepad},        // Xbox 360 Wired
    {0x045e, 0x028f, MapperXbox360Gamepad},        // Xbox 360 Wireless
    {0x045e, 0x02d1, MapperXbox360Gamepad},        // Xbox One Wired
    {0x045e, 0x02dd, MapperXbox360Gamepad},        // Xbox One Wired (2015 FW)
    {0x045e, 0x02e0, MapperXboxOneS},              // Xbox One S (Bluetooth)
    {0x045e, 0x02e3, MapperXbox360Gamepad},        // Xbox One Elite Wired
    {0x045e, 0x02ea, MapperXbox360Gamepad},        // Xbox One S (USB)
    {0x045e, 0x02fd, MapperXboxOneS2016Firmware},  // Xbox One S (Bluetooth)
    {0x045e, 0x0719, MapperXbox360Gamepad},        // Xbox 360 Wireless
    {0x046d, 0xc216, MapperDirectInputStyle},      // Logitech F310, D mode
    {0x046d, 0xc218, MapperDirectInputStyle},      // Logitech F510, D mode
    {0x046d, 0xc219, MapperDirectInputStyle},      // Logitech F710, D mode
    {0x054c, 0x0268, MapperPlaystationSixAxis},    // Playstation SIXAXIS
    {0x054c, 0x05c4, MapperDualshock4},            // Playstation Dualshock 4
    {0x054c, 0x09cc, MapperDualshock4},            // Dualshock 4 (PS4 Slim)
    {0x054c, 0x0ba0, MapperDualshock4},            // Dualshock 4 USB receiver
    {0x0583, 0x2060, MapperIBuffalo},              // iBuffalo Classic
    {0x0925, 0x0005, MapperSmartJoyPLUS},          // SmartJoy PLUS Adapter
    {0x0955, 0x7210, MapperNvShield},          // Nvidia Shield gamepad (2015)
    {0x0b05, 0x4500, MapperADT1},              // Nexus Player Controller
    {0x0e8f, 0x0003, MapperXGEAR},             // XFXforce XGEAR PS2 Controller
    {0x1532, 0x0900, MapperRazerServal},       // Razer Serval Controller
    {0x18d1, 0x2c40, MapperADT1},              // ADT-1 Controller
    {0x20d6, 0x6271, MapperMogaPro},           // Moga Pro Controller (HID mode)
    {0x2222, 0x0060, MapperDirectInputStyle},  // Macally iShockX, analog mode
    {0x2222, 0x4010, MapperMacallyIShock},     // Macally iShock
    {0x2378, 0x1008, MapperOnLiveWireless},    // OnLive Controller (Bluetooth)
    {0x2378, 0x100a, MapperOnLiveWireless},    // OnLive Controller (Wired)
    {0x2836, 0x0001, MapperOUYA},              // OUYA Controller
};
const size_t kAvailableMappingsLen = base::size(AvailableMappings);

}  // namespace

GamepadStandardMappingFunction GetGamepadStandardMappingFunction(
    const uint16_t vendor_id,
    const uint16_t product_id,
    const uint16_t version_number,
    GamepadBusType bus_type) {
  for (size_t i = 0; i < kAvailableMappingsLen; ++i) {
    MappingData& item = AvailableMappings[i];
    if (vendor_id == item.vendor_id && product_id == item.product_id)
      return item.function;
  }
  return nullptr;
}

}  // namespace device
