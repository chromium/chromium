// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/wgi_gamepad_device.h"

#include "base/trace_event/trace_event.h"

namespace device {

WgiGamepadDevice::WgiGamepadDevice(
    Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad> gamepad)
    : gamepad_(std::move(gamepad)) {}

WgiGamepadDevice::~WgiGamepadDevice() = default;

void WgiGamepadDevice::SetVibration(double strong_magnitude,
                                    double weak_magnitude) {
  ABI::Windows::Gaming::Input::GamepadVibration vibration = {
      .LeftMotor = strong_magnitude,
      .RightMotor = weak_magnitude,
  };
  HRESULT hr = gamepad_->put_Vibration(vibration);
  DCHECK(SUCCEEDED(hr));
}

base::WeakPtr<AbstractHapticGamepad> WgiGamepadDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
