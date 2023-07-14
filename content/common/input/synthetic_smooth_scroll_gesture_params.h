// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_PARAMS_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_PARAMS_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "ui/events/types/scroll_types.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"

namespace content {

struct CONTENT_EXPORT SyntheticSmoothScrollGestureParams
    : public SyntheticGestureParams {
 public:
  SyntheticSmoothScrollGestureParams();
  SyntheticSmoothScrollGestureParams(
      const SyntheticSmoothScrollGestureParams& other);
  ~SyntheticSmoothScrollGestureParams() override;

  GestureType GetGestureType() const override;

  gfx::PointF anchor;
  std::vector<gfx::Vector2dF> distances;  // Positive X/Y to scroll left/up.
  bool prevent_fling = true;              // Defaults to true.
  float speed_in_pixels_s = SyntheticGestureParams::kDefaultSpeedInPixelsPerSec;
  float fling_velocity_x = 0;
  float fling_velocity_y = 0;
  ui::ScrollGranularity granularity = ui::ScrollGranularity::kScrollByPixel;
  // A bitfield of values from blink::WebInputEvent::Modifiers.
  int modifiers = 0;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_PARAMS_H_
