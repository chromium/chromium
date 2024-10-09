// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_CONVERSIONS_H_
#define PDF_PDF_INK_CONVERSIONS_H_

#include "base/time/time.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class PointF;
}

namespace ink {
class Brush;
}

namespace chrome_pdf {

ink::StrokeInput CreateInkStrokeInput(ink::StrokeInput::ToolType tool_type,
                                      const gfx::PointF& position,
                                      base::TimeDelta elapsed_time);

SkColor GetSkColorFromInkBrush(const ink::Brush& brush);

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_CONVERSIONS_H_
