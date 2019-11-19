/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_RECT_H_

#include <iosfwd>
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DoubleRect;

class PLATFORM_EXPORT LayoutRect {
  DISALLOW_NEW();

 public:
  constexpr LayoutRect() = default;
  constexpr LayoutRect(const LayoutPoint& location, const LayoutSize& size)
      : location_(location), size_(size) {}
  constexpr LayoutRect(LayoutUnit x,
                       LayoutUnit y,
                       LayoutUnit width,
                       LayoutUnit height)
      : location_(LayoutPoint(x, y)), size_(LayoutSize(width, height)) {}
  constexpr LayoutRect(int x, int y, int width, int height)
      : location_(LayoutPoint(x, y)), size_(LayoutSize(width, height)) {}
  constexpr LayoutRect(const FloatPoint& location, const FloatSize& size)
      : location_(location), size_(size) {}
  constexpr LayoutRect(const DoublePoint& location, const DoubleSize& size)
      : location_(location), size_(size) {}
  constexpr LayoutRect(const IntPoint& location, const IntSize& size)
      : location_(location), size_(size) {}
  constexpr explicit LayoutRect(const IntRect& rect)
      : location_(rect.Location()), size_(rect.Size()) {}

  // Don't do these implicitly since they are lossy.
  constexpr explicit LayoutRect(const FloatRect& r)
      : location_(r.Location()), size_(r.Size()) {}
  explicit LayoutRect(const DoubleRect&);

  constexpr explicit operator FloatRect() const {
    return FloatRect(X(), Y(), Width(), Height());
  }

  constexpr LayoutPoint Location() const { return location_; }
  constexpr LayoutSize Size() const { return size_; }

  IntPoint PixelSnappedLocation() const { return RoundedIntPoint(location_); }
  IntSize PixelSnappedSize() const {
    return IntSize(SnapSizeToPixel(size_.Width(), location_.X()),
                   SnapSizeToPixel(size_.Height(), location_.Y()));
  }

  void SetLocation(const LayoutPoint& location) { location_ = location; }
  void SetSize(const LayoutSize& size) { size_ = size; }

  constexpr ALWAYS_INLINE LayoutUnit X() const { return location_.X(); }
  constexpr ALWAYS_INLINE LayoutUnit Y() const { return location_.Y(); }
  ALWAYS_INLINE LayoutUnit MaxX() const { return X() + Width(); }
  ALWAYS_INLINE LayoutUnit MaxY() const { return Y() + Height(); }
  constexpr LayoutUnit Width() const { return size_.Width(); }
  constexpr LayoutUnit Height() const { return size_.Height(); }

  int PixelSnappedWidth() const { return SnapSizeToPixel(Width(), X()); }
  int PixelSnappedHeight() const { return SnapSizeToPixel(Height(), Y()); }

  void SetX(LayoutUnit x) { location_.SetX(x); }
  void SetY(LayoutUnit y) { location_.SetY(y); }
  void SetWidth(LayoutUnit width) { size_.SetWidth(width); }
  void SetHeight(LayoutUnit height) { size_.SetHeight(height); }

  constexpr ALWAYS_INLINE bool IsEmpty() const { return size_.IsEmpty(); }

  // NOTE: The result is rounded to integer values, and thus may be not the
  // exact center point.
  LayoutPoint Center() const {
    return LayoutPoint(X() + Width() / 2, Y() + Height() / 2);
  }

  void Move(const LayoutSize& size) { location_ += size; }
  void Move(const IntSize& size) {
    location_.Move(LayoutUnit(size.Width()), LayoutUnit(size.Height()));
  }
  void MoveBy(const LayoutPoint& offset) {
    location_.Move(offset.X(), offset.Y());
  }
  void MoveBy(const IntPoint& offset) {
    location_.Move(LayoutUnit(offset.X()), LayoutUnit(offset.Y()));
  }
  void Move(LayoutUnit dx, LayoutUnit dy) { location_.Move(dx, dy); }
  void Move(int dx, int dy) { location_.Move(LayoutUnit(dx), LayoutUnit(dy)); }

  void Expand(const LayoutSize& size) { size_ += size; }
  void Expand(const LayoutRectOutsets& box) {
    location_.Move(-box.Left(), -box.Top());
    size_.Expand(box.Left() + box.Right(), box.Top() + box.Bottom());
  }
  void Expand(LayoutUnit dw, LayoutUnit dh) { size_.Expand(dw, dh); }
  void ExpandEdges(LayoutUnit top,
                   LayoutUnit right,
                   LayoutUnit bottom,
                   LayoutUnit left) {
    location_.Move(-left, -top);
    size_.Expand(left + right, top + bottom);
  }
  void Contract(const LayoutSize& size) { size_ -= size; }
  void Contract(LayoutUnit dw, LayoutUnit dh) { size_.Expand(-dw, -dh); }
  void Contract(int dw, int dh) { size_.Expand(-dw, -dh); }
  void Contract(const LayoutRectOutsets& box) {
    location_.Move(box.Left(), box.Top());
    size_.Shrink(box.Left() + box.Right(), box.Top() + box.Bottom());
  }
  void ContractEdges(LayoutUnit top,
                     LayoutUnit right,
                     LayoutUnit bottom,
                     LayoutUnit left) {
    location_.Move(left, top);
    size_.Shrink(left + right, top + bottom);
  }

