// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_TEST_WEBXR_TEST_GAMEPAD_UTILS_H_
#define DEVICE_VR_TEST_WEBXR_TEST_GAMEPAD_UTILS_H_

#include <cstddef>
#include <optional>

#include "base/component_export.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"

namespace device {

// Button identifiers used by WebXR test hooks and OpenXR runtime mock.
enum XrButtonId {
  kAxisTrigger = 0,
  kGrip = 1,
  kAxisTrackpad = 2,
  kAxisThumbstick = 3,
  kA = 4,
  kB = 5,
  kX = 6,
  kY = 7,
  kThumbRest = 8,
  kShoulder = 9,
  kMenu = 10,
  kMax = 11,
};

// Maps an XrButtonId to its button index in the test Gamepad layout:
// - [0] Trigger (kAxisTrigger)
// - [1] Grip / Squeeze (kGrip)
// - [2] Touchpad (kAxisTrackpad)
// - [3] Thumbstick (kAxisThumbstick)
// - [4] A / X (kA, kX)
// - [5] B / Y (kB, kY)
// - [6] ThumbRest / Shoulder (kThumbRest, kShoulder)
// - [7] Menu (kMenu)
constexpr std::optional<size_t> GamepadButtonIndexFromButtonId(XrButtonId id) {
  switch (id) {
    case XrButtonId::kAxisTrigger:
      return 0;
    case XrButtonId::kGrip:
      return 1;
    case XrButtonId::kAxisTrackpad:
      return 2;
    case XrButtonId::kAxisThumbstick:
      return 3;
    case XrButtonId::kA:
    case XrButtonId::kX:
      return 4;
    case XrButtonId::kB:
    case XrButtonId::kY:
      return 5;
    case XrButtonId::kThumbRest:
    case XrButtonId::kShoulder:
      return 6;
    case XrButtonId::kMenu:
    case XrButtonId::kMax:
      return 7;
    default:
      return std::nullopt;
  }
}

// Maps an XrButtonId for a 2D axis (trackpad or thumbstick) to its starting
// index in the Gamepad axes array:
// - Trackpad: axes 0 (X) and 1 (Y)
// - Thumbstick: axes 2 (X) and 3 (Y)
constexpr std::optional<size_t> GamepadAxisStartIndexFromButtonId(
    XrButtonId id) {
  switch (id) {
    case XrButtonId::kAxisTrackpad:
      // Trackpad uses axes 0 (X) and 1 (Y).
      return 0;
    case XrButtonId::kAxisThumbstick:
      // Thumbstick uses axes 2 (X) and 3 (Y).
      return 2;
    default:
      return std::nullopt;
  }
}

// Returns the minimum number of axes required in Gamepad::axes to store the 2D
// axis data for the specified button ID.
constexpr std::optional<size_t> RequiredGamepadAxesSizeFromButtonId(
    XrButtonId id) {
  switch (id) {
    case XrButtonId::kAxisTrackpad:
      // Trackpad starts at axis index 0 and uses 2 axes (0 and 1).
      return 2;
    case XrButtonId::kAxisThumbstick:
      // Thumbstick starts at axis index 2 and uses 2 axes (2 and 3), requiring
      // a total size of 4 axes.
      return 4;
    default:
      return std::nullopt;
  }
}

// Returns a pointer to the GamepadButton for the specified XrButtonId, or
// nullptr if the gamepad is null or index is out of bounds.
COMPONENT_EXPORT(VR_TEST_UTILS)
device::GamepadButton* GetGamepadButton(device::mojom::Gamepad* gamepad,
                                        XrButtonId id);

COMPONENT_EXPORT(VR_TEST_UTILS)
const device::GamepadButton* GetGamepadButton(
    const device::mojom::Gamepad* gamepad,
    XrButtonId id);

}  // namespace device

#endif  // DEVICE_VR_TEST_WEBXR_TEST_GAMEPAD_UTILS_H_
