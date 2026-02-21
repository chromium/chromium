// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/border_shape_painter.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/border_shape_utils.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_border_shape.h"
#include "third_party/blink/renderer/platform/geometry/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_types.h"
#include "third_party/blink/renderer/platform/text/writing_mode_utils.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {
namespace {

Path OuterPathWithoutStroke(const ComputedStyle& style,
                            const PhysicalRect& outer_reference_rect) {
  CHECK(style.HasBorderShape());

  const StyleBorderShape& border_shape = *style.BorderShape();
  return border_shape.OuterShape().GetPath(gfx::RectF(outer_reference_rect),
                                           style.EffectiveZoom(), 1);
}

}  // namespace

Path BorderShapePainter::OuterPath(const ComputedStyle& style,
                                   const PhysicalRect& outer_reference_rect) {
  CHECK(style.HasBorderShape());
  Path outer_path = OuterPathWithoutStroke(style, outer_reference_rect);
  if (style.BorderShape()->HasSeparateInnerShape()) {
    return outer_path;
  }

  // A stroke thickness of 0 renders a hairline path, but we want to render
  // nothing.
  DerivedStroke derived_stroke = RelevantSideForBorderShape(style);
  if (!derived_stroke.thickness) {
    return outer_path;
  }

  // Add stroke to the outer path, if we don't have an inner one.
  StrokeData stroke_data;
  stroke_data.SetThickness(derived_stroke.thickness);
  SkOpBuilder builder;
  builder.add(outer_path.GetSkPath(), SkPathOp::kUnion_SkPathOp);
  Path stroke_path = outer_path.StrokePath(stroke_data, AffineTransform());
  builder.add(stroke_path.GetSkPath(), SkPathOp::kUnion_SkPathOp);
  SkPath result;
  return builder.resolve(&result) ? Path(result) : outer_path;
}

Path BorderShapePainter::InnerPath(const ComputedStyle& style,
                                   const PhysicalRect& inner_reference_rect) {
  CHECK(style.HasBorderShape());

  const StyleBorderShape& border_shape = *style.BorderShape();
  return border_shape.InnerShape().GetPath(gfx::RectF(inner_reference_rect),
                                           style.EffectiveZoom(), 1);
}

Path BorderShapePainter::OverflowClipInnerPath(
    const ComputedStyle& style,
    const PhysicalRect& inner_reference_rect) {
  CHECK(style.HasBorderShape());
  Path inner_path = InnerPath(style, inner_reference_rect);

  // For single-shape border-shape, the border is drawn as a stroke centered on
  // the path with the border width as the stroke thickness. The inner edge of
  // the border is therefore at path - border_width/2. Contract the path inward
  // by half the border width so that overflow-clipped children do not paint
  // over the inner half of the border stroke.
  const StyleBorderShape& border_shape = *style.BorderShape();
  if (!border_shape.HasSeparateInnerShape()) {
    DerivedStroke derived_stroke = RelevantSideForBorderShape(style);
    if (derived_stroke.thickness > 0) {
      StrokeData stroke_data;
      stroke_data.SetThickness(derived_stroke.thickness);
      Path stroke_path = inner_path.StrokePath(stroke_data, AffineTransform());
      SkOpBuilder builder;
      builder.add(inner_path.GetSkPath(), SkPathOp::kUnion_SkPathOp);
      builder.add(stroke_path.GetSkPath(), SkPathOp::kDifference_SkPathOp);
      SkPath result;
      if (builder.resolve(&result)) {
        return Path(result);
      }
    }
  }
  return inner_path;
}

Path BorderShapePainter::OuterPathWithOffset(
    const ComputedStyle& style,
    const PhysicalRect& outer_reference_rect,
    float offset) {
  CHECK(style.HasBorderShape());
  Path outer_path = OuterPath(style, outer_reference_rect);

  if (offset == 0) {
    return outer_path;
  }

  StrokeData stroke_data;
  stroke_data.SetThickness(std::abs(offset) * 2);
  Path stroke_path = outer_path.StrokePath(stroke_data, AffineTransform());

  SkOpBuilder builder;
  builder.add(outer_path.GetSkPath(), SkPathOp::kUnion_SkPathOp);
  if (offset > 0) {
    // Expand: union the path with its stroke
    builder.add(stroke_path.GetSkPath(), SkPathOp::kUnion_SkPathOp);
  } else {
    // Contract: intersect the path with inverted stroke
    builder.add(stroke_path.GetSkPath(), SkPathOp::kDifference_SkPathOp);
  }
  SkPath result;
  return builder.resolve(&result) ? Path(result) : outer_path;
}