  // Convert to an outsets for {{0, 0}, size} rect.
  LayoutRectOutsets ToOutsets(const LayoutSize& size) const {
    return LayoutRectOutsets(-Y(), MaxX() - size.Width(),
                             MaxY() - size.Height(), -X());
  }

  void ShiftXEdgeTo(LayoutUnit edge) {
    LayoutUnit delta = edge - X();
    SetX(edge);
    SetWidth((Width() - delta).ClampNegativeToZero());
  }
  void ShiftMaxXEdgeTo(LayoutUnit edge) {
    LayoutUnit delta = edge - MaxX();
    SetWidth((Width() + delta).ClampNegativeToZero());
  }
  void ShiftYEdgeTo(LayoutUnit edge) {
    LayoutUnit delta = edge - Y();
    SetY(edge);
    SetHeight((Height() - delta).ClampNegativeToZero());
  }
  void ShiftMaxYEdgeTo(LayoutUnit edge) {
    LayoutUnit delta = edge - MaxY();
    SetHeight((Height() + delta).ClampNegativeToZero());
  }

  // Typically top left.
  constexpr LayoutPoint MinXMinYCorner() const { return location_; }

  // Typically top right.
  LayoutPoint MaxXMinYCorner() const {
    return LayoutPoint(location_.X() + size_.Width(), location_.Y());
  }

  // Typically bottom left.
  LayoutPoint MinXMaxYCorner() const {
    return LayoutPoint(location_.X(), location_.Y() + size_.Height());
  }

  // Typically bottom right.
  LayoutPoint MaxXMaxYCorner() const {
    return LayoutPoint(location_.X() + size_.Width(),
                       location_.Y() + size_.Height());
  }

  bool Intersects(const LayoutRect&) const;
  bool Contains(const LayoutRect&) const;

  // This checks to see if the rect contains x,y in the traditional sense.
  // Equivalent to checking if the rect contains a 1x1 rect below and to the
  // right of (px,py).
  bool Contains(LayoutUnit px, LayoutUnit py) const {
    return px >= X() && px < MaxX() && py >= Y() && py < MaxY();
  }
  bool Contains(const LayoutPoint& point) const {
    return Contains(point.X(), point.Y());
  }

  // Whether all edges of the rect are at full-pixel boundaries.
  // i.e.: EnclosingIntRect(this)) == this
  bool EdgesOnPixelBoundaries() const {
    return !location_.X().HasFraction() && !location_.Y().HasFraction() &&
           !size_.Width().HasFraction() && !size_.Height().HasFraction();
  }

  // Expand each edge outwards to the next full-pixel boundary.
  // i.e.: this = LayoutRect(EnclosingIntRect(this))
  void ExpandEdgesToPixelBoundaries() {
    int x = X().Floor();
    int y = Y().Floor();
    int max_x = MaxX().Ceil();
    int max_y = MaxY().Ceil();
    location_.SetX(LayoutUnit(x));
    location_.SetY(LayoutUnit(y));
    size_.SetWidth(LayoutUnit(max_x - x));
    size_.SetHeight(LayoutUnit(max_y - y));
  }

  void Intersect(const LayoutRect&);
  void Unite(const LayoutRect&);
  void UniteIfNonZero(const LayoutRect&);

  // Set this rect to be the intersection of itself and the argument rect
  // using edge-inclusive geometry.  If the two rectangles overlap but the
  // overlap region is zero-area (either because one of the two rectangles
  // is zero-area, or because the rectangles overlap at an edge or a corner),
  // the result is the zero-area intersection.  The return value indicates
  // whether the two rectangle actually have an intersection, since checking
  // the result for isEmpty() is not conclusive.
  bool InclusiveIntersect(const LayoutRect&);

  // Similar to |Intersects| but inclusive (see also: |InclusiveIntersect|).
  // For example, (0,0 10x10) would inclusively intersect (10,10 0x0) even
  // though the intersection has zero area and |Intersects| would be false.
  bool IntersectsInclusively(const LayoutRect&);

  // Besides non-empty rects, this method also unites empty rects (as points or
  // line segments).  For example, union of (100, 100, 0x0) and (200, 200, 50x0)
  // is (100, 100, 150x100).
  void UniteEvenIfEmpty(const LayoutRect&);

