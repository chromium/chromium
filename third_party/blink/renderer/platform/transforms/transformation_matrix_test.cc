// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/testing/transformation_matrix_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {

namespace {

gfx::DecomposedTransform GetRotationDecomp(double x,
                                           double y,
                                           double z,
                                           double w) {
  gfx::DecomposedTransform decomp;
  decomp.quaternion = gfx::Quaternion(x, y, z, w);
  return decomp;
}

}  // end namespace

TEST(TransformationMatrixTest, NonInvertableBlendTest) {
  TransformationMatrix from;
  auto to = TransformationMatrix::ColMajor(2.7133590938, 0.0, 0.0, 0.0, 0.0,
                                           2.4645137761, 0.0, 0.0, 0.0, 0.0,
                                           0.00, 0.01, 0.02, 0.03, 0.04, 0.05);
  TransformationMatrix result;

  result = to;
  result.Blend(from, 0.25);
  EXPECT_EQ(result, from);

  result = to;
  result.Blend(from, 0.75);
  EXPECT_EQ(result, to);
}

TEST(TransformationMatrixTest, BasicOperations) {
  // Just some arbitrary matrix that introduces no rounding, and is unlikely
  // to commute with other operations.
  auto m = TransformationMatrix::ColMajor(2.f, 3.f, 5.f, 0.f, 7.f, 11.f, 13.f,
                                          0.f, 17.f, 19.f, 23.f, 0.f, 29.f,
                                          31.f, 37.f, 1.f);

  gfx::Point3F p(41.f, 43.f, 47.f);

  EXPECT_EQ(gfx::Point3F(1211.f, 1520.f, 1882.f), m.MapPoint(p));

  {
    TransformationMatrix n;
    n.Scale(2.f);
    EXPECT_EQ(gfx::Point3F(82.f, 86.f, 47.f), n.MapPoint(p));

    TransformationMatrix mn = m;
    mn.Scale(2.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    TransformationMatrix n;
    n.Scale(2.f, 3.f);
    EXPECT_EQ(gfx::Point3F(82.f, 129.f, 47.f), n.MapPoint(p));

    TransformationMatrix mn = m;
    mn.Scale(2.f, 3.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    TransformationMatrix n;
    n.Scale3d(2.f, 3.f, 4.f);
    EXPECT_EQ(gfx::Point3F(82.f, 129.f, 188.f), n.MapPoint(p));

    TransformationMatrix mn = m;
    mn.Scale3d(2.f, 3.f, 4.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    TransformationMatrix n;
    n.Rotate(90.f);
    EXPECT_FLOAT_EQ(0.f,
                    (gfx::Point3F(-43.f, 41.f, 47.f) - n.MapPoint(p)).Length());

    TransformationMatrix mn = m;
    mn.Rotate(90.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).Length());
  }

  {
    TransformationMatrix n;
    n.RotateAbout(10.f, 10.f, 10.f, 120.f);
    EXPECT_FLOAT_EQ(0.f,
                    (gfx::Point3F(47.f, 41.f, 43.f) - n.MapPoint(p)).Length());

    TransformationMatrix mn = m;
    mn.RotateAbout(10.f, 10.f, 10.f, 120.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).Length());
  }

  {
    TransformationMatrix n;
    n.Translate(5.f, 6.f);
    EXPECT_EQ(gfx::Point3F(46.f, 49.f, 47.f), n.MapPoint(p));

    TransformationMatrix mn = m;
    mn.Translate(5.f, 6.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    TransformationMatrix n;
    n.Translate3d(5.f, 6.f, 7.f);
    EXPECT_EQ(gfx::Point3F(46.f, 49.f, 54.f), n.MapPoint(p));

    TransformationMatrix mn = m;
    mn.Translate3d(5.f, 6.f, 7.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    TransformationMatrix nm = m;
    nm.PostTranslate(5.f, 6.f);
    EXPECT_EQ(nm.MapPoint(p), m.MapPoint(p) + gfx::Vector3dF(5.f, 6.f, 0.f));
  }

  {
    TransformationMatrix nm = m;
    nm.PostTranslate3d(5.f, 6.f, 7.f);
    EXPECT_EQ(nm.MapPoint(p), m.MapPoint(p) + gfx::Vector3dF(5.f, 6.f, 7.f));
  }

  {
    TransformationMatrix n;
    n.Skew(45.f, -45.f);
    EXPECT_FLOAT_EQ(0.f,
                    (gfx::Point3F(84.f, 2.f, 47.f) - n.MapPoint(p)).Length());

    TransformationMatrix mn = m;
    mn.Skew(45.f, -45.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).Length());
  }

  {
    TransformationMatrix n;
    n.SkewX(45.f);
    EXPECT_FLOAT_EQ(0.f,
                    (gfx::Point3F(84.f, 43.f, 47.f) - n.MapPoint(p)).Length());

    TransformationMatrix mn = m;
    mn.SkewX(45.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).Length());
  }

  {
    TransformationMatrix n;
    n.SkewY(45.f);
    EXPECT_FLOAT_EQ(0.f,
                    (gfx::Point3F(41.f, 84.f, 47.f) - n.MapPoint(p)).Length());

    TransformationMatrix mn = m;
    mn.SkewY(45.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).Length());
  }

  {
    TransformationMatrix n;
    n.ApplyPerspectiveDepth(94.f);
    EXPECT_FLOAT_EQ(0.f,
                    (gfx::Point3F(82.f, 86.f, 94.f) - n.MapPoint(p)).Length());

    TransformationMatrix mn = m;
    mn.ApplyPerspectiveDepth(94.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).Length());
  }

  {
    TransformationMatrix n = m;
    n.Zoom(2.f);
    gfx::Point3F expectation = p;
    expectation.Scale(0.5f, 0.5f, 0.5f);
    expectation = m.MapPoint(expectation);
    expectation.Scale(2.f, 2.f, 2.f);
    EXPECT_EQ(expectation, n.MapPoint(p));
  }
}