bool BorderShapePainter::Paint(GraphicsContext& context,
                               const ComputedStyle& style,
                               const PhysicalRect& outer_reference_rect,
                               const PhysicalRect& inner_reference_rect) {
  const StyleBorderShape* border_shape = style.BorderShape();
  if (!border_shape) {
    return false;
  }

  const Path outer_path = OuterPathWithoutStroke(style, outer_reference_rect);

  DerivedStroke derived_stroke = RelevantSideForBorderShape(style);
  const AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBorder));
  context.SetShouldAntialias(true);

  // When two <basic-shape> values are given, the border is rendered as the
  // shape between the two paths.
  if (border_shape->HasSeparateInnerShape()) {
    SkOpBuilder builder;
    builder.add(outer_path.GetSkPath(), SkPathOp::kUnion_SkPathOp);
    const Path inner_path = InnerPath(style, inner_reference_rect);
    builder.add(inner_path.GetSkPath(), SkPathOp::kDifference_SkPathOp);
    SkPath result;
    builder.resolve(&result);
    Path fill_path(result);
    context.SetFillColor(derived_stroke.color);
    context.FillPath(fill_path, auto_dark_mode);
    return true;
  }

  // A stroke thickness of 0 renders a hairline path, but we want to render
  // nothing.
  if (!derived_stroke.thickness) {
    return true;
  }

  // When only a single <basic-shape> is given, the border is rendered as a
  // stroke with the relevant side’s computed border width as the stroke width.
  StrokeData stroke_data;
  stroke_data.SetThickness(derived_stroke.thickness);
  context.SetStrokeColor(derived_stroke.color);
  context.SetStroke(stroke_data);
  context.StrokePath(outer_path, auto_dark_mode);
  return true;
}

bool BorderShapePainter::PaintOutline(GraphicsContext& context,
                                      const ComputedStyle& style,
                                      const PhysicalRect& outer_reference_rect,
                                      int outline_width,
                                      int outline_offset) {
  const StyleBorderShape* border_shape = style.BorderShape();
  if (!border_shape || outline_width <= 0) {
    return false;
  }

  // Calculate the offset from the outer_path to the center of the outline
  // stroke.
  //
  // When border-shape uses a single shape, the border is drawn as a stroke
  // centered on the outer_path. The outer edge of the border is at
  // border_width/2 from the path. The outline starts from there.
  //
  // When border-shape uses double shapes (outer + inner), the border fills the
  // area between them. The outline starts from the outer_path directly
  // (border_stroke_offset = 0).
  float border_stroke_offset = 0;
  if (!border_shape->HasSeparateInnerShape()) {
    DerivedStroke derived_stroke = RelevantSideForBorderShape(style);
    border_stroke_offset = derived_stroke.thickness / 2.0f;
  }

  const float center_offset = border_stroke_offset +
                              static_cast<float>(outline_offset) +
                              static_cast<float>(outline_width) / 2.0f;
  Path center_path =
      OuterPathWithOffset(style, outer_reference_rect, center_offset);

  const Color outline_color =
      style.VisitedDependentColor(GetCSSPropertyOutlineColor());
  const AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBorder));

  context.SetShouldAntialias(true);

  EBorderStyle outline_style = style.OutlineStyle();

  // For solid outline, stroke the center path with the outline width.
  if (outline_style == EBorderStyle::kSolid) {
    StrokeData stroke_data;
    stroke_data.SetThickness(static_cast<float>(outline_width));
    context.SetStrokeColor(outline_color);
    context.SetStroke(stroke_data);
    context.StrokePath(center_path, auto_dark_mode);
    return true;
  } else if (outline_style == EBorderStyle::kDouble) {
    // For double outline, draw two strokes.
    const float stroke_width =
        std::round(static_cast<float>(outline_width) / 3.0f);
    if (stroke_width < 1) {
      // Fall back to solid if too thin.
      StrokeData stroke_data;
      stroke_data.SetThickness(static_cast<float>(outline_width));
      context.SetStrokeColor(outline_color);
      context.SetStroke(stroke_data);
      context.StrokePath(center_path, auto_dark_mode);
      return true;
    }

    // Outer stroke
    const float outer_offset = center_offset +
                               static_cast<float>(outline_width) / 2.0f -
                               stroke_width / 2.0f;
    Path outer_stroke_path =
        OuterPathWithOffset(style, outer_reference_rect, outer_offset);
    StrokeData outer_stroke_data;
    outer_stroke_data.SetThickness(stroke_width);
    context.SetStrokeColor(outline_color);
    context.SetStroke(outer_stroke_data);
    context.StrokePath(outer_stroke_path, auto_dark_mode);

    // Inner stroke
    const float inner_offset = center_offset -
                               static_cast<float>(outline_width) / 2.0f +
                               stroke_width / 2.0f;
    Path inner_stroke_path =
        OuterPathWithOffset(style, outer_reference_rect, inner_offset);
    context.StrokePath(inner_stroke_path, auto_dark_mode);
    return true;
  }

  // For other styles (dotted, dashed, groove, ridge, etc.), fall back to
  // standard outline painting by returning false.
  return false;
}

// static
PhysicalBoxStrut BorderShapePainter::VisualOutsets(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    const PhysicalRect& outer_reference_rect,
    const PhysicalRect& inner_reference_rect) {
  CHECK(style.HasBorderShape());

  const Path outer_path = OuterPath(style, outer_reference_rect);
  gfx::RectF visual_bounds = outer_path.BoundingRect();
  const Path inner_path = InnerPath(style, inner_reference_rect);
  visual_bounds.Union(inner_path.BoundingRect());

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
