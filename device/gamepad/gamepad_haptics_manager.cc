// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_haptics_manager.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "device/gamepad/gamepad_service.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace device {

GamepadHapticsManager::GamepadHapticsManager() = default;

GamepadHapticsManager::~GamepadHapticsManager() = default;

// static
void GamepadHapticsManager::Create(
    mojo::PendingReceiver<mojom::GamepadHapticsManager> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<GamepadHapticsManager>(),
                              std::move(receiver));
}

void GamepadHapticsManager::PlayVibrationEffectOnce(
    uint32_t pad_index,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    PlayVibrationEffectOnceCallback callback) {
  GamepadService::GetInstance()->PlayVibrationEffectOnce(
      pad_index, type, std::move(params), std::move(callback));
}

void GamepadHapticsManager::ResetVibrationActuator(
    uint32_t pad_index,
    ResetVibrationActuatorCallback callback) {
  GamepadService::GetInstance()->ResetVibrationActuator(pad_index,
                                                        std::move(callback));
}

}  // namespace device
