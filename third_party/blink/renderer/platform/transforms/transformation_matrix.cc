/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
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

#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

#include <cmath>
#include <cstdlib>

#include "base/logging.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/transforms/rotation.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

using gfx::Quaternion;

// TODO(crbug/937296): This implementation is virtually identical to the
// implementation in ui/gfx/geometry/transform_util with the main difference
// being the representation of the underlying matrix. These implementations
// should be consolidated.
bool TransformationMatrix::Decompose(DecomposedType& result) const {
  if (IsIdentity()) {
    result.translate_x = result.translate_y = result.translate_z = 0.0;
    result.scale_x = result.scale_y = result.scale_z = 1.0;
    result.skew_xy = result.skew_xz = result.skew_yz = 0.0;
    result.quaternion_x = result.quaternion_y = result.quaternion_z = 0.0;
    result.quaternion_w = 1.0;
    result.perspective_x = result.perspective_y = result.perspective_z = 0.0;
    result.perspective_w = 1.0;
    return true;
  }

  // https://www.w3.org/TR/css-transforms-2/#decomposing-a-3d-matrix.

  Double4 c0 = Col(0);
  Double4 c1 = Col(1);
  Double4 c2 = Col(2);
  Double4 c3 = Col(3);

  // Normalize the matrix.
  if (!std::isnormal(c3[3]))
    return false;

  double inv_w = 1.0 / c3[3];
  c0 *= inv_w;
  c1 *= inv_w;
  c2 *= inv_w;
  c3 *= inv_w;

  Double4 perspective = {c0[3], c1[3], c2[3], 1.0};
  // Clear the perspective partition.
  c0[3] = c1[3] = c2[3] = 0;
  c3[3] = 1;

  Double4 inverse_c0 = c0;
  Double4 inverse_c1 = c1;
  Double4 inverse_c2 = c2;
  Double4 inverse_c3 = c3;
  if (!InverseWithDouble4Cols<false>(inverse_c0, inverse_c1, inverse_c2,
                                     inverse_c3))
    return false;

  // First, isolate perspective.
  if (!gfx::AllTrue(perspective == Double4{0, 0, 0, 1})) {
    // Solve the equation by multiplying perspective by the inverse.
    result.perspective_x = gfx::Sum(perspective * inverse_c0);
    result.perspective_y = gfx::Sum(perspective * inverse_c1);
    result.perspective_z = gfx::Sum(perspective * inverse_c2);
    result.perspective_w = gfx::Sum(perspective * inverse_c3);
  } else {
    // No perspective.
    result.perspective_x = result.perspective_y = result.perspective_z = 0;
    result.perspective_w = 1;
  }

  // Next take care of translation (easy).
  result.translate_x = c3[0];
  c3[0] = 0;
  result.translate_y = c3[1];
  c3[1] = 0;
  result.translate_z = c3[2];
  c3[2] = 0;

  // Note: Deviating from the spec in terms of variable naming. The matrix is
  // stored on column major order and not row major. Using the variable 'row'
  // instead of 'column' in the spec pseudocode has been the source of
  // confusion, specifically in sorting out rotations.

  // From now on, only the first 3 components of the Double4 column is used.
  auto sum3 = [](Double4 c) -> double { return c[0] + c[1] + c[2]; };
  auto extract_scale = [&sum3](Double4& c, double& scale) -> bool {
    scale = std::sqrt(sum3(c * c));
    if (!std::isnormal(scale))
      return false;
    c *= 1.0 / scale;
    return true;
  };

  // Compute X scale factor and normalize the first column.
  if (!extract_scale(c0, result.scale_x))
    return false;

  // Compute XY shear factor and make 2nd row orthogonal to 1st.
  result.skew_xy = sum3(c0 * c1);
  c1 -= c0 * result.skew_xy;

  // Now, compute Y scale and normalize 2nd column.
  if (!extract_scale(c1, result.scale_y))
    return false;

  result.skew_xy /= result.scale_y;

  // Compute XZ and YZ shears, and orthogonalize the 3rd column.
  result.skew_xz = sum3(c0 * c2);
  c2 -= c0 * result.skew_xz;
  result.skew_yz = sum3(c1 * c2);
  c2 -= c1 * result.skew_yz;

  // Next, get Z scale and normalize the 3rd column.
  if (!extract_scale(c2, result.scale_z))
    return false;

  result.skew_xz /= result.scale_z;
  result.skew_yz /= result.scale_z;

  // At this point, the matrix (in column[]) is orthonormal.
  // Check for a coordinate system flip.  If the determinant
  // is -1, then negate the matrix and the scaling factors.
  auto cross3 = [](Double4 a, Double4 b) -> Double4 {
    return Double4{a[1], a[2], a[0], a[3]} * Double4{b[2], b[0], b[1], b[3]} -
           Double4{a[2], a[0], a[1], a[3]} * Double4{b[1], b[2], b[0], b[3]};
  };
  Double4 pdum3 = cross3(c1, c2);
  if (sum3(c0 * pdum3) < 0) {
    // Note that flipping only one of the 3 scaling factors would also flip
    // the sign of the determinant. By flipping all 3, we turn a 2D matrix
    // interpolation into a 3D interpolation.
    result.scale_x *= -1;
    result.scale_y *= -1;
    result.scale_z *= -1;
    c0 *= -1;
    c1 *= -1;
    c2 *= -1;
  }

  // Lastly, compute the quaternions.
  // See https://en.wikipedia.org/wiki/Rotation_matrix#Quaternion.
  // Note: deviating from spec (http://www.w3.org/TR/css3-transforms/)
  // which has a degenerate case when the trace (t) of the orthonormal matrix
  // (Q) approaches -1. In the Wikipedia article, Q_ij is indexing on row then
  // column. Thus, Q_ij = column[j][i].

  // The following are equivalent represnetations of the rotation matrix:
  //
  // Axis-angle form:
  //
  //      [ c+(1-c)x^2  (1-c)xy-sz  (1-c)xz+sy ]    c = cos theta
  // R =  [ (1-c)xy+sz  c+(1-c)y^2  (1-c)yz-sx ]    s = sin theta
  //      [ (1-c)xz-sy  (1-c)yz+sx  c+(1-c)z^2 ]    [x,y,z] = axis or rotation
  //
  // The sum of the diagonal elements (trace) is a simple function of the cosine
  // of the angle. The w component of the quaternion is cos(theta/2), and we
  // make use of the double angle formula to directly compute w from the
  // trace. Differences between pairs of skew symmetric elements in this matrix
  // isolate the remaining components. Since w can be zero (also numerically
  // unstable if near zero), we cannot rely solely on this approach to compute
  // the quaternion components.
  //
  // Quaternion form:
  //
  //       [ 1-2(y^2+z^2)    2(xy-zw)      2(xz+yw)   ]
  //  r =  [   2(xy+zw)    1-2(x^2+z^2)    2(yz-xw)   ]    q = (x,y,y,w)
  //       [   2(xz-yw)      2(yz+xw)    1-2(x^2+y^2) ]
  //
  // Different linear combinations of the diagonal elements isolates x, y or z.
  // Sums or differences between skew symmetric elements isolate the remainder.

  double r, s, t, x, y, z, w;

  t = c0[0] + c1[1] + c2[2];  // trace of Q

  // https://en.wikipedia.org/wiki/Rotation_matrix#Quaternion
  if (1 + t > 0.001) {
    // Numerically stable as long as 1+t is not close to zero. Otherwise use the
    // diagonal element with the greatest value to compute the quaternions.
    r = std::sqrt(1.0 + t);
    s = 0.5 / r;
    w = 0.5 * r;
    x = (c1[2] - c2[1]) * s;
    y = (c2[0] - c0[2]) * s;
    z = (c0[1] - c1[0]) * s;
  } else if (c0[0] > c1[1] && c0[0] > c2[2]) {
    // Q_xx is largest.
    r = std::sqrt(1.0 + c0[0] - c1[1] - c2[2]);
    s = 0.5 / r;
    x = 0.5 * r;
    y = (c1[0] - c0[1]) * s;
    z = (c2[0] + c0[2]) * s;
    w = (c1[2] - c2[1]) * s;
  } else if (c1[1] > c2[2]) {
    // Q_yy is largest.
    r = std::sqrt(1.0 - c0[0] + c1[1] - c2[2]);
    s = 0.5 / r;
    x = (c1[0] + c0[1]) * s;
    y = 0.5 * r;
    z = (c2[1] + c1[2]) * s;
    w = (c2[0] - c0[2]) * s;
  } else {
    // Q_zz is largest.
    r = std::sqrt(1.0 - c0[0] - c1[1] + c2[2]);
    s = 0.5 / r;
    x = (c2[0] + c0[2]) * s;
    y = (c2[1] + c1[2]) * s;
    z = 0.5 * r;
    w = (c0[1] - c1[0]) * s;
  }

  result.quaternion_x = x;
  result.quaternion_y = y;
  result.quaternion_z = z;
  result.quaternion_w = w;

  return true;
}

