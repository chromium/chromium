// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/wgi_gamepad_device.h"

#include "base/trace_event/trace_event.h"
#include "device/gamepad/abstract_haptic_gamepad.h"

namespace device {

WgiGamepadDevice::WgiGamepadDevice(
    Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad> gamepad)
    : gamepad_(std::move(gamepad)) {}

WgiGamepadDevice::~WgiGamepadDevice() = default;

void WgiGamepadDevice::SetVibration(mojom::GamepadEffectParametersPtr params) {
  ABI::Windows::Gaming::Input::GamepadVibration vibration = {
      .LeftMotor = params->strong_magnitude,
      .RightMotor = params->weak_magnitude,
      .LeftTrigger = params->left_trigger,
      .RightTrigger = params->right_trigger};
  HRESULT hr = gamepad_->put_Vibration(vibration);
  DCHECK(SUCCEEDED(hr));
}

base::WeakPtr<AbstractHapticGamepad> WgiGamepadDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
