// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/gamepad.h"

#include "device/gamepad/public/cpp/gamepad.h"

namespace blink {

GamepadButton::GamepadButton() : value_(0.), pressed_(false), touched_(false) {}

bool GamepadButton::IsEqual(const device::GamepadButton& device_button) const {
  return value_ == device_button.value && pressed_ == device_button.pressed &&
         touched_ == (device_button.touched || device_button.pressed ||
                      (device_button.value > 0.0f));
}

void GamepadButton::UpdateValuesFrom(
    const device::GamepadButton& device_button) {
  value_ = device_button.value;
  pressed_ = device_button.pressed;
  touched_ = (device_button.touched || device_button.pressed ||
              (device_button.value > 0.0f));
}

}  // namespace blink
