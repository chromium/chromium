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

#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/numerics/clamped_math.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_rect_outsets.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/rect_f.h"

#if defined(OS_MAC)
typedef struct CGRect CGRect;

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif
#endif

namespace blink {

class PLATFORM_EXPORT FloatRect {
  DISALLOW_NEW();

 public:
  constexpr FloatRect() = default;
  constexpr FloatRect(const FloatPoint& location, const FloatSize& size)
      : location_(location), size_(size) {}
  constexpr FloatRect(float x, float y, float width, float height)
      : location_(FloatPoint(x, y)), size_(FloatSize(width, height)) {}
  constexpr explicit FloatRect(const IntRect& r)
      : FloatRect(r.x(), r.y(), r.width(), r.height()) {}
  constexpr explicit FloatRect(const gfx::RectF& r)
      : FloatRect(r.x(), r.y(), r.width(), r.height()) {}
  FloatRect(const SkRect& r) : FloatRect(r.x(), r.y(), r.width(), r.height()) {}
  // We also have conversion operator to FloatRect defined in LayoutRect.

  static FloatRect NarrowPrecision(double x,
                                   double y,
                                   double width,
                                   double height);

  constexpr FloatPoint origin() const { return location_; }
  constexpr FloatSize size() const { return size_; }

  void set_origin(const FloatPoint& location) { location_ = location; }
  void set_size(const FloatSize& size) { size_ = size; }

  constexpr float x() const { return location_.x(); }
  constexpr float y() const { return location_.y(); }
  constexpr float right() const { return x() + width(); }
  constexpr float bottom() const { return y() + height(); }
  constexpr float width() const { return size_.width(); }
  constexpr float height() const { return size_.height(); }

  void set_x(float x) { location_.set_x(x); }
  void set_y(float y) { location_.set_y(y); }
  void set_width(float width) { size_.set_width(width); }
  void set_height(float height) { size_.set_height(height); }

  constexpr bool IsEmpty() const { return size_.IsEmpty(); }
  constexpr bool IsZero() const { return size_.IsZero(); }
  // True if no member is infinite or NaN.
  bool IsFinite() const;
  bool IsExpressibleAsIntRect() const;

  FloatPoint CenterPoint() const {
    return FloatPoint(x() + width() / 2, y() + height() / 2);
  }

  void Offset(const FloatSize& delta) { location_ += delta; }
  void MoveBy(const FloatPoint& delta) {
    location_.Offset(delta.x(), delta.y());
  }
  void Offset(float dx, float dy) { location_.Offset(dx, dy); }

  void Expand(const FloatSize& size) { size_ += size; }
  void Expand(float dw, float dh) { size_.Enlarge(dw, dh); }
  void Expand(const FloatRectOutsets& outsets) {
    location_.Offset(-outsets.Left(), -outsets.Top());
    size_.Enlarge(outsets.Left() + outsets.Right(),
                  outsets.Top() + outsets.Bottom());
  }

  void Contract(const FloatSize& size) { size_ -= size; }
  void Contract(float dw, float dh) { size_.Enlarge(-dw, -dh); }

  void ShiftXEdgeTo(float);
  void ShiftMaxXEdgeTo(float);
  void ShiftYEdgeTo(float);
  void ShiftMaxYEdgeTo(float);

  FloatPoint top_right() const {
    return FloatPoint(location_.x() + size_.width(), location_.y());
  }  // typically topRight
  FloatPoint bottom_left() const {
    return FloatPoint(location_.x(), location_.y() + size_.height());
  }  // typically bottomLeft
  FloatPoint bottom_right() const {
    return FloatPoint(location_.x() + size_.width(),
                      location_.y() + size_.height());
  }  // typically bottomRight

  WARN_UNUSED_RESULT bool Intersects(const IntRect&) const;
  WARN_UNUSED_RESULT bool Intersects(const FloatRect&) const;
  bool Contains(const IntRect&) const;
  bool Contains(const FloatRect&) const;

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
  void Union(const FloatRect&);
  void UnionEvenIfEmpty(const FloatRect&);
  void UnionIfNonZero(const FloatRect&);
  void Extend(const FloatPoint&);

