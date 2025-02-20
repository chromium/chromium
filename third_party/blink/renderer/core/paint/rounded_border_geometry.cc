// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

namespace {

FloatRoundedRect::Radii CalcRadiiFor(const ComputedStyle& style,
                                     gfx::SizeF size,
                                     PhysicalBoxSides sides_to_include) {
  return FloatRoundedRect::Radii(
      sides_to_include.top && sides_to_include.left &&
              !style.CornerTopLeftShape().IsDegenerate()
          ? SizeForLengthSize(style.BorderTopLeftRadius(), size)
          : gfx::SizeF(),
      sides_to_include.top && sides_to_include.right &&
              !style.CornerTopRightShape().IsDegenerate()
          ? SizeForLengthSize(style.BorderTopRightRadius(), size)
          : gfx::SizeF(),
      sides_to_include.bottom && sides_to_include.left &&
              !style.CornerBottomLeftShape().IsDegenerate()
          ? SizeForLengthSize(style.BorderBottomLeftRadius(), size)
          : gfx::SizeF(),
      sides_to_include.bottom && sides_to_include.right &&
              !style.CornerBottomRightShape().IsDegenerate()
          ? SizeForLengthSize(style.BorderBottomRightRadius(), size)
          : gfx::SizeF());
}

float EffectiveCurvature(Superellipse superellipse, const gfx::SizeF& radius) {
  return radius.IsEmpty() ? FloatRoundedRect::CornerCurvature::kRound
                          : superellipse.Exponent();
}

FloatRoundedRect::CornerCurvature CalcCurvatureFor(
    const ComputedStyle& style,
    const FloatRoundedRect::Radii& radii) {
  return FloatRoundedRect::CornerCurvature(
      EffectiveCurvature(style.CornerTopLeftShape(), radii.TopLeft()),
      EffectiveCurvature(style.CornerTopRightShape(), radii.TopRight()),
      EffectiveCurvature(style.CornerBottomRightShape(), radii.BottomRight()),
      EffectiveCurvature(style.CornerBottomLeftShape(), radii.BottomLeft()));
}

FloatRoundedRect PixelSnappedRoundedBorderInternal(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    PhysicalBoxSides sides_to_include) {
  FloatRoundedRect rounded_rect(ToPixelSnappedRect(border_rect));

  if (style.HasBorderRadius()) {
    rounded_rect.SetRadii(
        CalcRadiiFor(style, gfx::SizeF(border_rect.size), sides_to_include));
    rounded_rect.ConstrainRadii();
    rounded_rect.SetCornerCurvature(
        CalcCurvatureFor(style, rounded_rect.GetRadii()));
  }
  return rounded_rect;
}

}  // anonymous namespace

FloatRoundedRect RoundedBorderGeometry::RoundedBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect) {
  FloatRoundedRect rounded_rect((gfx::RectF(border_rect)));
  if (style.HasBorderRadius()) {
    rounded_rect.SetRadii(
        CalcRadiiFor(style, gfx::SizeF(border_rect.size), PhysicalBoxSides()));
    rounded_rect.ConstrainRadii();
    rounded_rect.SetCornerCurvature(
        CalcCurvatureFor(style, rounded_rect.GetRadii()));
  }
  return rounded_rect;
}

// Each corner is rendered independently and its rendering should remain within
// the corner bounds. With low curvature values (curvature<2),this creates an
// effect where lines/curves drawn at the
// border-width offset would seem thinner than the given border-width. To
// correct this, while maintaining the rule of drawing only within the corners,
// the outer corner is inset by some offset.
// Note that this doesn't account for elliptical corners, as those aren't
// corrected for ordinary rounded rects as well. See open spec issue:
// https://github.com/w3c/csswg-drafts/issues/11610
gfx::SizeF InsetOuterCornerSizeForCurvature(const gfx::SizeF& corner_size,
                                            float curvature,
                                            float horizontal_border_width,
                                            float vertical_border_width) {
  if (curvature >= 2) {
    return corner_size;
  }
  if (curvature <= 0.5) {
    return corner_size -
           gfx::SizeF(vertical_border_width, horizontal_border_width);
  }

  // The offset from the inner edge is sqrt(2 / curvature).
  // This would result in an offset of sqrt(2) for bevel, and and offset of 2
  // for scoop. We then subtract it by 1 to get the offset from the outer edge.
  float offset_from_outer_edge = std::sqrt(2. / curvature) - 1;
  return corner_size -
         gfx::SizeF(vertical_border_width * offset_from_outer_edge,
                    horizontal_border_width * offset_from_outer_edge);
}

