/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_TRANSFORMATION_MATRIX_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_TRANSFORMATION_MATRIX_H_

#include <string.h>  // for memcpy

#include <cmath>
#include <limits>
#include <memory>

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/skia/include/core/SkMatrix44.h"

namespace gfx {
class Transform;
}

namespace blink {

class AffineTransform;
class IntRect;
class LayoutRect;
class FloatRect;
class FloatQuad;
class FloatBox;
class JSONArray;
struct Rotation;

class PLATFORM_EXPORT TransformationMatrix {
  // TransformationMatrix must not be allocated on Oilpan's heap since
  // Oilpan doesn't (yet) have an ability to allocate the TransformationMatrix
  // with 16-byte alignment. PartitionAlloc has the ability.
  USING_FAST_MALLOC(TransformationMatrix);

 public:
  // Throughout this class, we will be speaking in column vector convention.
  // i.e. Applying a transform T to point P is T * P.
  // The elements of the matrix and the vector looks like:
  // | scale_x  skew_y_x skew_z_x translate_x |   | x |
  // | skew_x_y scale_y  skew_z_y translate_y | * | y |
  // | skew_x_z skew_y_z scale_z  translate_z |   | z |
  // | persp_x  persp_y  persp_z  persp_w     |   | w |
  // Internally the matrix is stored as a 2-dimensional array in col-major
  // order. In other words, this is the layout of the matrix:
  // | matrix_[0][0] matrix_[1][0] matrix_[2][0] matrix_[3][0] |
  // | matrix_[0][1] matrix_[1][1] matrix_[2][1] matrix_[3][1] |
  // | matrix_[0][2] matrix_[1][2] matrix_[2][2] matrix_[3][2] |
  // | matrix_[0][3] matrix_[1][3] matrix_[2][3] matrix_[3][3] |
  struct Matrix4 {
    using Column = double[4];
    Column& operator[](size_t i) { return columns[i]; }
    const Column& operator[](size_t i) const { return columns[i]; }
    Column columns[4];
  };

  TransformationMatrix() {
    MakeIdentity();
  }
  TransformationMatrix(const AffineTransform&);
  TransformationMatrix(const TransformationMatrix& t) {
    *this = t;
  }
  TransformationMatrix(double a,
                       double b,
                       double c,
                       double d,
                       double e,
                       double f) {
    SetMatrix(a, b, c, d, e, f);
  }
  TransformationMatrix(double m11,
                       double m12,
                       double m13,
                       double m14,
                       double m21,
                       double m22,
                       double m23,
                       double m24,
                       double m31,
                       double m32,
                       double m33,
                       double m34,
                       double m41,
                       double m42,
                       double m43,
                       double m44) {
    SetMatrix(m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41,
              m42, m43, m44);
  }
  TransformationMatrix(const SkMatrix44& matrix) {
    SetMatrix(
        matrix.get(0, 0), matrix.get(1, 0), matrix.get(2, 0), matrix.get(3, 0),
        matrix.get(0, 1), matrix.get(1, 1), matrix.get(2, 1), matrix.get(3, 1),
        matrix.get(0, 2), matrix.get(1, 2), matrix.get(2, 2), matrix.get(3, 2),
        matrix.get(0, 3), matrix.get(1, 3), matrix.get(2, 3), matrix.get(3, 3));
  }

  void SetMatrix(double a, double b, double c, double d, double e, double f) {
    matrix_[0][0] = a;
    matrix_[0][1] = b;
    matrix_[0][2] = 0;
    matrix_[0][3] = 0;
    matrix_[1][0] = c;
    matrix_[1][1] = d;
    matrix_[1][2] = 0;
    matrix_[1][3] = 0;
    matrix_[2][0] = 0;
    matrix_[2][1] = 0;
    matrix_[2][2] = 1;
    matrix_[2][3] = 0;
    matrix_[3][0] = e;
    matrix_[3][1] = f;
    matrix_[3][2] = 0;
    matrix_[3][3] = 1;
  }

