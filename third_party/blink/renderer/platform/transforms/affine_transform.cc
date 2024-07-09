/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 *               2010 Dirk Schulze <krit@webkit.org>
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

double AffineTransform::XScaleSquared() const {
  return transform_[0] * transform_[0] + transform_[1] * transform_[1];
}

double AffineTransform::XScale() const {
  return sqrt(XScaleSquared());
}

double AffineTransform::YScaleSquared() const {
  return transform_[2] * transform_[2] + transform_[3] * transform_[3];
}

double AffineTransform::YScale() const {
  return sqrt(YScaleSquared());
}

double AffineTransform::Det() const {
  return transform_[0] * transform_[3] - transform_[1] * transform_[2];
}

bool AffineTransform::IsInvertible() const {
  return std::isnormal(Det());
}

AffineTransform AffineTransform::Inverse() const {
  AffineTransform result;
  if (IsIdentityOrTranslation()) {
    result.transform_[4] = -transform_[4];
    result.transform_[5] = -transform_[5];
    return result;
  }

  double determinant = Det();
  if (!std::isnormal(determinant))
    return result;

  result.transform_[0] = transform_[3] / determinant;
  result.transform_[1] = -transform_[1] / determinant;
  result.transform_[2] = -transform_[2] / determinant;
  result.transform_[3] = transform_[0] / determinant;
  result.transform_[4] =
      (transform_[2] * transform_[5] - transform_[3] * transform_[4]) /
      determinant;
  result.transform_[5] =
      (transform_[1] * transform_[4] - transform_[0] * transform_[5]) /
      determinant;

  return result;
}

namespace {

inline AffineTransform DoMultiply(const AffineTransform& t1,
                                  const AffineTransform& t2) {
  if (t1.IsIdentityOrTranslation()) {
    return AffineTransform(t2.A(), t2.B(), t2.C(), t2.D(), t1.E() + t2.E(),
                           t1.F() + t2.F());
  }

  return AffineTransform(
      t1.A() * t2.A() + t1.C() * t2.B(), t1.B() * t2.A() + t1.D() * t2.B(),
      t1.A() * t2.C() + t1.C() * t2.D(), t1.B() * t2.C() + t1.D() * t2.D(),
      t1.A() * t2.E() + t1.C() * t2.F() + t1.E(),
      t1.B() * t2.E() + t1.D() * t2.F() + t1.F());
}

}  // anonymous namespace

AffineTransform& AffineTransform::PreConcat(const AffineTransform& other) {
  *this = DoMultiply(*this, other);
  return *this;
}

AffineTransform& AffineTransform::PostConcat(const AffineTransform& other) {
  *this = DoMultiply(other, *this);
  return *this;
}

AffineTransform& AffineTransform::Rotate(double a) {
  // angle is in degree. Switch to radian
  return RotateRadians(Deg2rad(a));
}

AffineTransform& AffineTransform::RotateRadians(double a) {
  double cos_angle = cos(a);
  double sin_angle = sin(a);
  AffineTransform rot(cos_angle, sin_angle, -sin_angle, cos_angle, 0, 0);

  PreConcat(rot);
  return *this;
}

AffineTransform& AffineTransform::Scale(double s) {
  return Scale(s, s);
}

AffineTransform& AffineTransform::Scale(double sx, double sy) {
  transform_[0] *= sx;
  transform_[1] *= sx;
  transform_[2] *= sy;
  transform_[3] *= sy;
  return *this;
}

// *this = *this * translation
AffineTransform& AffineTransform::Translate(double tx, double ty) {
  transform_[4] += tx * transform_[0] + ty * transform_[2];
  transform_[5] += tx * transform_[1] + ty * transform_[3];
  return *this;
}

AffineTransform& AffineTransform::ScaleNonUniform(double sx, double sy) {
  return Scale(sx, sy);
}

AffineTransform& AffineTransform::RotateFromVector(double x, double y) {
  return RotateRadians(atan2(y, x));
}