FloatRoundedRect RoundedBorderGeometry::PixelSnappedRoundedBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    PhysicalBoxSides sides_to_include) {
  FloatRoundedRect rounded_rect =
      PixelSnappedRoundedBorderInternal(style, border_rect, sides_to_include);
  const auto& curvature = rounded_rect.GetCornerCurvature();

  FloatRoundedRect::Radii radii = rounded_rect.GetRadii();
  if (radii.IsZero() || curvature.IsRound() || sides_to_include.IsEmpty()) {
    return rounded_rect;
  }
  if ((sides_to_include.left || sides_to_include.top) &&
      !radii.TopLeft().IsEmpty()) {
    radii.SetTopLeft(InsetOuterCornerSizeForCurvature(
        radii.TopLeft(), curvature.TopLeft(),
        sides_to_include.left ? style.BorderLeftWidth() : 0,
        sides_to_include.top ? style.BorderTopWidth() : 0));
  }
  if ((sides_to_include.right || sides_to_include.top) &&
      !radii.TopRight().IsEmpty()) {
    radii.SetTopRight(InsetOuterCornerSizeForCurvature(
        radii.TopRight(), curvature.TopRight(),
        sides_to_include.right ? style.BorderRightWidth() : 0,
        sides_to_include.top ? style.BorderTopWidth() : 0));
  }
  if ((sides_to_include.right || sides_to_include.bottom) &&
      !radii.BottomRight().IsEmpty()) {
    radii.SetBottomRight(InsetOuterCornerSizeForCurvature(
        radii.BottomRight(), curvature.BottomRight(),
        sides_to_include.right ? style.BorderRightWidth() : 0,
        sides_to_include.bottom ? style.BorderBottomWidth() : 0));
  }
  if ((sides_to_include.left || sides_to_include.bottom) &&
      !radii.BottomLeft().IsEmpty()) {
    radii.SetBottomLeft(InsetOuterCornerSizeForCurvature(
        radii.BottomLeft(), curvature.BottomLeft(),
        sides_to_include.left ? style.BorderLeftWidth() : 0,
        sides_to_include.bottom ? style.BorderBottomWidth() : 0));
  }
  rounded_rect.SetRadii(radii);
  return rounded_rect;
}

FloatRoundedRect RoundedBorderGeometry::RoundedInnerBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect) {
  FloatRoundedRect rounded_border = RoundedBorder(style, border_rect);
  rounded_border.Inset(gfx::InsetsF()
                           .set_top(style.BorderTopWidth())
                           .set_right(style.BorderRightWidth())
                           .set_bottom(style.BorderBottomWidth())
                           .set_left(style.BorderLeftWidth()));
  return rounded_border;
}

FloatRoundedRect RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    PhysicalBoxSides sides_to_include) {
  return PixelSnappedRoundedBorderWithOutsets(
      style, border_rect,
      PhysicalBoxStrut(-style.BorderTopWidth(), -style.BorderRightWidth(),
                       -style.BorderBottomWidth(), -style.BorderLeftWidth()),
      sides_to_include);
}

FloatRoundedRect RoundedBorderGeometry::PixelSnappedRoundedBorderWithOutsets(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    const PhysicalBoxStrut& outsets,
    PhysicalBoxSides sides_to_include) {
  PhysicalBoxStrut adjusted_outsets(
      sides_to_include.top ? outsets.top : LayoutUnit(),
      sides_to_include.right ? outsets.right : LayoutUnit(),
      sides_to_include.bottom ? outsets.bottom : LayoutUnit(),
      sides_to_include.left ? outsets.left : LayoutUnit());
  PhysicalRect rect_with_outsets = border_rect;
  rect_with_outsets.Expand(adjusted_outsets);
  rect_with_outsets.size.ClampNegativeToZero();

  // The standard ToPixelSnappedRect(const PhysicalRect&) will not
  // let small sizes snap to zero, but that has the side effect here of
  // preventing an inner border for a very thin element from snapping to
  // zero size as occurs when a unit width border is applied to a sub-pixel
  // sized element. So round without forcing non-near-zero sizes to one.
  FloatRoundedRect rounded_rect(gfx::Rect(
      ToRoundedPoint(rect_with_outsets.offset),
      gfx::Size(SnapSizeToPixelAllowingZero(rect_with_outsets.Width(),
                                            rect_with_outsets.X()),
                SnapSizeToPixelAllowingZero(rect_with_outsets.Height(),
                                            rect_with_outsets.Y()))));

  if (style.HasBorderRadius()) {
    FloatRoundedRect pixel_snapped_rounded_border =
        PixelSnappedRoundedBorderInternal(style, border_rect, sides_to_include);
    pixel_snapped_rounded_border.Outset(gfx::OutsetsF(adjusted_outsets));
    rounded_rect.SetRadii(pixel_snapped_rounded_border.GetRadii());
    rounded_rect.SetCornerCurvature(
        pixel_snapped_rounded_border.GetCornerCurvature());
  }
  return rounded_rect;
}

}  // namespace blink
