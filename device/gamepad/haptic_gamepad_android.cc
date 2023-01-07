// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/haptic_gamepad_android.h"

#include "device/gamepad/gamepad_platform_data_fetcher_android.h"

namespace device {

HapticGamepadAndroid::HapticGamepadAndroid(int device_index)
    : device_index_(device_index) {}

HapticGamepadAndroid::~HapticGamepadAndroid() = default;

void HapticGamepadAndroid::SetVibration(
    mojom::GamepadEffectParametersPtr params) {
  GamepadPlatformDataFetcherAndroid::SetVibration(
      device_index_, params->strong_magnitude, params->weak_magnitude);
}

void HapticGamepadAndroid::SetZeroVibration() {
  GamepadPlatformDataFetcherAndroid::SetZeroVibration(device_index_);
}

base::WeakPtr<AbstractHapticGamepad> HapticGamepadAndroid::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
