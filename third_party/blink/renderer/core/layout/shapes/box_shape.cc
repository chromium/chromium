/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/shapes/box_shape.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

LogicalRect BoxShape::ShapeMarginLogicalBoundingBox() const {
  gfx::RectF margin_bounds = bounds_.Rect();
  if (ShapeMargin() > 0)
    margin_bounds.Outset(ShapeMargin());
  return LogicalRect::EnclosingRect(margin_bounds);
}

FloatRoundedRect BoxShape::ShapeMarginBounds() const {
  FloatRoundedRect margin_bounds = bounds_;
  if (ShapeMargin() > 0)
    margin_bounds.OutsetForShapeMargin(ShapeMargin());
  return margin_bounds;
}

LineSegment BoxShape::GetExcludedInterval(LayoutUnit logical_top,
                                          LayoutUnit logical_height) const {
  const FloatRoundedRect& margin_bounds = ShapeMarginBounds();
  if (margin_bounds.IsEmpty() ||
      !LineOverlapsShapeMarginBounds(logical_top, logical_height))
    return LineSegment();

  float y1 = logical_top.ToFloat();
  float y2 = (logical_top + logical_height).ToFloat();
  const gfx::RectF& rect = margin_bounds.Rect();

  if (!margin_bounds.IsRounded())
    return LineSegment(margin_bounds.Rect().x(), margin_bounds.Rect().right());

  float top_corner_max_y =
      std::max<float>(margin_bounds.TopLeftCorner().bottom(),
                      margin_bounds.TopRightCorner().bottom());
  float bottom_corner_min_y =
      std::min<float>(margin_bounds.BottomLeftCorner().y(),
                      margin_bounds.BottomRightCorner().y());

  if (top_corner_max_y <= bottom_corner_min_y && y1 <= top_corner_max_y &&
      y2 >= bottom_corner_min_y)
    return LineSegment(rect.x(), rect.right());

  float x1 = rect.right();
  float x2 = rect.x();
  float min_x_intercept;
  float max_x_intercept;

  if (y1 <= margin_bounds.TopLeftCorner().bottom() &&
      y2 >= margin_bounds.BottomLeftCorner().y())
    x1 = rect.x();

  if (y1 <= margin_bounds.TopRightCorner().bottom() &&
      y2 >= margin_bounds.BottomRightCorner().y())
    x2 = rect.right();

  if (margin_bounds.XInterceptsAtY(y1, min_x_intercept, max_x_intercept)) {
    x1 = std::min<float>(x1, min_x_intercept);
    x2 = std::max<float>(x2, max_x_intercept);
  }

  if (margin_bounds.XInterceptsAtY(y2, min_x_intercept, max_x_intercept)) {
    x1 = std::min<float>(x1, min_x_intercept);
    x2 = std::max<float>(x2, max_x_intercept);
  }

  DCHECK_GE(x2, x1);
  return LineSegment(x1, x2);
}

void BoxShape::BuildDisplayPaths(DisplayPaths& paths) const {
  paths.shape.AddRoundedRect(bounds_);
  if (ShapeMargin())
    paths.margin_shape.AddRoundedRect(ShapeMarginBounds());
}

FloatRoundedRect BoxShape::ToLogical(const FloatRoundedRect& rect,
                                     const WritingModeConverter& converter) {
  if (converter.GetWritingMode() == WritingMode::kHorizontalTb) {
    return rect;
  }

  gfx::RectF logical_rect = converter.ToLogical(rect.Rect());
  gfx::SizeF top_left = rect.GetRadii().TopLeft();
  top_left.Transpose();
  gfx::SizeF top_right = rect.GetRadii().TopRight();
  top_right.Transpose();
  gfx::SizeF bottom_left = rect.GetRadii().BottomLeft();
  bottom_left.Transpose();
  gfx::SizeF bottom_right = rect.GetRadii().BottomRight();
  bottom_right.Transpose();

  switch (converter.GetWritingMode()) {
    case WritingMode::kHorizontalTb:
      NOTREACHED_NORETURN();
    case WritingMode::kVerticalLr:
      return FloatRoundedRect(logical_rect, top_left, bottom_left, top_right,
                              bottom_right);
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      return FloatRoundedRect(logical_rect, top_right, bottom_right, top_left,
                              bottom_left);
    case WritingMode::kSidewaysLr:
      return FloatRoundedRect(logical_rect, bottom_left, top_left, bottom_right,
                              top_right);
  }
}

}  // namespace blink
