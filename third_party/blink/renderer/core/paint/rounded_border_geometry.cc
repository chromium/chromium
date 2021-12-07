// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

FloatRoundedRect::Radii CalcRadiiFor(const ComputedStyle& style,
                                     gfx::SizeF size) {
  return FloatRoundedRect::Radii(
      SizeForLengthSize(style.BorderTopLeftRadius(), size),
      SizeForLengthSize(style.BorderTopRightRadius(), size),
      SizeForLengthSize(style.BorderBottomLeftRadius(), size),
      SizeForLengthSize(style.BorderBottomRightRadius(), size));
}

void ExcludeSides(PhysicalBoxSides sides_to_include,
                  FloatRoundedRect::Radii* radii) {
  if (!sides_to_include.left) {
    radii->SetTopLeft(gfx::SizeF(0, radii->TopLeft().height()));
    radii->SetBottomLeft(gfx::SizeF(0, radii->BottomLeft().height()));
  }
  if (!sides_to_include.right) {
    radii->SetTopRight(gfx::SizeF(0, radii->TopRight().height()));
    radii->SetBottomRight(gfx::SizeF(0, radii->BottomRight().height()));
  }
  if (!sides_to_include.top) {
    radii->SetTopLeft(gfx::SizeF(radii->TopLeft().width(), 0));
    radii->SetTopRight(gfx::SizeF(radii->TopRight().width(), 0));
  }
  if (!sides_to_include.bottom) {
    radii->SetBottomLeft(gfx::SizeF(radii->BottomLeft().width(), 0));
    radii->SetBottomRight(gfx::SizeF(radii->BottomRight().width(), 0));
  }
}

}  // anonymous namespace

FloatRoundedRect RoundedBorderGeometry::RoundedBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect) {
  FloatRoundedRect rounded_rect((gfx::RectF(border_rect)));
  if (style.HasBorderRadius()) {
    FloatRoundedRect::Radii radii =
        CalcRadiiFor(style, gfx::SizeF(border_rect.size));
    rounded_rect.SetRadii(radii);
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
    FloatRoundedRect::Radii radii =
        CalcRadiiFor(style, gfx::SizeF(border_rect.size));
    ExcludeSides(sides_to_include, &radii);
    rounded_rect.SetRadii(radii);
    rounded_rect.ConstrainRadii();
  }
  return rounded_rect;
}

FloatRoundedRect RoundedBorderGeometry::RoundedInnerBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect) {
  int left_width = style.BorderLeftWidth().ToInt();
  int right_width = style.BorderRightWidth().ToInt();
  int top_width = style.BorderTopWidth().ToInt();
  int bottom_width = style.BorderBottomWidth().ToInt();

  LayoutRectOutsets insets(-top_width, -right_width, -bottom_width,
                           -left_width);

  PhysicalRect inner_rect(border_rect);
  inner_rect.Expand(insets);
  inner_rect.size.ClampNegativeToZero();

  FloatRoundedRect float_inner_rect((gfx::RectF(inner_rect)));

  if (style.HasBorderRadius()) {
    FloatRoundedRect::Radii radii =
        RoundedBorder(style, border_rect).GetRadii();
    // Insets use negative values.
    radii.Shrink(-insets.Top().ToFloat(), -insets.Bottom().ToFloat(),
                 -insets.Left().ToFloat(), -insets.Right().ToFloat());
    float_inner_rect.SetRadii(radii);
  }
  return float_inner_rect;
}

FloatRoundedRect RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    PhysicalBoxSides sides_to_include) {
  int left_width = sides_to_include.left ? style.BorderLeftWidth().Floor() : 0;
  int right_width =
      sides_to_include.right ? style.BorderRightWidth().Floor() : 0;
  int top_width = sides_to_include.top ? style.BorderTopWidth().Floor() : 0;
  int bottom_width =
      sides_to_include.bottom ? style.BorderBottomWidth().Floor() : 0;

  return PixelSnappedRoundedBorderWithOutsets(
      style, border_rect,
      LayoutRectOutsets(-top_width, -right_width, -bottom_width, -left_width),
      sides_to_include);
}

FloatRoundedRect RoundedBorderGeometry::PixelSnappedRoundedBorderWithOutsets(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    const LayoutRectOutsets& outsets,
    PhysicalBoxSides sides_to_include) {
  PhysicalRect rect_with_outsets = border_rect;
  rect_with_outsets.Expand(outsets);
  rect_with_outsets.size.ClampNegativeToZero();

  // The standard LayoutRect::ToPixelSnappedRect() method will not
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
    FloatRoundedRect::Radii radii =
        PixelSnappedRoundedBorder(style, border_rect, sides_to_include)
            .GetRadii();
    if (outsets.Top() <= 0 && outsets.Bottom() <= 0 && outsets.Left() <= 0 &&
        outsets.Right() <= 0) {
      radii.Shrink(-outsets.Top().ToFloat(), -outsets.Bottom().ToFloat(),
                   -outsets.Left().ToFloat(), -outsets.Right().ToFloat());
    } else {
      // radii.Expand() will DCHECK if all values are >= 0.
      radii.Expand(outsets.Top().ToFloat(), outsets.Bottom().ToFloat(),
                   outsets.Left().ToFloat(), outsets.Right().ToFloat());
    }
    ExcludeSides(sides_to_include, &radii);
    rounded_rect.SetRadii(radii);
  }
  return rounded_rect;
}

}  // namespace blink
