// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/border_shape_painter.h"

#include <algorithm>
#include <numbers>

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_border_shape.h"
#include "third_party/blink/renderer/platform/geometry/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_types.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {
namespace {

std::optional<Path> InnerPathIgnoringStroke(const PhysicalRect& reference_rect,
                                            const ComputedStyle& style) {
  if (!style.HasBorderShape()) {
    return std::nullopt;
  }

  const StyleBorderShape& border_shape = *style.BorderShape();
  return border_shape.InnerShape().GetPath(gfx::RectF(reference_rect),
                                           style.EffectiveZoom(), 1);
}

// static
StrokeData GetBorderShapeStrokeData(const PhysicalRect& reference_rect,
                                    const ComputedStyle& style) {
  StrokeData stroke_data;
  const float zoom = style.EffectiveZoom();
  const float zoomed_reference_box_normal_length =
      gfx::Vector2dF(reference_rect.Width(), reference_rect.Height()).Length() /
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

std::optional<Path> BorderShapePainter::OuterPath(
    const ComputedStyle& style,
    const PhysicalRect& outer_reference_rect) {
  if (!style.HasBorderShape()) {
    return std::nullopt;
  }

  const StyleBorderShape& border_shape = *style.BorderShape();
  return border_shape.OuterShape().GetPath(gfx::RectF(outer_reference_rect),
                                           style.EffectiveZoom(), 1);
}

std::optional<Path> BorderShapePainter::InnerPath(
    const ComputedStyle& style,
    const PhysicalRect& inner_reference_rect) {
  std::optional<Path> inner_path_from_shape =
      InnerPathIgnoringStroke(inner_reference_rect, style);
  if (!inner_path_from_shape) {
    return std::nullopt;
  }

  if (!style.HasVisibleStroke()) {
    return inner_path_from_shape;
  }

  StrokeData stroke_data =
      GetBorderShapeStrokeData(inner_reference_rect, style);
  SkOpBuilder builder;
  builder.add(inner_path_from_shape->GetSkPath(), SkPathOp::kUnion_SkPathOp);
  Path stroke_path =
      inner_path_from_shape->StrokePath(stroke_data, AffineTransform());
  builder.add(stroke_path.GetSkPath(), kDifference_SkPathOp);
  SkPath result;
  return builder.resolve(&result) ? Path(result) : inner_path_from_shape;
}

// static
bool BorderShapePainter::Paint(GraphicsContext& context,
                               const ComputedStyle& style,
                               const PhysicalRect& outer_reference_rect,
                               const PhysicalRect& inner_reference_rect) {
  const StyleBorderShape* border_shape = style.BorderShape();
  if (!border_shape) {
    return false;
  }

  const Path outer_path = *OuterPath(style, outer_reference_rect);
  const Path inner_path = *InnerPathIgnoringStroke(inner_reference_rect, style);

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
  context.SetStroke(GetBorderShapeStrokeData(outer_reference_rect, style));
  context.StrokePath(outer_path, auto_dark_mode);
  if (outer_path != inner_path) {
    context.StrokePath(inner_path, auto_dark_mode);
  }
  return true;
}

// static
std::optional<PhysicalBoxStrut> BorderShapePainter::VisualOutsets(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    const PhysicalRect& outer_reference_rect,
    const PhysicalRect& inner_reference_rect) {
  if (!style.HasBorderShape()) {
    return std::nullopt;
  }

  std::optional<Path> outer_path = OuterPath(style, outer_reference_rect);
  if (!outer_path) {
    return std::nullopt;
  }

  gfx::RectF visual_bounds = outer_path->BoundingRect();
  if (style.HasVisibleStroke()) {
    StrokeData stroke_data =
        GetBorderShapeStrokeData(outer_reference_rect, style);
    visual_bounds.Union(outer_path->StrokeBoundingRect(stroke_data));

    if (std::optional<Path> inner_path =
            InnerPathIgnoringStroke(inner_reference_rect, style)) {
      visual_bounds.Union(inner_path->BoundingRect());
      visual_bounds.Union(inner_path->StrokeBoundingRect(stroke_data));
    }
  }

  const float top_outset = std::max(0.0f, border_rect.Y() - visual_bounds.y());
  const float left_outset = std::max(0.0f, border_rect.X() - visual_bounds.x());
  const float right_outset =
      std::max(0.0f, visual_bounds.right() - border_rect.Right());
  const float bottom_outset =
      std::max(0.0f, visual_bounds.bottom() - border_rect.Bottom());

  if (!top_outset && !right_outset && !bottom_outset && !left_outset) {
    return PhysicalBoxStrut();
  }

  return PhysicalBoxStrut::Enclosing(gfx::OutsetsF::TLBR(
      top_outset, left_outset, bottom_outset, right_outset));
}

}  // namespace blink
