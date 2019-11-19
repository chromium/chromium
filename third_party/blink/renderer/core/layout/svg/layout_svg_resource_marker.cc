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
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"

namespace blink {

LayoutSVGResourceMarker::LayoutSVGResourceMarker(SVGMarkerElement* node)
    : LayoutSVGResourceContainer(node), needs_transform_update_(true) {}

LayoutSVGResourceMarker::~LayoutSVGResourceMarker() = default;

void LayoutSVGResourceMarker::UpdateLayout() {
  DCHECK(NeedsLayout());
  if (is_in_layout_)
    return;

  base::AutoReset<bool> in_layout_change(&is_in_layout_, true);

  // LayoutSVGHiddenContainer overwrites layout(). We need the
  // layouting of LayoutSVGContainer for calculating  local
  // transformations and paint invalidation.
  LayoutSVGContainer::UpdateLayout();

  ClearInvalidationMask();
}

void LayoutSVGResourceMarker::RemoveAllClientsFromCache(
    bool mark_for_invalidation) {
  MarkAllClientsForInvalidation(
      mark_for_invalidation ? SVGResourceClient::kLayoutInvalidation |
                                  SVGResourceClient::kBoundariesInvalidation
                            : SVGResourceClient::kParentOnlyInvalidation);
}

FloatRect LayoutSVGResourceMarker::MarkerBoundaries(
    const AffineTransform& marker_transformation) const {
  FloatRect coordinates = LayoutSVGContainer::VisualRectInLocalSVGCoordinates();

  // Map visual rect into parent coordinate space, in which the marker
  // boundaries have to be evaluated.
  coordinates = LocalToSVGParentTransform().MapRect(coordinates);

  return marker_transformation.MapRect(coordinates);
}

FloatPoint LayoutSVGResourceMarker::ReferencePoint() const {
  SVGMarkerElement* marker = ToSVGMarkerElement(GetElement());
  DCHECK(marker);

  SVGLengthContext length_context(marker);
  return FloatPoint(marker->refX()->CurrentValue()->Value(length_context),
                    marker->refY()->CurrentValue()->Value(length_context));
}

float LayoutSVGResourceMarker::Angle() const {
  return ToSVGMarkerElement(GetElement())
      ->orientAngle()
      ->CurrentValue()
      ->Value();
}

SVGMarkerUnitsType LayoutSVGResourceMarker::MarkerUnits() const {
  return ToSVGMarkerElement(GetElement())
      ->markerUnits()
      ->CurrentValue()
      ->EnumValue();
}

SVGMarkerOrientType LayoutSVGResourceMarker::OrientType() const {
  return ToSVGMarkerElement(GetElement())
      ->orientType()
      ->CurrentValue()
      ->EnumValue();
}

AffineTransform LayoutSVGResourceMarker::MarkerTransformation(
    const FloatPoint& origin,
    float auto_angle,
    float stroke_width) const {
  // Apply scaling according to markerUnits ('strokeWidth' or 'userSpaceOnUse'.)
  float marker_scale =
      MarkerUnits() == kSVGMarkerUnitsStrokeWidth ? stroke_width : 1;

  AffineTransform transform;
  transform.Translate(origin.X(), origin.Y());
  transform.Rotate(OrientType() == kSVGMarkerOrientAngle ? Angle()
                                                         : auto_angle);
  transform.Scale(marker_scale);

  // The reference point (refX, refY) is in the coordinate space of the marker's
  // contents so we include the value in each marker's transform.
  FloatPoint mapped_reference_point =
      LocalToSVGParentTransform().MapPoint(ReferencePoint());
  transform.Translate(-mapped_reference_point.X(), -mapped_reference_point.Y());
  return transform;
}

bool LayoutSVGResourceMarker::ShouldPaint() const {
  // An empty viewBox disables rendering.
  SVGMarkerElement* marker = ToSVGMarkerElement(GetElement());
  DCHECK(marker);
  return !marker->viewBox()->IsSpecified() ||
         !marker->viewBox()->CurrentValue()->IsValid() ||
         !marker->viewBox()->CurrentValue()->Value().IsEmpty();
}

void LayoutSVGResourceMarker::SetNeedsTransformUpdate() {
  // The transform paint property relies on the SVG transform being up-to-date
  // (see: PaintPropertyTreeBuilder::updateTransformForNonRootSVG).
  SetNeedsPaintPropertyUpdate();
  needs_transform_update_ = true;
}

SVGTransformChange LayoutSVGResourceMarker::CalculateLocalTransform() {
  if (!needs_transform_update_)
    return SVGTransformChange::kNone;

  SVGMarkerElement* marker = ToSVGMarkerElement(GetElement());
  DCHECK(marker);

  SVGLengthContext length_context(marker);
  float width = marker->markerWidth()->CurrentValue()->Value(length_context);
  float height = marker->markerHeight()->CurrentValue()->Value(length_context);
  viewport_size_ = FloatSize(width, height);

  SVGTransformChangeDetector change_detector(local_to_parent_transform_);
  local_to_parent_transform_ = marker->ViewBoxToViewTransform(
      viewport_size_.Width(), viewport_size_.Height());

  needs_transform_update_ = false;
  return change_detector.ComputeChange(local_to_parent_transform_);
}

}  // namespace blink