Quaternion ToQuaterion(const TransformationMatrix::DecomposedType& decomp) {
  return Quaternion(decomp.quaternion_x, decomp.quaternion_y,
                    decomp.quaternion_z, decomp.quaternion_w);
}

void Slerp(TransformationMatrix::DecomposedType& from_decomp,
           const TransformationMatrix::DecomposedType& to_decomp,
           double progress) {
  Quaternion qa = ToQuaterion(from_decomp);
  Quaternion qb = ToQuaterion(to_decomp);
  Quaternion qc = qa.Slerp(qb, progress);
  from_decomp.quaternion_x = qc.x();
  from_decomp.quaternion_y = qc.y();
  from_decomp.quaternion_z = qc.z();
  from_decomp.quaternion_w = qc.w();
}

// clang-format off
TransformationMatrix::TransformationMatrix(const gfx::Transform& t)
    : TransformationMatrix(t.rc(0, 0), t.rc(1, 0), t.rc(2, 0), t.rc(3, 0),
                           t.rc(0, 1), t.rc(1, 1), t.rc(2, 1), t.rc(3, 1),
                           t.rc(0, 2), t.rc(1, 2), t.rc(2, 2), t.rc(3, 2),
                           t.rc(0, 3), t.rc(1, 3), t.rc(2, 3), t.rc(3, 3)) {}
// clang-format on

TransformationMatrix::TransformationMatrix(const AffineTransform& t) {
  *this = Affine(t.A(), t.B(), t.C(), t.D(), t.E(), t.F());
}

TransformationMatrix& TransformationMatrix::Scale(double s) {
  return ScaleNonUniform(s, s);
}

gfx::PointF TransformationMatrix::ProjectPoint(const gfx::PointF& p,
                                               bool* clamped) const {
  // This is basically raytracing. We have a point in the destination
  // plane with z=0, and we cast a ray parallel to the z-axis from that
  // point to find the z-position at which it intersects the z=0 plane
  // with the transform applied. Once we have that point we apply the
  // inverse transform to find the corresponding point in the source
  // space.
  //
  // Given a plane with normal Pn, and a ray starting at point R0 and
  // with direction defined by the vector Rd, we can find the
  // intersection point as a distance d from R0 in units of Rd by:
  //
  // d = -dot (Pn', R0) / dot (Pn', Rd)
  if (clamped)
    *clamped = false;

  if (matrix_[2][2] == 0) {
    // In this case, the projection plane is parallel to the ray we are trying
    // to trace, and there is no well-defined value for the projection.
    return gfx::PointF();
  }

  double x = p.x();
  double y = p.y();
  double z =
      -(matrix_[0][2] * x + matrix_[1][2] * y + matrix_[3][2]) / matrix_[2][2];

  double out_x =
      x * matrix_[0][0] + y * matrix_[1][0] + z * matrix_[2][0] + matrix_[3][0];
  double out_y =
      x * matrix_[0][1] + y * matrix_[1][1] + z * matrix_[2][1] + matrix_[3][1];

  double w =
      x * matrix_[0][3] + y * matrix_[1][3] + z * matrix_[2][3] + matrix_[3][3];
  if (w <= 0) {
    // Using int max causes overflow when other code uses the projected point.
    // To represent infinity yet reduce the risk of overflow, we use a large but
    // not-too-large number here when clamping.
    const int kLargeNumber = 100000000 / kFixedPointDenominator;
    out_x = copysign(kLargeNumber, out_x);
    out_y = copysign(kLargeNumber, out_y);
    if (clamped)
      *clamped = true;
  } else if (w != 1) {
    out_x /= w;
    out_y /= w;
  }

  return gfx::PointF(ClampToFloat(out_x), ClampToFloat(out_y));
}

gfx::QuadF TransformationMatrix::ProjectQuad(const gfx::QuadF& q) const {
  gfx::QuadF projected_quad;

  bool clamped1 = false;
  bool clamped2 = false;
  bool clamped3 = false;
  bool clamped4 = false;

  projected_quad.set_p1(ProjectPoint(q.p1(), &clamped1));
  projected_quad.set_p2(ProjectPoint(q.p2(), &clamped2));
  projected_quad.set_p3(ProjectPoint(q.p3(), &clamped3));
  projected_quad.set_p4(ProjectPoint(q.p4(), &clamped4));

  // If all points on the quad had w < 0, then the entire quad would not be
  // visible to the projected surface.
  bool everything_was_clipped = clamped1 && clamped2 && clamped3 && clamped4;
  if (everything_was_clipped)
    return gfx::QuadF();

  return projected_quad;
}

