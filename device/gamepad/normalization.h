// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_NORMALIZATION_H_
#define DEVICE_GAMEPAD_NORMALIZATION_H_

namespace device {

// The logical minimum and maximum for a gamepad input value.
struct GamepadLogicalBounds {
  double minimum = 0.0;
  double maximum = 0.0;
};

// Scales `value` from `bounds` to the range [-1 .. 1].
// https://www.w3.org/TR/gamepad/#dfn-map-and-normalize-axes
template <class T>
double NormalizeGamepadAxis(T value, const GamepadLogicalBounds& bounds) {
  const auto& [minimum, maximum] = bounds;
  if (minimum == maximum) {
    return 0.0;
  }
  return (2.0 * (value - minimum) / (maximum - minimum)) - 1.0;
}

// Scales `value` from `bounds` to the range [0 .. 1].
// https://www.w3.org/TR/gamepad/#dfn-map-and-normalize-buttons
template <class T>
double NormalizeGamepadButton(T value, const GamepadLogicalBounds& bounds) {
  const auto& [minimum, maximum] = bounds;
  if (minimum == maximum) {
    return 0.0;
  }
  return (value - minimum) / (maximum - minimum);
}

}  // namespace device

#endif  // DEVICE_GAMEPAD_NORMALIZATION_H_
