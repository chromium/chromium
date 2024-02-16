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

#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/svg/svg_circle_element.h"
#include "third_party/blink/renderer/core/svg/svg_ellipse_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"

namespace blink {

namespace {

bool GeometryPropertiesChanged(const ComputedStyle& old_style,
                               const ComputedStyle& new_style) {
  return old_style.Rx() != new_style.Rx() || old_style.Ry() != new_style.Ry() ||
         old_style.Cx() != new_style.Cx() || old_style.Cy() != new_style.Cy() ||
         old_style.R() != new_style.R();
}

}  // namespace

LayoutSVGEllipse::LayoutSVGEllipse(SVGGeometryElement* node)
    : LayoutSVGShape(node) {}

LayoutSVGEllipse::~LayoutSVGEllipse() = default;

void LayoutSVGEllipse::StyleDidChange(StyleDifference diff,
                                      const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutSVGShape::StyleDidChange(diff, old_style);

  if (old_style && GeometryPropertiesChanged(*old_style, StyleRef())) {
    SetNeedsShapeUpdate();
  }
}

gfx::RectF LayoutSVGEllipse::UpdateShapeFromElement() {
  NOT_DESTROYED();

  // Reset shape state.
  ClearPath();
  SetGeometryType(GeometryType::kEmpty);

  // This will always update/reset |center_| and |radii_|.
  CalculateRadiiAndCenter();
  DCHECK_GE(radius_x_, 0);
  DCHECK_GE(radius_y_, 0);

  if (radius_x_ && radius_y_) {
    const bool is_circle = radius_x_ == radius_y_;
    SetGeometryType(is_circle ? GeometryType::kCircle : GeometryType::kEllipse);
  }
  const gfx::RectF bounding_box(center_.x() - radius_x_,
                                center_.y() - radius_y_, radius_x_ * 2,
                                radius_y_ * 2);
  return bounding_box;
}

void LayoutSVGEllipse::CalculateRadiiAndCenter() {
  NOT_DESTROYED();
  DCHECK(GetElement());
  const SVGViewportResolver viewport_resolver(*this);
  const ComputedStyle& style = StyleRef();
  center_ =
      PointForLengthPair(style.Cx(), style.Cy(), viewport_resolver, style);

  if (IsA<SVGCircleElement>(*GetElement())) {
    radius_x_ = radius_y_ = ValueForLength(style.R(), viewport_resolver, style);
  } else {
    const gfx::Vector2dF radii =
        VectorForLengthPair(style.Rx(), style.Ry(), viewport_resolver, style);
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

bool LayoutSVGEllipse::CanUseStrokeHitTestFastPath() const {
  // Non-scaling-stroke needs special handling.
  if (HasNonScalingStroke()) {
    return false;
  }
  // We can compute intersections with continuous strokes on circles
  // without using a Path.
  return GetGeometryType() == GeometryType::kCircle && HasContinuousStroke();
}

bool LayoutSVGEllipse::ShapeDependentStrokeContains(
    const HitTestLocation& location) {
  NOT_DESTROYED();
  DCHECK_GE(radius_x_, 0);
  DCHECK_GE(radius_y_, 0);
  if (!radius_x_ || !radius_y_)
    return false;

  if (!CanUseStrokeHitTestFastPath()) {
    EnsurePath();
    return LayoutSVGShape::ShapeDependentStrokeContains(location);
  }
  return location.IntersectsCircleStroke(center_, radius_x_, StrokeWidth());
}

bool LayoutSVGEllipse::ShapeDependentFillContains(
    const HitTestLocation& location,
    const WindRule fill_rule) const {
  NOT_DESTROYED();
  DCHECK_GE(radius_x_, 0);
  DCHECK_GE(radius_y_, 0);
  if (!radius_x_ || !radius_y_)
    return false;
  return location.IntersectsEllipse(center_, gfx::SizeF(radius_x_, radius_y_));
}

bool LayoutSVGEllipse::HasContinuousStroke() const {
  NOT_DESTROYED();
  return !StyleRef().HasDashArray();
}

}  // namespace blink
