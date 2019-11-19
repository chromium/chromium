/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TEST_LOCATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TEST_LOCATION_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class FloatRoundedRect;

class CORE_EXPORT HitTestLocation {
  DISALLOW_NEW();

 public:
  // Note that all points are in contents (aka "page") coordinate space for the
  // document that is being hit tested. All points and size are in root frame
  // coordinates (physical pixel scaled by page_scale when zoom for dsf is
  // enabled; otherwise in dip scaled by page_scale), Which means the points
  // should already applied page_scale_factor, but not page_zoom_factor and
  // scroll offset. See:
  // http://www.chromium.org/developers/design-documents/blink-coordinate-spaces
  HitTestLocation();
  explicit HitTestLocation(const PhysicalOffset&);
  explicit HitTestLocation(const IntPoint&);
  explicit HitTestLocation(const FloatPoint&);
  explicit HitTestLocation(const DoublePoint&);
  explicit HitTestLocation(const FloatPoint&, const FloatQuad&);
  explicit HitTestLocation(const PhysicalRect&);

  // The bounding box isn't always a 1x1 rect even when the hit test is not
  // rect-based. When we hit test a transformed box and transform the hit test
  // location into the box's local coordinate space, the bounding box should
  // also be transformed accordingly.
  explicit HitTestLocation(const FloatPoint& point,
                           const PhysicalRect& bounding_box);

  HitTestLocation(const HitTestLocation&, const PhysicalOffset& offset);
  HitTestLocation(const HitTestLocation&);
  ~HitTestLocation();
  HitTestLocation& operator=(const HitTestLocation&);

  const PhysicalOffset& Point() const { return point_; }
  IntPoint RoundedPoint() const { return RoundedIntPoint(point_); }

  // Rect-based hit test related methods.
  bool IsRectBasedTest() const { return is_rect_based_; }
  bool IsRectilinear() const { return is_rectilinear_; }
  const PhysicalRect& BoundingBox() const { return bounding_box_; }
  IntRect EnclosingIntRect() const {
    return ::blink::EnclosingIntRect(bounding_box_);
  }

  // Returns the 1px x 1px hit test rect for a point.
  static PhysicalRect RectForPoint(const PhysicalOffset& point) {
    return PhysicalRect(point, PhysicalSize(LayoutUnit(1), LayoutUnit(1)));
  }

  bool Intersects(const PhysicalRect&) const;
  // Uses floating-point intersection, which uses inclusive intersection
  // (see LayoutRect::InclusiveIntersect for a definition)
  bool Intersects(const FloatRect&) const;
  bool Intersects(const FloatRoundedRect&) const;
  bool Intersects(const FloatQuad&) const;
  bool ContainsPoint(const FloatPoint&) const;

  const FloatPoint& TransformedPoint() const { return transformed_point_; }
  const FloatQuad& TransformedRect() const { return transformed_rect_; }

 private:
  template <typename RectType>
  bool IntersectsRect(const RectType&, const RectType& bounding_box) const;
  void Move(const PhysicalOffset& offset);

  // These are cached forms of the more accurate |transformed_point_| and
  // |transformed_rect_|, below.
  PhysicalOffset point_;
  PhysicalRect bounding_box_;

  FloatPoint transformed_point_;
  FloatQuad transformed_rect_;

  bool is_rect_based_;
  bool is_rectilinear_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TEST_LOCATION_H_
