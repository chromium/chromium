// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/border_shape_painter.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
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

struct DerivedStroke {
  float thickness;
  Color color;
};

// https://drafts.csswg.org/css-borders-4/#relevant-side-for-border-shape
DerivedStroke RelevantSideForBorderShape(const ComputedStyle& style) {
  DCHECK(style.HasBorderShape());

  BorderEdgeArray edges;
  style.GetBorderEdgeInfo(edges);
  style.BorderBlockStartWidth();
  PhysicalToLogical<BorderEdge> logical_edges(
      style.GetWritingDirection(), edges[static_cast<unsigned>(BoxSide::kTop)],
      edges[static_cast<unsigned>(BoxSide::kRight)],
      edges[static_cast<unsigned>(BoxSide::kBottom)],
      edges[static_cast<unsigned>(BoxSide::kLeft)]);

  const BorderEdge block_start_edge = logical_edges.BlockStart();
  const BorderEdge inline_start_edge = logical_edges.InlineStart();
  const BorderEdge block_end_edge = logical_edges.BlockEnd();
  const BorderEdge inline_end_edge = logical_edges.InlineEnd();

  const BorderEdge edges_in_order[4] = {block_start_edge, inline_start_edge,
                                        block_end_edge, inline_end_edge};
  for (const BorderEdge& edge : edges_in_order) {
    if (edge.BorderStyle() == EBorderStyle::kNone) {
      continue;
    }
    return DerivedStroke{static_cast<float>(edge.UsedWidth()), edge.GetColor()};
  }
  // Return block-start.
  return DerivedStroke{static_cast<float>(edges_in_order[0].UsedWidth()),
                       edges_in_order[0].GetColor()};
}

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
