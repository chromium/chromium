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

#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

AffineTransform::AffineTransform() {
  const Transform kIdentity = IDENTITY_TRANSFORM;
  SetMatrix(kIdentity);
}

AffineTransform::AffineTransform(double a,
                                 double b,
                                 double c,
                                 double d,
                                 double e,
                                 double f) {
  SetMatrix(a, b, c, d, e, f);
}

void AffineTransform::MakeIdentity() {
  SetMatrix(1, 0, 0, 1, 0, 0);
}

void AffineTransform::SetMatrix(double a,
                                double b,
                                double c,
                                double d,
                                double e,
                                double f) {
  transform_[0] = a;
  transform_[1] = b;
  transform_[2] = c;
  transform_[3] = d;
  transform_[4] = e;
  transform_[5] = f;
}

bool AffineTransform::IsIdentity() const {
  return (transform_[0] == 1 && transform_[1] == 0 && transform_[2] == 0 &&
          transform_[3] == 1 && transform_[4] == 0 && transform_[5] == 0);
}

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
  return Det() != 0.0;
}

AffineTransform AffineTransform::Inverse() const {
  double determinant = Det();
  AffineTransform result;

  if (determinant == 0.0)
    return result;

  if (IsIdentityOrTranslation()) {
    result.transform_[4] = -transform_[4];
    result.transform_[5] = -transform_[5];
    return result;
  }

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

// res = t1 * t2
inline void DoMultiply(const AffineTransform& t1,
                       const AffineTransform& t2,
                       AffineTransform* res) {
  res->SetA(t1.A() * t2.A() + t1.C() * t2.B());
  res->SetB(t1.B() * t2.A() + t1.D() * t2.B());
  res->SetC(t1.A() * t2.C() + t1.C() * t2.D());
  res->SetD(t1.B() * t2.C() + t1.D() * t2.D());
  res->SetE(t1.A() * t2.E() + t1.C() * t2.F() + t1.E());
  res->SetF(t1.B() * t2.E() + t1.D() * t2.F() + t1.F());
}

}  // anonymous namespace

AffineTransform& AffineTransform::Multiply(const AffineTransform& other) {
  if (other.IsIdentityOrTranslation()) {
    if (other.transform_[4] || other.transform_[5])
      Translate(other.transform_[4], other.transform_[5]);
    return *this;
  }

  AffineTransform trans;
  DoMultiply(*this, other, &trans);
  SetMatrix(trans.transform_);

  return *this;
}

AffineTransform& AffineTransform::PreMultiply(const AffineTransform& other) {
  if (other.IsIdentityOrTranslation()) {
    if (other.transform_[4] || other.transform_[5]) {
      transform_[4] += other.transform_[4];
      transform_[5] += other.transform_[5];
    }
    return *this;
  }

  AffineTransform trans;
  DoMultiply(other, *this, &trans);
  SetMatrix(trans.transform_);

  return *this;
}

AffineTransform& AffineTransform::Rotate(double a) {
  // angle is in degree. Switch to radian
  return RotateRadians(deg2rad(a));
}

AffineTransform& AffineTransform::RotateRadians(double a) {
  double cos_angle = cos(a);
  double sin_angle = sin(a);
  AffineTransform rot(cos_angle, sin_angle, -sin_angle, cos_angle, 0, 0);

  Multiply(rot);
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
  if (IsIdentityOrTranslation()) {
    transform_[4] += tx;
    transform_[5] += ty;
    return *this;
  }

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
  return Shear(tan(deg2rad(angle_x)), tan(deg2rad(angle_y)));
}

AffineTransform& AffineTransform::SkewX(double angle) {
  return Shear(tan(deg2rad(angle)), 0);
}

AffineTransform& AffineTransform::SkewY(double angle) {
  return Shear(0, tan(deg2rad(angle)));
}

void AffineTransform::Map(double x, double y, double& x2, double& y2) const {
  x2 = (transform_[0] * x + transform_[2] * y + transform_[4]);
  y2 = (transform_[1] * x + transform_[3] * y + transform_[5]);
}

IntPoint AffineTransform::MapPoint(const IntPoint& point) const {
  double x2, y2;
  Map(point.X(), point.Y(), x2, y2);

  // Round the point.
  return IntPoint(static_cast<int>(lround(x2)), static_cast<int>(lround(y2)));
}

FloatPoint AffineTransform::MapPoint(const FloatPoint& point) const {
  double x2, y2;
  Map(point.X(), point.Y(), x2, y2);

  return FloatPoint(clampTo<float>(x2), clampTo<float>(y2));
}

IntSize AffineTransform::MapSize(const IntSize& size) const {
  double width2 = size.Width() * XScale();
  double height2 = size.Height() * YScale();

  return IntSize(static_cast<int>(lround(width2)),
                 static_cast<int>(lround(height2)));
}

