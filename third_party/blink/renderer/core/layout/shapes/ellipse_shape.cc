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

#include "third_party/blink/renderer/core/layout/shapes/ellipse_shape.h"

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

static inline float EllipseXIntercept(float y, float rx, float ry) {
  DCHECK_GT(ry, 0);
  return rx * sqrt(1 - (y * y) / (ry * ry));
}

std::pair<float, float> EllipseShape::InlineAndBlockRadiiIncludingMargin()
    const {
  float margin_radius_x = radius_x_ + ShapeMargin();
  float margin_radius_y = radius_y_ + ShapeMargin();
  if (!RuntimeEnabledFeatures::ShapeOutsideWritingModeFixEnabled() ||
      IsHorizontalWritingMode(writing_mode_)) {
    return {margin_radius_x, margin_radius_y};
  }
  return {margin_radius_y, margin_radius_x};
}

LogicalRect EllipseShape::ShapeMarginLogicalBoundingBox() const {
  DCHECK_GE(ShapeMargin(), 0);
  auto [margin_radius_x, margin_radius_y] =
      InlineAndBlockRadiiIncludingMargin();
  return LogicalRect(LayoutUnit(center_.x() - margin_radius_x),
                     LayoutUnit(center_.y() - margin_radius_y),
                     LayoutUnit(margin_radius_x * 2),
                     LayoutUnit(margin_radius_y * 2));
}

LineSegment EllipseShape::GetExcludedInterval(LayoutUnit logical_top,
                                              LayoutUnit logical_height) const {
  auto [margin_radius_x, margin_radius_y] =
      InlineAndBlockRadiiIncludingMargin();
  if (!margin_radius_x || !margin_radius_y)
    return LineSegment();

  float y1 = logical_top.ToFloat();
  float y2 = (logical_top + logical_height).ToFloat();

  float top = center_.y() - margin_radius_y;
  float bottom = center_.y() + margin_radius_y;
  // The y interval doesn't intersect with the ellipse.
  if (y2 < top || y1 >= bottom)
    return LineSegment();

  // Assume the y interval covers the vertical center of the ellipse.
  float x_intercept = margin_radius_x;
  if (y1 > center_.y() || y2 < center_.y()) {
    // Recalculate x_intercept if the y interval only intersects the upper half
    // or the lower half of the ellipse.
    float y_intercept = y1 > center_.y() ? y1 - center_.y() : y2 - center_.y();
    x_intercept =
        EllipseXIntercept(y_intercept, margin_radius_x, margin_radius_y);
  }
  return LineSegment(center_.x() - x_intercept, center_.x() + x_intercept);
}

void EllipseShape::BuildDisplayPaths(DisplayPaths& paths) const {
  paths.shape.AddEllipse(center_, radius_x_, radius_y_);
  if (ShapeMargin()) {
    paths.margin_shape.AddEllipse(center_, radius_x_ + ShapeMargin(),
                                  radius_y_ + ShapeMargin());
  }
}

}  // namespace blink
