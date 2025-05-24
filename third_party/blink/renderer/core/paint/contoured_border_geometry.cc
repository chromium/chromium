// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/contoured_border_geometry.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"
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
  return radius.IsEmpty() ? ContouredRect::CornerCurvature::kRound
                          : superellipse.Exponent();
}

ContouredRect::CornerCurvature CalcCurvatureFor(
    const ComputedStyle& style,
    const FloatRoundedRect::Radii& radii) {
  return ContouredRect::CornerCurvature(
      EffectiveCurvature(style.CornerTopLeftShape(), radii.TopLeft()),
      EffectiveCurvature(style.CornerTopRightShape(), radii.TopRight()),
      EffectiveCurvature(style.CornerBottomRightShape(), radii.BottomRight()),
      EffectiveCurvature(style.CornerBottomLeftShape(), radii.BottomLeft()));
}

ContouredRect PixelSnappedContouredBorderInternal(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    PhysicalBoxSides sides_to_include) {
  FloatRoundedRect rounded_rect(ToPixelSnappedRect(border_rect));
  if (style.HasBorderRadius()) {
    rounded_rect.SetRadii(
        CalcRadiiFor(style, gfx::SizeF(border_rect.size), sides_to_include));
    rounded_rect.ConstrainRadii();
  }

  ContouredRect contoured_rect(rounded_rect);
  if (rounded_rect.IsRounded()) {
    contoured_rect.SetCornerCurvature(
        CalcCurvatureFor(style, rounded_rect.GetRadii()));
    if (!contoured_rect.HasRoundCurvature()) {
      contoured_rect.SetOriginRect(rounded_rect);
    }
  }
  return contoured_rect;
}

}  // anonymous namespace

ContouredRect ContouredBorderGeometry::ContouredBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect) {
  FloatRoundedRect rounded_rect((gfx::RectF(border_rect)));
  if (style.HasBorderRadius()) {
    rounded_rect.SetRadii(
        CalcRadiiFor(style, gfx::SizeF(border_rect.size), PhysicalBoxSides()));
    rounded_rect.ConstrainRadii();
  }
  ContouredRect contoured_rect(rounded_rect);
  if (rounded_rect.IsRounded()) {
    contoured_rect.SetCornerCurvature(
        CalcCurvatureFor(style, rounded_rect.GetRadii()));
    if (!contoured_rect.HasRoundCurvature()) {
      contoured_rect.SetOriginRect(rounded_rect);
    }
  }
  return contoured_rect;
}


ContouredRect ContouredBorderGeometry::PixelSnappedContouredBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    PhysicalBoxSides sides_to_include) {
  return PixelSnappedContouredBorderInternal(style, border_rect,
                                             sides_to_include);
}

ContouredRect ContouredBorderGeometry::ContouredInnerBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect) {
  ContouredRect rounded_border = ContouredBorder(style, border_rect);
  rounded_border.Inset(gfx::InsetsF()
                           .set_top(style.BorderTopWidth())
                           .set_right(style.BorderRightWidth())
                           .set_bottom(style.BorderBottomWidth())
                           .set_left(style.BorderLeftWidth()));
  return rounded_border;
}

ContouredRect ContouredBorderGeometry::PixelSnappedContouredInnerBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    PhysicalBoxSides sides_to_include) {
  return PixelSnappedContouredBorderWithOutsets(
      style, border_rect,
      PhysicalBoxStrut(-style.BorderTopWidth(), -style.BorderRightWidth(),
                       -style.BorderBottomWidth(), -style.BorderLeftWidth()),
      sides_to_include);
}

ContouredRect ContouredBorderGeometry::PixelSnappedContouredBorderWithOutsets(
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
  ContouredRect contoured_rect(FloatRoundedRect(gfx::Rect(
      ToRoundedPoint(rect_with_outsets.offset),
      gfx::Size(SnapSizeToPixelAllowingZero(rect_with_outsets.Width(),
                                            rect_with_outsets.X()),
                SnapSizeToPixelAllowingZero(rect_with_outsets.Height(),
                                            rect_with_outsets.Y())))));

  if (style.HasBorderRadius()) {
    ContouredRect pixel_snapped_rounded_border =
        PixelSnappedContouredBorderInternal(style, border_rect,
                                            sides_to_include);
    pixel_snapped_rounded_border.Outset(gfx::OutsetsF(adjusted_outsets));
    contoured_rect.SetRadii(pixel_snapped_rounded_border.GetRadii());
    contoured_rect.SetCornerCurvature(
        pixel_snapped_rounded_border.GetCornerCurvature());
    contoured_rect.SetOriginRect(pixel_snapped_rounded_border.GetOriginRect());
  }
  return contoured_rect;
}

}  // namespace blink
