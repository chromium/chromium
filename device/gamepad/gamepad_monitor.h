// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_MONITOR_H_
#define DEVICE_GAMEPAD_GAMEPAD_MONITOR_H_

#include "base/compiler_specific.h"
#include "device/gamepad/gamepad_consumer.h"
#include "device/gamepad/gamepad_export.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace device {

class DEVICE_GAMEPAD_EXPORT GamepadMonitor : public GamepadConsumer,
                                             public mojom::GamepadMonitor {
 public:
  GamepadMonitor();

  GamepadMonitor(const GamepadMonitor&) = delete;
  GamepadMonitor& operator=(const GamepadMonitor&) = delete;

  ~GamepadMonitor() override;

  static void Create(mojo::PendingReceiver<mojom::GamepadMonitor> receiver);

  // GamepadConsumer implementation.
  void OnGamepadConnected(uint32_t index, const Gamepad& gamepad) override;
  void OnGamepadDisconnected(uint32_t index, const Gamepad& gamepad) override;
  void OnGamepadChanged(const mojom::GamepadChanges& change) override;

  // mojom::GamepadMonitor implementation.
  void GamepadStartPolling(GamepadStartPollingCallback callback) override;
  void GamepadStopPolling(GamepadStopPollingCallback callback) override;
  void SetObserver(
      mojo::PendingRemote<mojom::GamepadObserver> gamepad_observer) override;

 private:
  mojo::Remote<mojom::GamepadObserver> gamepad_observer_remote_;

  // True if this monitor is an active gamepad consumer.
  bool is_started_ = false;

  // True if this monitor has been registered with the gamepad service.
  bool is_registered_consumer_ = false;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_MONITOR_H_
