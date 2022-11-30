// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_CONSUMER_H_
#define DEVICE_GAMEPAD_GAMEPAD_CONSUMER_H_

#include "device/gamepad/gamepad_export.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/gamepad/public/mojom/gamepad.mojom-forward.h"

namespace device {

class DEVICE_GAMEPAD_EXPORT GamepadConsumer {
 public:
  GamepadConsumer();
  virtual ~GamepadConsumer();

  virtual void OnGamepadConnected(uint32_t index, const Gamepad& gamepad) = 0;
  virtual void OnGamepadDisconnected(uint32_t index,
                                     const Gamepad& gamepad) = 0;
  virtual void OnGamepadChanged(const mojom::GamepadChanges& change);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_CONSUMER_H_