static float ClampEdgeValue(float f) {
  DCHECK(!std::isnan(f));
  return ClampTo(f, (-LayoutUnit::Max() / 2).ToFloat(),
                 (LayoutUnit::Max() / 2).ToFloat());
}

LayoutRect TransformationMatrix::ClampedBoundsOfProjectedQuad(
    const gfx::QuadF& q) const {
  gfx::RectF mapped_quad_bounds = ProjectQuad(q).BoundingBox();
  // mapped_quad_bounds.width()/height() may be infinity if e.g.
  // right - left > float_max.
  DCHECK(std::isfinite(mapped_quad_bounds.x()));
  DCHECK(std::isfinite(mapped_quad_bounds.y()));
  DCHECK(!std::isnan(mapped_quad_bounds.width()));
  DCHECK(!std::isnan(mapped_quad_bounds.height()));

  float left = ClampEdgeValue(floorf(mapped_quad_bounds.x()));
  float top = ClampEdgeValue(floorf(mapped_quad_bounds.y()));
  float right = ClampEdgeValue(ceilf(mapped_quad_bounds.right()));
  float bottom = ClampEdgeValue(ceilf(mapped_quad_bounds.bottom()));

  return LayoutRect(LayoutUnit::Clamp(left), LayoutUnit::Clamp(top),
                    LayoutUnit::Clamp(right - left),
                    LayoutUnit::Clamp(bottom - top));
}

void TransformationMatrix::TransformBox(gfx::BoxF& box) const {
  gfx::BoxF bounds;
  bool first_point = true;
  for (size_t i = 0; i < 2; ++i) {
    for (size_t j = 0; j < 2; ++j) {
      for (size_t k = 0; k < 2; ++k) {
        gfx::Point3F point(box.x(), box.y(), box.z());
        point +=
            gfx::Vector3dF(i * box.width(), j * box.height(), k * box.depth());
        point = MapPoint(point);
        if (first_point) {
          bounds.set_origin(point);
          first_point = false;
        } else {
          bounds.ExpandTo(point);
        }
      }
    }
  }
  box = bounds;
}

gfx::PointF TransformationMatrix::MapPoint(const gfx::PointF& p) const {
  if (IsIdentityOrTranslation())
    return TranslatePoint(p);
  return InternalMapPoint(p);
}

gfx::Point3F TransformationMatrix::MapPoint(const gfx::Point3F& p) const {
  if (IsIdentityOrTranslation()) {
    return gfx::Point3F(ClampToFloat(p.x() + matrix_[3][0]),
                        ClampToFloat(p.y() + matrix_[3][1]),
                        ClampToFloat(p.z() + matrix_[3][2]));
  }
  return InternalMapPoint(p);
}

gfx::Rect TransformationMatrix::MapRect(const gfx::Rect& rect) const {
  return gfx::ToEnclosingRect(MapRect(gfx::RectF(rect)));
}

LayoutRect TransformationMatrix::MapRect(const LayoutRect& r) const {
  return EnclosingLayoutRect(MapRect(gfx::RectF(r)));
}

gfx::RectF TransformationMatrix::MapRect(const gfx::RectF& r) const {
  auto result = IsIdentityOrTranslation()
                    ? gfx::RectF(TranslatePoint(r.origin()), r.size())
                    : InternalMapQuad(gfx::QuadF(r)).BoundingBox();
  // result.width()/height() may be infinity if e.g. right - left > float_max.
  DCHECK(std::isfinite(result.x()));
  DCHECK(std::isfinite(result.y()));
  result.set_width(ClampToFloat(result.width()));
  result.set_height(ClampToFloat(result.height()));
  return result;
}

gfx::QuadF TransformationMatrix::MapQuad(const gfx::QuadF& q) const {
  if (IsIdentityOrTranslation()) {
    return gfx::QuadF(TranslatePoint(q.p1()), TranslatePoint(q.p2()),
                      TranslatePoint(q.p3()), TranslatePoint(q.p4()));
  }
  return InternalMapQuad(q);
}

TransformationMatrix& TransformationMatrix::ScaleNonUniform(double sx,
                                                            double sy) {
  SetCol(0, Col(0) * sx);
  SetCol(1, Col(1) * sy);
  return *this;
}

TransformationMatrix& TransformationMatrix::Scale3d(double sx,
                                                    double sy,
                                                    double sz) {
  ScaleNonUniform(sx, sy);

  SetCol(2, Col(2) * sz);
  return *this;
}

TransformationMatrix& TransformationMatrix::Rotate(double angle) {
  angle = Deg2rad(angle);

  TransformationMatrix rotation_matrix;
  double sin_theta = std::sin(angle);
  double cos_theta = std::cos(angle);

  rotation_matrix.matrix_[0][0] = cos_theta;
  rotation_matrix.matrix_[0][1] = sin_theta;
  rotation_matrix.matrix_[1][0] = -sin_theta;
  rotation_matrix.matrix_[1][1] = cos_theta;

  Multiply(rotation_matrix);
  return *this;
}

TransformationMatrix& TransformationMatrix::Rotate3d(const Rotation& rotation) {
  return Rotate3d(rotation.axis.x(), rotation.axis.y(), rotation.axis.z(),
                  rotation.angle);
}

