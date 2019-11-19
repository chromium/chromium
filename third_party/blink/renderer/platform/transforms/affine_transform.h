/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 *               2010 Dirk Schulze <krit@webkit.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_AFFINE_TRANSFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_AFFINE_TRANSFORM_H_

#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

#include <string.h>  // for memcpy
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FloatPoint;
class FloatQuad;
class FloatRect;
class IntPoint;
class IntRect;
class TransformationMatrix;

#define IDENTITY_TRANSFORM \
  { 1, 0, 0, 1, 0, 0 }

class PLATFORM_EXPORT AffineTransform {
  DISALLOW_NEW();

 public:
  typedef double Transform[6];

  AffineTransform();
  AffineTransform(double a, double b, double c, double d, double e, double f);
  AffineTransform(const Transform transform) { SetMatrix(transform); }

  void SetMatrix(double a, double b, double c, double d, double e, double f);

  void SetTransform(const AffineTransform& other) {
    SetMatrix(other.transform_);
  }

  void Map(double x, double y, double& x2, double& y2) const;

  // Rounds the mapped point to the nearest integer value.
  IntPoint MapPoint(const IntPoint&) const;

  FloatPoint MapPoint(const FloatPoint&) const;

  IntSize MapSize(const IntSize&) const;

  FloatSize MapSize(const FloatSize&) const;

  // Rounds the resulting mapped rectangle out. This is helpful for bounding
  // box computations but may not be what is wanted in other contexts.
  IntRect MapRect(const IntRect&) const;

  FloatRect MapRect(const FloatRect&) const;
  FloatQuad MapQuad(const FloatQuad&) const;

  bool IsIdentity() const;

  double A() const { return transform_[0]; }
  void SetA(double a) { transform_[0] = a; }
  double B() const { return transform_[1]; }
  void SetB(double b) { transform_[1] = b; }
  double C() const { return transform_[2]; }
  void SetC(double c) { transform_[2] = c; }
  double D() const { return transform_[3]; }
  void SetD(double d) { transform_[3] = d; }
  double E() const { return transform_[4]; }
  void SetE(double e) { transform_[4] = e; }
  double F() const { return transform_[5]; }
  void SetF(double f) { transform_[5] = f; }

  void MakeIdentity();

  // this' = this * other
  AffineTransform& Multiply(const AffineTransform& other);
  // this' = other * this
  AffineTransform& PreMultiply(const AffineTransform& other);

  AffineTransform& Scale(double);
  AffineTransform& Scale(double sx, double sy);
  AffineTransform& ScaleNonUniform(double sx, double sy);
  AffineTransform& Rotate(double a);
  AffineTransform& RotateRadians(double a);
  AffineTransform& RotateFromVector(double x, double y);
  AffineTransform& Translate(double tx, double ty);
  AffineTransform& Shear(double sx, double sy);
  AffineTransform& FlipX();
  AffineTransform& FlipY();
  AffineTransform& Skew(double angle_x, double angle_y);
  AffineTransform& SkewX(double angle);
  AffineTransform& SkewY(double angle);

  double XScaleSquared() const;
  double XScale() const;
  double YScaleSquared() const;
  double YScale() const;

  double Det() const;
  bool IsInvertible() const;
  AffineTransform Inverse() const;

  TransformationMatrix ToTransformationMatrix() const;

  bool IsIdentityOrTranslation() const {
    return transform_[0] == 1 && transform_[1] == 0 && transform_[2] == 0 &&
           transform_[3] == 1;
  }

  bool IsIdentityOrTranslationOrFlipped() const {
    return transform_[0] == 1 && transform_[1] == 0 && transform_[2] == 0 &&
           (transform_[3] == 1 || transform_[3] == -1);
  }

  bool PreservesAxisAlignment() const {
    return (transform_[1] == 0 && transform_[2] == 0) ||
           (transform_[0] == 0 && transform_[3] == 0);
  }

  bool operator==(const AffineTransform& m2) const {
    return (transform_[0] == m2.transform_[0] &&
            transform_[1] == m2.transform_[1] &&
            transform_[2] == m2.transform_[2] &&
            transform_[3] == m2.transform_[3] &&
            transform_[4] == m2.transform_[4] &&
            transform_[5] == m2.transform_[5]);
  }

  bool operator!=(const AffineTransform& other) const {
    return !(*this == other);
  }

  // *this = *this * t (i.e., a multRight)
  AffineTransform& operator*=(const AffineTransform& t) { return Multiply(t); }

  // result = *this * t (i.e., a multRight)
  AffineTransform operator*(const AffineTransform& t) const {
    AffineTransform result = *this;
    result *= t;
    return result;
  }

  static AffineTransform Translation(double x, double y) {
    return AffineTransform(1, 0, 0, 1, x, y);
  }

  // decompose the matrix into its component parts
  typedef struct {
    double scale_x, scale_y;
    double angle;
    double remainder_a, remainder_b, remainder_c, remainder_d;
    double translate_x, translate_y;
  } DecomposedType;

  bool Decompose(DecomposedType&) const;
  void Recompose(const DecomposedType&);

  void CopyTransformTo(Transform m) {
    memcpy(m, transform_, sizeof(Transform));
  }

  // If |asMatrix| is true, the transform is returned as a matrix in row-major
  // order. Otherwise, the transform's decomposition is returned which shows
  // the translation, scale, etc.
  String ToString(bool as_matrix = false) const;

 private:
  void SetMatrix(const Transform m) {
    if (m && m != transform_)
      memcpy(transform_, m, sizeof(Transform));
  }

  Transform transform_;
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const AffineTransform&);

}  // namespace blink

#endif