TEST(TransformationMatrixTest, Blend2dXFlipTest) {
  // Test 2D x-flip (crbug.com/797472).
  auto from = TransformationMatrix::Affine(1, 0, 0, 1, 100, 150);
  auto to = TransformationMatrix::Affine(-1, 0, 0, 1, 400, 150);

  EXPECT_TRUE(from.IsAffine());
  EXPECT_TRUE(to.IsAffine());

  // OK for interpolated transform to be degenerate.
  TransformationMatrix result = to;
  result.Blend(from, 0.5);
  auto expected = TransformationMatrix::Affine(0, 0, 0, 1, 250, 150);
  EXPECT_TRANSFORMATION_MATRIX(expected, result);
}

TEST(TransformationMatrixTest, Blend2dRotationDirectionTest) {
  // Interpolate taking shorter rotation path.
  auto from = TransformationMatrix::Affine(-0.5, 0.86602575498, -0.86602575498,
                                           -0.5, 0, 0);
  auto to = TransformationMatrix::Affine(-0.5, -0.86602575498, 0.86602575498,
                                         -0.5, 0, 0);

  // Expect clockwise Rotation.
  TransformationMatrix result = to;
  result.Blend(from, 0.5);
  auto expected = TransformationMatrix::Affine(-1, 0, 0, -1, 0, 0);
  EXPECT_TRANSFORMATION_MATRIX(expected, result);

  // Reverse from and to.
  // Expect same midpoint with counter-clockwise rotation.
  result = from;
  result.Blend(to, 0.5);
  EXPECT_TRANSFORMATION_MATRIX(expected, result);
}