FloatSize AffineTransform::MapSize(const FloatSize& size) const {
  double width2 = size.Width() * XScale();
  double height2 = size.Height() * YScale();

  return FloatSize(clampTo<float>(width2), clampTo<float>(height2));
}

IntRect AffineTransform::MapRect(const IntRect& rect) const {
  return EnclosingIntRect(MapRect(FloatRect(rect)));
}

FloatRect AffineTransform::MapRect(const FloatRect& rect) const {
  if (IsIdentityOrTranslation()) {
    if (!transform_[4] && !transform_[5])
      return rect;

    FloatRect mapped_rect(rect);
    mapped_rect.Move(clampTo<float>(transform_[4]),
                     clampTo<float>(transform_[5]));
    return mapped_rect;
  }

  FloatQuad result;
  result.SetP1(MapPoint(rect.Location()));
  result.SetP2(MapPoint(FloatPoint(rect.MaxX(), rect.Y())));
  result.SetP3(MapPoint(FloatPoint(rect.MaxX(), rect.MaxY())));
  result.SetP4(MapPoint(FloatPoint(rect.X(), rect.MaxY())));
  return result.BoundingBox();
}

FloatQuad AffineTransform::MapQuad(const FloatQuad& q) const {
  if (IsIdentityOrTranslation()) {
    FloatQuad mapped_quad(q);
    mapped_quad.Move(clampTo<float>(transform_[4]),
                     clampTo<float>(transform_[5]));
    return mapped_quad;
  }

  FloatQuad result;
  result.SetP1(MapPoint(q.P1()));
  result.SetP2(MapPoint(q.P2()));
  result.SetP3(MapPoint(q.P3()));
  result.SetP4(MapPoint(q.P4()));
  return result;
}

TransformationMatrix AffineTransform::ToTransformationMatrix() const {
  return TransformationMatrix(transform_[0], transform_[1], transform_[2],
                              transform_[3], transform_[4], transform_[5]);
}

bool AffineTransform::Decompose(DecomposedType& decomp) const {
  AffineTransform m(*this);

  // Compute scaling factors
  double sx = XScale();
  double sy = YScale();

  // Compute cross product of transformed unit vectors. If negative,
  // one axis was flipped.
  if (m.A() * m.D() - m.C() * m.B() < 0) {
    // Flip axis with minimum unit vector dot product
    if (m.A() < m.D())
      sx = -sx;
    else
      sy = -sy;
  }

  // Remove scale from matrix
  m.Scale(1 / sx, 1 / sy);

  // Compute rotation
  double angle = atan2(m.B(), m.A());

  // Remove rotation from matrix
  m.RotateRadians(-angle);

  // Return results
  decomp.scale_x = sx;
  decomp.scale_y = sy;
  decomp.angle = angle;
  decomp.remainder_a = m.A();
  decomp.remainder_b = m.B();
  decomp.remainder_c = m.C();
  decomp.remainder_d = m.D();
  decomp.translate_x = m.E();
  decomp.translate_y = m.F();

  return true;
}

void AffineTransform::Recompose(const DecomposedType& decomp) {
  this->SetA(decomp.remainder_a);
  this->SetB(decomp.remainder_b);
  this->SetC(decomp.remainder_c);
  this->SetD(decomp.remainder_d);
  this->SetE(decomp.translate_x);
  this->SetF(decomp.translate_y);
  this->RotateRadians(decomp.angle);
  this->Scale(decomp.scale_x, decomp.scale_y);
}

String AffineTransform::ToString(bool as_matrix) const {
  if (as_matrix) {
    // Return as a matrix in row-major order.
    return String::Format("[%lg,%lg,%lg,\n%lg,%lg,%lg]", A(), C(), E(), B(),
                          D(), F());
  }

  if (IsIdentity())
    return "identity";

  AffineTransform::DecomposedType decomposition;
  Decompose(decomposition);

  if (IsIdentityOrTranslation())
    return String::Format("translation(%lg,%lg)", decomposition.translate_x,
                          decomposition.translate_y);

  return String::Format(
      "translation(%lg,%lg), scale(%lg,%lg), angle(%lgdeg), "
      "remainder(%lg,%lg,%lg,%lg)",
      decomposition.translate_x, decomposition.translate_y,
      decomposition.scale_x, decomposition.scale_y,
      rad2deg(decomposition.angle), decomposition.remainder_a,
      decomposition.remainder_b, decomposition.remainder_c,
      decomposition.remainder_d);
}

std::ostream& operator<<(std::ostream& ostream,
                         const AffineTransform& transform) {
  return ostream << transform.ToString();
}

}  // namespace blink
