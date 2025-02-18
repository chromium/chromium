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
      sides_to_include.top && sides_to_include.left
          ? SizeForLengthSize(style.BorderTopLeftRadius(), size)
          : gfx::SizeF(),
      sides_to_include.top && sides_to_include.right
          ? SizeForLengthSize(style.BorderTopRightRadius(), size)
          : gfx::SizeF(),
      sides_to_include.bottom && sides_to_include.left
          ? SizeForLengthSize(style.BorderBottomLeftRadius(), size)
          : gfx::SizeF(),
      sides_to_include.bottom && sides_to_include.right
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
// the corner bounds. With low curvature values (curvature<2), as well as with
// elliptical corners, this creates an effect where lines/curves drawn at the
// border-width offset would seem thinner than the given border-width. To
// correct this, while maintaining the rule of drawing only within the corners,
// the outer corner is inset by some offset, computed by the slope and different
// border-widths. The resulting smaller outer corner size creates the effect of
// having a larger border, which matches the specified border-width. Note that
// it's currently implemented for bevel (straight line), and for other
// curvatures it would have to use something equivalent to the tangent (the
// nearest control point). Note that some of this is an open spec issue:
// https://github.com/w3c/csswg-drafts/issues/11610
gfx::SizeF InsetOuterCornerSizeForCurvature(const gfx::SizeF& corner_size,
                                            float curvature,
                                            float horizontal_border_width,
                                            float vertical_border_width) {
  // TODO(noamr) implement for other curvatures.
  if (curvature != FloatRoundedRect::CornerCurvature::kBevel) {
    return corner_size;
  }

  CHECK(!corner_size.IsEmpty());

  // The slope would be different for curvatures other than bevel. The slope
  // would be based on the control point.
  float slope = corner_size.height() / corner_size.width();

  // compute two vector that would be perpendicular to the straight line,
  // with the border widths as their sizes.
  gfx::Vector2dF perpendicular_vector(-corner_size.height(),
                                      corner_size.width());
  float magnitude = perpendicular_vector.Length();
  gfx::Vector2dF horizontal_side_translation = gfx::ScaleVector2d(
      perpendicular_vector, horizontal_border_width / magnitude,
      horizontal_border_width / magnitude);
  gfx::Vector2dF vertical_side_translation = gfx::ScaleVector2d(
      perpendicular_vector, vertical_border_width / magnitude,
      vertical_border_width / magnitude);

  // Given the translations, compute the intercepts (b) of the wanted equation.
  float vertical_side_intercept =
      vertical_side_translation.y() - slope * vertical_side_translation.x();
  float horizontal_side_intercept =
      horizontal_side_translation.y() - slope * horizontal_side_translation.x();

  // The found intercepts are relative to the *inner* rect, as the requisited
  // distance is relative to the inner rect. So correct using the border width,
  // as the offset applies to the outer rect.
  return gfx::SizeF(
      corner_size.width() + vertical_border_width - vertical_side_intercept,
      corner_size.height() + horizontal_border_width -
          horizontal_side_intercept);
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
