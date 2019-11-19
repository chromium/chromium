/*
 * Copyright (C) 2006, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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
 *
*/

#include "third_party/blink/renderer/core/layout/hit_test_location.h"

#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"

namespace blink {

HitTestLocation::HitTestLocation()
    : is_rect_based_(false), is_rectilinear_(true) {}

HitTestLocation::HitTestLocation(const IntPoint& point)
    : HitTestLocation(PhysicalOffset(point)) {}

HitTestLocation::HitTestLocation(const PhysicalOffset& point)
    : point_(point),
      bounding_box_(RectForPoint(point)),
      transformed_point_(point),
      transformed_rect_(FloatRect(bounding_box_)),
      is_rect_based_(false),
      is_rectilinear_(true) {}

HitTestLocation::HitTestLocation(const FloatPoint& point)
    : point_(PhysicalOffset::FromFloatPointFloor(point)),
      bounding_box_(RectForPoint(point_)),
      transformed_point_(point),
      transformed_rect_(FloatRect(bounding_box_)),
      is_rect_based_(false),
      is_rectilinear_(true) {}

HitTestLocation::HitTestLocation(const FloatPoint& point,
                                 const PhysicalRect& bounding_box)
    : point_(PhysicalOffset::FromFloatPointFloor(point)),
      bounding_box_(bounding_box),
      transformed_point_(point),
      transformed_rect_(FloatRect(bounding_box)),
      is_rect_based_(false),
      is_rectilinear_(true) {}

HitTestLocation::HitTestLocation(const DoublePoint& point)
    : HitTestLocation(FloatPoint(point)) {}

HitTestLocation::HitTestLocation(const FloatPoint& point, const FloatQuad& quad)
    : transformed_point_(point), transformed_rect_(quad), is_rect_based_(true) {
  point_ = PhysicalOffset::FromFloatPointFloor(point);
  bounding_box_ = PhysicalRect::EnclosingRect(quad.BoundingBox());
  is_rectilinear_ = quad.IsRectilinear();
}

HitTestLocation::HitTestLocation(const PhysicalRect& rect)
    : point_(rect.Center()),
      bounding_box_(rect),
      transformed_point_(point_),
      is_rect_based_(true),
      is_rectilinear_(true) {
  transformed_rect_ = FloatQuad(FloatRect(bounding_box_));
}

HitTestLocation::HitTestLocation(const HitTestLocation& other,
                                 const PhysicalOffset& offset)
    : point_(other.point_),
      bounding_box_(other.bounding_box_),
      transformed_point_(other.transformed_point_),
      transformed_rect_(other.transformed_rect_),
      is_rect_based_(other.is_rect_based_),
      is_rectilinear_(other.is_rectilinear_) {
  Move(offset);
}

HitTestLocation::HitTestLocation(const HitTestLocation& other) = default;

HitTestLocation::~HitTestLocation() = default;

HitTestLocation& HitTestLocation::operator=(const HitTestLocation& other) =
    default;

void HitTestLocation::Move(const PhysicalOffset& offset) {
  point_ += offset;
  bounding_box_.Move(offset);
  transformed_point_.Move(FloatSize(offset));
  transformed_rect_.Move(FloatSize(offset));
}

template <typename RectType>
bool HitTestLocation::IntersectsRect(const RectType& rect,
                                     const RectType& bounding_box) const {
  // FIXME: When the hit test is not rect based we should use
  // rect.contains(m_point).
  // That does change some corner case tests though.

  // First check if rect even intersects our bounding box.
  if (!rect.Intersects(bounding_box))
    return false;

  // If the transformed rect is rectilinear the bounding box intersection was
  // accurate.
  if (is_rectilinear_)
    return true;

  // If rect fully contains our bounding box, we are also sure of an
  // intersection.
  if (rect.Contains(bounding_box))
    return true;

  // Otherwise we need to do a slower quad based intersection test.
  return transformed_rect_.IntersectsRect(FloatRect(rect));
}

bool HitTestLocation::Intersects(const PhysicalRect& rect) const {
  return IntersectsRect(rect, bounding_box_);
}

bool HitTestLocation::Intersects(const FloatRect& rect) const {
  if (is_rect_based_)
    return transformed_rect_.IntersectsRect(rect);
  return rect.Contains(transformed_point_);
}

bool HitTestLocation::Intersects(const FloatRoundedRect& rect) const {
  return rect.IntersectsQuad(transformed_rect_);
}

bool HitTestLocation::Intersects(const FloatQuad& quad) const {
  // TODO(chrishtr): if the quads are not rectilinear, calling Intersects
  // has false positives.
  if (is_rect_based_)
    return Intersects(quad.BoundingBox());
  return quad.ContainsPoint(FloatPoint(point_));
}

bool HitTestLocation::ContainsPoint(const FloatPoint& point) const {
  return transformed_rect_.ContainsPoint(point);
}

}  // namespace blink
