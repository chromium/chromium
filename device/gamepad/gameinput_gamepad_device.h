// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEINPUT_GAMEPAD_DEVICE_H_
#define DEVICE_GAMEPAD_GAMEINPUT_GAMEPAD_DEVICE_H_

#include <GameInput.h>
#include <wrl/client.h>

#include <string_view>

#include "base/memory/weak_ptr.h"
#include "device/gamepad/abstract_haptic_gamepad.h"

namespace device {

class DEVICE_GAMEPAD_EXPORT GameInputGamepadDevice final
    : public AbstractHapticGamepad {
 public:
  GameInputGamepadDevice(IGameInputDevice* gamepad,
                         std::string product_identifier);
  GameInputGamepadDevice(const GameInputGamepadDevice& other) = delete;
  GameInputGamepadDevice& operator=(const GameInputGamepadDevice& other) =
      delete;
  ~GameInputGamepadDevice() override;

  // AbstractHapticGamepad implementation.
  void SetVibration(mojom::GamepadEffectParametersPtr params) override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

  Microsoft::WRL::ComPtr<IGameInputDevice> GetGamepad() { return gamepad_; }

  std::string_view GetProductIdentifier() const { return product_identifier_; }

 private:
  Microsoft::WRL::ComPtr<IGameInputDevice> gamepad_;
  std::string product_identifier_;

  base::WeakPtrFactory<GameInputGamepadDevice> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEINPUT_GAMEPAD_DEVICE_H_
