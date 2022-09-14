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
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkM44.h"
#include "ui/gfx/geometry/double4.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gfx {
class BoxF;
class PointF;
class QuadF;
class Rect;
class RectF;
class Transform;
}

namespace blink {

class AffineTransform;
class LayoutRect;
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
  //   \ col
  // r  \     0        1        2          3
  // o 0 | scale_x  skew_xy  skew_xz  translate_x |   | x |
  // w 1 | skew_yx  scale_y  skew_yz  translate_y | * | y |
  //   2 | skew_zx  skew_zy  scale_z  translate_z |   | z |
  //   3 | persp_x  persp_y  persp_z  persp_w     |   | w |
  //
  // The components correspond to the DOMMatrix mij (i,j = 1..4) components:
  //   i = col + 1
  //   j = row + 1
  constexpr TransformationMatrix()
      : matrix_{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}} {}

  explicit TransformationMatrix(const AffineTransform&);
  explicit TransformationMatrix(const gfx::Transform&);
  explicit TransformationMatrix(const SkM44& matrix)
      : TransformationMatrix(matrix.rc(0, 0),
                             matrix.rc(1, 0),
                             matrix.rc(2, 0),
                             matrix.rc(3, 0),
                             matrix.rc(0, 1),
                             matrix.rc(1, 1),
                             matrix.rc(2, 1),
                             matrix.rc(3, 1),
                             matrix.rc(0, 2),
                             matrix.rc(1, 2),
                             matrix.rc(2, 2),
                             matrix.rc(3, 2),
                             matrix.rc(0, 3),
                             matrix.rc(1, 3),
                             matrix.rc(2, 3),
                             matrix.rc(3, 3)) {}

  double rc(int row, int col) const {
    CheckRowCol(row, col);
    return matrix_[col][row];
  }
  void set_rc(int row, int col, double v) {
    CheckRowCol(row, col);
    matrix_[col][row] = v;
  }

  [[nodiscard]] static TransformationMatrix Affine(double a,
                                                   double b,
                                                   double c,
                                                   double d,
                                                   double e,
                                                   double f) {
    return ColMajor(a, b, 0, 0, c, d, 0, 0, 0, 0, 1, 0, e, f, 0, 1);
  }

  [[nodiscard]] static TransformationMatrix ColMajor(double r0c0,
                                                     double r1c0,
                                                     double r2c0,
                                                     double r3c0,
                                                     double r0c1,
                                                     double r1c1,
                                                     double r2c1,
                                                     double r3c1,
                                                     double r0c2,
                                                     double r1c2,
                                                     double r2c2,
                                                     double r3c2,
                                                     double r0c3,
                                                     double r1c3,
                                                     double r2c3,
                                                     double r3c3) {
    return TransformationMatrix(r0c0, r1c0, r2c0, r3c0, r0c1, r1c1, r2c1, r3c1,
                                r0c2, r1c2, r2c2, r3c2, r0c3, r1c3, r2c3, r3c3);
  }

  [[nodiscard]] static TransformationMatrix ColMajor(const double v[16]) {
    return ColMajor(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9],
                    v[10], v[11], v[12], v[13], v[14], v[15]);
  }
  void GetColMajor(double v[16]) const {
    std::copy(ColMajorData(), ColMajorData() + 16, v);
  }
  const double* ColMajorData() const { return &matrix_[0][0]; }

  [[nodiscard]] static TransformationMatrix ColMajorF(const float v[16]) {
    return ColMajor(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9],
                    v[10], v[11], v[12], v[13], v[14], v[15]);
  }
  // This method preserves NaN and infinity components.
  void GetColMajorF(float v[16]) const;

  TransformationMatrix& MakeIdentity() {
    *this = TransformationMatrix();
    return *this;
  }

  bool IsIdentity() const {
    return gfx::AllTrue(
        (Col(0) == Double4{1, 0, 0, 0}) & (Col(1) == Double4{0, 1, 0, 0}) &
        (Col(2) == Double4{0, 0, 1, 0}) & (Col(3) == Double4{0, 0, 0, 1}));
  }

  // The float values produced by the following methods are clamped with
  // ClampToFloat() which converts NaN to 0 and +-infinity to minimum/maximum
  // value of float.

  // Map a 3D point through the transform, returning a 3D point.
  [[nodiscard]] gfx::Point3F MapPoint(const gfx::Point3F&) const;

  // Map a 2D point through the transform, returning a 2D point.
  // Note that this ignores the z component, effectively projecting the point
  // into the z=0 plane.
  [[nodiscard]] gfx::PointF MapPoint(const gfx::PointF&) const;

  // If the matrix has 3D components, the z component of the result is
  // dropped, effectively projecting the rect into the z=0 plane
  [[nodiscard]] gfx::RectF MapRect(const gfx::RectF&) const;

  // Rounds the resulting mapped rectangle out. This is helpful for bounding
  // box computations but may not be what is wanted in other contexts.
  [[nodiscard]] gfx::Rect MapRect(const gfx::Rect&) const;

  [[nodiscard]] LayoutRect MapRect(const LayoutRect&) const;

  // If the matrix has 3D components, the z component of the result is
  // dropped, effectively projecting the quad into the z=0 plane
  [[nodiscard]] gfx::QuadF MapQuad(const gfx::QuadF&) const;

  // Map a point on the z=0 plane into a point on the plane with which the
  // transform applied, by extending a ray perpendicular to the source plane and
  // computing the local x,y position of the point where that ray intersects
  // with the destination plane.
  [[nodiscard]] gfx::PointF ProjectPoint(const gfx::PointF&,
                                         bool* clamped = nullptr) const;
  // Projects the four corners of the quad.
  [[nodiscard]] gfx::QuadF ProjectQuad(const gfx::QuadF&) const;
  // Projects the four corners of the quad and takes a bounding box,
  // while sanitizing values created when the w component is negative.
  [[nodiscard]] LayoutRect ClampedBoundsOfProjectedQuad(
      const gfx::QuadF&) const;

  void TransformBox(gfx::BoxF&) const;

  // *this = *this * mat.
  TransformationMatrix& Multiply(const TransformationMatrix&);

  TransformationMatrix& Scale(double);
  TransformationMatrix& ScaleNonUniform(double sx, double sy);
  TransformationMatrix& Scale3d(double sx, double sy, double sz);

  // Angles are in degrees.
  TransformationMatrix& Rotate(double d);
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
  // t2.MapPoint(p) == t1.MapPoint(p) + gfx::PointF(x, y)
  TransformationMatrix& PostTranslate(double tx, double ty);
  TransformationMatrix& PostTranslate3d(double tx, double ty, double tz);

  TransformationMatrix& Skew(double angle_x, double angle_y);
  TransformationMatrix& SkewX(double angle) { return Skew(angle, 0); }
  TransformationMatrix& SkewY(double angle) { return Skew(0, angle); }

  TransformationMatrix& ApplyPerspective(double p);

  // Changes the transform to apply as if the origin were at (x, y, z).
  TransformationMatrix& ApplyTransformOrigin(double x, double y, double z);
  TransformationMatrix& ApplyTransformOrigin(const gfx::Point3F& origin) {
    return ApplyTransformOrigin(origin.x(), origin.y(), origin.z());
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
  // Use GetInverse() if you also need to know the invertibility.
  [[nodiscard]] TransformationMatrix Inverse() const;

  // If this matrix is invertible, this method sets |result| to the inverse of
  // this matrix and returns true, otherwise sets |result| to identity and
  // returns false. |result| can't be null (but is not a reference to be
  // consistent with gfx::Transform::GetInverse()).
  [[nodiscard]] bool GetInverse(TransformationMatrix* result) const;

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

  [[nodiscard]] bool Decompose(DecomposedType&) const;
  [[nodiscard]] bool Decompose2D(Decomposed2dType&) const;
  void Recompose(const DecomposedType&);
  void Recompose2D(const Decomposed2dType&);
  void Blend(const TransformationMatrix& from, double progress);
  void Blend2D(const TransformationMatrix& from, double progress);

  bool IsAffine() const { return IsFlat() && !HasPerspective(); }
  bool Is2dTransform() const { return IsAffine(); }

  // Throw away the non-affine parts of the matrix (lossy!)
  void MakeAffine();

  [[nodiscard]] AffineTransform ToAffineTransform() const;

  // Flatten into a 2-D transformation (non-invertable).
  // Same as gfx::Transform::FlattenTo2d(); see the docs for that function for
  // details and discussion.
  void FlattenTo2d();

  bool operator==(const TransformationMatrix& m2) const {
    return gfx::AllTrue((Col(0) == m2.Col(0)) & (Col(1) == m2.Col(1)) &
                        (Col(2) == m2.Col(2)) & (Col(3) == m2.Col(3)));
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
           gfx::AllTrue(Col(2) == Double4{0, 0, 1, 0}) && matrix_[3][2] == 0.f;
  }

  bool IsIdentityOrTranslation() const {
    return gfx::AllTrue((Col(0) == Double4{1, 0, 0, 0}) &
                        (Col(1) == Double4{0, 1, 0, 0}) &
                        (Col(2) == Double4{0, 0, 1, 0})) &&
           matrix_[3][3] == 1;
  }

  bool IsIdentityOr2DTranslation() const {
    return IsIdentityOrTranslation() && matrix_[3][2] == 0;
  }

  bool Is2DProportionalUpscaleAndOr2DTranslation() const {
    if (matrix_[0][0] < 1 || matrix_[0][0] != matrix_[1][1])
      return false;
    return gfx::AllTrue((Col(0) == Double4{matrix_[0][0], 0, 0, 0}) &
                        (Col(1) == Double4{0, matrix_[1][1], 0, 0}) &
                        (Col(2) == Double4{0, 0, 1, 0})) &&
           matrix_[3][2] == 0 && matrix_[3][3] == 1;
  }

  bool IsInteger2DTranslation() const;

  // Returns whether this matrix can transform a z=0 plane to something
  // containing points where z != 0. This is primarily intended for metrics.
  bool Creates3D() const {
    return !gfx::AllTrue(Double4{matrix_[0][2], matrix_[1][2], 1,
                                 matrix_[3][2]} == Double4{0, 0, 1, 0});
  }

  // Returns true if axis-aligned 2d rects will remain axis-aligned after being
  // transformed by this matrix.
  bool Preserves2dAxisAlignment() const;

  bool HasPerspective() const {
    return !gfx::AllTrue(Double4{matrix_[0][3], matrix_[1][3], matrix_[2][3],
                                 matrix_[3][3]} == Double4{0, 0, 0, 1});
  }

  // Returns the components that create a 2d translation, ignoring other
  // components. This may be lossy.
  gfx::Vector2dF To2DTranslation() const {
    return gfx::Vector2dF(ClampToFloat(matrix_[3][0]),
                          ClampToFloat(matrix_[3][1]));
  }

  // Returns the components that create a 3d translation, ignoring other
  // components. This may be lossy.
  gfx::Vector3dF To3dTranslation() const {
    return gfx::Vector3dF(ClampToFloat(matrix_[3][0]),
                          ClampToFloat(matrix_[3][1]),
                          ClampToFloat(matrix_[3][2]));
  }

  // This method converts double to float using ClampToFloat() which converts
  // NaN to 0 and +-infinity to minimum/maximum value of float.
  SkM44 ToSkM44() const;
  // Performs same conversions as ToSkM44.
  gfx::Transform ToTransform() const;

  // If |asMatrix|, return the matrix in row-major order. Otherwise, return
  // the transform's decomposition which shows the translation, scale, etc.
  String ToString(bool as_matrix = false) const;

 private:
  // Used internally to construct TransformationMatrix with parameters in
  // col-major order.
  TransformationMatrix(double r0c0,
                       double r1c0,
                       double r2c0,
                       double r3c0,
                       double r0c1,
                       double r1c1,
                       double r2c1,
                       double r3c1,
                       double r0c2,
                       double r1c2,
                       double r2c2,
                       double r3c2,
                       double r0c3,
                       double r1c3,
                       double r2c3,
                       double r3c3)
      : matrix_{{r0c0, r1c0, r2c0, r3c0},
                {r0c1, r1c1, r2c1, r3c1},
                {r0c2, r1c2, r2c2, r3c2},
                {r0c3, r1c3, r2c3, r3c3}} {}

  void CheckRowCol(int row, int col) const {
    DCHECK_GE(row, 0);
    DCHECK_LT(row, 4);
    DCHECK_GE(col, 0);
    DCHECK_LT(col, 4);
  }

  gfx::PointF TranslatePoint(const gfx::PointF&) const;
  gfx::PointF InternalMapPoint(const gfx::PointF&) const;
  gfx::Point3F InternalMapPoint(const gfx::Point3F&) const;
  gfx::QuadF InternalMapQuad(const gfx::QuadF&) const;
  template <bool check_invertibility_only>
  bool InternalInverse(TransformationMatrix* result) const;

  static float ClampToFloat(double value) {
    return ClampToWithNaNTo0<float>(value);
  }

  using Double4 = gfx::Double4;

  ALWAYS_INLINE Double4 Col(int c) const {
    return gfx::LoadDouble4(matrix_[c]);
  }
  ALWAYS_INLINE void SetCol(int c, Double4 v) {
    gfx::StoreDouble4(v, matrix_[c]);
  }

  template <bool check_invertibility_only>
  ALWAYS_INLINE static bool InverseWithDouble4Cols(Double4& c0,
                                                   Double4& c1,
                                                   Double4& c2,
                                                   Double4& c3);

  // This is indexed by [col][row].
  double matrix_[4][4];
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         const TransformationMatrix&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_TRANSFORMATION_MATRIX_H_
