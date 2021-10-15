/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/shapes/rectangle_shape.h"

#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

static inline float EllipseXIntercept(float y, float rx, float ry) {
  DCHECK_GT(ry, 0);
  return rx * sqrt(1 - (y * y) / (ry * ry));
}

FloatRect RectangleShape::ShapeMarginBounds() const {
  DCHECK_GE(ShapeMargin(), 0);
  if (!ShapeMargin())
    return bounds_;

  float bounds_x = X() - ShapeMargin();
  float bounds_y = Y() - ShapeMargin();
  float bounds_width = Width() + ShapeMargin() * 2;
  float bounds_height = Height() + ShapeMargin() * 2;
  return FloatRect(bounds_x, bounds_y, bounds_width, bounds_height);
}

LineSegment RectangleShape::GetExcludedInterval(
    LayoutUnit logical_top,
    LayoutUnit logical_height) const {
  const FloatRect& bounds = ShapeMarginBounds();
  if (bounds.IsEmpty())
    return LineSegment();

  float y1 = logical_top.ToFloat();
  float y2 = (logical_top + logical_height).ToFloat();

  if (y2 < bounds.y() || y1 >= bounds.bottom())
    return LineSegment();

  float x1 = bounds.x();
  float x2 = bounds.right();

  float margin_radius_x = Rx() + ShapeMargin();
  float margin_radius_y = Ry() + ShapeMargin();

  if (margin_radius_y > 0) {
    if (y2 < bounds.y() + margin_radius_y) {
      float yi = y2 - bounds.y() - margin_radius_y;
      float xi = EllipseXIntercept(yi, margin_radius_x, margin_radius_y);
      x1 = bounds.x() + margin_radius_x - xi;
      x2 = bounds.right() - margin_radius_x + xi;
    } else if (y1 > bounds.bottom() - margin_radius_y) {
      float yi = y1 - (bounds.bottom() - margin_radius_y);
      float xi = EllipseXIntercept(yi, margin_radius_x, margin_radius_y);
      x1 = bounds.x() + margin_radius_x - xi;
      x2 = bounds.right() - margin_radius_x + xi;
    }
  }

  return LineSegment(x1, x2);
}

void RectangleShape::BuildDisplayPaths(DisplayPaths& paths) const {
  paths.shape.AddRoundedRect(bounds_, radii_);
  if (ShapeMargin()) {
    paths.margin_shape.AddRoundedRect(
        ShapeMarginBounds(), FloatSize(radii_.width() + ShapeMargin(),
                                       radii_.height() + ShapeMargin()));
  }
}

}  // namespace blink
