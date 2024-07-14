// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/gamepad_dispatcher.h"

#include <utility>

#include "device/gamepad/public/cpp/gamepads.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_shared_memory_reader.h"
#include "third_party/blink/renderer/modules/gamepad/navigator_gamepad.h"

namespace blink {

using device::mojom::blink::GamepadHapticsManager;

void GamepadDispatcher::SampleGamepads(device::Gamepads& gamepads) {
  if (reader_) {
    reader_->SampleGamepads(&gamepads);
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

GamepadDispatcher::GamepadDispatcher(ExecutionContext& context)
    : execution_context_(&context), gamepad_haptics_manager_remote_(&context) {}

GamepadDispatcher::~GamepadDispatcher() = default;

void GamepadDispatcher::InitializeHaptics() {
  if (!gamepad_haptics_manager_remote_.is_bound() && execution_context_) {
    // See https://bit.ly/2S0zRAS for task types.
    auto task_runner =
        execution_context_->GetTaskRunner(TaskType::kMiscPlatformAPI);
    execution_context_->GetBrowserInterfaceBroker().GetInterface(
        gamepad_haptics_manager_remote_.BindNewPipeAndPassReceiver(
            std::move(task_runner)));
  }
}

void GamepadDispatcher::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  visitor->Trace(reader_);
  visitor->Trace(gamepad_haptics_manager_remote_);
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

void GamepadDispatcher::StartListening(LocalDOMWindow* window) {
  if (!reader_) {
    DCHECK(window);
    reader_ = MakeGarbageCollected<GamepadSharedMemoryReader>(*window);
  }
  reader_->Start(this);
}

void GamepadDispatcher::StopListening() {
  if (reader_)
    reader_->Stop();
}

}  // namespace blink