TransformationMatrix& TransformationMatrix::Rotate3d(double x,
                                                     double y,
                                                     double z,
                                                     double angle) {
  // Normalize the axis of rotation
  double length = std::sqrt(x * x + y * y + z * z);
  if (length == 0) {
    // A direction vector that cannot be normalized, such as [0, 0, 0], will
    // cause the rotation to not be applied.
    return *this;
  } else if (length != 1) {
    x /= length;
    y /= length;
    z /= length;
  }

  // Angles are in degrees. Switch to radians.
  angle = Deg2rad(angle);

  double sin_theta = std::sin(angle);
  double cos_theta = std::cos(angle);

  TransformationMatrix mat;

  // Optimize cases where the axis is along a major axis
  // Since we've already normalized the vector we don't need to check that the
  // other two dimensions are zero
  if (x == 1.0) {
    mat.matrix_[1][1] = cos_theta;
    mat.matrix_[1][2] = sin_theta;
    mat.matrix_[2][1] = -sin_theta;
    mat.matrix_[2][2] = cos_theta;
  } else if (y == 1.0) {
    mat.matrix_[0][0] = cos_theta;
    mat.matrix_[0][2] = -sin_theta;
    mat.matrix_[2][0] = sin_theta;
    mat.matrix_[2][2] = cos_theta;
  } else if (z == 1.0) {
    mat.matrix_[0][0] = cos_theta;
    mat.matrix_[0][1] = sin_theta;
    mat.matrix_[1][0] = -sin_theta;
    mat.matrix_[1][1] = cos_theta;
  } else {
    // This case is the rotation about an arbitrary unit vector.
    //
    // Formula is adapted from Wikipedia article on Rotation matrix,
    // http://en.wikipedia.org/wiki/Rotation_matrix#Rotation_matrix_from_axis_and_angle
    //
    // An alternate resource with the same matrix:
    // http://www.fastgraph.com/makegames/3drotation/
    //
    double one_minus_cos_theta = 1 - cos_theta;
    mat.matrix_[0][0] = cos_theta + x * x * one_minus_cos_theta;
    mat.matrix_[0][1] = y * x * one_minus_cos_theta + z * sin_theta;
    mat.matrix_[0][2] = z * x * one_minus_cos_theta - y * sin_theta;
    mat.matrix_[1][0] = x * y * one_minus_cos_theta - z * sin_theta;
    mat.matrix_[1][1] = cos_theta + y * y * one_minus_cos_theta;
    mat.matrix_[1][2] = z * y * one_minus_cos_theta + x * sin_theta;
    mat.matrix_[2][0] = x * z * one_minus_cos_theta + y * sin_theta;
    mat.matrix_[2][1] = y * z * one_minus_cos_theta - x * sin_theta;
    mat.matrix_[2][2] = cos_theta + z * z * one_minus_cos_theta;
  }
  Multiply(mat);
  return *this;
}

TransformationMatrix& TransformationMatrix::Rotate3d(double rx,
                                                     double ry,
                                                     double rz) {
  // Angles are in degrees. Switch to radians.
  rx = Deg2rad(rx);
  ry = Deg2rad(ry);
  rz = Deg2rad(rz);

  TransformationMatrix mat;

  double sin_theta = std::sin(rz);
  double cos_theta = std::cos(rz);

  mat.matrix_[0][0] = cos_theta;
  mat.matrix_[0][1] = sin_theta;
  mat.matrix_[1][0] = -sin_theta;
  mat.matrix_[1][1] = cos_theta;

  TransformationMatrix mat_y;
  sin_theta = std::sin(ry);
  cos_theta = std::cos(ry);

  mat_y.matrix_[0][0] = cos_theta;
  mat_y.matrix_[0][2] = -sin_theta;
  mat_y.matrix_[2][0] = sin_theta;
  mat_y.matrix_[2][2] = cos_theta;

  mat.Multiply(mat_y);

  TransformationMatrix mat_x;
  sin_theta = std::sin(rx);
  cos_theta = std::cos(rx);

  mat_x.matrix_[1][1] = cos_theta;
  mat_x.matrix_[1][2] = sin_theta;
  mat_x.matrix_[2][1] = -sin_theta;
  mat_x.matrix_[2][2] = cos_theta;

  mat.Multiply(mat_x);

  Multiply(mat);
  return *this;
}

TransformationMatrix& TransformationMatrix::Translate(double tx, double ty) {
  SetCol(3, tx * Col(0) + ty * Col(1) + Col(3));
  return *this;
}

TransformationMatrix& TransformationMatrix::Translate3d(double tx,
                                                        double ty,
                                                        double tz) {
  SetCol(3, tx * Col(0) + ty * Col(1) + tz * Col(2) + Col(3));
  return *this;
}

TransformationMatrix& TransformationMatrix::PostTranslate(double tx,
                                                          double ty) {
  if (tx != 0) {
    matrix_[0][0] += matrix_[0][3] * tx;
    matrix_[1][0] += matrix_[1][3] * tx;
    matrix_[2][0] += matrix_[2][3] * tx;
    matrix_[3][0] += matrix_[3][3] * tx;
  }

  if (ty != 0) {
    matrix_[0][1] += matrix_[0][3] * ty;
    matrix_[1][1] += matrix_[1][3] * ty;
    matrix_[2][1] += matrix_[2][3] * ty;
    matrix_[3][1] += matrix_[3][3] * ty;
  }
  return *this;
}

TransformationMatrix& TransformationMatrix::PostTranslate3d(double tx,
                                                            double ty,
                                                            double tz) {
  PostTranslate(tx, ty);
  if (tz != 0) {
    matrix_[0][2] += matrix_[0][3] * tz;
    matrix_[1][2] += matrix_[1][3] * tz;
    matrix_[2][2] += matrix_[2][3] * tz;
    matrix_[3][2] += matrix_[3][3] * tz;
  }
  return *this;
}

TransformationMatrix& TransformationMatrix::Skew(double sx, double sy) {
  // Angles are in degrees. Switch to radians.
  sx = Deg2rad(sx);
  sy = Deg2rad(sy);

  TransformationMatrix mat;
  // Note that the y shear goes in the first row.
  mat.matrix_[0][1] = std::tan(sy);
  // And the x shear in the second row.
  mat.matrix_[1][0] = std::tan(sx);

  Multiply(mat);
  return *this;
}

TransformationMatrix& TransformationMatrix::ApplyPerspective(double p) {
  TransformationMatrix mat;
  if (p != 0)
    mat.matrix_[2][3] = -1 / p;

  Multiply(mat);
  return *this;
}

TransformationMatrix& TransformationMatrix::ApplyTransformOrigin(double x,
                                                                 double y,
                                                                 double z) {
  PostTranslate3d(x, y, z);
  Translate3d(-x, -y, -z);
  return *this;
}

TransformationMatrix& TransformationMatrix::Zoom(double zoom_factor) {
  matrix_[0][3] /= zoom_factor;
  matrix_[1][3] /= zoom_factor;
  matrix_[2][3] /= zoom_factor;
  matrix_[3][0] *= zoom_factor;
  matrix_[3][1] *= zoom_factor;
  matrix_[3][2] *= zoom_factor;
  return *this;
}