AffineTransform& AffineTransform::FlipX() {
  return Scale(-1, 1);
}

AffineTransform& AffineTransform::FlipY() {
  return Scale(1, -1);
}

AffineTransform& AffineTransform::Shear(double sx, double sy) {
  double a = transform_[0];
  double b = transform_[1];

  transform_[0] += sy * transform_[2];
  transform_[1] += sy * transform_[3];
  transform_[2] += sx * a;
  transform_[3] += sx * b;

  return *this;
}

AffineTransform& AffineTransform::Skew(double angle_x, double angle_y) {
  return Shear(tan(Deg2rad(angle_x)), tan(Deg2rad(angle_y)));
}

AffineTransform& AffineTransform::SkewX(double angle) {
  return Shear(tan(Deg2rad(angle)), 0);
}

AffineTransform& AffineTransform::SkewY(double angle) {
  return Shear(0, tan(Deg2rad(angle)));
}

gfx::PointF AffineTransform::MapPoint(const gfx::PointF& point) const {
  return gfx::PointF(ClampToFloat(transform_[0] * point.x() +
                                  transform_[2] * point.y() + transform_[4]),
                     ClampToFloat(transform_[1] * point.x() +
                                  transform_[3] * point.y() + transform_[5]));
}

gfx::Rect AffineTransform::MapRect(const gfx::Rect& rect) const {
  return gfx::ToEnclosingRect(MapRect(gfx::RectF(rect)));
}

gfx::RectF AffineTransform::MapRect(const gfx::RectF& rect) const {
  auto result = IsIdentityOrTranslation()
                    ? gfx::RectF(MapPoint(rect.origin()), rect.size())
                    : MapQuad(gfx::QuadF(rect)).BoundingBox();
  // result.width()/height() may be infinity if e.g. right - left > float_max.
  DCHECK(std::isfinite(result.x()));
  DCHECK(std::isfinite(result.y()));
  result.set_width(ClampToFloat(result.width()));
  result.set_height(ClampToFloat(result.height()));
  return result;
}

gfx::QuadF AffineTransform::MapQuad(const gfx::QuadF& q) const {
  return gfx::QuadF(MapPoint(q.p1()), MapPoint(q.p2()), MapPoint(q.p3()),
                    MapPoint(q.p4()));
}

// static
AffineTransform AffineTransform::FromTransform(const gfx::Transform& t) {
  return AffineTransform(t.rc(0, 0), t.rc(1, 0), t.rc(0, 1), t.rc(1, 1),
                         t.rc(0, 3), t.rc(1, 3));
}

gfx::Transform AffineTransform::ToTransform() const {
  return gfx::Transform::Affine(A(), B(), C(), D(), E(), F());
}

AffineTransform& AffineTransform::Zoom(double zoom_factor) {
  transform_[4] *= zoom_factor;
  transform_[5] *= zoom_factor;
  return *this;
}

String AffineTransform::ToString(bool as_matrix) const {
  if (as_matrix) {
    // Return as a matrix in row-major order.
    return String::Format("[%lg,%lg,%lg,\n%lg,%lg,%lg]", A(), C(), E(), B(),
                          D(), F());
  }

  if (IsIdentity())
    return "identity";

  std::optional<gfx::DecomposedTransform> decomp = ToTransform().Decompose();
  if (!decomp)
    return ToString(true) + " (degenerate)";

  if (IsIdentityOrTranslation()) {
    return String::Format("translation(%lg,%lg)", decomp->translate[0],
                          decomp->translate[1]);
  }

  double angle = Rad2deg(std::asin(decomp->quaternion.z())) * 2;
  return String::Format(
      "translation(%lg,%lg), scale(%lg,%lg), angle(%lgdeg), skewxy(%lg)",
      decomp->translate[0], decomp->translate[1], decomp->scale[0],
      decomp->scale[1], angle, decomp->skew[0]);
}

std::ostream& operator<<(std::ostream& ostream,
                         const AffineTransform& transform) {
  return ostream << transform.ToString();
}

}  // namespace blink
