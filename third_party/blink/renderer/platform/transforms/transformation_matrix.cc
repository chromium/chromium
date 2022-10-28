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
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

TransformationMatrix::TransformationMatrix(const AffineTransform& t) {
  *this = Affine(t.A(), t.B(), t.C(), t.D(), t.E(), t.F());
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

  if (rc(2, 2) == 0) {
    // In this case, the projection plane is parallel to the ray we are trying
    // to trace, and there is no well-defined value for the projection.
    return gfx::PointF();
  }

  double x = p.x();
  double y = p.y();
  double z = -(rc(2, 0) * x + rc(2, 1) * y + rc(2, 3)) / rc(2, 2);

  double out_x = x * rc(0, 0) + y * rc(0, 1) + z * rc(0, 2) + rc(0, 3);
  double out_y = x * rc(1, 0) + y * rc(1, 1) + z * rc(1, 2) + rc(1, 3);

  double w = x * rc(3, 0) + y * rc(3, 1) + z * rc(3, 2) + rc(3, 3);
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

LayoutRect TransformationMatrix::MapRect(const LayoutRect& r) const {
  return EnclosingLayoutRect(MapRect(gfx::RectF(r)));
}

gfx::QuadF TransformationMatrix::MapQuad(const gfx::QuadF& q) const {
  return gfx::QuadF(MapPoint(q.p1()), MapPoint(q.p2()), MapPoint(q.p3()),
                    MapPoint(q.p4()));
}

void TransformationMatrix::ApplyTransformOrigin(double x, double y, double z) {
  PostTranslate3d(x, y, z);
  Translate3d(-x, -y, -z);
}

void TransformationMatrix::Zoom(double zoom_factor) {
  set_rc(3, 0, rc(3, 0) / zoom_factor);
  set_rc(3, 1, rc(3, 1) / zoom_factor);
  set_rc(3, 2, rc(3, 2) / zoom_factor);
  set_rc(0, 3, rc(0, 3) * zoom_factor);
  set_rc(1, 3, rc(1, 3) * zoom_factor);
  set_rc(2, 3, rc(2, 3) * zoom_factor);
}

TransformationMatrix TransformationMatrix::Inverse() const {
  TransformationMatrix m;
  return GetInverse(&m) ? m : m;
}

AffineTransform TransformationMatrix::ToAffineTransform() const {
  return AffineTransform(rc(0, 0), rc(1, 0), rc(0, 1), rc(1, 1), rc(0, 3),
                         rc(1, 3));
}

static inline void BlendFloat(double& from, double to, double progress) {
  if (from != to)
    from = from + (to - from) * progress;
}

void TransformationMatrix::Blend(const TransformationMatrix& from,
                                 double progress) {
  if (gfx::Transform::Blend(from, progress))
    return;
  if (progress < 0.5)
    *this = from;
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

  double m11 = rc(0, 0);
  double m21 = rc(0, 1);
  double m12 = rc(1, 0);
  double m22 = rc(1, 1);

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
  decomp.translate_x = rc(0, 3);
  decomp.translate_y = rc(1, 3);

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

void TransformationMatrix::Recompose(const gfx::DecomposedTransform& decomp) {
  static_cast<gfx::Transform&>(*this) = Compose(decomp);
}

void TransformationMatrix::Recompose2D(const Decomposed2dType& decomp) {
  MakeIdentity();

  // Translate transform.
  set_rc(0, 3, decomp.translate_x);
  set_rc(1, 3, decomp.translate_y);

  // Rotate transform.
  double cos_angle = cos(decomp.angle);
  double sin_angle = sin(decomp.angle);
  set_rc(0, 0, cos_angle);
  set_rc(0, 1, -sin_angle);
  set_rc(1, 0, sin_angle);
  set_rc(1, 1, cos_angle);

  // skew transform.
  if (decomp.skew_xy) {
    TransformationMatrix skew_transform;
    skew_transform.set_rc(0, 1, decomp.skew_xy);
    PreConcat(skew_transform);
  }

  // Scale transform.
  Scale3d(decomp.scale_x, decomp.scale_y, 1);
}

SkM44 TransformationMatrix::ToSkM44() const {
  return SkM44(
      ClampToFloat(rc(0, 0)), ClampToFloat(rc(0, 1)), ClampToFloat(rc(0, 2)),
      ClampToFloat(rc(0, 3)), ClampToFloat(rc(1, 0)), ClampToFloat(rc(1, 1)),
      ClampToFloat(rc(1, 2)), ClampToFloat(rc(1, 3)), ClampToFloat(rc(2, 0)),
      ClampToFloat(rc(2, 1)), ClampToFloat(rc(2, 2)), ClampToFloat(rc(2, 3)),
      ClampToFloat(rc(3, 0)), ClampToFloat(rc(3, 1)), ClampToFloat(rc(3, 2)),
      ClampToFloat(rc(3, 3)));
}

String TransformationMatrix::ToString(bool as_matrix) const {
  if (as_matrix) {
    // Return as a matrix in row-major order.
    return String::Format(
        "[%lg,%lg,%lg,%lg,\n%lg,%lg,%lg,%lg,\n%lg,%lg,%lg,%lg,\n%lg,%lg,%lg,%"
        "lg]",
        rc(0, 0), rc(0, 1), rc(0, 2), rc(0, 3), rc(1, 0), rc(1, 1), rc(1, 2),
        rc(1, 3), rc(2, 0), rc(2, 1), rc(2, 2), rc(2, 3), rc(3, 0), rc(3, 1),
        rc(3, 2), rc(3, 3));
  }

  absl::optional<gfx::DecomposedTransform> decomp = Decompose();
  if (!decomp)
    return ToString(true) + " (degenerate)";

  if (IsIdentityOrTranslation()) {
    if (decomp->translate[0] == 0 && decomp->translate[1] == 0 &&
        decomp->translate[2] == 0)
      return "identity";
    return String::Format("translation(%lg,%lg,%lg)", decomp->translate[0],
                          decomp->translate[1], decomp->translate[2]);
  }

  return String::Format(
      "translation(%lg,%lg,%lg), scale(%lg,%lg,%lg), skew(%lg,%lg,%lg), "
      "quaternion(%lg,%lg,%lg,%lg), perspective(%lg,%lg,%lg,%lg)",
      decomp->translate[0], decomp->translate[1], decomp->translate[2],
      decomp->scale[0], decomp->scale[1], decomp->scale[2], decomp->skew[0],
      decomp->skew[1], decomp->skew[2], decomp->quaternion.x(),
      decomp->quaternion.y(), decomp->quaternion.z(), decomp->quaternion.w(),
      decomp->perspective[0], decomp->perspective[1], decomp->perspective[2],
      decomp->perspective[3]);
}

std::ostream& operator<<(std::ostream& ostream,
                         const TransformationMatrix& transform) {
  return ostream << transform.ToString();
}

}  // namespace blink
