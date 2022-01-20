
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_DOUBLE_POINT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_DOUBLE_POINT_H_

#include <iosfwd>
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

class PLATFORM_EXPORT DoublePoint {
  DISALLOW_NEW();

 public:
  constexpr DoublePoint() = default;
  constexpr DoublePoint(double x, double y) : x_(x), y_(y) {}
  constexpr DoublePoint(const gfx::Point& p) : x_(p.x()), y_(p.y()) {}
  constexpr DoublePoint(const gfx::PointF& p) : x_(p.x()), y_(p.y()) {}
  // We also have conversion operator to DoublePoint defined in LayoutPoint.

  explicit operator gfx::PointF() const;

  constexpr double X() const { return x_; }
  constexpr double Y() const { return y_; }
  void SetX(double x) { x_ = x; }
  void SetY(double y) { y_ = y; }

  void Move(double x, double y) {
    x_ += x;
    y_ += y;
  }

  void Scale(float sx, float sy) {
    x_ *= sx;
    y_ *= sy;
  }

  DoublePoint ScaledBy(float scale) const {
    return DoublePoint(x_ * scale, y_ * scale);
  }

  String ToString() const;

 private:
  double x_ = 0;
  double y_ = 0;
};

constexpr bool operator==(const DoublePoint& a, const DoublePoint& b) {
  return a.X() == b.X() && a.Y() == b.Y();
}

constexpr bool operator!=(const DoublePoint& a, const DoublePoint& b) {
  return !(a == b);
}

constexpr DoublePoint operator-(const DoublePoint& a) {
  return DoublePoint(-a.X(), -a.Y());
}

inline gfx::Point ToRoundedPoint(const DoublePoint& p) {
  return gfx::Point(ClampTo<int>(round(p.X())), ClampTo<int>(round(p.Y())));
}

inline gfx::Point ToCeiledPoint(const DoublePoint& p) {
  return gfx::Point(ClampTo<int>(ceil(p.X())), ClampTo<int>(ceil(p.Y())));
}

inline gfx::Point ToFlooredPoint(const DoublePoint& p) {
  return gfx::Point(ClampTo<int>(floor(p.X())), ClampTo<int>(floor(p.Y())));
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const DoublePoint&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_DOUBLE_POINT_H_
