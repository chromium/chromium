// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_conversions.h"

#include "base/time/time.h"
#include "third_party/ink/src/ink/brush/brush.h"
#include "third_party/ink/src/ink/color/color.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input.h"
#include "ui/gfx/geometry/point_f.h"

namespace chrome_pdf {

ink::StrokeInput CreateInkStrokeInput(ink::StrokeInput::ToolType tool_type,
                                      const gfx::PointF& position,
                                      base::TimeDelta elapsed_time) {
  return {
      .tool_type = tool_type,
      .position = {position.x(), position.y()},
      .elapsed_time = ink::Duration32::Seconds(
          static_cast<float>(elapsed_time.InSecondsF())),
  };
}

SkColor GetSkColorFromInkBrush(const ink::Brush& brush) {
  ink::Color::RgbaUint8 rgba =
      brush.GetColor().AsUint8(ink::Color::Format::kGammaEncoded);
  return SkColorSetARGB(rgba.a, rgba.r, rgba.g, rgba.b);
}

}  // namespace chrome_pdf