TEST(TransformationMatrixTest, Decompose2dShearTest) {
  // Test that x and y-shear transforms are properly decomposed.
  // The canonical decomposition is: transform, rotate, x-axis shear, scale.
  auto transform_shear_x = TransformationMatrix::Affine(1, 0, 1, 1, 0, 0);
  TransformationMatrix::Decomposed2dType decomp_shear_x;
  EXPECT_TRUE(transform_shear_x.Decompose2D(decomp_shear_x));
  EXPECT_FLOAT(1, decomp_shear_x.scale_x);
  EXPECT_FLOAT(1, decomp_shear_x.scale_y);
  EXPECT_FLOAT(0, decomp_shear_x.translate_x);
  EXPECT_FLOAT(0, decomp_shear_x.translate_y);
  EXPECT_FLOAT(0, decomp_shear_x.angle);
  EXPECT_FLOAT(1, decomp_shear_x.skew_xy);
  TransformationMatrix recomp_shear_x;
  recomp_shear_x.Recompose2D(decomp_shear_x);
  EXPECT_TRANSFORMATION_MATRIX(transform_shear_x, recomp_shear_x);

  auto transform_shear_y = TransformationMatrix::Affine(1, 1, 0, 1, 0, 0);
  TransformationMatrix::Decomposed2dType decomp_shear_y;
  EXPECT_TRUE(transform_shear_y.Decompose2D(decomp_shear_y));
  EXPECT_FLOAT(sqrt(2), decomp_shear_y.scale_x);
  EXPECT_FLOAT(1 / sqrt(2), decomp_shear_y.scale_y);
  EXPECT_FLOAT(0, decomp_shear_y.translate_x);
  EXPECT_FLOAT(0, decomp_shear_y.translate_y);
  EXPECT_FLOAT(M_PI / 4, decomp_shear_y.angle);
  EXPECT_FLOAT(1, decomp_shear_y.skew_xy);
  TransformationMatrix recomp_shear_y;
  recomp_shear_y.Recompose2D(decomp_shear_y);
  EXPECT_TRANSFORMATION_MATRIX(transform_shear_y, recomp_shear_y);
}

TEST(TransformationMatrixTest, QuaternionFromRotationMatrixTest) {
  double cos30deg = std::cos(M_PI / 6);
  double sin30deg = 0.5;
  double root2 = std::sqrt(2);

  // Test rotation around each axis.

  TransformationMatrix m;
  m.RotateAbout(1, 0, 0, 60);
  absl::optional<gfx::DecomposedTransform> decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion,
                         gfx::Quaternion(sin30deg, 0, 0, cos30deg), 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 1, 0, 60);
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion,
                         gfx::Quaternion(0, sin30deg, 0, cos30deg), 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 0, 1, 60);
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion,
                         gfx::Quaternion(0, 0, sin30deg, cos30deg), 1e-6);

  // Test rotation around non-axis aligned vector.

  m.MakeIdentity();
  m.RotateAbout(1, 1, 0, 60);
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(
      decomp->quaternion,
      gfx::Quaternion(sin30deg / root2, sin30deg / root2, 0, cos30deg), 1e-6);

  // Test edge tests.

  // Cases where q_w = 0. In such cases we resort to basing the calculations on
  // the largest diagonal element in the rotation matrix to ensure numerical
  // stability.

  m.MakeIdentity();
  m.RotateAbout(1, 0, 0, 180);
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion, gfx::Quaternion(1, 0, 0, 0), 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 1, 0, 180);
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion, gfx::Quaternion(0, 1, 0, 0), 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 0, 1, 180);
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion, gfx::Quaternion(0, 0, 1, 0), 1e-6);

  // No rotation.

  m.MakeIdentity();
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion, gfx::Quaternion(0, 0, 0, 1), 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 0, 1, 360);
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion, gfx::Quaternion(0, 0, 0, 1), 1e-6);
}

