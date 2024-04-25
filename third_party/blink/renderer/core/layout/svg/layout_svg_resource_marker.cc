/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_marker.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_info.h"
#include "third_party/blink/renderer/core/layout/svg/svg_marker_data.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/svg/svg_animated_angle.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_animated_rect.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"

namespace blink {

LayoutSVGResourceMarker::LayoutSVGResourceMarker(SVGMarkerElement* node)
    : LayoutSVGResourceContainer(node), is_in_layout_(false) {}

LayoutSVGResourceMarker::~LayoutSVGResourceMarker() = default;

SVGLayoutResult LayoutSVGResourceMarker::UpdateSVGLayout(
    const SVGLayoutInfo& layout_info) {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());
  if (is_in_layout_)
    return {};

  base::AutoReset<bool> in_layout_change(&is_in_layout_, true);

  ClearInvalidationMask();
  // LayoutSVGHiddenContainer overrides UpdateSVGLayout(). We need the
  // LayoutSVGContainer behavior for calculating local transformations and paint
  // invalidation.
  return LayoutSVGContainer::UpdateSVGLayout(layout_info);
}

bool LayoutSVGResourceMarker::FindCycleFromSelf() const {
  NOT_DESTROYED();
  return FindCycleInSubtree(*this);
}

void LayoutSVGResourceMarker::RemoveAllClientsFromCache() {
  NOT_DESTROYED();
  MarkAllClientsForInvalidation(kLayoutInvalidation | kBoundariesInvalidation);
}

gfx::RectF LayoutSVGResourceMarker::MarkerBoundaries(
    const AffineTransform& marker_transformation) const {
  NOT_DESTROYED();
  gfx::RectF coordinates =
      LayoutSVGContainer::VisualRectInLocalSVGCoordinates();

  // Map visual rect into parent coordinate space, in which the marker
  // boundaries have to be evaluated.
  coordinates = LocalToSVGParentTransform().MapRect(coordinates);

  return marker_transformation.MapRect(coordinates);
}

gfx::PointF LayoutSVGResourceMarker::ReferencePoint() const {
  NOT_DESTROYED();
  auto* marker = To<SVGMarkerElement>(GetElement());
  DCHECK(marker);

  SVGLengthContext length_context(marker);
  return gfx::PointF(marker->refX()->CurrentValue()->Value(length_context),
                     marker->refY()->CurrentValue()->Value(length_context));
}

float LayoutSVGResourceMarker::Angle() const {
  NOT_DESTROYED();
  return To<SVGMarkerElement>(GetElement())
      ->orientAngle()
      ->CurrentValue()
      ->Value();
}

SVGMarkerUnitsType LayoutSVGResourceMarker::MarkerUnits() const {
  NOT_DESTROYED();
  return To<SVGMarkerElement>(GetElement())->markerUnits()->CurrentEnumValue();
}

SVGMarkerOrientType LayoutSVGResourceMarker::OrientType() const {
  NOT_DESTROYED();
  return To<SVGMarkerElement>(GetElement())->orientType()->CurrentEnumValue();
}

AffineTransform LayoutSVGResourceMarker::MarkerTransformation(
    const MarkerPosition& position,
    float stroke_width) const {
  NOT_DESTROYED();
  // Apply scaling according to markerUnits ('strokeWidth' or 'userSpaceOnUse'.)
  float marker_scale =
      MarkerUnits() == kSVGMarkerUnitsStrokeWidth ? stroke_width : 1;

  double computed_angle = position.angle;
  SVGMarkerOrientType orient_type = OrientType();
  if (orient_type == kSVGMarkerOrientAngle) {
    computed_angle = Angle();
  } else if (position.type == kStartMarker &&
             orient_type == kSVGMarkerOrientAutoStartReverse) {
    computed_angle += 180;
  }

  AffineTransform transform;
  transform.Translate(position.origin.x(), position.origin.y());
  transform.Rotate(computed_angle);
  transform.Scale(marker_scale);

  // The reference point (refX, refY) is in the coordinate space of the marker's
  // contents so we include the value in each marker's transform.
  gfx::PointF mapped_reference_point =
      LocalToSVGParentTransform().MapPoint(ReferencePoint());
  transform.Translate(-mapped_reference_point.x(), -mapped_reference_point.y());
  return transform;
}

bool LayoutSVGResourceMarker::ShouldPaint() const {
  NOT_DESTROYED();
  // An empty viewBox disables rendering.
  auto* marker = To<SVGMarkerElement>(GetElement());
  DCHECK(marker);
  return !marker->HasValidViewBox() ||
         !marker->viewBox()->CurrentValue()->Rect().IsEmpty();
}

void LayoutSVGResourceMarker::SetNeedsTransformUpdate() {
  NOT_DESTROYED();
  LayoutSVGContainer::SetNeedsTransformUpdate();
}

SVGTransformChange LayoutSVGResourceMarker::UpdateLocalTransform(
    const gfx::RectF& reference_box) {
  NOT_DESTROYED();
  auto* marker = To<SVGMarkerElement>(GetElement());
  DCHECK(marker);

  SVGLengthContext length_context(marker);
  float width = marker->markerWidth()->CurrentValue()->Value(length_context);
  float height = marker->markerHeight()->CurrentValue()->Value(length_context);
  viewport_size_.SetSize(width, height);

  SVGTransformChangeDetector change_detector(local_to_parent_transform_);
  local_to_parent_transform_ = marker->ViewBoxToViewTransform(viewport_size_);
  return change_detector.ComputeChange(local_to_parent_transform_);
}

}  // namespace blink
