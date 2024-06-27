// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_STROKE_INPUT_H_
#define PDF_INK_INK_STROKE_INPUT_H_

#include <cstdint>

#include "pdf/ink/ink_point.h"

namespace chrome_pdf {

struct InkStrokeInput {
  enum class ToolType : int8_t { kUnknown, kMouse, kTouch, kStylus };

  static constexpr float kNoPressure = -1;
  static constexpr float kNoTilt = -1;
  static constexpr float kNoOrientation = -1;

  ToolType tool_type = ToolType::kUnknown;
  InkPoint position;
  float elapsed_time_seconds;
  float pressure = kNoPressure;
  float tilt_in_radians = kNoTilt;
  float orientation_in_radians = kNoOrientation;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_STROKE_INPUT_H_