  // Returns true if |p| is in the rect or is on any of the edges of the rect.
  bool InclusiveContains(const FloatPoint& p) const {
    return p.x() >= x() && p.x() <= right() && p.y() >= y() &&
           p.y() <= bottom();
  }

  void OutsetX(float dx) {
    location_.set_x(location_.x() - dx);
    size_.set_width(size_.width() + dx + dx);
  }
  void OutsetY(float dy) {
    location_.set_y(location_.y() - dy);
    size_.set_height(size_.height() + dy + dy);
  }
  void Outset(float d) {
    OutsetX(d);
    OutsetY(d);
  }
  void Scale(float s) { Scale(s, s); }
  void Scale(float sx, float sy);

  FloatRect TransposedRect() const {
    return FloatRect(location_.TransposedPoint(), size_.TransposedSize());
  }

  float SquaredDistanceTo(const FloatPoint&) const;

#if defined(OS_MAC)
  FloatRect(const CGRect&);
  operator CGRect() const;
#endif

  operator SkRect() const {
    return SkRect::MakeXYWH(x(), y(), width(), height());
  }

  // This is deleted during blink geometry type to gfx migration.
  // Use ToGfxRectF() instead.
  operator gfx::RectF() const = delete;

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
    location_.SetPoint(left, top);
    size_.set_width(right - left);
    size_.set_height(bottom - top);
  }
};

inline FloatRect IntersectRects(const FloatRect& a, const FloatRect& b) {
  FloatRect c = a;
  c.Intersect(b);
  return c;
}

inline FloatRect UnionRects(const FloatRect& a, const FloatRect& b) {
  FloatRect c = a;
  c.Union(b);
  return c;
}

PLATFORM_EXPORT FloatRect UnionRects(const Vector<FloatRect>&);

// Return a maximum rectangle in which any point is covered by either a or b.
PLATFORM_EXPORT FloatRect MaximumCoveredRect(const FloatRect& a,
                                             const FloatRect& b);

inline FloatRect& operator+=(FloatRect& a, const FloatRect& b) {
  a.Offset(b.x(), b.y());
  a.set_width(a.width() + b.width());
  a.set_height(a.height() + b.height());
  return a;
}

constexpr FloatRect operator+(const FloatRect& a, const FloatRect& b) {
  return FloatRect(a.origin() + b.origin(), a.size() + b.size());
}

constexpr bool operator==(const FloatRect& a, const FloatRect& b) {
  return a.origin() == b.origin() && a.size() == b.size();
}

constexpr bool operator!=(const FloatRect& a, const FloatRect& b) {
  return !(a == b);
}

// Returns a IntRect containing the given FloatRect.
inline IntRect EnclosingIntRect(const FloatRect& rect) {
  IntPoint location = FlooredIntPoint(rect.origin());
  IntPoint max_point = CeiledIntPoint(rect.bottom_right());
  return IntRect(location,
                 IntSize(base::ClampSub(max_point.x(), location.x()),
                         base::ClampSub(max_point.y(), location.y())));
}

// Returns a valid IntRect contained within the given FloatRect.
PLATFORM_EXPORT IntRect EnclosedIntRect(const FloatRect&);

PLATFORM_EXPORT IntRect RoundedIntRect(const FloatRect&);

// Map supplied rect from srcRect to an equivalent rect in destRect.
PLATFORM_EXPORT FloatRect MapRect(const FloatRect&,
                                  const FloatRect& src_rect,
                                  const FloatRect& dest_rect);

constexpr gfx::RectF ToGfxRectF(const FloatRect& r) {
  return gfx::RectF(r.x(), r.y(), r.width(), r.height());
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const FloatRect&);
PLATFORM_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const FloatRect&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_RECT_H_
