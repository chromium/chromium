// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/gamepad_dispatcher.h"

#include "device/gamepad/public/cpp/gamepads.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_shared_memory_reader.h"
#include "third_party/blink/renderer/modules/gamepad/navigator_gamepad.h"

namespace blink {

using device::mojom::blink::GamepadHapticsManager;

void GamepadDispatcher::SampleGamepads(device::Gamepads& gamepads) {
  if (reader_) {
    reader_->SampleGamepads(gamepads);
  }
}

void GamepadDispatcher::PlayVibrationEffectOnce(
    uint32_t pad_index,
    device::mojom::blink::GamepadHapticEffectType type,
    device::mojom::blink::GamepadEffectParametersPtr params,
    GamepadHapticsManager::PlayVibrationEffectOnceCallback callback) {
  InitializeHaptics();
  gamepad_haptics_manager_remote_->PlayVibrationEffectOnce(
      pad_index, type, std::move(params), std::move(callback));
}

void GamepadDispatcher::ResetVibrationActuator(
    uint32_t pad_index,
    GamepadHapticsManager::ResetVibrationActuatorCallback callback) {
  InitializeHaptics();
  gamepad_haptics_manager_remote_->ResetVibrationActuator(pad_index,
                                                          std::move(callback));
}

GamepadDispatcher::GamepadDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

GamepadDispatcher::~GamepadDispatcher() = default;

void GamepadDispatcher::InitializeHaptics() {
  if (!gamepad_haptics_manager_remote_) {
    Platform::Current()->GetInterfaceProvider()->GetInterface(
        gamepad_haptics_manager_remote_.BindNewPipeAndPassReceiver(
            task_runner_));
  }
}

void GamepadDispatcher::Trace(blink::Visitor* visitor) {
  PlatformEventDispatcher::Trace(visitor);
}

void GamepadDispatcher::DidConnectGamepad(uint32_t index,
                                          const device::Gamepad& gamepad) {
  DispatchDidConnectOrDisconnectGamepad(index, gamepad, true);
}

void GamepadDispatcher::DidDisconnectGamepad(uint32_t index,
                                             const device::Gamepad& gamepad) {
  DispatchDidConnectOrDisconnectGamepad(index, gamepad, false);
}

void GamepadDispatcher::ButtonOrAxisDidChange(uint32_t index,
                                              const device::Gamepad& gamepad) {
  DCHECK_LT(index, device::Gamepads::kItemsLengthCap);
  NotifyControllers();
}

void GamepadDispatcher::DispatchDidConnectOrDisconnectGamepad(
    uint32_t index,
    const device::Gamepad& gamepad,
    bool connected) {
  DCHECK_LT(index, device::Gamepads::kItemsLengthCap);
  DCHECK_EQ(connected, gamepad.connected);

  NotifyControllers();
}

void GamepadDispatcher::StartListening(LocalFrame* frame) {
  if (!reader_) {
    DCHECK(frame);
    reader_ = std::make_unique<GamepadSharedMemoryReader>(*frame);
  }
  reader_->Start(this);
}

void GamepadDispatcher::StopListening() {
  if (reader_)
    reader_->Stop();
}

}  // namespace blink
