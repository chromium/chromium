// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"

#include "base/check_op.h"

namespace content {

SyntheticSmoothScrollGestureParams::SyntheticSmoothScrollGestureParams() =
    default;

SyntheticSmoothScrollGestureParams::SyntheticSmoothScrollGestureParams(
    const SyntheticSmoothScrollGestureParams& other) = default;

SyntheticSmoothScrollGestureParams::~SyntheticSmoothScrollGestureParams() =
    default;

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
