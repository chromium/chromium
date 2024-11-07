// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_brush.h"

#include <optional>

#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/ink/src/ink/brush/brush.h"
#include "third_party/ink/src/ink/brush/brush_family.h"
#include "third_party/ink/src/ink/brush/brush_paint.h"
#include "third_party/ink/src/ink/brush/brush_tip.h"
#include "third_party/ink/src/ink/color/color.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_conversions.h"

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

ink::Brush CreateInkBrush(PdfInkBrush::Type type, SkColor color, float size) {
  CHECK(PdfInkBrush::IsToolSizeInRange(size));

  // TODO(crbug.com/353942923): Use real values here.
  ink::BrushTip tip;
  tip.corner_rounding = GetCornerRounding(type);
  tip.opacity_multiplier = GetOpacity(type);

  // TODO(crbug.com/353942923): Use real `uri_string` here.
  auto family = ink::BrushFamily::Create(tip, ink::BrushPaint(),
                                         /*uri_string=*/"");
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
  auto size_result = ink_brush_.SetSize(std::move(size));
  CHECK(size_result.ok());
}

}  // namespace chrome_pdf
