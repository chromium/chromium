// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_SIMULATED_GAMEPAD_INPUTS_H_
#define DEVICE_GAMEPAD_SIMULATED_GAMEPAD_INPUTS_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <vector>

#include "device/gamepad/normalization.h"

namespace device {

// The values for a simulated gamepad button.
struct SimulatedGamepadButton {
  // The simulated button's logical input value.
  double logical_value = 0.0;

  // The simulated button's pressed state. If set to `nullopt`, the pressed
  // state is determined by the normalized button value and the button press
  // threshold.
  std::optional<bool> pressed;

  // The simulated button's touched state. If set to `nullopt`, the touched
  // state mirrors the pressed state.
  std::optional<bool> touched;
};

// The values for a simulated gamepad touch.
struct SimulatedGamepadTouch {
  // The touch identifier.
  uint32_t touch_id = 0;

  // The surface identifier.
  uint32_t surface_id = 0;

  // The logical values for the x and y touch coordinates.
  double logical_x = 0.0;
  double logical_y = 0.0;
};

// A batched set of gamepad inputs representing the changes that occur at the
// same time or within the same update.
struct SimulatedGamepadInputs {
  SimulatedGamepadInputs();
  SimulatedGamepadInputs(SimulatedGamepadInputs&& other);
  SimulatedGamepadInputs& operator=(SimulatedGamepadInputs&& other);
  ~SimulatedGamepadInputs();

  // A mapping of axis index to axis values for axes with updated values.
  std::map<uint32_t, double> pending_axis_inputs;

  // A mapping of button index to button values for buttons with updated values.
  std::map<uint32_t, SimulatedGamepadButton> pending_button_inputs;

  // A list of active touch points.
  std::vector<SimulatedGamepadTouch> active_touches;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_SIMULATED_GAMEPAD_INPUTS_H_
