// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/simulated_gamepad_inputs.h"

namespace device {

SimulatedGamepadInputs::SimulatedGamepadInputs() = default;
SimulatedGamepadInputs::SimulatedGamepadInputs(SimulatedGamepadInputs&& other) =
    default;
SimulatedGamepadInputs& SimulatedGamepadInputs::operator=(
    SimulatedGamepadInputs&& other) = default;
SimulatedGamepadInputs::~SimulatedGamepadInputs() = default;

}  // namespace device
