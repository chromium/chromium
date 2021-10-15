/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_INT_POINT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_INT_POINT_H_

#include "base/numerics/clamped_math.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

#if defined(OS_MAC)
typedef struct CGPoint CGPoint;

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif
#endif

namespace blink {

class PLATFORM_EXPORT IntPoint {
  USING_FAST_MALLOC(IntPoint);

 public:
  constexpr IntPoint() : x_(0), y_(0) {}
  constexpr IntPoint(int x, int y) : x_(x), y_(y) {}
  constexpr explicit IntPoint(const IntSize& size)
      : x_(size.width()), y_(size.height()) {}
  constexpr explicit IntPoint(const gfx::Point& p) : x_(p.x()), y_(p.y()) {}
  constexpr explicit IntPoint(const gfx::Vector2d& v) : x_(v.x()), y_(v.y()) {}

  static IntPoint Zero() { return IntPoint(); }

  constexpr int x() const { return x_; }
  constexpr int y() const { return y_; }

  void set_x(int x) { x_ = x; }
  void set_y(int y) { y_ = y; }

  void Offset(const IntSize& s) { Offset(s.width(), s.height()); }
  void MoveBy(const IntPoint& offset) { Offset(offset.x(), offset.y()); }
  void Offset(int dx, int dy) {
    x_ += dx;
    y_ += dy;
  }
  void SaturatedMove(int dx, int dy) {
    x_ = base::ClampAdd(x_, dx);
    y_ = base::ClampAdd(y_, dy);
  }

  void Scale(float sx, float sy) {
    x_ = static_cast<int>(lroundf(static_cast<float>(x_ * sx)));
    y_ = static_cast<int>(lroundf(static_cast<float>(y_ * sy)));
  }

  IntPoint ExpandedTo(const IntPoint& other) const {
    return IntPoint(x_ > other.x_ ? x_ : other.x_,
                    y_ > other.y_ ? y_ : other.y_);
  }

  IntPoint ShrunkTo(const IntPoint& other) const {
    return IntPoint(x_ < other.x_ ? x_ : other.x_,
                    y_ < other.y_ ? y_ : other.y_);
  }

  int DistanceSquaredToPoint(const IntPoint&) const;

  void ClampNegativeToZero() { *this = ExpandedTo(Zero()); }

  IntPoint TransposedPoint() const { return IntPoint(y_, x_); }

#if defined(OS_MAC)
  explicit IntPoint(
      const CGPoint&);  // don't do this implicitly since it's lossy
  operator CGPoint() const;
#endif

  // These are deleted during blink geometry type to gfx migration.
  // Use ToGfxPoint() and ToGfxVector2d() instead.
  operator gfx::Point() const = delete;
  operator gfx::Vector2d() const = delete;

  String ToString() const;

 private:
  int x_, y_;
};

inline IntPoint& operator+=(IntPoint& a, const IntSize& b) {
  a.Offset(b.width(), b.height());
  return a;
}

inline IntPoint& operator-=(IntPoint& a, const IntSize& b) {
  a.Offset(-b.width(), -b.height());
  return a;
}

constexpr IntPoint operator+(const IntPoint& a, const IntSize& b) {
  return IntPoint(a.x() + b.width(), a.y() + b.height());
}

constexpr IntPoint operator+(const IntPoint& a, const IntPoint& b) {
  return IntPoint(a.x() + b.x(), a.y() + b.y());
}

constexpr IntSize operator-(const IntPoint& a, const IntPoint& b) {
  return IntSize(a.x() - b.x(), a.y() - b.y());
}

constexpr IntPoint operator-(const IntPoint& a, const IntSize& b) {
  return IntPoint(a.x() - b.width(), a.y() - b.height());
}

constexpr IntPoint operator-(const IntPoint& point) {
  return IntPoint(-point.x(), -point.y());
}

constexpr bool operator==(const IntPoint& a, const IntPoint& b) {
  return a.x() == b.x() && a.y() == b.y();
}

constexpr bool operator!=(const IntPoint& a, const IntPoint& b) {
  return !(a == b);
}

inline IntSize ToIntSize(const IntPoint& a) {
  return IntSize(a.x(), a.y());
}

inline int IntPoint::DistanceSquaredToPoint(const IntPoint& point) const {
  return ((*this) - point).DiagonalLengthSquared();
}

constexpr gfx::Point ToGfxPoint(const IntPoint& p) {
  return gfx::Point(p.x(), p.y());
}

constexpr gfx::Vector2d ToGfxVector2d(const IntPoint& p) {
  return gfx::Vector2d(p.x(), p.y());
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const IntPoint&);
PLATFORM_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const IntPoint&);

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::IntPoint)

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_INT_POINT_H_