TEST(TransformationMatrixTest, QuaternionToRotationMatrixTest) {
  double cos30deg = std::cos(M_PI / 6);
  double sin30deg = 0.5;
  double cos60deg = 0.5;
  double sin60deg = std::sin(M_PI / 3);
  double root2 = std::sqrt(2);

  TransformationMatrix m;
  gfx::DecomposedTransform decomp;

  // Test rotation about each axis.

  decomp = GetRotationDecomp(sin30deg, 0, 0, cos30deg);
  m = TransformationMatrix::Compose(decomp);
  auto rotate_x_60deg =
      TransformationMatrix::ColMajor(1, 0, 0, 0,                 // column 1
                                     0, cos60deg, sin60deg, 0,   // column 2
                                     0, -sin60deg, cos60deg, 0,  // column 3
                                     0, 0, 0, 1);                // column 4
  EXPECT_TRANSFORMATION_MATRIX(rotate_x_60deg, m);

  decomp = GetRotationDecomp(0, sin30deg, 0, cos30deg);
  m = TransformationMatrix::Compose(decomp);
  auto rotate_y_60deg =
      TransformationMatrix::ColMajor(cos60deg, 0, -sin60deg, 0,  // column 1
                                     0, 1, 0, 0,                 // column 2
                                     sin60deg, 0, cos60deg, 0,   // column 3
                                     0, 0, 0, 1);                // column 4
  EXPECT_TRANSFORMATION_MATRIX(rotate_y_60deg, m);

  decomp = GetRotationDecomp(0, 0, sin30deg, cos30deg);
  m = TransformationMatrix::Compose(decomp);
  auto rotate_z_60deg =
      TransformationMatrix::ColMajor(cos60deg, sin60deg, 0, 0,   // column 1
                                     -sin60deg, cos60deg, 0, 0,  // column 2
                                     0, 0, 1, 0,                 // column 3
                                     0, 0, 0, 1);                // column 4
  EXPECT_TRANSFORMATION_MATRIX(rotate_z_60deg, m);

  // Test non-axis aligned rotation
  decomp = GetRotationDecomp(sin30deg / root2, sin30deg / root2, 0, cos30deg);
  m = TransformationMatrix::Compose(decomp);
  TransformationMatrix rotate_xy_60deg;
  rotate_xy_60deg.RotateAbout(1, 1, 0, 60);
  EXPECT_TRANSFORMATION_MATRIX(rotate_xy_60deg, m);

  // Test 180deg rotation.
  decomp = GetRotationDecomp(0, 0, 1, 0);
  m = TransformationMatrix::Compose(decomp);
  auto rotate_z_180deg = TransformationMatrix::Affine(-1, 0, 0, -1, 0, 0);
  EXPECT_TRANSFORMATION_MATRIX(rotate_z_180deg, m);
}

TEST(TransformationMatrixTest, QuaternionInterpolation) {
  double cos60deg = 0.5;
  double sin60deg = std::sin(M_PI / 3);
  double root2 = std::sqrt(2);

  // Rotate from identity matrix.
  TransformationMatrix from_matrix;
  TransformationMatrix to_matrix;
  to_matrix.RotateAbout(0, 0, 1, 120);
  to_matrix.Blend(from_matrix, 0.5);
  auto rotate_z_60 = TransformationMatrix::Affine(cos60deg, sin60deg, -sin60deg,
                                                  cos60deg, 0, 0);
  EXPECT_TRANSFORMATION_MATRIX(rotate_z_60, to_matrix);

  // Rotate to identity matrix.
  from_matrix.MakeIdentity();
  from_matrix.RotateAbout(0, 0, 1, 120);
  to_matrix.MakeIdentity();
  to_matrix.Blend(from_matrix, 0.5);
  EXPECT_TRANSFORMATION_MATRIX(rotate_z_60, to_matrix);

  // Interpolation about a common axis of rotation.
  from_matrix.MakeIdentity();
  from_matrix.RotateAbout(1, 1, 0, 45);
  to_matrix.MakeIdentity();
  from_matrix.RotateAbout(1, 1, 0, 135);
  to_matrix.Blend(from_matrix, 0.5);
  TransformationMatrix rotate_xy_90;
  rotate_xy_90.RotateAbout(1, 1, 0, 90);
  EXPECT_TRANSFORMATION_MATRIX(rotate_xy_90, to_matrix);

  // Interpolation without a common axis of rotation.

  from_matrix.MakeIdentity();
  from_matrix.RotateAbout(1, 0, 0, 90);
  to_matrix.MakeIdentity();
  to_matrix.RotateAbout(0, 0, 1, 90);
  EXPECT_TRUE(to_matrix.Decompose());
  to_matrix.Blend(from_matrix, 0.5);
  TransformationMatrix expected;
  expected.RotateAbout(1 / root2, 0, 1 / root2, 70.528778372);
  EXPECT_TRANSFORMATION_MATRIX(expected, to_matrix);
}

}  // namespace blink
