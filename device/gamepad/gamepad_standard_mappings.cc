// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/gamepad_standard_mappings.h"

namespace device {

namespace {

const float kButtonAxisDeadzone = 0.01f;

GamepadButton ValueToButton(float value) {
  bool pressed = value > GamepadButton::kDefaultButtonPressedThreshold;
  bool touched = value > 0.f;
  return GamepadButton(pressed, touched, value);
}

}  // namespace

GamepadButton AxisToButton(float input) {
  float value = (input + 1.f) / 2.f;
  return ValueToButton(value);
}

GamepadButton AxisNegativeAsButton(float input) {
  float value = input < -kButtonAxisDeadzone ? -input : 0.f;
  return ValueToButton(value);
}

GamepadButton AxisPositiveAsButton(float input) {
  float value = input > kButtonAxisDeadzone ? input : 0.f;
  return ValueToButton(value);
}

GamepadButton ButtonFromButtonAndAxis(GamepadButton button, float axis) {
  float value = (axis + 1.f) / 2.f;
  return GamepadButton(button.pressed, button.touched, value);
}

GamepadButton NullButton() {
  return GamepadButton();
}

void DpadFromAxis(Gamepad* mapped, float dir) {
  bool up = false;
  bool right = false;
  bool down = false;
  bool left = false;

  // Dpad is mapped as a direction on one axis, where -1 is up and it
  // increases clockwise to 1, which is up + left. It's set to a large (> 1.f)
  // number when nothing is depressed, except on start up, sometimes it's 0.0
  // for no data, rather than the large number.
  if (dir != 0.0f) {
    up = (dir >= -1.f && dir < -0.7f) || (dir >= .95f && dir <= 1.f);
    right = dir >= -.75f && dir < -.1f;
    down = dir >= -.2f && dir < .45f;
    left = dir >= .4f && dir <= 1.f;
  }

  mapped->buttons[BUTTON_INDEX_DPAD_UP].pressed = up;
  mapped->buttons[BUTTON_INDEX_DPAD_UP].touched = up;
  mapped->buttons[BUTTON_INDEX_DPAD_UP].value = up ? 1.f : 0.f;
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT].pressed = right;
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT].touched = right;
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT].value = right ? 1.f : 0.f;
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN].pressed = down;
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN].touched = down;
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN].value = down ? 1.f : 0.f;
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT].pressed = left;
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT].touched = left;
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT].value = left ? 1.f : 0.f;
}

float RenormalizeAndClampAxis(float value, float min, float max) {
  value = (2.f * (value - min) / (max - min)) - 1.f;
  return value < -1.f ? -1.f : (value > 1.f ? 1.f : value);
}

void MapperSwitchPro(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons_length = SWITCH_PRO_BUTTON_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

void MapperSwitchJoyCon(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;
  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = 2;
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
  constexpr size_t kSwitchCompositeExtraButtonCount = 5;
  *mapped = input;
  mapped->buttons_length =
      BUTTON_INDEX_COUNT + kSwitchCompositeExtraButtonCount;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

}  // namespace device
