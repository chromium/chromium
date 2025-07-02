// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/border_shape_painter.h"

#include <numbers>

#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_border_shape.h"
#include "third_party/blink/renderer/platform/geometry/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_types.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {
namespace {
std::optional<Path> InnerPathIgnoringStroke(const PhysicalRect& rect,
                                            const ComputedStyle& style) {
  if (!style.HasBorderShape()) {
    return std::nullopt;
  }

  return style.BorderShape()->InnerShape().GetPath(gfx::RectF(rect),
                                                   style.EffectiveZoom(), 1);
}

// static
StrokeData GetBorderShapeStrokeData(const PhysicalRect& rect,
                                    const ComputedStyle& style) {
  StrokeData stroke_data;
  const float zoom = style.EffectiveZoom();
  const float zoomed_reference_box_normal_length =
      gfx::Vector2dF(rect.size.width, rect.size.height).Length() /
      std::numbers::sqrt2;
  const float unzoomed_reference_box_normal_length =
      zoomed_reference_box_normal_length / zoom;
  const float unzoomed_thickness =
      ValueForLength(style.StrokeWidth().length(),
                     LayoutUnit(unzoomed_reference_box_normal_length));
  const float thickness = unzoomed_thickness * zoom;
  stroke_data.SetThickness(thickness);
  stroke_data.SetLineJoin(style.JoinStyle());
  stroke_data.SetMiterLimit(style.StrokeMiterLimit());
  stroke_data.SetLineCap(style.CapStyle());
  return stroke_data;
}
}  // namespace

std::optional<Path> BorderShapePainter::OuterPath(const PhysicalRect& rect,
                                                  const ComputedStyle& style) {
  if (!style.HasBorderShape()) {
    return std::nullopt;
  }

  return style.BorderShape()->OuterShape().GetPath(gfx::RectF(rect),
                                                   style.EffectiveZoom(), 1);
}

std::optional<Path> BorderShapePainter::InnerPath(const PhysicalRect& rect,
                                                  const ComputedStyle& style) {
  std::optional<Path> inner_path_from_shape =
      InnerPathIgnoringStroke(rect, style);
  if (!inner_path_from_shape) {
    return std::nullopt;
  }

  if (!style.HasVisibleStroke()) {
    return inner_path_from_shape;
  }
  SkOpBuilder builder;
  builder.add(inner_path_from_shape->GetSkPath(), SkPathOp::kUnion_SkPathOp);
  Path stroke_path = inner_path_from_shape->StrokePath(
      GetBorderShapeStrokeData(rect, style), AffineTransform());
  builder.add(stroke_path.GetSkPath(), kDifference_SkPathOp);
  SkPath result;
  return builder.resolve(&result) ? Path(result) : inner_path_from_shape;
}

// static
bool BorderShapePainter::Paint(GraphicsContext& context,
                               const PhysicalRect& rect,
                               const ComputedStyle& style) {
  const StyleBorderShape* border_shape = style.BorderShape();
  if (!border_shape) {
    return false;
  }

  const Path outer_path = *OuterPath(rect, style);
  const Path inner_path = *InnerPathIgnoringStroke(rect, style);

  const AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBorder));
  context.SetShouldAntialias(true);

  // TODO(nrosenthal) support other fill methods
  if (style.FillPaint().HasColor() && outer_path != inner_path) {
    GraphicsContextStateSaver saver(context);
    context.ClipPath(inner_path.GetSkPath(), kAntiAliased,
                     SkClipOp::kDifference);

    context.SetFillColor(style.FillPaint().color.GetColor());
    context.FillPath(outer_path, auto_dark_mode);
  }

  if (!style.HasVisibleStroke()) {
    return true;
  }

  context.SetStrokeColor(style.StrokePaint().GetColor().GetColor());
  context.SetStroke(GetBorderShapeStrokeData(rect, style));
  context.StrokePath(outer_path, auto_dark_mode);
  if (outer_path != inner_path) {
    context.StrokePath(inner_path, auto_dark_mode);
  }
  return true;
}

}  // namespace blink