// Calculates *this = *this * mat.
// Note: As we are using the column vector convention, i.e. T * P,
// (lhs * rhs) * P = lhs * (rhs * P)
// That means from the perspective of the transformed object, the combined
// transform is equal to applying the rhs(mat) first, then lhs(*this) second.
// For example:
// TransformationMatrix lhs; lhs.Rotate(90.f);
// TransformationMatrix rhs; rhs.Translate(12.f, 34.f);
// TransformationMatrix prod = lhs;
// prod.Multiply(rhs);
// lhs.MapPoint(rhs.MapPoint(p)) == prod.MapPoint(p)
// Also 'prod' corresponds to CSS transform:rotateZ(90deg)translate(12px,34px).
TransformationMatrix& TransformationMatrix::Multiply(
    const TransformationMatrix& mat) {
  auto c0 = Col(0);
  auto c1 = Col(1);
  auto c2 = Col(2);
  auto c3 = Col(3);

  auto mc0 = mat.Col(0);
  auto mc1 = mat.Col(1);
  auto mc2 = mat.Col(2);
  auto mc3 = mat.Col(3);

  SetCol(0, c0 * mc0[0] + c1 * mc0[1] + c2 * mc0[2] + c3 * mc0[3]);
  SetCol(1, c0 * mc1[0] + c1 * mc1[1] + c2 * mc1[2] + c3 * mc1[3]);
  SetCol(2, c0 * mc2[0] + c1 * mc2[1] + c2 * mc2[2] + c3 * mc2[3]);
  SetCol(3, c0 * mc3[0] + c1 * mc3[1] + c2 * mc3[2] + c3 * mc3[3]);
  return *this;
}

gfx::PointF TransformationMatrix::TranslatePoint(const gfx::PointF& p) const {
  DCHECK(IsIdentityOrTranslation());
  return gfx::PointF(ClampToFloat(p.x() + matrix_[3][0]),
                     ClampToFloat(p.y() + matrix_[3][1]));
}

gfx::PointF TransformationMatrix::InternalMapPoint(
    const gfx::PointF& source_point) const {
  DCHECK(!IsIdentityOrTranslation());
  double x = source_point.x();
  double y = source_point.y();
  double result_x = matrix_[3][0] + x * matrix_[0][0] + y * matrix_[1][0];
  double result_y = matrix_[3][1] + x * matrix_[0][1] + y * matrix_[1][1];
  double w = matrix_[3][3] + x * matrix_[0][3] + y * matrix_[1][3];
  if (w != 1 && w != 0) {
    result_x /= w;
    result_y /= w;
  }
  return gfx::PointF(ClampToFloat(result_x), ClampToFloat(result_y));
}

gfx::Point3F TransformationMatrix::InternalMapPoint(
    const gfx::Point3F& source_point) const {
  DCHECK(!IsIdentityOrTranslation());
  double x = source_point.x();
  double y = source_point.y();
  double z = source_point.z();
  double result_x =
      matrix_[3][0] + x * matrix_[0][0] + y * matrix_[1][0] + z * matrix_[2][0];
  double result_y =
      matrix_[3][1] + x * matrix_[0][1] + y * matrix_[1][1] + z * matrix_[2][1];
  double result_z =
      matrix_[3][2] + x * matrix_[0][2] + y * matrix_[1][2] + z * matrix_[2][2];
  double w =
      matrix_[3][3] + x * matrix_[0][3] + y * matrix_[1][3] + z * matrix_[2][3];
  if (w != 1 && w != 0) {
    result_x /= w;
    result_y /= w;
    result_z /= w;
  }
  return gfx::Point3F(ClampToFloat(result_x), ClampToFloat(result_y),
                      ClampToFloat(result_z));
}

gfx::QuadF TransformationMatrix::InternalMapQuad(const gfx::QuadF& q) const {
  return gfx::QuadF(InternalMapPoint(q.p1()), InternalMapPoint(q.p2()),
                    InternalMapPoint(q.p3()), InternalMapPoint(q.p4()));
}

bool TransformationMatrix::IsInvertible() const {
  return InternalInverse<true>(nullptr);
}

TransformationMatrix TransformationMatrix::Inverse() const {
  TransformationMatrix m;
  InternalInverse<false>(&m);
  return m;
}

bool TransformationMatrix::GetInverse(TransformationMatrix* m) const {
  DCHECK(m);
  if (InternalInverse<false>(m))
    return true;

  m->MakeIdentity();
  return false;
}

template <bool check_invertibility_only>
bool TransformationMatrix::InternalInverse(TransformationMatrix* result) const {
  DCHECK_EQ(check_invertibility_only, !result);

  if (IsIdentityOrTranslation()) {
    // Identity matrix.
    if (gfx::AllTrue(Col(3) == Double4{0, 0, 0, 1})) {
      if (!check_invertibility_only)
        result->MakeIdentity();
      return true;
    }

    // Translation.
    if (!check_invertibility_only) {
      *result = TransformationMatrix().Translate3d(
          -matrix_[3][0], -matrix_[3][1], -matrix_[3][2]);
    }
    return true;
  }

  Double4 c0 = Col(0);
  Double4 c1 = Col(1);
  Double4 c2 = Col(2);
  Double4 c3 = Col(3);

  bool invertible =
      InverseWithDouble4Cols<check_invertibility_only>(c0, c1, c2, c3);
  if (invertible && !check_invertibility_only) {
    result->SetCol(0, c0);
    result->SetCol(1, c1);
    result->SetCol(2, c2);
    result->SetCol(3, c3);
  }
  return invertible;
}

// This is based on
// https://github.com/niswegmann/small-matrix-inverse/blob/master/invert4x4_llvm.h,
// which is based on Intel AP-928 "Streaming SIMD Extensions - Inverse of 4x4
// Matrix": https://drive.google.com/file/d/0B9rh9tVI0J5mX1RUam5nZm85OFE/view.
template <bool check_invertibility_only>
bool TransformationMatrix::InverseWithDouble4Cols(Double4& c0,
                                                  Double4& c1,
                                                  Double4& c2,
                                                  Double4& c3) {
  // Note that r1 and r3 have components 2/3 and 0/1 swapped.
  Double4 r0 = {c0[0], c1[0], c2[0], c3[0]};
  Double4 r1 = {c2[1], c3[1], c0[1], c1[1]};
  Double4 r2 = {c0[2], c1[2], c2[2], c3[2]};
  Double4 r3 = {c2[3], c3[3], c0[3], c1[3]};

  auto swap_hi_lo = [](Double4 v) -> Double4 {
    return Double4{v[2], v[3], v[0], v[1]};
  };
  auto swap_in_pairs = [](Double4 v) -> Double4 {
    return Double4{v[1], v[0], v[3], v[2]};
  };

  Double4 t = swap_in_pairs(r2 * r3);
  c0 = r1 * t;
  c1 = r0 * t;

  t = swap_hi_lo(t);
  c0 = r1 * t - c0;
  c1 = swap_hi_lo(r0 * t - c1);

  t = swap_in_pairs(r1 * r2);
  c0 += r3 * t;
  c3 = r0 * t;

  t = swap_hi_lo(t);
  c0 -= r3 * t;
  c3 = swap_hi_lo(r0 * t - c3);

  t = swap_in_pairs(swap_hi_lo(r1) * r3);
  r2 = swap_hi_lo(r2);
  c0 += r2 * t;
  c2 = r0 * t;

  t = swap_hi_lo(t);
  c0 -= r2 * t;

  Double4 det = r0 * c0;
  det += swap_hi_lo(det);
  det += swap_in_pairs(det);
  if (!std::isnormal(det[0]))
    return false;
  if (check_invertibility_only)
    return true;

  c2 = swap_hi_lo(r0 * t - c2);

  t = swap_in_pairs(r0 * r1);
  c2 = r3 * t + c2;
  c3 = r2 * t - c3;

  t = swap_hi_lo(t);
  c2 = r3 * t - c2;
  c3 -= r2 * t;

  t = swap_in_pairs(r0 * r3);
  c1 -= r2 * t;
  c2 = r1 * t + c2;

  t = swap_hi_lo(t);
  c1 = r2 * t + c1;
  c2 -= r1 * t;

  t = swap_in_pairs(r0 * r2);
  c1 = r3 * t + c1;
  c3 -= r1 * t;

  t = swap_hi_lo(t);
  c1 -= r3 * t;
  c3 = r1 * t + c3;

  det = 1 / det;
  c0 *= det;
  c1 *= det;
  c2 *= det;
  c3 *= det;
  return true;
}

