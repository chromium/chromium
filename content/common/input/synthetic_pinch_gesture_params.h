// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_PINCH_GESTURE_PARAMS_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_PINCH_GESTURE_PARAMS_H_

#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "ui/gfx/geometry/point_f.h"

namespace content {

struct CONTENT_EXPORT SyntheticPinchGestureParams
    : public SyntheticGestureParams {
 public:
  SyntheticPinchGestureParams();
  SyntheticPinchGestureParams(
      const SyntheticPinchGestureParams& other);
  ~SyntheticPinchGestureParams() override;

  GestureType GetGestureType() const override;

  float scale_factor;
  gfx::PointF anchor;
  float relative_pointer_speed_in_pixels_s;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_PINCH_GESTURE_PARAMS_H_
