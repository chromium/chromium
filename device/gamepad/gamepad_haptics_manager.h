// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_HAPTICS_MANAGER_H_
#define DEVICE_GAMEPAD_GAMEPAD_HAPTICS_MANAGER_H_

#include "device/gamepad/gamepad_export.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"

namespace device {

class DEVICE_GAMEPAD_EXPORT GamepadHapticsManager
    : public mojom::GamepadHapticsManager {
 public:
  GamepadHapticsManager();

  GamepadHapticsManager(const GamepadHapticsManager&) = delete;
  GamepadHapticsManager& operator=(const GamepadHapticsManager&) = delete;

  ~GamepadHapticsManager() override;

  static void Create(
      mojo::PendingReceiver<mojom::GamepadHapticsManager> receiver);

  // mojom::GamepadHapticsManager implementation.
  void PlayVibrationEffectOnce(uint32_t pad_index,
                               mojom::GamepadHapticEffectType,
                               mojom::GamepadEffectParametersPtr,
                               PlayVibrationEffectOnceCallback) override;
  void ResetVibrationActuator(uint32_t pad_index,
                              ResetVibrationActuatorCallback) override;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_HAPTICS_MANAGER_H_