  void SetMatrix(double m11,
                 double m12,
                 double m13,
                 double m14,
                 double m21,
                 double m22,
                 double m23,
                 double m24,
                 double m31,
                 double m32,
                 double m33,
                 double m34,
                 double m41,
                 double m42,
                 double m43,
                 double m44) {
    matrix_[0][0] = m11;
    matrix_[0][1] = m12;
    matrix_[0][2] = m13;
    matrix_[0][3] = m14;
    matrix_[1][0] = m21;
    matrix_[1][1] = m22;
    matrix_[1][2] = m23;
    matrix_[1][3] = m24;
    matrix_[2][0] = m31;
    matrix_[2][1] = m32;
    matrix_[2][2] = m33;
    matrix_[2][3] = m34;
    matrix_[3][0] = m41;
    matrix_[3][1] = m42;
    matrix_[3][2] = m43;
    matrix_[3][3] = m44;
  }

  TransformationMatrix& operator=(const TransformationMatrix& t) {
    SetMatrix(t.matrix_);
    return *this;
  }

  TransformationMatrix& MakeIdentity() {
    SetMatrix(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    return *this;
  }

  bool IsIdentity() const {
    return matrix_[0][0] == 1 && matrix_[0][1] == 0 && matrix_[0][2] == 0 &&
           matrix_[0][3] == 0 && matrix_[1][0] == 0 && matrix_[1][1] == 1 &&
           matrix_[1][2] == 0 && matrix_[1][3] == 0 && matrix_[2][0] == 0 &&
           matrix_[2][1] == 0 && matrix_[2][2] == 1 && matrix_[2][3] == 0 &&
           matrix_[3][0] == 0 && matrix_[3][1] == 0 && matrix_[3][2] == 0 &&
           matrix_[3][3] == 1;
  }

  // Map a 3D point through the transform, returning a 3D point.
  FloatPoint3D MapPoint(const FloatPoint3D&) const;

  // Map a 2D point through the transform, returning a 2D point.
  // Note that this ignores the z component, effectively projecting the point
  // into the z=0 plane.
  FloatPoint MapPoint(const FloatPoint&) const;

  // If the matrix has 3D components, the z component of the result is
  // dropped, effectively projecting the rect into the z=0 plane
  FloatRect MapRect(const FloatRect&) const;

  // Rounds the resulting mapped rectangle out. This is helpful for bounding
  // box computations but may not be what is wanted in other contexts.
  IntRect MapRect(const IntRect&) const;
  LayoutRect MapRect(const LayoutRect&) const;

  // If the matrix has 3D components, the z component of the result is
  // dropped, effectively projecting the quad into the z=0 plane
  FloatQuad MapQuad(const FloatQuad&) const;

  // Map a point on the z=0 plane into a point on
  // the plane with with the transform applied, by extending
  // a ray perpendicular to the source plane and computing
  // the local x,y position of the point where that ray intersects
  // with the destination plane.
  FloatPoint ProjectPoint(const FloatPoint&, bool* clamped = nullptr) const;
  // Projects the four corners of the quad
  FloatQuad ProjectQuad(const FloatQuad&, bool* clamped = nullptr) const;
  // Projects the four corners of the quad and takes a bounding box,
  // while sanitizing values created when the w component is negative.
  LayoutRect ClampedBoundsOfProjectedQuad(const FloatQuad&) const;

  void TransformBox(FloatBox&) const;

  // Important: These indices are spoken in col-major order. i.e.:
  // | M11() M21() M31() M41() |
  // | M12() M22() M32() M42() |
  // | M13() M23() M33() M43() |
  // | M14() M24() M34() M44() |
  double M11() const { return matrix_[0][0]; }
  void SetM11(double f) { matrix_[0][0] = f; }
  double M12() const { return matrix_[0][1]; }
  void SetM12(double f) { matrix_[0][1] = f; }
  double M13() const { return matrix_[0][2]; }
  void SetM13(double f) { matrix_[0][2] = f; }
  double M14() const { return matrix_[0][3]; }
  void SetM14(double f) { matrix_[0][3] = f; }
  double M21() const { return matrix_[1][0]; }
  void SetM21(double f) { matrix_[1][0] = f; }
  double M22() const { return matrix_[1][1]; }
  void SetM22(double f) { matrix_[1][1] = f; }
  double M23() const { return matrix_[1][2]; }
  void SetM23(double f) { matrix_[1][2] = f; }
  double M24() const { return matrix_[1][3]; }
  void SetM24(double f) { matrix_[1][3] = f; }
  double M31() const { return matrix_[2][0]; }
  void SetM31(double f) { matrix_[2][0] = f; }
  double M32() const { return matrix_[2][1]; }
  void SetM32(double f) { matrix_[2][1] = f; }
  double M33() const { return matrix_[2][2]; }
  void SetM33(double f) { matrix_[2][2] = f; }
  double M34() const { return matrix_[2][3]; }
  void SetM34(double f) { matrix_[2][3] = f; }
  double M41() const { return matrix_[3][0]; }
  void SetM41(double f) { matrix_[3][0] = f; }
  double M42() const { return matrix_[3][1]; }
  void SetM42(double f) { matrix_[3][1] = f; }
  double M43() const { return matrix_[3][2]; }
  void SetM43(double f) { matrix_[3][2] = f; }
  double M44() const { return matrix_[3][3]; }
  void SetM44(double f) { matrix_[3][3] = f; }

  double A() const { return matrix_[0][0]; }
  void SetA(double a) { matrix_[0][0] = a; }

  double B() const { return matrix_[0][1]; }
  void SetB(double b) { matrix_[0][1] = b; }

  double C() const { return matrix_[1][0]; }
  void SetC(double c) { matrix_[1][0] = c; }

  double D() const { return matrix_[1][1]; }
  void SetD(double d) { matrix_[1][1] = d; }

  double E() const { return matrix_[3][0]; }
  void SetE(double e) { matrix_[3][0] = e; }

  double F() const { return matrix_[3][1]; }
  void SetF(double f) { matrix_[3][1] = f; }

  // *this = *this * mat.
  TransformationMatrix& Multiply(const TransformationMatrix&);

  TransformationMatrix& Scale(double);
  TransformationMatrix& ScaleNonUniform(double sx, double sy);
  TransformationMatrix& Scale3d(double sx, double sy, double sz);

  TransformationMatrix& Rotate(double d) { return Rotate3d(0, 0, d); }
  // Angles are in degrees.
  TransformationMatrix& Rotate3d(double rx, double ry, double rz);
  TransformationMatrix& Rotate3d(const Rotation&);

  // The vector (x,y,z) is normalized if it's not already. A vector of
  // (0,0,0) uses a vector of (0,0,1).
  TransformationMatrix& Rotate3d(double x, double y, double z, double angle);

  TransformationMatrix& Translate(double tx, double ty);
  TransformationMatrix& Translate3d(double tx, double ty, double tz);

  // Append translation after existing operations. i.e.
  // TransformationMatrix t2 = t1;
  // t2.PostTranslate(x, y);
  // t2.MapPoint(p) == t1.MapPoint(p) + FloatPoint(x, y)
  TransformationMatrix& PostTranslate(double tx, double ty);
  TransformationMatrix& PostTranslate3d(double tx, double ty, double tz);

  TransformationMatrix& Skew(double angle_x, double angle_y);
  TransformationMatrix& SkewX(double angle) { return Skew(angle, 0); }
  TransformationMatrix& SkewY(double angle) { return Skew(0, angle); }

  TransformationMatrix& ApplyPerspective(double p);

  // Changes the transform to apply as if the origin were at (x, y, z).
  TransformationMatrix& ApplyTransformOrigin(double x, double y, double z);
  TransformationMatrix& ApplyTransformOrigin(const FloatPoint3D& origin) {
    return ApplyTransformOrigin(origin.X(), origin.Y(), origin.Z());
  }

  // Changes the transform to:
  //
  //     scale3d(z, z, z) * mat * scale3d(1/z, 1/z, 1/z)
  //
  // Useful for mapping zoomed points to their zoomed transformed result:
  //
  //     new_mat * (scale3d(z, z, z) * x) == scale3d(z, z, z) * (mat * x)
  //
  TransformationMatrix& Zoom(double zoom_factor);

  bool IsInvertible() const;

  // This method returns the identity matrix if it is not invertible.
  // Use isInvertible() before calling this if you need to know.
  TransformationMatrix Inverse() const;

  // decompose the matrix into its component parts
  typedef struct {
    double scale_x, scale_y, scale_z;
    double skew_xy, skew_xz, skew_yz;
    double quaternion_x, quaternion_y, quaternion_z, quaternion_w;
    double translate_x, translate_y, translate_z;
    double perspective_x, perspective_y, perspective_z, perspective_w;
  } DecomposedType;

  // Decompose 2-D transform matrix into its component parts.
  typedef struct {
    double scale_x, scale_y;
    double skew_xy;
    double translate_x, translate_y;
    double angle;
  } Decomposed2dType;

  WARN_UNUSED_RESULT bool Decompose(DecomposedType&) const;
  WARN_UNUSED_RESULT bool Decompose2D(Decomposed2dType&) const;
  void Recompose(const DecomposedType&);
  void Recompose2D(const Decomposed2dType&);
  void Blend(const TransformationMatrix& from, double progress);
  void Blend2D(const TransformationMatrix& from, double progress);

  bool IsAffine() const {
    return M13() == 0 && M14() == 0 && M23() == 0 && M24() == 0 && M31() == 0 &&
           M32() == 0 && M33() == 1 && M34() == 0 && M43() == 0 && M44() == 1;
  }

  // Throw away the non-affine parts of the matrix (lossy!)
  void MakeAffine();

  AffineTransform ToAffineTransform() const;

  // Flatten into a 2-D transformation (non-invertable).
  // Same as gfx::Transform::FlattenTo2d(); see the docs for that function for
  // details and discussion.
  void FlattenTo2d();

  bool operator==(const TransformationMatrix& m2) const {
    return matrix_[0][0] == m2.matrix_[0][0] &&
           matrix_[0][1] == m2.matrix_[0][1] &&
           matrix_[0][2] == m2.matrix_[0][2] &&
           matrix_[0][3] == m2.matrix_[0][3] &&
           matrix_[1][0] == m2.matrix_[1][0] &&
           matrix_[1][1] == m2.matrix_[1][1] &&
           matrix_[1][2] == m2.matrix_[1][2] &&
           matrix_[1][3] == m2.matrix_[1][3] &&
           matrix_[2][0] == m2.matrix_[2][0] &&
           matrix_[2][1] == m2.matrix_[2][1] &&
           matrix_[2][2] == m2.matrix_[2][2] &&
           matrix_[2][3] == m2.matrix_[2][3] &&
           matrix_[3][0] == m2.matrix_[3][0] &&
           matrix_[3][1] == m2.matrix_[3][1] &&
           matrix_[3][2] == m2.matrix_[3][2] &&
           matrix_[3][3] == m2.matrix_[3][3];
  }

  bool operator!=(const TransformationMatrix& other) const {
    return !(*this == other);
  }

  // *this = *this * t
  TransformationMatrix& operator*=(const TransformationMatrix& t) {
    return Multiply(t);
  }

  // result = *this * t
  TransformationMatrix operator*(const TransformationMatrix& t) const {
    TransformationMatrix result = *this;
    result.Multiply(t);
    return result;
  }

  bool IsFlat() const {
    return matrix_[0][2] == 0.f && matrix_[1][2] == 0.f &&
           matrix_[2][0] == 0.f && matrix_[2][1] == 0.f &&
           matrix_[2][2] == 1.f && matrix_[2][3] == 0.f && matrix_[3][2] == 0.f;
  }

  bool IsIdentityOrTranslation() const {
    return matrix_[0][0] == 1 && matrix_[0][1] == 0 && matrix_[0][2] == 0 &&
           matrix_[0][3] == 0 && matrix_[1][0] == 0 && matrix_[1][1] == 1 &&
           matrix_[1][2] == 0 && matrix_[1][3] == 0 && matrix_[2][0] == 0 &&
           matrix_[2][1] == 0 && matrix_[2][2] == 1 && matrix_[2][3] == 0 &&
           matrix_[3][3] == 1;
  }

  bool IsIdentityOr2DTranslation() const {
    return IsIdentityOrTranslation() && matrix_[3][2] == 0;
  }

  bool Is2DProportionalUpscaleAndOr2DTranslation() const {
    if (matrix_[0][0] < 1 || matrix_[0][0] != matrix_[1][1])
      return false;
    return matrix_[0][1] == 0 && matrix_[0][2] == 0 && matrix_[0][3] == 0 &&
           matrix_[1][0] == 0 && matrix_[1][2] == 0 && matrix_[1][3] == 0 &&
           matrix_[2][0] == 0 && matrix_[2][1] == 0 && matrix_[2][2] == 1 &&
           matrix_[2][3] == 0 && matrix_[3][2] == 0 && matrix_[3][3] == 1;
  }

  bool Is2dTransform() const;

  bool IsIntegerTranslation() const;

  // Returns true if axis-aligned 2d rects will remain axis-aligned after being
  // transformed by this matrix.
  bool Preserves2dAxisAlignment() const;

  // If this transformation is identity or 2D translation, returns the
  // translation.
  FloatSize To2DTranslation() const {
    DCHECK(IsIdentityOr2DTranslation());
    return FloatSize(matrix_[3][0], matrix_[3][1]);
  }

  typedef float FloatMatrix4[16];
  void ToColumnMajorFloatArray(FloatMatrix4& result) const;

  static SkMatrix44 ToSkMatrix44(const TransformationMatrix&);
  static gfx::Transform ToTransform(const TransformationMatrix&);

  // If |asMatrix|, return the matrix in row-major order. Otherwise, return
  // the transform's decomposition which shows the translation, scale, etc.
  String ToString(bool as_matrix = false) const;

 private:
  // multiply passed 2D point by matrix (assume z=0)
  void MultVecMatrix(double x, double y, double& dst_x, double& dst_y) const;
  FloatPoint InternalMapPoint(const FloatPoint& source_point) const {
    double result_x;
    double result_y;
    MultVecMatrix(source_point.X(), source_point.Y(), result_x, result_y);
    return FloatPoint(static_cast<float>(result_x),
                      static_cast<float>(result_y));
  }

  // multiply passed 3D point by matrix
  void MultVecMatrix(double x,
                     double y,
                     double z,
                     double& dst_x,
                     double& dst_y,
                     double& dst_z) const;
  FloatPoint3D InternalMapPoint(const FloatPoint3D& source_point) const {
    double result_x;
    double result_y;
    double result_z;
    MultVecMatrix(source_point.X(), source_point.Y(), source_point.Z(),
                  result_x, result_y, result_z);
    return FloatPoint3D(static_cast<float>(result_x),
                        static_cast<float>(result_y),
                        static_cast<float>(result_z));
  }

  void SetMatrix(const Matrix4& m) { memcpy(&matrix_, &m, sizeof(Matrix4)); }

  Matrix4 matrix_;
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         const TransformationMatrix&);
PLATFORM_EXPORT std::unique_ptr<JSONArray> TransformAsJSONArray(
    const TransformationMatrix&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_TRANSFORMATION_MATRIX_H_
