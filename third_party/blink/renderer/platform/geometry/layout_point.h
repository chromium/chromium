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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_POINT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_POINT_H_

#include <iosfwd>
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace blink {

class PLATFORM_EXPORT LayoutPoint {
  DISALLOW_NEW();

 public:
  constexpr LayoutPoint() = default;
  constexpr LayoutPoint(LayoutUnit x, LayoutUnit y) : x_(x), y_(y) {}
  constexpr LayoutPoint(int x, int y) : x_(LayoutUnit(x)), y_(LayoutUnit(y)) {}
  constexpr explicit LayoutPoint(const gfx::Point& point)
      : x_(point.x()), y_(point.y()) {}
  constexpr explicit LayoutPoint(const gfx::PointF& point)
      : x_(point.x()), y_(point.y()) {}
  constexpr explicit LayoutPoint(const LayoutSize& size)
      : x_(size.Width()), y_(size.Height()) {}

  constexpr explicit operator gfx::PointF() const {
    return gfx::PointF(x_.ToFloat(), y_.ToFloat());
  }

  // This is deleted to avoid unwanted lossy conversion from float or double to
  // LayoutUnit or int. Use explicit LayoutUnit constructor for each parameter
  // instead.
  LayoutPoint(double, double) = delete;

  static constexpr LayoutPoint Zero() { return LayoutPoint(); }

  constexpr LayoutUnit X() const { return x_; }
  constexpr LayoutUnit Y() const { return y_; }

  void SetX(LayoutUnit x) { x_ = x; }
  void SetY(LayoutUnit y) { y_ = y; }

  void Move(const LayoutSize& s) { Move(s.Width(), s.Height()); }
  void Move(const gfx::Vector2d& s) { Move(s.x(), s.y()); }
  void MoveBy(const LayoutPoint& offset) { Move(offset.X(), offset.Y()); }
  void Move(int dx, int dy) { Move(LayoutUnit(dx), LayoutUnit(dy)); }
  void Move(LayoutUnit dx, LayoutUnit dy) {
    x_ += dx;
    y_ += dy;
  }
  void Scale(float sx, float sy) {
    x_ *= sx;
    y_ *= sy;
  }

  LayoutPoint ExpandedTo(const LayoutPoint&) const;
  LayoutPoint ShrunkTo(const LayoutPoint&) const;

  void ClampNegativeToZero() { *this = ExpandedTo(Zero()); }

  LayoutPoint TransposedPoint() const { return LayoutPoint(y_, x_); }

  String ToString() const;

 private:
  LayoutUnit x_, y_;
};

ALWAYS_INLINE LayoutPoint& operator+=(LayoutPoint& a, const LayoutSize& b) {
  a.Move(b.Width(), b.Height());
  return a;
}

ALWAYS_INLINE LayoutPoint& operator+=(LayoutPoint& a, const LayoutPoint& b) {
  a.Move(b.X(), b.Y());
  return a;
}

inline LayoutPoint& operator+=(LayoutPoint& a, const gfx::Vector2d& b) {
  a.Move(b.x(), b.y());
  return a;
}

ALWAYS_INLINE LayoutPoint& operator-=(LayoutPoint& a, const LayoutPoint& b) {
  a.Move(-b.X(), -b.Y());
  return a;
}

ALWAYS_INLINE LayoutPoint& operator-=(LayoutPoint& a, const LayoutSize& b) {
  a.Move(-b.Width(), -b.Height());
  return a;
}

inline LayoutPoint& operator-=(LayoutPoint& a, const gfx::Vector2d& b) {
  a.Move(-b.x(), -b.y());
  return a;
}

inline LayoutPoint operator+(const LayoutPoint& a, const LayoutSize& b) {
  return LayoutPoint(a.X() + b.Width(), a.Y() + b.Height());
}

ALWAYS_INLINE LayoutPoint operator+(const LayoutPoint& a,
                                    const LayoutPoint& b) {
  return LayoutPoint(a.X() + b.X(), a.Y() + b.Y());
}

ALWAYS_INLINE LayoutSize operator-(const LayoutPoint& a, const LayoutPoint& b) {
  return LayoutSize(a.X() - b.X(), a.Y() - b.Y());
}

ALWAYS_INLINE LayoutSize operator-(const LayoutPoint& a, const gfx::Point& b) {
  return LayoutSize(a.X() - b.x(), a.Y() - b.y());
}

inline LayoutPoint operator-(const LayoutPoint& a, const LayoutSize& b) {
  return LayoutPoint(a.X() - b.Width(), a.Y() - b.Height());
}

inline LayoutPoint operator-(const LayoutPoint& a, const gfx::Vector2d& b) {
  return LayoutPoint(a.X() - b.x(), a.Y() - b.y());
}

inline LayoutPoint operator-(const LayoutPoint& point) {
  return LayoutPoint(-point.X(), -point.Y());
}

ALWAYS_INLINE constexpr bool operator==(const LayoutPoint& a,
                                        const LayoutPoint& b) {
  return a.X() == b.X() && a.Y() == b.Y();
}

constexpr bool operator!=(const LayoutPoint& a, const LayoutPoint& b) {
  return !(a == b);
}

constexpr LayoutPoint ToPoint(const LayoutSize& size) {
  return LayoutPoint(size.Width(), size.Height());
}

constexpr LayoutPoint ToLayoutPoint(const LayoutSize& p) {
  return LayoutPoint(p.Width(), p.Height());
}

constexpr LayoutSize ToSize(const LayoutPoint& a) {
  return LayoutSize(a.X(), a.Y());
}

inline gfx::Point ToFlooredPoint(const LayoutPoint& point) {
  return gfx::Point(point.X().Floor(), point.Y().Floor());
}

inline gfx::Point ToRoundedPoint(const LayoutPoint& point) {
  return gfx::Point(point.X().Round(), point.Y().Round());
}

inline gfx::Point ToCeiledPoint(const LayoutPoint& point) {
  return gfx::Point(point.X().Ceil(), point.Y().Ceil());
}

inline LayoutPoint FlooredLayoutPoint(const gfx::PointF& p) {
  return LayoutPoint(LayoutUnit::FromFloatFloor(p.x()),
                     LayoutUnit::FromFloatFloor(p.y()));
}

inline LayoutPoint CeiledLayoutPoint(const gfx::PointF& p) {
  return LayoutPoint(LayoutUnit::FromFloatCeil(p.x()),
                     LayoutUnit::FromFloatCeil(p.y()));
}

inline gfx::Size ToPixelSnappedSize(const LayoutSize& s, const LayoutPoint& p) {
  return gfx::Size(SnapSizeToPixel(s.Width(), p.X()),
                   SnapSizeToPixel(s.Height(), p.Y()));
}

inline gfx::Vector2d ToRoundedVector2d(const LayoutPoint& p) {
  return gfx::Vector2d(p.X().Round(), p.Y().Round());
}
inline gfx::Size ToRoundedSize(const LayoutPoint& p) {
  return gfx::Size(p.X().Round(), p.Y().Round());
}

inline LayoutSize ToLayoutSize(const LayoutPoint& p) {
  return LayoutSize(p.X(), p.Y());
}

inline LayoutPoint FlooredLayoutPoint(const gfx::SizeF& s) {
  return FlooredLayoutPoint(gfx::PointF(s.width(), s.height()));
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const LayoutPoint&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_POINT_H_
