// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gameinput_gamepad_device.h"

#include "device/gamepad/abstract_haptic_gamepad.h"

namespace device {

GameInputGamepadDevice::GameInputGamepadDevice(IGameInputDevice* gamepad,
                                               std::string product_identifier)
    : gamepad_(gamepad), product_identifier_(std::move(product_identifier)) {}

GameInputGamepadDevice::~GameInputGamepadDevice() = default;

void GameInputGamepadDevice::SetVibration(
    mojom::GamepadEffectParametersPtr params) {
  GameInputRumbleParams vibration = {
      .lowFrequency = static_cast<float>(params->strong_magnitude),
      .highFrequency = static_cast<float>(params->weak_magnitude),
      .leftTrigger = static_cast<float>(params->left_trigger),
      .rightTrigger = static_cast<float>(params->right_trigger)};
  gamepad_->SetRumbleState(&vibration);
}

base::WeakPtr<AbstractHapticGamepad> GameInputGamepadDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
