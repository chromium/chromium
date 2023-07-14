// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_pinch_gesture_params.h"

#include "base/check_op.h"

namespace content {

SyntheticPinchGestureParams::SyntheticPinchGestureParams()
    : scale_factor(1.0f),
      relative_pointer_speed_in_pixels_s(500) {}

SyntheticPinchGestureParams::SyntheticPinchGestureParams(
    const SyntheticPinchGestureParams& other)
    : SyntheticGestureParams(other),
      scale_factor(other.scale_factor),
      anchor(other.anchor),
      relative_pointer_speed_in_pixels_s(
          other.relative_pointer_speed_in_pixels_s) {}

SyntheticPinchGestureParams::~SyntheticPinchGestureParams() {}

SyntheticGestureParams::GestureType
SyntheticPinchGestureParams::GetGestureType() const {
  return PINCH_GESTURE;
}

}  // namespace content
