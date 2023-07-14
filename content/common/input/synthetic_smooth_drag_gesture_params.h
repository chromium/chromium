// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_DRAG_GESTURE_PARAMS_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_DRAG_GESTURE_PARAMS_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"

namespace content {

struct CONTENT_EXPORT SyntheticSmoothDragGestureParams
    : public SyntheticGestureParams {
 public:
  SyntheticSmoothDragGestureParams();
  SyntheticSmoothDragGestureParams(
      const SyntheticSmoothDragGestureParams& other);
  ~SyntheticSmoothDragGestureParams() override;

  GestureType GetGestureType() const override;

  gfx::PointF start_point;
  std::vector<gfx::Vector2dF> distances;
  float speed_in_pixels_s = SyntheticGestureParams::kDefaultSpeedInPixelsPerSec;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_DRAG_GESTURE_PARAMS_H_
