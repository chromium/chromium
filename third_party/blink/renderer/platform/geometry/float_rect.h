/*
 * Copyright (C) 2003, 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2005 Nokia.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_RECT_H_

#include <iosfwd>

#include "base/numerics/clamped_math.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_rect_outsets.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/rect_f.h"

#if defined(OS_MACOSX)
typedef struct CGRect CGRect;

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif
#endif

namespace blink {

class PLATFORM_EXPORT FloatRect {
  DISALLOW_NEW();

 public:
  enum ContainsMode { kInsideOrOnStroke, kInsideButNotOnStroke };

  constexpr FloatRect() = default;
  constexpr FloatRect(const FloatPoint& location, const FloatSize& size)
      : location_(location), size_(size) {}
  constexpr FloatRect(float x, float y, float width, float height)
      : location_(FloatPoint(x, y)), size_(FloatSize(width, height)) {}
  constexpr explicit FloatRect(const IntRect& r)
      : FloatRect(r.X(), r.Y(), r.Width(), r.Height()) {}
  constexpr explicit FloatRect(const gfx::RectF& r)
      : FloatRect(r.x(), r.y(), r.width(), r.height()) {}
  FloatRect(const SkRect& r) : FloatRect(r.x(), r.y(), r.width(), r.height()) {}
  // We also have conversion operator to FloatRect defined in LayoutRect.

  static FloatRect NarrowPrecision(double x,
                                   double y,
                                   double width,
                                   double height);

  constexpr FloatPoint Location() const { return location_; }
  constexpr FloatSize Size() const { return size_; }

  void SetLocation(const FloatPoint& location) { location_ = location; }
  void SetSize(const FloatSize& size) { size_ = size; }

  constexpr float X() const { return location_.X(); }
  constexpr float Y() const { return location_.Y(); }
  constexpr float MaxX() const { return X() + Width(); }
  constexpr float MaxY() const { return Y() + Height(); }
  constexpr float Width() const { return size_.Width(); }
  constexpr float Height() const { return size_.Height(); }

  void SetX(float x) { location_.SetX(x); }
  void SetY(float y) { location_.SetY(y); }
  void SetWidth(float width) { size_.SetWidth(width); }
  void SetHeight(float height) { size_.SetHeight(height); }

  constexpr bool IsEmpty() const { return size_.IsEmpty(); }
  constexpr bool IsZero() const { return size_.IsZero(); }
  bool IsExpressibleAsIntRect() const;

  FloatPoint Center() const {
    return FloatPoint(X() + Width() / 2, Y() + Height() / 2);
  }

  void Move(const FloatSize& delta) { location_ += delta; }
  void MoveBy(const FloatPoint& delta) { location_.Move(delta.X(), delta.Y()); }
  void Move(float dx, float dy) { location_.Move(dx, dy); }

  void Expand(const FloatSize& size) { size_ += size; }
  void Expand(float dw, float dh) { size_.Expand(dw, dh); }
  void Expand(const FloatRectOutsets& outsets) {
    location_.Move(-outsets.Left(), -outsets.Top());
    size_.Expand(outsets.Left() + outsets.Right(),
                 outsets.Top() + outsets.Bottom());
  }

  void Contract(const FloatSize& size) { size_ -= size; }
  void Contract(float dw, float dh) { size_.Expand(-dw, -dh); }

  void ShiftXEdgeTo(float);
  void ShiftMaxXEdgeTo(float);
  void ShiftYEdgeTo(float);
  void ShiftMaxYEdgeTo(float);

  FloatPoint MinXMinYCorner() const { return location_; }  // typically topLeft
  FloatPoint MaxXMinYCorner() const {
    return FloatPoint(location_.X() + size_.Width(), location_.Y());
  }  // typically topRight
  FloatPoint MinXMaxYCorner() const {
    return FloatPoint(location_.X(), location_.Y() + size_.Height());
  }  // typically bottomLeft
  FloatPoint MaxXMaxYCorner() const {
    return FloatPoint(location_.X() + size_.Width(),
                      location_.Y() + size_.Height());
  }  // typically bottomRight

  bool Intersects(const IntRect&) const;
  bool Intersects(const FloatRect&) const;
  bool Contains(const IntRect&) const;
  bool Contains(const FloatRect&) const;
  bool Contains(const FloatPoint&, ContainsMode = kInsideOrOnStroke) const;

  void Intersect(const IntRect&);
  void Intersect(const FloatRect&);
  // Set this rect to be the intersection of itself and the argument rect
  // using edge-inclusive geometry. If the two rectangles overlap but the
  // overlap region is zero-area (either because one of the two rectangles
  // is zero-area, or because the rectangles overlap at an edge or a corner),
  // the result is the zero-area intersection. The return value indicates
  // whether the two rectangle actually have an intersection, since checking
  // the result for isEmpty() is not conclusive.
  bool InclusiveIntersect(const FloatRect&);
  void Unite(const FloatRect&);
  void UniteEvenIfEmpty(const FloatRect&);
  void UniteIfNonZero(const FloatRect&);
  void Extend(const FloatPoint&);

  // Note, this doesn't match what IntRect::contains(IntPoint&) does; the int
  // version is really checking for containment of 1x1 rect, but that doesn't
  // make sense with floats.
  bool Contains(float px, float py) const {
    return px >= X() && px <= MaxX() && py >= Y() && py <= MaxY();
  }

  void InflateX(float dx) {
    location_.SetX(location_.X() - dx);
    size_.SetWidth(size_.Width() + dx + dx);
  }
  void InflateY(float dy) {
    location_.SetY(location_.Y() - dy);
    size_.SetHeight(size_.Height() + dy + dy);
  }
  void Inflate(float d) {
    InflateX(d);
    InflateY(d);
  }
  void Scale(float s) { Scale(s, s); }
  void Scale(float sx, float sy);

  FloatRect TransposedRect() const {
    return FloatRect(location_.TransposedPoint(), size_.TransposedSize());
  }

  float SquaredDistanceTo(const FloatPoint&) const;

#if defined(OS_MACOSX)
  FloatRect(const CGRect&);
  operator CGRect() const;
#endif

  operator SkRect() const {
    return SkRect::MakeXYWH(X(), Y(), Width(), Height());
  }
  constexpr operator gfx::RectF() const {
    return gfx::RectF(X(), Y(), Width(), Height());
  }

#if DCHECK_IS_ON()
  bool MayNotHaveExactIntRectRepresentation() const;
  bool EqualWithinEpsilon(const FloatRect& other, float epsilon) const;
#endif

  String ToString() const;

 private:
  FloatPoint location_;
  FloatSize size_;

  void SetLocationAndSizeFromEdges(float left,
                                   float top,
                                   float right,
                                   float bottom) {
    location_.Set(left, top);
    size_.SetWidth(right - left);
    size_.SetHeight(bottom - top);
  }
};

inline FloatRect Intersection(const FloatRect& a, const FloatRect& b) {
  FloatRect c = a;
  c.Intersect(b);
  return c;
}

inline FloatRect UnionRect(const FloatRect& a, const FloatRect& b) {
  FloatRect c = a;
  c.Unite(b);
  return c;
}

PLATFORM_EXPORT FloatRect UnionRect(const Vector<FloatRect>&);

inline FloatRect& operator+=(FloatRect& a, const FloatRect& b) {
  a.Move(b.X(), b.Y());
  a.SetWidth(a.Width() + b.Width());
  a.SetHeight(a.Height() + b.Height());
  return a;
}

constexpr FloatRect operator+(const FloatRect& a, const FloatRect& b) {
  return FloatRect(a.Location() + b.Location(), a.Size() + b.Size());
}

constexpr bool operator==(const FloatRect& a, const FloatRect& b) {
  return a.Location() == b.Location() && a.Size() == b.Size();
}

constexpr bool operator!=(const FloatRect& a, const FloatRect& b) {
  return !(a == b);
}

// Returns a IntRect containing the given FloatRect.
inline IntRect EnclosingIntRect(const FloatRect& rect) {
  IntPoint location = FlooredIntPoint(rect.Location());
  IntPoint max_point = CeiledIntPoint(rect.MaxXMaxYCorner());
  return IntRect(location,
                 IntSize(base::ClampSub(max_point.X(), location.X()),
                         base::ClampSub(max_point.Y(), location.Y())));
}

// Returns a valid IntRect contained within the given FloatRect.
PLATFORM_EXPORT IntRect EnclosedIntRect(const FloatRect&);

PLATFORM_EXPORT IntRect RoundedIntRect(const FloatRect&);

// Map supplied rect from srcRect to an equivalent rect in destRect.
PLATFORM_EXPORT FloatRect MapRect(const FloatRect&,
                                  const FloatRect& src_rect,
                                  const FloatRect& dest_rect);

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const FloatRect&);
PLATFORM_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const FloatRect&);

}  // namespace blink

#endif
