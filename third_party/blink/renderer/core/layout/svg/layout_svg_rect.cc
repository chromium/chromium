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

#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/core/svg/svg_rect_element.h"

namespace blink {

namespace {

bool GeometryPropertiesChanged(const ComputedStyle& old_style,
                               const ComputedStyle& new_style) {
  return old_style.X() != new_style.X() || old_style.Y() != new_style.Y() ||
         old_style.Width() != new_style.Width() ||
         old_style.Height() != new_style.Height() ||
         old_style.Rx() != new_style.Rx() || old_style.Ry() != new_style.Ry();
}

}  // namespace

LayoutSVGRect::LayoutSVGRect(SVGRectElement* node) : LayoutSVGShape(node) {}

LayoutSVGRect::~LayoutSVGRect() = default;

void LayoutSVGRect::StyleDidChange(StyleDifference diff,
                                   const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutSVGShape::StyleDidChange(diff, old_style);

  if (old_style && GeometryPropertiesChanged(*old_style, StyleRef())) {
    SetNeedsShapeUpdate();
  }
}

gfx::RectF LayoutSVGRect::UpdateShapeFromElement() {
  NOT_DESTROYED();

  // Reset shape state.
  ClearPath();
  SetGeometryType(GeometryType::kEmpty);

  const SVGViewportResolver viewport_resolver(*this);
  const ComputedStyle& style = StyleRef();
  const gfx::PointF origin =
      PointForLengthPair(style.X(), style.Y(), viewport_resolver, style);
  const gfx::Vector2dF size = VectorForLengthPair(style.Width(), style.Height(),
                                                  viewport_resolver, style);
  // Spec: "A negative value is an error." gfx::SizeF() clamps negative
  // width/height to 0.
  const gfx::RectF bounding_box(origin, gfx::SizeF(size.x(), size.y()));

  // Spec: "A value of zero disables rendering of the element."
  if (!bounding_box.IsEmpty()) {
    const gfx::Vector2dF radii =
        VectorForLengthPair(style.Rx(), style.Ry(), viewport_resolver, style);
    const bool has_radii = radii.x() > 0 || radii.y() > 0;
    SetGeometryType(has_radii ? GeometryType::kRoundedRectangle
                              : GeometryType::kRectangle);

    // If this is a rounded rectangle, we'll need a Path.
    if (GetGeometryType() != GeometryType::kRectangle) {
      CreatePath();
    }
  }
  return bounding_box;
}

bool LayoutSVGRect::CanUseStrokeHitTestFastPath() const {
  // Non-scaling-stroke needs special handling.
  if (HasNonScalingStroke()) {
    return false;
  }
  // We can compute intersections with simple, continuous strokes on
  // regular rectangles without using a Path.
  return GetGeometryType() == GeometryType::kRectangle &&
         DefinitelyHasSimpleStroke();
}

bool LayoutSVGRect::ShapeDependentStrokeContains(
    const HitTestLocation& location) {
  NOT_DESTROYED();
  if (!CanUseStrokeHitTestFastPath()) {
    EnsurePath();
    return LayoutSVGShape::ShapeDependentStrokeContains(location);
  }
  return location.IntersectsStroke(fill_bounding_box_, StrokeWidth());
}

bool LayoutSVGRect::ShapeDependentFillContains(const HitTestLocation& location,
                                               const WindRule fill_rule) const {
  NOT_DESTROYED();
  if (GetGeometryType() != GeometryType::kRectangle) {
    return LayoutSVGShape::ShapeDependentFillContains(location, fill_rule);
  }
  return location.Intersects(fill_bounding_box_);
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
