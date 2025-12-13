// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_SIMULATED_GAMEPAD_PARAMS_H_
#define DEVICE_GAMEPAD_SIMULATED_GAMEPAD_PARAMS_H_

#include <stdint.h>

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "device/gamepad/gamepad_export.h"
#include "device/gamepad/normalization.h"
#include "device/gamepad/public/cpp/gamepad.h"

namespace device {

// A collection of information used when initializing a simulated gamepad.
struct DEVICE_GAMEPAD_EXPORT SimulatedGamepadParams {
  SimulatedGamepadParams();
  SimulatedGamepadParams(SimulatedGamepadParams&& other);
  SimulatedGamepadParams& operator=(SimulatedGamepadParams&& other);
  ~SimulatedGamepadParams();

  // The product name string, or `nullopt` if the simulated gamepad has no name.
  std::optional<std::string> name;

  struct VendorProduct {
    uint16_t vendor = 0;
    uint16_t product = 0;
  };

  // The vendor and product ID, or `nullopt` if the simulated gamepad has no
  // such device identifiers.
  std::optional<VendorProduct> vendor_product;

  // The value that should be set for the Gamepad.mapping attribute, or
  // `nullopt` if the implementation should decide.
  std::optional<GamepadMapping> mapping;

  // The logical minimum and maximum for each axis and button input, or
  // `nullopt` if the simulated gamepad should not normalize this input.
  std::vector<std::optional<GamepadLogicalBounds>> axis_bounds;
  std::vector<std::optional<GamepadLogicalBounds>> button_bounds;

  struct TouchSurfaceBounds {
    GamepadLogicalBounds x;
    GamepadLogicalBounds y;
  };

  // The logical minimum and maximum for the X and Y axes of each touch surface
  // on the simulated gamepad, in `surface_id` order. `nullopt` indicates that
  // the touch surface does not have surface dimensions and the simulated
  // gamepad should not normalize inputs from this surface.
  std::vector<std::optional<TouchSurfaceBounds>> touch_surface_bounds;

  // The set of haptic effect types supported by the simulated gamepad.
  std::set<GamepadHapticEffectType> vibration;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_SIMULATED_GAMEPAD_PARAMS_H_
