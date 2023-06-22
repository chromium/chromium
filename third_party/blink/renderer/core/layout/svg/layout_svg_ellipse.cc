/*
 * Copyright (C) 2012 Google, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/svg/layout_svg_ellipse.h"

#include <cmath>
#include "third_party/blink/renderer/core/svg/svg_circle_element.h"
#include "third_party/blink/renderer/core/svg/svg_ellipse_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"

namespace blink {

LayoutSVGEllipse::LayoutSVGEllipse(SVGGeometryElement* node)
    : LayoutSVGShape(node, kSimple) {}

LayoutSVGEllipse::~LayoutSVGEllipse() = default;

void LayoutSVGEllipse::UpdateShapeFromElement() {
  NOT_DESTROYED();

  decorated_bounding_box_ = gfx::RectF();
  use_path_fallback_ = false;

  CalculateRadiiAndCenter();
  DCHECK_GE(radius_x_, 0);
  DCHECK_GE(radius_y_, 0);

  fill_bounding_box_.SetRect(center_.x() - radius_x_, center_.y() - radius_y_,
                             radius_x_ * 2, radius_y_ * 2);

  if (radius_x_ && radius_y_) {
    // Fall back to LayoutSVGShape and path-based hit detection if the ellipse
    // has a non-scaling or discontinuous stroke.
    // However, only use LayoutSVGShape bounding-box calculations for the
    // non-scaling stroke case, since the computation below should be accurate
    // for the other cases.
    if (HasNonScalingStroke()) {
      LayoutSVGShape::UpdateShapeFromElement();
      use_path_fallback_ = true;
      return;
    }
    if (!HasContinuousStroke()) {
      CreatePath();
      use_path_fallback_ = true;
    }
  }

  if (!use_path_fallback_)
    ClearPath();

  decorated_bounding_box_ = CalculateStrokeBoundingBox();
}

void LayoutSVGEllipse::CalculateRadiiAndCenter() {
  NOT_DESTROYED();
  DCHECK(GetElement());
  SVGLengthContext length_context(GetElement());
  const ComputedStyle& style = StyleRef();
  center_ = gfx::PointAtOffsetFromOrigin(
      length_context.ResolveLengthPair(style.Cx(), style.Cy(), style));

  if (IsA<SVGCircleElement>(*GetElement())) {
    radius_x_ = radius_y_ =
        length_context.ValueForLength(style.R(), style, SVGLengthMode::kOther);
  } else {
    gfx::Vector2dF radii =
        length_context.ResolveLengthPair(style.Rx(), style.Ry(), style);
    radius_x_ = radii.x();
    radius_y_ = radii.y();
    if (style.Rx().IsAuto())
      radius_x_ = radius_y_;
    else if (style.Ry().IsAuto())
      radius_y_ = radius_x_;
  }

  // Spec: "A negative value is an error. A value of zero disables rendering of
  // the element."
  radius_x_ = std::max(radius_x_, 0.f);
  radius_y_ = std::max(radius_y_, 0.f);
}

bool LayoutSVGEllipse::ShapeDependentStrokeContains(
    const HitTestLocation& location) {
  NOT_DESTROYED();
  DCHECK_GE(radius_x_, 0);
  DCHECK_GE(radius_y_, 0);
  if (!radius_x_ || !radius_y_)
    return false;

  // The optimized check below for circles does not support non-circular and
  // the cases that we set use_path_fallback_ in UpdateShapeFromElement().
  if (use_path_fallback_ || radius_x_ != radius_y_)
    return LayoutSVGShape::ShapeDependentStrokeContains(location);

  const gfx::PointF& point = location.TransformedPoint();
  const gfx::Vector2dF center_offset = center_ - point;
  const float half_stroke_width = StrokeWidth() / 2;
  return std::abs(center_offset.Length() - radius_x_) <= half_stroke_width;
}

bool LayoutSVGEllipse::ShapeDependentFillContains(
    const HitTestLocation& location,
    const WindRule fill_rule) const {
  NOT_DESTROYED();
  DCHECK_GE(radius_x_, 0);
  DCHECK_GE(radius_y_, 0);
  if (!radius_x_ || !radius_y_)
    return false;

  const gfx::PointF& point = location.TransformedPoint();
  const gfx::PointF center =
      gfx::PointF(center_.x() - point.x(), center_.y() - point.y());

  // This works by checking if the point satisfies the ellipse equation.
  // (x/rX)^2 + (y/rY)^2 <= 1
  const float xr_x = center.x() / radius_x_;
  const float yr_y = center.y() / radius_y_;
  return xr_x * xr_x + yr_y * yr_y <= 1.0;
}

bool LayoutSVGEllipse::HasContinuousStroke() const {
  NOT_DESTROYED();
  return !StyleRef().HasDashArray();
}

}  // namespace blink
