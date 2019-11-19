// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/public/cpp/gamepad.h"

#include <string.h>

namespace device {

const float GamepadButton::kDefaultButtonPressedThreshold;
const double GamepadHapticActuator::kMaxEffectDurationMillis;
const size_t Gamepad::kIdLengthCap;
const size_t Gamepad::kAxesLengthCap;
const size_t Gamepad::kButtonsLengthCap;

Gamepad::Gamepad()
    : connected(false),
      timestamp(0),
      axes_length(0),
      buttons_length(0),
      mapping(GamepadMapping::kNone),
      display_id(0) {
  id[0] = 0;
}

Gamepad::Gamepad(const Gamepad& other) = default;

void Gamepad::SetID(const base::string16& src) {
  memset(id, 0, sizeof(id));
  src.copy(id, kIdLengthCap - 1);
}

}  // namespace device
