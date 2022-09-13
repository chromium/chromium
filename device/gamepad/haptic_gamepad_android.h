// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_HAPTIC_GAMEPAD_ANDROID_H_
#define DEVICE_GAMEPAD_HAPTIC_GAMEPAD_ANDROID_H_

#include "base/memory/weak_ptr.h"
#include "device/gamepad/abstract_haptic_gamepad.h"

namespace device {

class HapticGamepadAndroid final : public AbstractHapticGamepad {
 public:
  explicit HapticGamepadAndroid(int device_index);
  HapticGamepadAndroid(const HapticGamepadAndroid&) = delete;
  HapticGamepadAndroid& operator=(const HapticGamepadAndroid&) = delete;
  ~HapticGamepadAndroid() override;

  // AbstractHapticGamepad implementation.
  void SetVibration(mojom::GamepadEffectParametersPtr params) override;
  void SetZeroVibration() override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

 private:
  const int device_index_;
  base::WeakPtrFactory<HapticGamepadAndroid> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_HAPTIC_GAMEPAD_ANDROID_H_
