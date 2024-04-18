// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_BRUSH_TIP_H_
#define PDF_INK_INK_BRUSH_TIP_H_

#include <vector>

#include "pdf/ink/ink_brush_behavior.h"

namespace chrome_pdf {

struct InkBrushTip {
  InkBrushTip();
  ~InkBrushTip();

  float scale_x = 1;
  float scale_y = 1;
  float corner_rounding = 1;
  float slant = 0;
  float pinch = 0;
  float rotation = 0;
  float opacity_multiplier = 1;
  std::vector<InkBrushBehavior> behaviors;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_BRUSH_TIP_H_
