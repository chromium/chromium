// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_WGI_GAMEPAD_DEVICE_H_
#define DEVICE_GAMEPAD_WGI_GAMEPAD_DEVICE_H_

#include <Windows.Gaming.Input.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include "base/memory/weak_ptr.h"
#include "base/win/core_winrt_util.h"
#include "device/gamepad/abstract_haptic_gamepad.h"

namespace device {

class DEVICE_GAMEPAD_EXPORT WgiGamepadDevice final
    : public AbstractHapticGamepad {
 public:
  explicit WgiGamepadDevice(
      Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad> gamepad);
  WgiGamepadDevice(const WgiGamepadDevice& other) = delete;
  WgiGamepadDevice& operator=(const WgiGamepadDevice& other) = delete;
  ~WgiGamepadDevice() override;

  // AbstractHapticGamepad implementation.
  void SetVibration(mojom::GamepadEffectParametersPtr params) override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

  Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad> GetGamepad() {
    return gamepad_;
  }

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad> gamepad_;

  base::WeakPtrFactory<WgiGamepadDevice> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_WGI_GAMEPAD_DEVICE_H_
