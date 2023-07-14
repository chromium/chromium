// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_tap_gesture_params.h"

#include "base/check_op.h"

namespace content {

// Set the default tap duration to 50ms to lie within the bounds of the Aura
// gesture recognizer for identifying clicks (currently 0.01s-0.80s).
SyntheticTapGestureParams::SyntheticTapGestureParams() : duration_ms(50) {}

SyntheticTapGestureParams::SyntheticTapGestureParams(
    const SyntheticTapGestureParams& other)
    : SyntheticGestureParams(other),
      position(other.position),
      duration_ms(other.duration_ms) {}

SyntheticTapGestureParams::~SyntheticTapGestureParams() {}

SyntheticGestureParams::GestureType SyntheticTapGestureParams::GetGestureType()
    const {
  return TAP_GESTURE;
}

}  // namespace content
