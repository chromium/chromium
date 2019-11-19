/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_QUAD_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_QUAD_H_

#include <iosfwd>
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

struct SkPoint;

namespace blink {

// A FloatQuad is a collection of 4 points, often representing the result of
// mapping a rectangle through transforms. When initialized from a rect, the
// points are in clockwise order from top left.
class PLATFORM_EXPORT FloatQuad {
  USING_FAST_MALLOC(FloatQuad);

 public:
  constexpr FloatQuad() = default;

  constexpr FloatQuad(const FloatPoint& p1,
                      const FloatPoint& p2,
                      const FloatPoint& p3,
                      const FloatPoint& p4)
      : p1_(p1), p2_(p2), p3_(p3), p4_(p4) {}

  constexpr FloatQuad(const FloatRect& in_rect)
      : p1_(in_rect.Location()),
        p2_(in_rect.MaxX(), in_rect.Y()),
        p3_(in_rect.MaxX(), in_rect.MaxY()),
        p4_(in_rect.X(), in_rect.MaxY()) {}

  explicit FloatQuad(const IntRect& in_rect)
      : p1_(in_rect.Location()),
        p2_(in_rect.MaxX(), in_rect.Y()),
        p3_(in_rect.MaxX(), in_rect.MaxY()),
        p4_(in_rect.X(), in_rect.MaxY()) {}

  // Converts from an array of four SkPoints, as from SkMatrix::mapRectToQuad.
  explicit FloatQuad(const SkPoint (&)[4]);

  constexpr FloatPoint P1() const { return p1_; }
  constexpr FloatPoint P2() const { return p2_; }
  constexpr FloatPoint P3() const { return p3_; }
  constexpr FloatPoint P4() const { return p4_; }

  void SetP1(const FloatPoint& p) { p1_ = p; }
  void SetP2(const FloatPoint& p) { p2_ = p; }
  void SetP3(const FloatPoint& p) { p3_ = p; }
  void SetP4(const FloatPoint& p) { p4_ = p; }

  // isEmpty tests that the bounding box is empty. This will not identify
  // "slanted" empty quads.
  bool IsEmpty() const { return BoundingBox().IsEmpty(); }

  // Tests whether this quad can be losslessly represented by a FloatRect,
  // that is, if two edges are parallel to the x-axis and the other two
  // are parallel to the y-axis. If this method returns true, the
  // corresponding FloatRect can be retrieved with boundingBox().
  bool IsRectilinear() const;

  // Tests whether the given point is inside, or on an edge or corner of this
  // quad.
  bool ContainsPoint(const FloatPoint&) const;

  // Tests whether the four corners of other are inside, or coincident with the
  // sides of this quad.  Note that this only works for convex quads, but that
  // includes all quads that originate
  // from transformed rects.
  bool ContainsQuad(const FloatQuad&) const;

  // Tests whether any part of the rectangle intersects with this quad.
  // This only works for convex quads.
  // This intersection is edge-inclusive and will return true even if the
  // intersecting area is empty (i.e., the intersection is a line or a point).
  bool IntersectsRect(const FloatRect&) const;

  // Test whether any part of the circle/ellipse intersects with this quad.
  // Note that these two functions only work for convex quads.
  // These intersections are edge-inclusive and will return true even if the
  // intersecting area is empty (i.e., the intersection is a line or a point).
  bool IntersectsCircle(const FloatPoint& center, float radius) const;
  bool IntersectsEllipse(const FloatPoint& center,
                         const FloatSize& radii) const;

  // The center of the quad. If the quad is the result of a affine-transformed
  // rectangle this is the same as the original center transformed.
  FloatPoint Center() const {
    return FloatPoint((p1_.X() + p2_.X() + p3_.X() + p4_.X()) / 4.0,
                      (p1_.Y() + p2_.Y() + p3_.Y() + p4_.Y()) / 4.0);
  }

  FloatRect BoundingBox() const;
  IntRect EnclosingBoundingBox() const {
    return EnclosingIntRect(BoundingBox());
  }

  void Move(const FloatSize& offset) {
    p1_ += offset;
    p2_ += offset;
    p3_ += offset;
    p4_ += offset;
  }

  void Move(const LayoutSize& offset) {
    Move(offset.Width().ToFloat(), offset.Height().ToFloat());
  }

  void Move(float dx, float dy) {
    p1_.Move(dx, dy);
    p2_.Move(dx, dy);
    p3_.Move(dx, dy);
    p4_.Move(dx, dy);
  }

  void Scale(float dx, float dy) {
    p1_.Scale(dx, dy);
    p2_.Scale(dx, dy);
    p3_.Scale(dx, dy);
    p4_.Scale(dx, dy);
  }

  // Tests whether points are in clock-wise, or counter clock-wise order.
  // Note that output is undefined when all points are colinear.
  bool IsCounterclockwise() const;

  String ToString() const;

 private:
  FloatPoint p1_;
  FloatPoint p2_;
  FloatPoint p3_;
  FloatPoint p4_;
};

inline FloatQuad& operator+=(FloatQuad& a, const FloatSize& b) {
  a.Move(b);
  return a;
}

inline FloatQuad& operator-=(FloatQuad& a, const FloatSize& b) {
  a.Move(-b.Width(), -b.Height());
  return a;
}

constexpr bool operator==(const FloatQuad& a, const FloatQuad& b) {
  return a.P1() == b.P1() && a.P2() == b.P2() && a.P3() == b.P3() &&
         a.P4() == b.P4();
}

constexpr bool operator!=(const FloatQuad& a, const FloatQuad& b) {
  return !(a == b);
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const FloatQuad&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_QUAD_H_