void TransformationMatrix::MakeAffine() {
  matrix_[0][2] = 0;
  matrix_[0][3] = 0;

  matrix_[1][2] = 0;
  matrix_[1][3] = 0;

  matrix_[2][0] = 0;
  matrix_[2][1] = 0;
  matrix_[2][2] = 1;
  matrix_[2][3] = 0;

  matrix_[3][2] = 0;
  matrix_[3][3] = 1;
}

AffineTransform TransformationMatrix::ToAffineTransform() const {
  return AffineTransform(matrix_[0][0], matrix_[0][1], matrix_[1][0],
                         matrix_[1][1], matrix_[3][0], matrix_[3][1]);
}

void TransformationMatrix::FlattenTo2d() {
  matrix_[2][0] = 0;
  matrix_[2][1] = 0;
  matrix_[0][2] = 0;
  matrix_[1][2] = 0;
  matrix_[2][2] = 1;
  matrix_[3][2] = 0;
  matrix_[2][3] = 0;
}

static inline void BlendFloat(double& from, double to, double progress) {
  if (from != to)
    from = from + (to - from) * progress;
}

void TransformationMatrix::Blend(const TransformationMatrix& from,
                                 double progress) {
  if (from.IsIdentity() && IsIdentity())
    return;

  if (from.Is2dTransform() && Is2dTransform()) {
    Blend2D(from, progress);
    return;
  }

  // decompose
  DecomposedType from_decomp;
  DecomposedType to_decomp;
  if (!from.Decompose(from_decomp) || !Decompose(to_decomp)) {
    if (progress < 0.5)
      *this = from;
    return;
  }

  // interpolate
  BlendFloat(from_decomp.scale_x, to_decomp.scale_x, progress);
  BlendFloat(from_decomp.scale_y, to_decomp.scale_y, progress);
  BlendFloat(from_decomp.scale_z, to_decomp.scale_z, progress);
  BlendFloat(from_decomp.skew_xy, to_decomp.skew_xy, progress);
  BlendFloat(from_decomp.skew_xz, to_decomp.skew_xz, progress);
  BlendFloat(from_decomp.skew_yz, to_decomp.skew_yz, progress);
  BlendFloat(from_decomp.translate_x, to_decomp.translate_x, progress);
  BlendFloat(from_decomp.translate_y, to_decomp.translate_y, progress);
  BlendFloat(from_decomp.translate_z, to_decomp.translate_z, progress);
  BlendFloat(from_decomp.perspective_x, to_decomp.perspective_x, progress);
  BlendFloat(from_decomp.perspective_y, to_decomp.perspective_y, progress);
  BlendFloat(from_decomp.perspective_z, to_decomp.perspective_z, progress);
  BlendFloat(from_decomp.perspective_w, to_decomp.perspective_w, progress);

  Slerp(from_decomp, to_decomp, progress);

  // recompose
  Recompose(from_decomp);
}

void TransformationMatrix::Blend2D(const TransformationMatrix& from,
                                   double progress) {
  // Decompose into scale, rotate, translate and skew transforms.
  Decomposed2dType from_decomp;
  Decomposed2dType to_decomp;
  if (!from.Decompose2D(from_decomp) || !Decompose2D(to_decomp)) {
    if (progress < 0.5)
      *this = from;
    return;
  }

  // Take the shorter of the clockwise or counter-clockwise paths.
  double rotation = abs(from_decomp.angle - to_decomp.angle);
  DCHECK(rotation < 2 * M_PI);
  if (rotation > M_PI) {
    if (from_decomp.angle > to_decomp.angle) {
      from_decomp.angle -= 2 * M_PI;
    } else {
      to_decomp.angle -= 2 * M_PI;
    }
  }

  // Interpolate.
  BlendFloat(from_decomp.scale_x, to_decomp.scale_x, progress);
  BlendFloat(from_decomp.scale_y, to_decomp.scale_y, progress);
  BlendFloat(from_decomp.skew_xy, to_decomp.skew_xy, progress);
  BlendFloat(from_decomp.translate_x, to_decomp.translate_x, progress);
  BlendFloat(from_decomp.translate_y, to_decomp.translate_y, progress);
  BlendFloat(from_decomp.angle, to_decomp.angle, progress);

  // Recompose.
  Recompose2D(from_decomp);
}

