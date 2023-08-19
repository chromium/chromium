// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_smooth_drag_gesture_params.h"

#include "base/check_op.h"

namespace content {

SyntheticSmoothDragGestureParams::SyntheticSmoothDragGestureParams() = default;

SyntheticSmoothDragGestureParams::SyntheticSmoothDragGestureParams(
    const SyntheticSmoothDragGestureParams& other)
    : SyntheticGestureParams(other),
      start_point(other.start_point),
      distances(other.distances),
      speed_in_pixels_s(other.speed_in_pixels_s) {
}

SyntheticSmoothDragGestureParams::~SyntheticSmoothDragGestureParams() = default;

SyntheticGestureParams::GestureType
SyntheticSmoothDragGestureParams::GetGestureType() const {
  return SMOOTH_DRAG_GESTURE;
}

}  // namespace content