  void InflateX(LayoutUnit dx) {
    location_.SetX(location_.X() - dx);
    size_.SetWidth(size_.Width() + dx + dx);
  }
  void InflateY(LayoutUnit dy) {
    location_.SetY(location_.Y() - dy);
    size_.SetHeight(size_.Height() + dy + dy);
  }
  void Inflate(LayoutUnit d) {
    InflateX(d);
    InflateY(d);
  }
  void Inflate(int d) { Inflate(LayoutUnit(d)); }
  void Scale(float s);
  void Scale(float x_axis_scale, float y_axis_scale);

  LayoutRect TransposedRect() const {
    return LayoutRect(location_.TransposedPoint(), size_.TransposedSize());
  }

  static IntRect InfiniteIntRect() {
    // Due to saturated arithemetic this value is not the same as
    // LayoutRect(IntRect(INT_MIN/2, INT_MIN/2, INT_MAX, INT_MAX)).
    static IntRect infinite_int_rect(LayoutUnit::NearlyMin().ToInt() / 2,
                                     LayoutUnit::NearlyMin().ToInt() / 2,
                                     LayoutUnit::NearlyMax().ToInt(),
                                     LayoutUnit::NearlyMax().ToInt());
    return infinite_int_rect;
  }

  String ToString() const;

 private:
  LayoutPoint location_;
  LayoutSize size_;
};

inline LayoutRect Intersection(const LayoutRect& a, const LayoutRect& b) {
  LayoutRect c = a;
  c.Intersect(b);
  return c;
}

inline LayoutRect UnionRect(const LayoutRect& a, const LayoutRect& b) {
  LayoutRect c = a;
  c.Unite(b);
  return c;
}

PLATFORM_EXPORT LayoutRect UnionRect(const Vector<LayoutRect>&);

inline LayoutRect UnionRectEvenIfEmpty(const LayoutRect& a,
                                       const LayoutRect& b) {
  LayoutRect c = a;
  c.UniteEvenIfEmpty(b);
  return c;
}

PLATFORM_EXPORT LayoutRect UnionRectEvenIfEmpty(const Vector<LayoutRect>&);

constexpr ALWAYS_INLINE bool operator==(const LayoutRect& a,
                                        const LayoutRect& b) {
  return a.Location() == b.Location() && a.Size() == b.Size();
}

constexpr bool operator!=(const LayoutRect& a, const LayoutRect& b) {
  return !(a == b);
}

inline IntRect PixelSnappedIntRect(const LayoutRect& rect) {
  return IntRect(RoundedIntPoint(rect.Location()),
                 IntSize(SnapSizeToPixel(rect.Width(), rect.X()),
                         SnapSizeToPixel(rect.Height(), rect.Y())));
}

inline IntRect EnclosingIntRect(const LayoutRect& rect) {
  IntPoint location = FlooredIntPoint(rect.MinXMinYCorner());
  IntPoint max_point = CeiledIntPoint(rect.MaxXMaxYCorner());
  return IntRect(location, max_point - location);
}

inline IntRect EnclosedIntRect(const LayoutRect& rect) {
  IntPoint location = CeiledIntPoint(rect.MinXMinYCorner());
  IntPoint max_point = FlooredIntPoint(rect.MaxXMaxYCorner());
  return IntRect(location, max_point - location);
}

inline LayoutRect EnclosingLayoutRect(const FloatRect& rect) {
  LayoutUnit x = LayoutUnit::FromFloatFloor(rect.X());
  LayoutUnit y = LayoutUnit::FromFloatFloor(rect.Y());
  LayoutUnit max_x = LayoutUnit::FromFloatCeil(rect.MaxX());
  LayoutUnit max_y = LayoutUnit::FromFloatCeil(rect.MaxY());
  return LayoutRect(x, y, max_x - x, max_y - y);
}

inline IntRect PixelSnappedIntRect(LayoutUnit left,
                                   LayoutUnit top,
                                   LayoutUnit width,
                                   LayoutUnit height) {
  return IntRect(left.Round(), top.Round(), SnapSizeToPixel(width, left),
                 SnapSizeToPixel(height, top));
}

inline IntRect PixelSnappedIntRectFromEdges(LayoutUnit left,
                                            LayoutUnit top,
                                            LayoutUnit right,
                                            LayoutUnit bottom) {
  return IntRect(left.Round(), top.Round(), SnapSizeToPixel(right - left, left),
                 SnapSizeToPixel(bottom - top, top));
}

inline IntRect PixelSnappedIntRect(LayoutPoint location, LayoutSize size) {
  return IntRect(RoundedIntPoint(location),
                 PixelSnappedIntSize(size, location));
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const LayoutRect&);
PLATFORM_EXPORT WTF::TextStream& operator<<(WTF::TextStream&,
                                            const LayoutRect&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_RECT_H_
