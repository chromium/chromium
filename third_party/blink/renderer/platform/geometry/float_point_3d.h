/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>
                  2010 Zoltan Herczeg <zherczeg@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_POINT_3D_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_POINT_3D_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/skia/include/core/SkPoint3.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace blink {

class PLATFORM_EXPORT FloatPoint3D {
  DISALLOW_NEW();

 public:
  constexpr FloatPoint3D() : x_(0), y_(0), z_(0) {}

  constexpr FloatPoint3D(float x, float y, float z) : x_(x), y_(y), z_(z) {}

  constexpr FloatPoint3D(const gfx::PointF& p) : x_(p.x()), y_(p.y()), z_(0) {}

  constexpr FloatPoint3D(const FloatPoint3D&) = default;

  constexpr FloatPoint3D& operator=(const FloatPoint3D&) = default;

  FloatPoint3D(const gfx::Point3F&);

  constexpr float x() const { return x_; }
  void set_x(float x) { x_ = x; }

  constexpr float y() const { return y_; }
  void set_y(float y) { y_ = y; }

  constexpr float z() const { return z_; }
  void set_z(float z) { z_ = z; }
  void SetPoint(float x, float y, float z) {
    x_ = x;
    y_ = y;
    z_ = z;
  }
  void Offset(float dx, float dy, float dz) {
    x_ += dx;
    y_ += dy;
    z_ += dz;
  }
  void Scale(float sx, float sy, float sz) {
    x_ *= sx;
    y_ *= sy;
    z_ *= sz;
  }

  constexpr bool IsZero() const { return !x_ && !y_ && !z_; }

  void Normalize();

  float Dot(const FloatPoint3D& a) const {
    return x_ * a.x() + y_ * a.y() + z_ * a.z();
  }

  // Compute the angle (in radians) between this and y.  If either vector is the
  // zero vector, return an angle of 0.
  float AngleBetween(const FloatPoint3D& y) const;

  // Sets this FloatPoint3D to the cross product of the passed two.
  // It is safe for "this" to be the same as either or both of the
  // arguments.
  void Cross(const FloatPoint3D& a, const FloatPoint3D& b) {
    float x = a.y() * b.z() - a.z() * b.y();
    float y = a.z() * b.x() - a.x() * b.z();
    float z = a.x() * b.y() - a.y() * b.x();
    x_ = x;
    y_ = y;
    z_ = z;
  }

  // Convenience function returning "this cross point" as a
  // stack-allocated result.
  FloatPoint3D Cross(const FloatPoint3D& point) const {
    FloatPoint3D result;
    result.Cross(*this, point);
    return result;
  }

  float LengthSquared() const { return this->Dot(*this); }
  float length() const { return sqrtf(LengthSquared()); }

  float DistanceTo(const FloatPoint3D& a) const;

  operator SkPoint3() const { return SkPoint3::Make(x_, y_, z_); }

  // These are deleted during blink geometry type to gfx migration.
  // Use ToGfxPoint3F() and ToGfxVector3dF() instead.
  operator gfx::Point3F() const = delete;
  operator gfx::Vector3dF() const = delete;

  String ToString() const;

 private:
  float x_;
  float y_;
  float z_;
};

inline FloatPoint3D& operator+=(FloatPoint3D& a, const FloatPoint3D& b) {
  a.Offset(b.x(), b.y(), b.z());
  return a;
}

inline FloatPoint3D& operator-=(FloatPoint3D& a, const FloatPoint3D& b) {
  a.Offset(-b.x(), -b.y(), -b.z());
  return a;
}

constexpr FloatPoint3D operator+(const FloatPoint3D& a, const FloatPoint3D& b) {
  return FloatPoint3D(a.x() + b.x(), a.y() + b.y(), a.z() + b.z());
}

constexpr FloatPoint3D operator-(const FloatPoint3D& a, const FloatPoint3D& b) {
  return FloatPoint3D(a.x() - b.x(), a.y() - b.y(), a.z() - b.z());
}

constexpr bool operator==(const FloatPoint3D& a, const FloatPoint3D& b) {
  return a.x() == b.x() && a.y() == b.y() && a.z() == b.z();
}

constexpr bool operator!=(const FloatPoint3D& a, const FloatPoint3D& b) {
  return !(a == b);
}

inline float operator*(const FloatPoint3D& a, const FloatPoint3D& b) {
  // dot product
  return a.Dot(b);
}

inline FloatPoint3D operator*(float k, const FloatPoint3D& v) {
  return FloatPoint3D(k * v.x(), k * v.y(), k * v.z());
}

inline FloatPoint3D operator*(const FloatPoint3D& v, float k) {
  return FloatPoint3D(k * v.x(), k * v.y(), k * v.z());
}

inline float FloatPoint3D::DistanceTo(const FloatPoint3D& a) const {
  return (*this - a).length();
}

constexpr gfx::Point3F ToGfxPoint3F(const FloatPoint3D& p) {
  return gfx::Point3F(p.x(), p.y(), p.z());
}

constexpr gfx::Vector3dF ToGfxVector3dF(const FloatPoint3D& p) {
  return gfx::Vector3dF(p.x(), p.y(), p.z());
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const FloatPoint3D&);
WTF::TextStream& operator<<(WTF::TextStream&, const FloatPoint3D&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_POINT_3D_H_
