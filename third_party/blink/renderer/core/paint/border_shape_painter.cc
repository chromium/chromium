// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/border_shape_painter.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/border_shape_utils.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
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

StrokeData GetStrokeData(const StyleBorderShape& border_shape,
                         float thickness) {
  StrokeData stroke_data;
  stroke_data.SetThickness(thickness);

  // If the shape is a polygon, check if the CSS `round <length>` modifier was
  // used. If so, we change the join type to round to handle two scenarios:
  //
  //  1) Floating-point noise: When a rounding radius consumes segment length
  //  within an epsilon distance of the midpoint of the first and second points,
  //  floating-point noise in the `lineTo` calculation can cause slight jitter.
  //  Although the distance is small, it can result in a large turning angle,
  //  causing a miter join to spike outwards. Round and bevel joins are
  //  guaranteed to stay within the stroke width, avoiding this. See:
  //  https://g-issues.chromium.org/issues/504697281
  //
  //  2) Bevel flattening: While a bevel join avoids the miter spike, consider
  //  a rounded polygon with a thick stroke and a corner segment smaller than
  //  `kMinRoundingThreshold`. If the stroke is thick enough, a bevel join
  //  will draw a visually noticeable flat edge. A round join prevents this.
  //
  // Since all points on the path produced by rounding the polygon must be
  // collinear with the original line segments, any movement (and resulting
  // joins) caused by floating-point noise is, by definition, visually
  // negligible.
  const BasicShape& outer_shape = border_shape.OuterShape();
  if (outer_shape.GetType() == BasicShape::kBasicShapePolygonType &&
      To<BasicShapePolygon>(outer_shape).HasRoundingRadius()) {
    stroke_data.SetLineJoin(kRoundJoin);
  }

  return stroke_data;
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
  StrokeData stroke_data =
      GetStrokeData(*style.BorderShape(), derived_stroke.thickness);
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
  Path inner_path = border_shape.InnerShape().GetPath(
      gfx::RectF(inner_reference_rect), style.EffectiveZoom(), 1);

  if (border_shape.HasSeparateInnerShape()) {
    return inner_path;
  }

  // For single-shape border-shape, the border is drawn as a stroke centered on
  // the path with the border width as the stroke thickness. The inner edge of
  // the border is therefore at path - border_width/2.
  DerivedStroke derived_stroke = RelevantSideForBorderShape(style);
  if (derived_stroke.thickness <= 0) {
    return inner_path;
  }

  StrokeData stroke_data =
      GetStrokeData(*style.BorderShape(), derived_stroke.thickness);
  Path stroke_path = inner_path.StrokePath(stroke_data, AffineTransform());
  SkOpBuilder builder;
  builder.add(inner_path.GetSkPath(), SkPathOp::kUnion_SkPathOp);
  builder.add(stroke_path.GetSkPath(), SkPathOp::kDifference_SkPathOp);
  SkPath result;
  return builder.resolve(&result) ? Path(result) : inner_path;
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

  StrokeData stroke_data =
      GetStrokeData(*style.BorderShape(), std::abs(offset) * 2);
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

// Shared implementation for Paint() and PaintBorderArea().
static void PaintBorderShape(GraphicsContext& context,
                             const ComputedStyle& style,
                             const StyleBorderShape& border_shape,
                             const PhysicalRect& outer_reference_rect,
                             const PhysicalRect& inner_reference_rect,
                             const Color& color,
                             float stroke_thickness) {
  const Path outer_path = OuterPathWithoutStroke(style, outer_reference_rect);

  const AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBorder));
  context.SetShouldAntialias(true);

  // When two <basic-shape> values are given, the border is rendered as the
  // shape between the two paths.
  if (border_shape.HasSeparateInnerShape()) {
    SkOpBuilder builder;
    builder.add(outer_path.GetSkPath(), SkPathOp::kUnion_SkPathOp);
    const Path inner_path =
        BorderShapePainter::InnerPath(style, inner_reference_rect);
    builder.add(inner_path.GetSkPath(), SkPathOp::kDifference_SkPathOp);
    SkPath result;
    builder.resolve(&result);
    Path fill_path(result);
    context.SetFillColor(color);
    context.FillPath(fill_path, auto_dark_mode);
    return;
  }

  // A stroke thickness of 0 renders a hairline path, but we want to render
  // nothing.
  if (!stroke_thickness) {
    return;
  }

  // When only a single <basic-shape> is given, the border is rendered as a
  // stroke with the relevant side’s computed border width as the stroke width.
  StrokeData stroke_data =
      GetStrokeData(*style.BorderShape(), stroke_thickness);
  context.SetStrokeColor(color);
  context.SetStroke(stroke_data);
  context.StrokePath(outer_path, auto_dark_mode);
}

bool BorderShapePainter::Paint(GraphicsContext& context,
                               const ComputedStyle& style,
                               const PhysicalRect& outer_reference_rect,
                               const PhysicalRect& inner_reference_rect) {
  const StyleBorderShape* border_shape = style.BorderShape();
  if (!border_shape) {
    return false;
  }

  DerivedStroke derived_stroke = RelevantSideForBorderShape(style);
  PaintBorderShape(context, style, *border_shape, outer_reference_rect,
                   inner_reference_rect, derived_stroke.color,
                   derived_stroke.thickness);
  return true;
}

