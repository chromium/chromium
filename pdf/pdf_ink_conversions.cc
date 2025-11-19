// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_conversions.h"

#include <cmath>
#include <variant>

#include "base/check_op.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/ink/src/ink/brush/brush.h"
#include "third_party/ink/src/ink/brush/brush_coat.h"
#include "third_party/ink/src/ink/brush/brush_paint.h"
#include "third_party/ink/src/ink/brush/color_function.h"
#include "third_party/ink/src/ink/color/color.h"
#include "third_party/ink/src/ink/geometry/point.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input.h"
#include "ui/gfx/geometry/point_f.h"

namespace chrome_pdf {

namespace {

float InkPressureFromBlinkPressure(float pressure) {
  return std::isnan(pressure) ? ink::StrokeInput::kNoPressure : pressure;
}

}  // namespace

ink::StrokeInput CreateInkStrokeInput(ink::StrokeInput::ToolType tool_type,
                                      const gfx::PointF& position,
                                      base::TimeDelta elapsed_time) {
  return CreateInkStrokeInputWithProperties(tool_type, position, elapsed_time,
                                            /*properties=*/nullptr);
}

ink::StrokeInput CreateInkStrokeInputWithProperties(
    ink::StrokeInput::ToolType tool_type,
    const gfx::PointF& position,
    base::TimeDelta elapsed_time,
    const blink::WebPointerProperties* properties) {
  ink::StrokeInput result{
      .tool_type = tool_type,
      .position = InkPointFromGfxPoint(position),
      .elapsed_time = ink::Duration32::Seconds(
          static_cast<float>(elapsed_time.InSecondsF())),
  };
  if (properties) {
    result.pressure = InkPressureFromBlinkPressure(properties->force);
  }
  return result;
}

SkColor GetSkColorFromInkBrush(const ink::Brush& brush) {
  ink::Color::RgbaUint8 rgba =
      brush.GetColor().AsUint8(ink::Color::Format::kGammaEncoded);
  return SkColorSetARGB(rgba.a, rgba.r, rgba.g, rgba.b);
}

float GetOpacityMultiplierFromBrush(const ink::Brush& brush) {
  CHECK_EQ(brush.CoatCount(), 1u);
  const ink::BrushCoat& coat = brush.GetCoats()[0];
  CHECK_EQ(coat.paint_preferences.size(), 1u);
  const ink::BrushPaint& paint = coat.paint_preferences[0];
  CHECK_EQ(paint.color_functions.size(), 1u);
  const auto& parameters = paint.color_functions[0].parameters;
  return std::get<ink::ColorFunction::OpacityMultiplier>(parameters).multiplier;
}

ink::Point InkPointFromGfxPoint(const gfx::PointF& point) {
  return ink::Point(point.x(), point.y());
}

}  // namespace chrome_pdf
