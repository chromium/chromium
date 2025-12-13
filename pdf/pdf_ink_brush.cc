// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_brush.h"

#include <optional>
#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/ink/src/ink/brush/brush.h"
#include "third_party/ink/src/ink/brush/brush_behavior.h"
#include "third_party/ink/src/ink/brush/brush_family.h"
#include "third_party/ink/src/ink/brush/brush_paint.h"
#include "third_party/ink/src/ink/brush/brush_tip.h"
#include "third_party/ink/src/ink/color/color.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

namespace {

float GetCornerRounding(PdfInkBrush::Type type) {
  switch (type) {
    case PdfInkBrush::Type::kHighlighter:
      return 0.0f;
    case PdfInkBrush::Type::kPen:
      return 1.0f;
  }
  NOTREACHED();
}

float GetOpacity(PdfInkBrush::Type type) {
  switch (type) {
    case PdfInkBrush::Type::kHighlighter:
      // LINT.IfChange(HighlighterOpacity)
      return 0.4f;
      // LINT.ThenChange(//chrome/browser/resources/pdf/pdf_viewer_utils.ts:HighlighterOpacity)
    case PdfInkBrush::Type::kPen:
      return 1.0f;
  }
  NOTREACHED();
}

// ink::Brush actually uses ink::Color, but pdf/ uses SkColor. To avoid having
// multiple color representations, do not expose ink::Color and just convert
// `color`.
ink::Color GetInkColorFromSkColor(SkColor color) {
  return ink::Color::FromUint8(
      /*red=*/SkColorGetR(color),
      /*green=*/SkColorGetG(color),
      /*blue=*/SkColorGetB(color),
      /*alpha=*/SkColorGetA(color));
}

std::vector<ink::BrushBehavior> GetTipBehaviors(PdfInkBrush::Type type) {
  switch (type) {
    case PdfInkBrush::Type::kHighlighter:
      return {};
    case PdfInkBrush::Type::kPen:
      return {
          ink::BrushBehavior{{
              ink::BrushBehavior::SourceNode{
                  .source = ink::BrushBehavior::Source::kNormalizedPressure,
                  .source_value_range = {0.8, 1},
              },
              ink::BrushBehavior::ToolTypeFilterNode{{.stylus = true}},
              ink::BrushBehavior::DampingNode{
                  .damping_source =
                      ink::BrushBehavior::DampingSource::kTimeInSeconds,
                  .damping_gap = 0.025,
              },
              ink::BrushBehavior::TargetNode{
                  .target = ink::BrushBehavior::Target::kSizeMultiplier,
                  .target_modifier_range = {1, 1.5},
              },
          }},
          ink::BrushBehavior{{
              ink::BrushBehavior::SourceNode{
                  .source =
                      ink::BrushBehavior::Source::kPredictedTimeElapsedInMillis,
                  .source_value_range = {0, 24},
              },
              ink::BrushBehavior::SourceNode{
                  .source = ink::BrushBehavior::Source::
                      kPredictedDistanceTraveledInMultiplesOfBrushSize,
                  .source_value_range = {1.5, 2},
              },
              ink::BrushBehavior::ResponseNode{
                  .response_curve =
                      {ink::EasingFunction::Predefined::kEaseInOut},
              },
              ink::BrushBehavior::BinaryOpNode{
                  .operation = ink::BrushBehavior::BinaryOp::kProduct,
              },
              ink::BrushBehavior::TargetNode{
                  .target = ink::BrushBehavior::Target::kOpacityMultiplier,
                  .target_modifier_range = {1, 0.3},
              },
          }}};
  }
  NOTREACHED();
}

ink::Brush CreateInkBrush(PdfInkBrush::Type type, SkColor color, float size) {
  ink::BrushTip tip;
  tip.corner_rounding = GetCornerRounding(type);
  tip.behaviors = GetTipBehaviors(type);

  ink::BrushPaint paint;
  paint.color_functions.emplace_back(
      ink::ColorFunction::OpacityMultiplier{.multiplier = GetOpacity(type)});

  // TODO(crbug.com/353942923): Use real `client_brush_family_id` here.
  auto family = ink::BrushFamily::Create(tip, paint,
                                         /*client_brush_family_id=*/"");
  CHECK(family.ok());

  auto brush = ink::Brush::Create(*family,
                                  /*color=*/
                                  GetInkColorFromSkColor(color),
                                  /*size=*/size,
                                  /*epsilon=*/0.1f);
  CHECK(brush.ok());
  return *brush;
}

// Determine the area to invalidate centered around a point where a brush is
// applied.
gfx::Rect GetPointInvalidateArea(float brush_diameter,
                                 const gfx::PointF& center) {
  // Choose a rectangle that surrounds the point for the brush radius.
  float brush_radius = brush_diameter / 2;
  return gfx::ToEnclosingRect(gfx::RectF(center.x() - brush_radius,
                                         center.y() - brush_radius,
                                         brush_diameter, brush_diameter));
}

}  // namespace

// static
std::optional<PdfInkBrush::Type> PdfInkBrush::StringToType(
    const std::string& brush_type) {
  if (brush_type == "highlighter") {
    return Type::kHighlighter;
  }
  if (brush_type == "pen") {
    return Type::kPen;
  }
  return std::nullopt;
}

// static
std::string PdfInkBrush::TypeToString(Type brush_type) {
  switch (brush_type) {
    case Type::kHighlighter:
      return "highlighter";
    case Type::kPen:
      return "pen";
  }
  NOTREACHED();
}

// static
bool PdfInkBrush::IsToolSizeInRange(float size) {
  return size >= 1 && size <= 16;
}

PdfInkBrush::PdfInkBrush(Type brush_type, SkColor color, float size)
    : ink_brush_(CreateInkBrush(brush_type, color, size)) {}

PdfInkBrush::~PdfInkBrush() = default;

gfx::Rect PdfInkBrush::GetInvalidateArea(const gfx::PointF& center1,
                                         const gfx::PointF& center2) const {
  // For a line connecting `center1` to `center2`, the invalidate
  // region is the union between the areas affected by them both.
  float brush_diameter = ink_brush_.GetSize();
  gfx::Rect area1 = GetPointInvalidateArea(brush_diameter, center1);
  gfx::Rect area2 = GetPointInvalidateArea(brush_diameter, center2);
  area2.Union(area1);
  return area2;
}

void PdfInkBrush::SetColor(SkColor color) {
  ink_brush_.SetColor(GetInkColorFromSkColor(color));
}

void PdfInkBrush::SetSize(float size) {
  auto size_result = ink_brush_.SetSize(size);
  CHECK(size_result.ok());
}

}  // namespace chrome_pdf
