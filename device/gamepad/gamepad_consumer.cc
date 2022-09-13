// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_consumer.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"

namespace device {

GamepadConsumer::GamepadConsumer() = default;

GamepadConsumer::~GamepadConsumer() = default;

void GamepadConsumer::OnGamepadChanged(const mojom::GamepadChanges& change) {}

}  // namespace device
