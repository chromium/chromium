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

namespace blink {

LayoutSVGEllipse::LayoutSVGEllipse(SVGGeometryElement* node)
    : LayoutSVGShape(node, kSimple), use_path_fallback_(false) {}

LayoutSVGEllipse::~LayoutSVGEllipse() = default;

void LayoutSVGEllipse::UpdateShapeFromElement() {
  // Before creating a new object we need to clear the cached bounding box
  // to avoid using garbage.
  fill_bounding_box_ = FloatRect();
  stroke_bounding_box_ = FloatRect();
  center_ = FloatPoint();
  radii_ = FloatSize();
  use_path_fallback_ = false;

  CalculateRadiiAndCenter();

  // Spec: "A negative value is an error. A value of zero disables rendering of
  // the element."
  if (radii_.Width() < 0 || radii_.Height() < 0)
    return;

  if (!radii_.IsEmpty()) {
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

  fill_bounding_box_ = FloatRect(center_ - radii_, radii_.ScaledBy(2));
  stroke_bounding_box_ = CalculateStrokeBoundingBox();
}

void LayoutSVGEllipse::CalculateRadiiAndCenter() {
  DCHECK(GetElement());
  SVGLengthContext length_context(GetElement());
  const ComputedStyle& style = StyleRef();
  const SVGComputedStyle& svg_style = style.SvgStyle();
  center_ =
      length_context.ResolveLengthPair(svg_style.Cx(), svg_style.Cy(), style);

  if (IsSVGCircleElement(*GetElement())) {
    float radius = length_context.ValueForLength(svg_style.R(), style,
                                                 SVGLengthMode::kOther);
    radii_ = FloatSize(radius, radius);
  } else {
    radii_ = ToFloatSize(length_context.ResolveLengthPair(
        svg_style.Rx(), svg_style.Ry(), style));
    if (svg_style.Rx().IsAuto())
      radii_.SetWidth(radii_.Height());
    else if (svg_style.Ry().IsAuto())
      radii_.SetHeight(radii_.Width());
  }
}

bool LayoutSVGEllipse::ShapeDependentStrokeContains(
    const HitTestLocation& location) {
  if (radii_.Width() < 0 || radii_.Height() < 0)
    return false;

  // The optimized check below for circles does not support non-circular and
  // the cases that we set use_path_fallback_ in UpdateShapeFromElement().
  if (use_path_fallback_ || radii_.Width() != radii_.Height())
    return LayoutSVGShape::ShapeDependentStrokeContains(location);

  const FloatPoint& point = location.TransformedPoint();
  const FloatPoint center =
      FloatPoint(center_.X() - point.X(), center_.Y() - point.Y());
  const float half_stroke_width = StrokeWidth() / 2;
  const float r = radii_.Width();
  return std::abs(center.length() - r) <= half_stroke_width;
}

bool LayoutSVGEllipse::ShapeDependentFillContains(
    const HitTestLocation& location,
    const WindRule fill_rule) const {
  const FloatPoint& point = location.TransformedPoint();
  const FloatPoint center =
      FloatPoint(center_.X() - point.X(), center_.Y() - point.Y());

  // This works by checking if the point satisfies the ellipse equation.
  // (x/rX)^2 + (y/rY)^2 <= 1
  const float xr_x = center.X() / radii_.Width();
  const float yr_y = center.Y() / radii_.Height();
  return xr_x * xr_x + yr_y * yr_y <= 1.0;
}

bool LayoutSVGEllipse::HasContinuousStroke() const {
  const SVGComputedStyle& svg_style = StyleRef().SvgStyle();
  return svg_style.StrokeDashArray()->data.IsEmpty();
}

}  // namespace blink