// Decompose a 2D transformation matrix of the form:
// [m11 m21 0 m41]
// [m12 m22 0 m42]
// [ 0   0  1  0 ]
// [ 0   0  0  1 ]
//
// The decomposition is of the form:
// M = translate * rotate * skew * scale
//     [1 0 0 Tx] [cos(R) -sin(R) 0 0] [1 K 0 0] [Sx 0  0 0]
//   = [0 1 0 Ty] [sin(R)  cos(R) 0 0] [0 1 0 0] [0  Sy 0 0]
//     [0 0 1 0 ] [  0       0    1 0] [0 0 1 0] [0  0  1 0]
//     [0 0 0 1 ] [  0       0    0 1] [0 0 0 1] [0  0  0 1]
//
bool TransformationMatrix::Decompose2D(Decomposed2dType& decomp) const {
  if (!Is2dTransform()) {
    LOG(ERROR) << "2-D decomposition cannot be performed on a 3-D transform.";
    return false;
  }

  double m11 = matrix_[0][0];
  double m21 = matrix_[1][0];
  double m12 = matrix_[0][1];
  double m22 = matrix_[1][1];

  double determinant = m11 * m22 - m12 * m21;
  // Test for matrix being singular.
  if (determinant == 0) {
    return false;
  }

  // Translation transform.
  // [m11 m21 0 m41]    [1 0 0 Tx] [m11 m21 0 0]
  // [m12 m22 0 m42]  = [0 1 0 Ty] [m12 m22 0 0]
  // [ 0   0  1  0 ]    [0 0 1 0 ] [ 0   0  1 0]
  // [ 0   0  0  1 ]    [0 0 0 1 ] [ 0   0  0 1]
  decomp.translate_x = matrix_[3][0];
  decomp.translate_y = matrix_[3][1];

  // For the remainder of the decomposition process, we can focus on the upper
  // 2x2 submatrix
  // [m11 m21] = [cos(R) -sin(R)] [1 K] [Sx 0 ]
  // [m12 m22]   [sin(R)  cos(R)] [0 1] [0  Sy]
  //           = [Sx*cos(R) Sy*(K*cos(R) - sin(R))]
  //             [Sx*sin(R) Sy*(K*sin(R) + cos(R))]

  // Determine sign of the x and y scale.
  decomp.scale_x = 1;
  decomp.scale_y = 1;
  if (determinant < 0) {
    // If the determinant is negative, we need to flip either the x or y scale.
    // Flipping both is equivalent to rotating by 180 degrees.
    // Flip the axis with the minimum unit vector dot product.
    if (m11 < m22) {
      decomp.scale_x = -decomp.scale_x;
    } else {
      decomp.scale_y = -decomp.scale_y;
    }
  }

  // X Scale.
  // m11^2 + m12^2 = Sx^2*(cos^2(R) + sin^2(R)) = Sx^2.
  // Sx = +/-sqrt(m11^2 + m22^2)
  decomp.scale_x *= sqrt(m11 * m11 + m12 * m12);
  m11 /= decomp.scale_x;
  m12 /= decomp.scale_x;

  // Post normalization, the submatrix is now of the form:
  // [m11 m21] = [cos(R)  Sy*(K*cos(R) - sin(R))]
  // [m12 m22]   [sin(R)  Sy*(K*sin(R) + cos(R))]

  // XY Shear.
  // m11 * m21 + m12 * m22 = Sy*K*cos^2(R) - Sy*sin(R)*cos(R) +
  //                         Sy*K*sin^2(R) + Sy*cos(R)*sin(R)
  //                       = Sy*K
  double scaledShear = m11 * m21 + m12 * m22;
  m21 -= m11 * scaledShear;
  m22 -= m12 * scaledShear;

  // Post normalization, the submatrix is now of the form:
  // [m11 m21] = [cos(R)  -Sy*sin(R)]
  // [m12 m22]   [sin(R)   Sy*cos(R)]

  // Y Scale.
  // Similar process to determining x-scale.
  decomp.scale_y *= sqrt(m21 * m21 + m22 * m22);
  m21 /= decomp.scale_y;
  m22 /= decomp.scale_y;
  decomp.skew_xy = scaledShear / decomp.scale_y;

  // Rotation transform.
  decomp.angle = atan2(m12, m11);
  return true;
}

void TransformationMatrix::Recompose(const DecomposedType& decomp) {
  MakeIdentity();

  // first apply perspective
  matrix_[0][3] = decomp.perspective_x;
  matrix_[1][3] = decomp.perspective_y;
  matrix_[2][3] = decomp.perspective_z;
  matrix_[3][3] = decomp.perspective_w;

  // now translate
  Translate3d(decomp.translate_x, decomp.translate_y, decomp.translate_z);

  // apply rotation
  double xx = decomp.quaternion_x * decomp.quaternion_x;
  double xy = decomp.quaternion_x * decomp.quaternion_y;
  double xz = decomp.quaternion_x * decomp.quaternion_z;
  double xw = decomp.quaternion_x * decomp.quaternion_w;
  double yy = decomp.quaternion_y * decomp.quaternion_y;
  double yz = decomp.quaternion_y * decomp.quaternion_z;
  double yw = decomp.quaternion_y * decomp.quaternion_w;
  double zz = decomp.quaternion_z * decomp.quaternion_z;
  double zw = decomp.quaternion_z * decomp.quaternion_w;

  // Construct a composite rotation matrix from the quaternion values.
  // Arguments are in column order.
  // https://en.wikipedia.org/wiki/Rotation_matrix#Quaternion
  TransformationMatrix rotation_matrix(1 - 2 * (yy + zz),     // Q_xx
                                       2 * (xy + zw),         // Q_yx
                                       2 * (xz - yw), 0,      // Q_zx
                                       2 * (xy - zw),         // Q_xy
                                       1 - 2 * (xx + zz),     // Q_yy
                                       2 * (yz + xw), 0,      // Q_zy
                                       2 * (xz + yw),         // Q_xz
                                       2 * (yz - xw),         // Q_yz
                                       1 - 2 * (xx + yy), 0,  // Q_zz
                                       0, 0, 0, 1);

  Multiply(rotation_matrix);

  // now apply skew
  if (decomp.skew_yz) {
    TransformationMatrix tmp;
    tmp.matrix_[2][1] = decomp.skew_yz;
    Multiply(tmp);
  }

  if (decomp.skew_xz) {
    TransformationMatrix tmp;
    tmp.matrix_[2][0] = decomp.skew_xz;
    Multiply(tmp);
  }

  if (decomp.skew_xy) {
    TransformationMatrix tmp;
    tmp.matrix_[1][0] = decomp.skew_xy;
    Multiply(tmp);
  }

  // finally, apply scale
  Scale3d(decomp.scale_x, decomp.scale_y, decomp.scale_z);
}

void TransformationMatrix::Recompose2D(const Decomposed2dType& decomp) {
  MakeIdentity();

  // Translate transform.
  matrix_[3][0] = decomp.translate_x;
  matrix_[3][1] = decomp.translate_y;

  // Rotate transform.
  double cos_angle = cos(decomp.angle);
  double sin_angle = sin(decomp.angle);
  matrix_[0][0] = cos_angle;
  matrix_[1][0] = -sin_angle;
  matrix_[0][1] = sin_angle;
  matrix_[1][1] = cos_angle;

  // skew transform.
  if (decomp.skew_xy) {
    TransformationMatrix skew_transform;
    skew_transform.matrix_[1][0] = decomp.skew_xy;
    Multiply(skew_transform);
  }

  // Scale transform.
  Scale3d(decomp.scale_x, decomp.scale_y, 1);
}

