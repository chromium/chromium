// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"

#include "base/logging.h"

namespace content {
namespace {

const float kDefaultSpeedInPixelsS = 800;

}  // namespace

SyntheticSmoothScrollGestureParams::SyntheticSmoothScrollGestureParams()
    : prevent_fling(true),
      speed_in_pixels_s(kDefaultSpeedInPixelsS),
      fling_velocity_x(0),
      fling_velocity_y(0),
      granularity(ui::input_types::ScrollGranularity::kScrollByPixel) {}

SyntheticSmoothScrollGestureParams::SyntheticSmoothScrollGestureParams(
    const SyntheticSmoothScrollGestureParams& other) = default;

SyntheticSmoothScrollGestureParams::~SyntheticSmoothScrollGestureParams() {}

SyntheticGestureParams::GestureType
SyntheticSmoothScrollGestureParams::GetGestureType() const {
  return SMOOTH_SCROLL_GESTURE;
}

const SyntheticSmoothScrollGestureParams*
SyntheticSmoothScrollGestureParams::Cast(
    const SyntheticGestureParams* gesture_params) {
  DCHECK(gesture_params);
  DCHECK_EQ(SMOOTH_SCROLL_GESTURE, gesture_params->GetGestureType());
  return static_cast<const SyntheticSmoothScrollGestureParams*>(gesture_params);
}

}  // namespace content
