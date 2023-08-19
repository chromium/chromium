// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_TAP_GESTURE_PARAMS_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_TAP_GESTURE_PARAMS_H_

#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "ui/gfx/geometry/point_f.h"

namespace content {

struct CONTENT_EXPORT SyntheticTapGestureParams
    : public SyntheticGestureParams {
 public:
  SyntheticTapGestureParams();
  SyntheticTapGestureParams(const SyntheticTapGestureParams& other);
  ~SyntheticTapGestureParams() override;

  GestureType GetGestureType() const override;

  gfx::PointF position;
  float duration_ms;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_TAP_GESTURE_PARAMS_H_
