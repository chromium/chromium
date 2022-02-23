/*
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_rect.h"

#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_rect_element.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

LayoutSVGRect::LayoutSVGRect(SVGRectElement* node)
    : LayoutSVGShape(node, kSimple), use_path_fallback_(false) {}

LayoutSVGRect::~LayoutSVGRect() = default;

void LayoutSVGRect::UpdateShapeFromElement() {
  NOT_DESTROYED();

  stroke_bounding_box_ = gfx::RectF();
  use_path_fallback_ = false;

  SVGLengthContext length_context(GetElement());
  const ComputedStyle& style = StyleRef();
  gfx::Vector2dF origin =
      length_context.ResolveLengthPair(style.X(), style.Y(), style);
  gfx::Vector2dF size =
      length_context.ResolveLengthPair(style.Width(), style.Height(), style);
  // Spec: "A negative value is an error." gfx::Rect::SetRect() clamps negative
  // width/height to 0.
  fill_bounding_box_.SetRect(origin.x(), origin.y(), size.x(), size.y());

  // Spec: "A value of zero disables rendering of the element."
  if (!fill_bounding_box_.IsEmpty()) {
    // Fallback to LayoutSVGShape and path-based hit detection if the rect
    // has rounded corners or a non-scaling or non-simple stroke.
    // However, only use LayoutSVGShape bounding-box calculations for the
    // non-scaling stroke case, since the computation below should be accurate
    // for the other cases.
    if (HasNonScalingStroke()) {
      LayoutSVGShape::UpdateShapeFromElement();
      use_path_fallback_ = true;
      return;
    }
    gfx::Vector2dF radii =
        length_context.ResolveLengthPair(style.Rx(), style.Ry(), style);
    if (radii.x() > 0 || radii.y() > 0 || !DefinitelyHasSimpleStroke()) {
      CreatePath();
      use_path_fallback_ = true;
    }
  }

  if (!use_path_fallback_)
    ClearPath();

  stroke_bounding_box_ = CalculateStrokeBoundingBox();
}

bool LayoutSVGRect::ShapeDependentStrokeContains(
    const HitTestLocation& location) {
  NOT_DESTROYED();
  // The optimized code below does not support the cases that we set
  // use_path_fallback_ in UpdateShapeFromElement().
  if (use_path_fallback_)
    return LayoutSVGShape::ShapeDependentStrokeContains(location);

  const gfx::PointF& point = location.TransformedPoint();
  const float half_stroke_width = StrokeWidth() / 2;
  const float half_width = fill_bounding_box_.width() / 2;
  const float half_height = fill_bounding_box_.height() / 2;

  const gfx::PointF fill_bounding_box_center =
      gfx::PointF(fill_bounding_box_.x() + half_width,
                  fill_bounding_box_.y() + half_height);
  const float abs_delta_x = std::abs(point.x() - fill_bounding_box_center.x());
  const float abs_delta_y = std::abs(point.y() - fill_bounding_box_center.y());

  if (!(abs_delta_x <= half_width + half_stroke_width &&
        abs_delta_y <= half_height + half_stroke_width))
    return false;

  return (half_width - half_stroke_width <= abs_delta_x) ||
         (half_height - half_stroke_width <= abs_delta_y);
}

bool LayoutSVGRect::ShapeDependentFillContains(const HitTestLocation& location,
                                               const WindRule fill_rule) const {
  NOT_DESTROYED();
  if (use_path_fallback_)
    return LayoutSVGShape::ShapeDependentFillContains(location, fill_rule);
  return fill_bounding_box_.InclusiveContains(location.TransformedPoint());
}

// Returns true if the stroke is continuous and definitely uses miter joins.
bool LayoutSVGRect::DefinitelyHasSimpleStroke() const {
  NOT_DESTROYED();
  const ComputedStyle& style = StyleRef();

  // The four angles of a rect are 90 degrees. Using the formula at:
  // http://www.w3.org/TR/SVG/painting.html#StrokeMiterlimitProperty
  // when the join style of the rect is "miter", the ratio of the miterLength
  // to the stroke-width is found to be
  // miterLength / stroke-width = 1 / sin(45 degrees)
  //                            = 1 / (1 / sqrt(2))
  //                            = sqrt(2)
  //                            = 1.414213562373095...
  // When sqrt(2) exceeds the miterlimit, then the join style switches to
  // "bevel". When the miterlimit is greater than or equal to sqrt(2) then
  // the join style remains "miter".
  //
  // An approximation of sqrt(2) is used here because at certain precise
  // miterlimits, the join style used might not be correct (e.g. a miterlimit
  // of 1.4142135 should result in bevel joins, but may be drawn using miter
  // joins).
  return !style.HasDashArray() && style.JoinStyle() == kMiterJoin &&
         style.StrokeMiterLimit() >= 1.5;
}

}  // namespace blink