bool TransformationMatrix::IsInteger2DTranslation() const {
  if (!IsIdentityOr2DTranslation())
    return false;

  // Check for non-integer translate X/Y.
  if (ClampTo<int>(matrix_[3][0]) != matrix_[3][0] ||
      ClampTo<int>(matrix_[3][1]) != matrix_[3][1])
    return false;

  return true;
}

// This is the same as gfx::Transform::Preserves2dAxisAlignment().
bool TransformationMatrix::Preserves2dAxisAlignment() const {
  // Check whether an axis aligned 2-dimensional rect would remain axis-aligned
  // after being transformed by this matrix (and implicitly projected by
  // dropping any non-zero z-values).
  //
  // The 4th column can be ignored because translations don't affect axis
  // alignment. The 3rd column can be ignored because we are assuming 2d
  // inputs, where z-values will be zero. The 3rd row can also be ignored
  // because we are assuming 2d outputs, and any resulting z-value is dropped
  // anyway. For the inner 2x2 portion, the only effects that keep a rect axis
  // aligned are (1) swapping axes and (2) scaling axes. This can be checked by
  // verifying only 1 element of every column and row is non-zero.  Degenerate
  // cases that project the x or y dimension to zero are considered to preserve
  // axis alignment.
  //
  // If the matrix does have perspective component that is affected by x or y
  // values: The current implementation conservatively assumes that axis
  // alignment is not preserved.
  bool has_x_or_y_perspective = matrix_[0][3] != 0 || matrix_[1][3] != 0;
  if (has_x_or_y_perspective)
    return false;

  // Use float epsilon here, not double, to round very small rotations back
  // to zero.
  constexpr double kEpsilon = std::numeric_limits<float>::epsilon();

  int num_non_zero_in_row_1 = 0;
  int num_non_zero_in_row_2 = 0;
  int num_non_zero_in_col_1 = 0;
  int num_non_zero_in_col_2 = 0;
  if (std::abs(matrix_[0][0]) > kEpsilon) {
    num_non_zero_in_col_1++;
    num_non_zero_in_row_1++;
  }
  if (std::abs(matrix_[0][1]) > kEpsilon) {
    num_non_zero_in_col_1++;
    num_non_zero_in_row_2++;
  }
  if (std::abs(matrix_[1][0]) > kEpsilon) {
    num_non_zero_in_col_2++;
    num_non_zero_in_row_1++;
  }
  if (std::abs(matrix_[1][1]) > kEpsilon) {
    num_non_zero_in_col_2++;
    num_non_zero_in_row_2++;
  }

  return num_non_zero_in_row_1 <= 1 && num_non_zero_in_row_2 <= 1 &&
         num_non_zero_in_col_1 <= 1 && num_non_zero_in_col_2 <= 1;
}

void TransformationMatrix::GetColMajorF(float result[16]) const {
  // This doesn't use ClampToFloat() intentionally to preserve NaN and infinity
  // values.
  for (int i = 0; i < 16; i++)
    result[i] = static_cast<float>(ColMajorData()[i]);
}

SkM44 TransformationMatrix::ToSkM44() const {
  return SkM44(ClampToFloat(matrix_[0][0]), ClampToFloat(matrix_[1][0]),
               ClampToFloat(matrix_[2][0]), ClampToFloat(matrix_[3][0]),
               ClampToFloat(matrix_[0][1]), ClampToFloat(matrix_[1][1]),
               ClampToFloat(matrix_[2][1]), ClampToFloat(matrix_[3][1]),
               ClampToFloat(matrix_[0][2]), ClampToFloat(matrix_[1][2]),
               ClampToFloat(matrix_[2][2]), ClampToFloat(matrix_[3][2]),
               ClampToFloat(matrix_[0][3]), ClampToFloat(matrix_[1][3]),
               ClampToFloat(matrix_[2][3]), ClampToFloat(matrix_[3][3]));
}

gfx::Transform TransformationMatrix::ToTransform() const {
  return gfx::Transform(
      ClampToFloat(matrix_[0][0]), ClampToFloat(matrix_[1][0]),
      ClampToFloat(matrix_[2][0]), ClampToFloat(matrix_[3][0]),
      ClampToFloat(matrix_[0][1]), ClampToFloat(matrix_[1][1]),
      ClampToFloat(matrix_[2][1]), ClampToFloat(matrix_[3][1]),
      ClampToFloat(matrix_[0][2]), ClampToFloat(matrix_[1][2]),
      ClampToFloat(matrix_[2][2]), ClampToFloat(matrix_[3][2]),
      ClampToFloat(matrix_[0][3]), ClampToFloat(matrix_[1][3]),
      ClampToFloat(matrix_[2][3]), ClampToFloat(matrix_[3][3]));
}

String TransformationMatrix::ToString(bool as_matrix) const {
  if (as_matrix) {
    // Return as a matrix in row-major order.
    return String::Format(
        "[%lg,%lg,%lg,%lg,\n%lg,%lg,%lg,%lg,\n%lg,%lg,%lg,%lg,\n%lg,%lg,%lg,%"
        "lg]",
        matrix_[0][0], matrix_[1][0], matrix_[2][0], matrix_[3][0],
        matrix_[0][1], matrix_[1][1], matrix_[2][1], matrix_[3][1],
        matrix_[0][2], matrix_[1][2], matrix_[2][2], matrix_[3][2],
        matrix_[0][3], matrix_[1][3], matrix_[2][3], matrix_[3][3]);
  }

  TransformationMatrix::DecomposedType decomposition;
  if (!Decompose(decomposition))
    return ToString(true) + " (degenerate)";

  if (IsIdentityOrTranslation()) {
    if (decomposition.translate_x == 0 && decomposition.translate_y == 0 &&
        decomposition.translate_z == 0)
      return "identity";
    return String::Format("translation(%lg,%lg,%lg)", decomposition.translate_x,
                          decomposition.translate_y, decomposition.translate_z);
  }

  return String::Format(
      "translation(%lg,%lg,%lg), scale(%lg,%lg,%lg), skew(%lg,%lg,%lg), "
      "quaternion(%lg,%lg,%lg,%lg), perspective(%lg,%lg,%lg,%lg)",
      decomposition.translate_x, decomposition.translate_y,
      decomposition.translate_z, decomposition.scale_x, decomposition.scale_y,
      decomposition.scale_z, decomposition.skew_xy, decomposition.skew_xz,
      decomposition.skew_yz, decomposition.quaternion_x,
      decomposition.quaternion_y, decomposition.quaternion_z,
      decomposition.quaternion_w, decomposition.perspective_x,
      decomposition.perspective_y, decomposition.perspective_z,
      decomposition.perspective_w);
}

std::ostream& operator<<(std::ostream& ostream,
                         const TransformationMatrix& transform) {
  return ostream << transform.ToString();
}

}  // namespace blink
