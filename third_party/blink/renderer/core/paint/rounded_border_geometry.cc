// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "ui/gfx/geometry/rect_conversions.h"

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

}  // anonymous namespace

FloatRoundedRect RoundedBorderGeometry::RoundedBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect) {
  FloatRoundedRect rounded_rect((gfx::RectF(border_rect)));
  if (style.HasBorderRadius()) {
    rounded_rect.SetRadii(
        CalcRadiiFor(style, gfx::SizeF(border_rect.size), PhysicalBoxSides()));
    rounded_rect.ConstrainRadii();
  }
  return rounded_rect;
}

FloatRoundedRect RoundedBorderGeometry::PixelSnappedRoundedBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    PhysicalBoxSides sides_to_include) {
  FloatRoundedRect rounded_rect(ToPixelSnappedRect(border_rect));
  if (style.HasBorderRadius()) {
    rounded_rect.SetRadii(
        CalcRadiiFor(style, gfx::SizeF(border_rect.size), sides_to_include));
    rounded_rect.ConstrainRadii();
  }
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
        PixelSnappedRoundedBorder(style, border_rect, sides_to_include);
    pixel_snapped_rounded_border.Outset(gfx::OutsetsF(adjusted_outsets));
    rounded_rect.SetRadii(pixel_snapped_rounded_border.GetRadii());
  }
  return rounded_rect;
}

}  // namespace blink
