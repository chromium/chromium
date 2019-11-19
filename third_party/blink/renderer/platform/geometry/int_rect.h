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

#include "base/numerics/clamped_math.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/geometry/int_rect_outsets.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_MACOSX)
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
  constexpr IntRect(const IntPoint& location, const IntSize& size)
      : location_(location), size_(size) {}
  constexpr IntRect(int x, int y, int width, int height)
      : location_(IntPoint(x, y)), size_(IntSize(width, height)) {}

  // Use EnclosingIntRect(), EnclosedIntRect(), RoundedIntRect(),
  // PixelSnappedIntRect(), etc. instead.
  constexpr explicit IntRect(const FloatRect&) = delete;
  constexpr explicit IntRect(const LayoutRect&) = delete;

  constexpr explicit IntRect(const gfx::Rect& r)
      : IntRect(r.x(), r.y(), r.width(), r.height()) {}
  explicit IntRect(const SkIRect& r)
      : IntRect(r.x(), r.y(), r.width(), r.height()) {}

  constexpr IntPoint Location() const { return location_; }
  constexpr IntSize Size() const { return size_; }

  void SetLocation(const IntPoint& location) { location_ = location; }
  void SetSize(const IntSize& size) { size_ = size; }

  constexpr int X() const { return location_.X(); }
  constexpr int Y() const { return location_.Y(); }
  constexpr int MaxX() const { return X() + Width(); }
  constexpr int MaxY() const { return Y() + Height(); }
  constexpr int Width() const { return size_.Width(); }
  constexpr int Height() const { return size_.Height(); }

  void SetX(int x) { location_.SetX(x); }
  void SetY(int y) { location_.SetY(y); }
  void SetWidth(int width) { size_.SetWidth(width); }
  void SetHeight(int height) { size_.SetHeight(height); }

  constexpr bool IsEmpty() const { return size_.IsEmpty(); }

  // NOTE: The result is rounded to integer values, and thus may be not the
  // exact center point.
  IntPoint Center() const {
    return IntPoint(X() + Width() / 2, Y() + Height() / 2);
  }

  void Move(const IntSize& size) { location_ += size; }
  void MoveBy(const IntPoint& offset) {
    location_.Move(offset.X(), offset.Y());
  }
  void Move(int dx, int dy) { location_.Move(dx, dy); }
  void SaturatedMove(int dx, int dy) { location_.SaturatedMove(dx, dy); }

  void Expand(const IntSize& size) { size_ += size; }
  void Expand(int dw, int dh) { size_.Expand(dw, dh); }
  void Expand(const IntRectOutsets& outsets) {
    location_.Move(-outsets.Left(), -outsets.Top());
    size_.Expand(outsets.Left() + outsets.Right(),
                 outsets.Top() + outsets.Bottom());
  }

  void Contract(const IntSize& size) { size_ -= size; }
  void Contract(int dw, int dh) { size_.Expand(-dw, -dh); }

  void ShiftXEdgeTo(int);
  void ShiftMaxXEdgeTo(int);
  void ShiftYEdgeTo(int);
  void ShiftMaxYEdgeTo(int);

  constexpr IntPoint MinXMinYCorner() const {
    return location_;
  }  // typically topLeft
  constexpr IntPoint MaxXMinYCorner() const {
    return IntPoint(location_.X() + size_.Width(), location_.Y());
  }  // typically topRight
  constexpr IntPoint MinXMaxYCorner() const {
    return IntPoint(location_.X(), location_.Y() + size_.Height());
  }  // typically bottomLeft
  constexpr IntPoint MaxXMaxYCorner() const {
    return IntPoint(location_.X() + size_.Width(),
                    location_.Y() + size_.Height());
  }  // typically bottomRight

  bool Intersects(const IntRect&) const;
  bool Contains(const IntRect&) const;

  // This checks to see if the rect contains x,y in the traditional sense.
  // Equivalent to checking if the rect contains a 1x1 rect below and to the
  // right of (px,py).
  bool Contains(int px, int py) const {
    return px >= X() && px < MaxX() && py >= Y() && py < MaxY();
  }
  bool Contains(const IntPoint& point) const {
    return Contains(point.X(), point.Y());
  }

  void Intersect(const IntRect&);
  void Unite(const IntRect&);
  void UniteIfNonZero(const IntRect&);

  // Besides non-empty rects, this method also unites empty rects (as points or
  // line segments).  For example, union of (100, 100, 0x0) and (200, 200, 50x0)
  // is (100, 100, 150x100).
  void UniteEvenIfEmpty(const IntRect&);

  void InflateX(int dx) {
    location_.SetX(location_.X() - dx);
    size_.SetWidth(size_.Width() + dx + dx);
  }
  void InflateY(int dy) {
    location_.SetY(location_.Y() - dy);
    size_.SetHeight(size_.Height() + dy + dy);
  }
  void Inflate(int d) {
    InflateX(d);
    InflateY(d);
  }
  void Scale(float s);

  IntSize DifferenceToPoint(const IntPoint&) const;
  int DistanceSquaredToPoint(const IntPoint& p) const {
    return DifferenceToPoint(p).DiagonalLengthSquared();
  }

  IntRect TransposedRect() const {
    return IntRect(location_.TransposedPoint(), size_.TransposedSize());
  }

#if defined(OS_MACOSX)
  explicit operator CGRect() const;
#endif

  operator SkRect() const { return SkRect::MakeLTRB(X(), Y(), MaxX(), MaxY()); }
  operator SkIRect() const {
    return SkIRect::MakeLTRB(X(), Y(), MaxX(), MaxY());
  }
  constexpr operator gfx::Rect() const {
    return gfx::Rect(X(), Y(), Width(), Height());
  }

  String ToString() const;

  // Return false if x + width or y + height overflows.
  bool IsValid() const;

 private:
  IntPoint location_;
  IntSize size_;
};

inline IntRect Intersection(const IntRect& a, const IntRect& b) {
  IntRect c = a;
  c.Intersect(b);
  return c;
}

inline IntRect UnionRect(const IntRect& a, const IntRect& b) {
  IntRect c = a;
  c.Unite(b);
  return c;
}

PLATFORM_EXPORT IntRect UnionRect(const Vector<IntRect>&);

inline IntRect UnionRectEvenIfEmpty(const IntRect& a, const IntRect& b) {
  IntRect c = a;
  c.UniteEvenIfEmpty(b);
  return c;
}

PLATFORM_EXPORT IntRect UnionRectEvenIfEmpty(const Vector<IntRect>&);

constexpr IntRect SaturatedRect(const IntRect& r) {
  return IntRect(r.X(), r.Y(), base::ClampAdd(r.X(), r.Width()) - r.X(),
                 base::ClampAdd(r.Y(), r.Height()) - r.Y());
}

constexpr bool operator==(const IntRect& a, const IntRect& b) {
  return a.Location() == b.Location() && a.Size() == b.Size();
}

constexpr bool operator!=(const IntRect& a, const IntRect& b) {
  return !(a == b);
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
