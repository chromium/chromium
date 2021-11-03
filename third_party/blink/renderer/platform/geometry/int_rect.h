/*
 * Copyright (C) 2003, 2006, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_INT_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_INT_RECT_H_

#include <iosfwd>

#include "base/compiler_specific.h"
#include "base/numerics/clamped_math.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/geometry/int_rect_outsets.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_MAC)
typedef struct CGRect CGRect;

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif
#endif

namespace blink {

class FloatRect;
class LayoutRect;

class PLATFORM_EXPORT IntRect {
  USING_FAST_MALLOC(IntRect);

 public:
  constexpr IntRect() = default;
  constexpr IntRect(const gfx::Point& location, const IntSize& size)
      : location_(location), size_(size) {}
  constexpr IntRect(int x, int y, int width, int height)
      : location_(gfx::Point(x, y)), size_(IntSize(width, height)) {}

  // Use EnclosingIntRect(), EnclosedIntRect(), RoundedIntRect(),
  // PixelSnappedIntRect(), etc. instead.
  constexpr explicit IntRect(const FloatRect&) = delete;
  constexpr explicit IntRect(const LayoutRect&) = delete;

  constexpr explicit IntRect(const gfx::Rect& r)
      : IntRect(r.x(), r.y(), r.width(), r.height()) {}
  explicit IntRect(const SkIRect& r)
      : IntRect(r.x(), r.y(), r.width(), r.height()) {}

  constexpr gfx::Point origin() const { return location_; }
  constexpr IntSize size() const { return size_; }

  gfx::Vector2d OffsetFromOrigin() const {
    return location_.OffsetFromOrigin();
  }

  void set_origin(const gfx::Point& location) { location_ = location; }
  void set_size(const IntSize& size) { size_ = size; }

  constexpr int x() const { return location_.x(); }
  constexpr int y() const { return location_.y(); }
  constexpr int right() const { return x() + width(); }
  constexpr int bottom() const { return y() + height(); }
  constexpr int width() const { return size_.width(); }
  constexpr int height() const { return size_.height(); }

  void set_x(int x) { location_.set_x(x); }
  void set_y(int y) { location_.set_y(y); }
  void set_width(int width) { size_.set_width(width); }
  void set_height(int height) { size_.set_height(height); }

  constexpr bool IsEmpty() const { return size_.IsEmpty(); }

  // NOTE: The result is rounded to integer values, and thus may be not the
  // exact center point.
  gfx::Point CenterPoint() const {
    return gfx::Point(x() + width() / 2, y() + height() / 2);
  }

  void Offset(const IntSize& size) { Offset(ToGfxVector2d(size)); }
  void Offset(const gfx::Vector2d& offset) { location_ += offset; }
  void MoveBy(const gfx::Point& offset) {
    location_.Offset(offset.x(), offset.y());
  }
  void Offset(int dx, int dy) { location_.Offset(dx, dy); }
  void SaturatedMove(int dx, int dy) { location_.Offset(dx, dy); }

  void Expand(const IntSize& size) { size_ += size; }
  void Expand(int dw, int dh) { size_.Enlarge(dw, dh); }
  void Expand(const IntRectOutsets& outsets) {
    location_.Offset(-outsets.Left(), -outsets.Top());
    size_.Enlarge(outsets.Left() + outsets.Right(),
                  outsets.Top() + outsets.Bottom());
  }

  void Contract(const IntSize& size) { size_ -= size; }
  void Contract(int dw, int dh) { size_.Enlarge(-dw, -dh); }

  void ShiftXEdgeTo(int);
  void ShiftMaxXEdgeTo(int);
  void ShiftYEdgeTo(int);
  void ShiftMaxYEdgeTo(int);

  constexpr gfx::Point top_right() const {
    return gfx::Point(location_.x() + size_.width(), location_.y());
  }  // typically topRight
  constexpr gfx::Point bottom_left() const {
    return gfx::Point(location_.x(), location_.y() + size_.height());
  }  // typically bottomLeft
  constexpr gfx::Point bottom_right() const {
    return gfx::Point(location_.x() + size_.width(),
                      location_.y() + size_.height());
  }  // typically bottomRight

  WARN_UNUSED_RESULT bool Intersects(const IntRect&) const;
  bool Contains(const IntRect&) const;

  // This checks to see if the rect contains x,y in the traditional sense.
  // Equivalent to checking if the rect contains a 1x1 rect below and to the
  // right of (px,py).
  bool Contains(int px, int py) const {
    return px >= x() && px < right() && py >= y() && py < bottom();
  }
  bool Contains(const gfx::Point& point) const {
    return Contains(point.x(), point.y());
  }

  void Intersect(const IntRect&);

  void Union(const IntRect&);
  void UnionIfNonZero(const IntRect&);

  // Besides non-empty rects, this method also unites empty rects (as points or
  // line segments).  For example, union of (100, 100, 0x0) and (200, 200, 50x0)
  // is (100, 100, 150x100).
  void UnionEvenIfEmpty(const IntRect&);

  void OutsetX(int dx) {
    location_.set_x(location_.x() - dx);
    size_.set_width(size_.width() + dx + dx);
  }
  void OutsetY(int dy) {
    location_.set_y(location_.y() - dy);
    size_.set_height(size_.height() + dy + dy);
  }
  void Outset(int d) {
    OutsetX(d);
    OutsetY(d);
  }
  void Scale(float s);

  IntSize DifferenceToPoint(const gfx::Point&) const;
  int DistanceSquaredToPoint(const gfx::Point& p) const {
    return DifferenceToPoint(p).DiagonalLengthSquared();
  }

  IntRect TransposedRect() const {
    return IntRect(location_.y(), location_.x(), size_.height(), size_.width());
  }

#if defined(OS_MAC)
  explicit operator CGRect() const;
#endif

  operator SkRect() const {
    return SkRect::MakeLTRB(x(), y(), right(), bottom());
  }
  operator SkIRect() const {
    return SkIRect::MakeLTRB(x(), y(), right(), bottom());
  }

  // This is deleted during blink geometry type to gfx migration.
  // Use ToGfxRect() instead.
  operator gfx::Rect() const = delete;

  String ToString() const;

  // Return false if x + width or y + height overflows.
  bool IsValid() const;

 private:
  void SetLocationAndSizeFromEdges(int left, int top, int right, int bottom) {
    location_.set_x(left);
    location_.set_y(top);
    size_.set_width(base::ClampSub(right, left));
    size_.set_height(base::ClampSub(bottom, top));
  }

  gfx::Point location_;
  IntSize size_;
};

inline IntRect IntersectRects(const IntRect& a, const IntRect& b) {
  IntRect c = a;
  c.Intersect(b);
  return c;
}

inline IntRect UnionRects(const IntRect& a, const IntRect& b) {
  IntRect c = a;
  c.Union(b);
  return c;
}

PLATFORM_EXPORT IntRect UnionRects(const Vector<IntRect>&);

inline IntRect UnionRectsEvenIfEmpty(const IntRect& a, const IntRect& b) {
  IntRect c = a;
  c.UnionEvenIfEmpty(b);
  return c;
}

PLATFORM_EXPORT IntRect UnionRectsEvenIfEmpty(const Vector<IntRect>&);

constexpr IntRect SaturatedRect(const IntRect& r) {
  return IntRect(r.x(), r.y(), base::ClampAdd(r.x(), r.width()) - r.x(),
                 base::ClampAdd(r.y(), r.height()) - r.y());
}

constexpr bool operator==(const IntRect& a, const IntRect& b) {
  return a.origin() == b.origin() && a.size() == b.size();
}

constexpr bool operator!=(const IntRect& a, const IntRect& b) {
  return !(a == b);
}

constexpr gfx::Rect ToGfxRect(const IntRect& r) {
  return gfx::Rect(r.x(), r.y(), r.width(), r.height());
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const IntRect&);
PLATFORM_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const IntRect&);

}  // namespace blink

namespace WTF {
template <>
struct CrossThreadCopier<blink::IntRect>
    : public CrossThreadCopierPassThrough<blink::IntRect> {
  STATIC_ONLY(CrossThreadCopier);
};
}

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::IntRect)

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_INT_RECT_H_
