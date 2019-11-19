// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_monitor.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "device/gamepad/gamepad_service.h"
#include "device/gamepad/gamepad_shared_buffer.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace device {

GamepadMonitor::GamepadMonitor() = default;

GamepadMonitor::~GamepadMonitor() {
  if (is_registered_consumer_)
    GamepadService::GetInstance()->RemoveConsumer(this);
}

// static
void GamepadMonitor::Create(
    mojo::PendingReceiver<mojom::GamepadMonitor> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<GamepadMonitor>(),
                              std::move(receiver));
}

void GamepadMonitor::OnGamepadConnected(uint32_t index,
                                        const Gamepad& gamepad) {
  if (gamepad_observer_remote_)
    gamepad_observer_remote_->GamepadConnected(index, gamepad);
}

void GamepadMonitor::OnGamepadDisconnected(uint32_t index,
                                           const Gamepad& gamepad) {
  if (gamepad_observer_remote_)
    gamepad_observer_remote_->GamepadDisconnected(index, gamepad);
}

void GamepadMonitor::OnGamepadButtonOrAxisChanged(uint32_t index,
                                                  const Gamepad& gamepad) {
  if (gamepad_observer_remote_)
    gamepad_observer_remote_->GamepadButtonOrAxisChanged(index, gamepad);
}

void GamepadMonitor::GamepadStartPolling(GamepadStartPollingCallback callback) {
  DCHECK(!is_started_);
  is_started_ = true;
  is_registered_consumer_ = true;

  GamepadService* service = GamepadService::GetInstance();
  if (!service->ConsumerBecameActive(this)) {
    mojo::ReportBadMessage("GamepadMonitor::GamepadStartPolling failed");
  }
  std::move(callback).Run(service->DuplicateSharedMemoryRegion());
}

void GamepadMonitor::GamepadStopPolling(GamepadStopPollingCallback callback) {
  DCHECK(is_started_);
  is_started_ = false;

  if (!GamepadService::GetInstance()->ConsumerBecameInactive(this)) {
    mojo::ReportBadMessage("GamepadMonitor::GamepadStopPolling failed");
  }
  std::move(callback).Run();
}

void GamepadMonitor::SetObserver(
    mojo::PendingRemote<mojom::GamepadObserver> gamepad_observer) {
  gamepad_observer_remote_.Bind(std::move(gamepad_observer));
}

}  // namespace device
