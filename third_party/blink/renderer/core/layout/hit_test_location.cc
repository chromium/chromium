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

#include <cmath>

#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/graphics/path.h"

namespace blink {

namespace {

bool PointInRectangleStroke(const gfx::PointF& point,
                            const gfx::RectF& rect,
                            float stroke_width) {
  const float half_stroke_width = stroke_width / 2;
  const float half_width = rect.width() / 2;
  const float half_height = rect.height() / 2;

  const gfx::PointF rect_center(rect.x() + half_width, rect.y() + half_height);
  const float abs_delta_x = std::abs(point.x() - rect_center.x());
  const float abs_delta_y = std::abs(point.y() - rect_center.y());

  if (!(abs_delta_x <= half_width + half_stroke_width &&
        abs_delta_y <= half_height + half_stroke_width)) {
    return false;
  }

  return (half_width - half_stroke_width <= abs_delta_x) ||
         (half_height - half_stroke_width <= abs_delta_y);
}

bool PointInEllipse(const gfx::PointF& point,
                    const gfx::PointF& center,
                    const gfx::SizeF& radii) {
  const gfx::PointF point_to_center =
      gfx::PointF(center.x() - point.x(), center.y() - point.y());

  // This works by checking if the point satisfies the ellipse equation.
  // (x/rX)^2 + (y/rY)^2 <= 1
  const float xr_x = point_to_center.x() / radii.width();
  const float yr_y = point_to_center.y() / radii.height();
  return xr_x * xr_x + yr_y * yr_y <= 1.0;
}

bool PointInCircleStroke(const gfx::PointF& point,
                         const gfx::PointF& center,
                         float radius,
                         float stroke_width) {
  const gfx::Vector2dF center_offset = center - point;
  const float half_stroke_width = stroke_width / 2;
  return std::abs(center_offset.Length() - radius) <= half_stroke_width;
}

}  // namespace

HitTestLocation::HitTestLocation()
    : is_rect_based_(false), is_rectilinear_(true) {}

HitTestLocation::HitTestLocation(const gfx::Point& point)
    : HitTestLocation(PhysicalOffset(point)) {}

HitTestLocation::HitTestLocation(const PhysicalOffset& point)
    : point_(point),
      bounding_box_(RectForPoint(point)),
      transformed_point_(point),
      transformed_rect_(gfx::RectF(bounding_box_)),
      is_rect_based_(false),
      is_rectilinear_(true) {}

HitTestLocation::HitTestLocation(const gfx::PointF& point)
    : point_(PhysicalOffset::FromPointFFloor(point)),
      bounding_box_(RectForPoint(point_)),
      transformed_point_(point),
      transformed_rect_(gfx::RectF(bounding_box_)),
      is_rect_based_(false),
      is_rectilinear_(true) {}

HitTestLocation::HitTestLocation(const gfx::PointF& point,
                                 const PhysicalRect& bounding_box)
    : point_(PhysicalOffset::FromPointFFloor(point)),
      bounding_box_(bounding_box),
      transformed_point_(point),
      transformed_rect_(gfx::RectF(bounding_box)),
      is_rect_based_(false),
      is_rectilinear_(true) {}

HitTestLocation::HitTestLocation(const gfx::PointF& point,
                                 const gfx::QuadF& quad)
    : transformed_point_(point), transformed_rect_(quad), is_rect_based_(true) {
  point_ = PhysicalOffset::FromPointFFloor(point);
  bounding_box_ = PhysicalRect::EnclosingRect(quad.BoundingBox());
  is_rectilinear_ = quad.IsRectilinear();
}

HitTestLocation::HitTestLocation(const PhysicalRect& rect)
    : point_(rect.Center()),
      bounding_box_(rect),
      transformed_point_(point_),
      is_rect_based_(true),
      is_rectilinear_(true) {
  transformed_rect_ = gfx::QuadF(gfx::RectF(bounding_box_));
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

HitTestLocation::HitTestLocation(const HitTestLocation& other,
                                 wtf_size_t fragment_index)
    : point_(other.point_),
      bounding_box_(other.bounding_box_),
      transformed_point_(other.transformed_point_),
      transformed_rect_(other.transformed_rect_),
      fragment_index_(fragment_index),
      is_rect_based_(other.is_rect_based_),
      is_rectilinear_(other.is_rectilinear_) {}

HitTestLocation::HitTestLocation(const HitTestLocation& other) = default;

HitTestLocation& HitTestLocation::operator=(const HitTestLocation& other) =
    default;

void HitTestLocation::Move(const PhysicalOffset& offset) {
  point_ += offset;
  bounding_box_.Move(offset);
  transformed_point_ += gfx::Vector2dF(offset);
  transformed_rect_ += gfx::Vector2dF(offset);
}

bool HitTestLocation::Intersects(const PhysicalRect& rect) const {
  // FIXME: When the hit test is not rect based we should use
  // rect.contains(m_point).
  // That does change some corner case tests though.

  // First check if rect even intersects our bounding box.
  if (!rect.Intersects(bounding_box_))
    return false;

  // If the transformed rect is rectilinear the bounding box intersection was
  // accurate.
  if (is_rectilinear_)
    return true;

  // If rect fully contains our bounding box, we are also sure of an
  // intersection.
  if (rect.Contains(bounding_box_))
    return true;

  // Otherwise we need to do a slower quad based intersection test.
  return transformed_rect_.IntersectsRectPartial(gfx::RectF(rect));
}

bool HitTestLocation::Intersects(const gfx::RectF& rect) const {
  if (is_rect_based_)
    return transformed_rect_.IntersectsRect(rect);
  return rect.InclusiveContains(transformed_point_);
}

bool HitTestLocation::Intersects(const FloatRoundedRect& rect) const {
  return rect.IntersectsQuad(transformed_rect_);
}

bool HitTestLocation::Intersects(const gfx::QuadF& quad) const {
  // TODO(chrishtr): if the quads are not rectilinear, calling Intersects
  // has false positives.
  if (is_rect_based_)
    return Intersects(quad.BoundingBox());
  return quad.Contains(gfx::PointF(point_));
}

bool HitTestLocation::ContainsPoint(const gfx::PointF& point) const {
  return transformed_rect_.Contains(point);
}

bool HitTestLocation::Intersects(const Path& path) const {
  // TODO(fs): Support rect-based hit-test.
  return path.Contains(transformed_point_);
}

bool HitTestLocation::Intersects(const Path& path,
                                 WindRule winding_rule) const {
  // TODO(fs): Support rect-based hit-test.
  return path.Contains(transformed_point_, winding_rule);
}

bool HitTestLocation::IntersectsStroke(const gfx::RectF& rect,
                                       float stroke_width) const {
  // TODO(fs): Support rect-based hit-test.
  return PointInRectangleStroke(transformed_point_, rect, stroke_width);
}

bool HitTestLocation::IntersectsEllipse(const gfx::PointF& center,
                                        const gfx::SizeF& radii) const {
  // TODO(fs): Support rect-based hit-test.
  return PointInEllipse(transformed_point_, center, radii);
}

bool HitTestLocation::IntersectsCircleStroke(const gfx::PointF& center,
                                             float radius,
                                             float stroke_width) const {
  // TODO(fs): Support rect-based hit-test.
  return PointInCircleStroke(transformed_point_, center, radius, stroke_width);
}

}  // namespace blink