// static
void BorderShapePainter::PaintBorderArea(
    GraphicsContext& context,
    const ComputedStyle& style,
    const PhysicalRect& outer_reference_rect,
    const PhysicalRect& inner_reference_rect) {
  const StyleBorderShape* border_shape = style.BorderShape();
  CHECK(border_shape);

  // Dark mode color inversion is harmless here because the result is used as
  // a DstIn mask where only the alpha channel matters, not the RGB values.
  float thickness = border_shape->HasSeparateInnerShape()
                        ? 0
                        : RelevantSideForBorderShape(style).thickness;
  PaintBorderShape(context, style, *border_shape, outer_reference_rect,
                   inner_reference_rect, Color::kBlack, thickness);
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
  // OuterPathWithOffset already starts from the expanded OuterPath, which
  // represents the outer edge of the border. Therefore, we only need to
  // add outline_offset and half of the outline_width.
  const float center_offset = static_cast<float>(outline_offset) +
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
    StrokeData stroke_data =
        GetStrokeData(*style.BorderShape(), static_cast<float>(outline_width));
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
      StrokeData stroke_data = GetStrokeData(*style.BorderShape(),
                                             static_cast<float>(outline_width));
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
    StrokeData outer_stroke_data =
        GetStrokeData(*style.BorderShape(), stroke_width);
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

  const gfx::RectF border_gfx = gfx::RectF(border_rect);

  // Border path visual bounds: where the border stroke/fill draws.
  const Path outer_path = OuterPath(style, outer_reference_rect);
  gfx::RectF visual_bounds = outer_path.BoundingRect();
  const Path inner_path = InnerPath(style, inner_reference_rect);
  visual_bounds.Union(inner_path.BoundingRect());

  PhysicalBoxStrut outsets = PhysicalBoxStrut::Enclosing(gfx::OutsetsF::TLBR(
      std::max(0.0f, border_gfx.y() - visual_bounds.y()),
      std::max(0.0f, border_gfx.x() - visual_bounds.x()),
      std::max(0.0f, visual_bounds.bottom() - border_gfx.bottom()),
      std::max(0.0f, visual_bounds.right() - border_gfx.right())));

  // Box-shadow visual bounds. BoxDecorationOutsets() uses (spread + sigma_3)
  // for shadows, but for border-shape the shadow path is built via:
  //   OuterPathWithOffset(style, outer_reference_rect, spread)
  // giving a total visual extent of (spread + sigma_3). Replicate the
  // exact path the painter builds so the overflow bounds match the pixels
  // actually drawn.
  if (const ShadowList* box_shadow = style.BoxShadow()) {
    for (const ShadowData& shadow : box_shadow->Shadows()) {
      if (shadow.Style() == ShadowStyle::kInset) {
        continue;
      }
      const float spread = shadow.Spread();
      // 3 * sigma is how Skia computes the box blur extent.
      // See ShadowData::RectOutsets().
      const float sigma_3 = std::ceil(3.0f * shadow.BlurAsSigma());

      const Path shadow_path =
          OuterPathWithOffset(style, outer_reference_rect, spread);

      // The draw looper blurs shadow_path by sigma_3, then offsets by (X, Y).
      gfx::RectF shadow_visual_rect = shadow_path.BoundingRect();
      shadow_visual_rect.Outset(sigma_3);
      shadow_visual_rect.Offset(shadow.X(), shadow.Y());

      outsets.Unite(PhysicalBoxStrut::Enclosing(gfx::OutsetsF::TLBR(
          std::max(0.0f, border_gfx.y() - shadow_visual_rect.y()),
          std::max(0.0f, border_gfx.x() - shadow_visual_rect.x()),
          std::max(0.0f, shadow_visual_rect.bottom() - border_gfx.bottom()),
          std::max(0.0f, shadow_visual_rect.right() - border_gfx.right()))));
    }
  }

  // Outline visual bounds.
  if (style.HasOutline() && !style.OutlineStyleIsAuto() &&
      style.OutlineWidth() > 0) {
    const float outline_extent = static_cast<float>(style.OutlineOffset()) +
                                 static_cast<float>(style.OutlineWidth());
    const Path outline_path =
        OuterPathWithOffset(style, outer_reference_rect, outline_extent);
    const gfx::RectF outline_visual_rect = outline_path.BoundingRect();

    outsets.Unite(PhysicalBoxStrut::Enclosing(gfx::OutsetsF::TLBR(
        std::max(0.0f, border_gfx.y() - outline_visual_rect.y()),
        std::max(0.0f, border_gfx.x() - outline_visual_rect.x()),
        std::max(0.0f, outline_visual_rect.bottom() - border_gfx.bottom()),
        std::max(0.0f, outline_visual_rect.right() - border_gfx.right()))));
  }

  return outsets;
}

}  // namespace blink
