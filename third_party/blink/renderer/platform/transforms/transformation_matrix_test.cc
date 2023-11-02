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

void SetRotationDecomp(double x,
                       double y,
                       double z,
                       double w,
                       TransformationMatrix::DecomposedType& decomp) {
  decomp.scale_x = 1;
  decomp.scale_y = 1;
  decomp.scale_z = 1;
  decomp.skew_xy = 0;
  decomp.skew_xz = 0;
  decomp.skew_yz = 0;
  decomp.quaternion_x = x;
  decomp.quaternion_y = y;
  decomp.quaternion_z = z;
  decomp.quaternion_w = w;
  decomp.translate_x = 0;
  decomp.translate_y = 0;
  decomp.translate_z = 0;
  decomp.perspective_x = 0;
  decomp.perspective_y = 0;
  decomp.perspective_z = 0;
  decomp.perspective_w = 1;
}

}  // end namespace

// This test is to make it easier to understand the order of operations.
TEST(TransformationMatrixTest, PrePostOperations) {
  auto m1 = TransformationMatrix::Affine(1, 2, 3, 4, 5, 6);
  auto m2 = m1;
  m1.Translate(10, 20);
  m2.PreConcat(TransformationMatrix::MakeTranslation(10, 20));
  EXPECT_EQ(m1, m2);

  m1.PostTranslate(11, 22);
  m2 = TransformationMatrix::MakeTranslation(11, 22) * m2;
  EXPECT_EQ(m1, m2);

  m1.Scale(3, 4);
  m2.PreConcat(TransformationMatrix::MakeScale(3, 4));
  EXPECT_EQ(m1, m2);

  // TODO(wangxianzhu): Add PostScale tests when moving this test into
  // ui/gfx/geometry/transform_unittest.cc.
#if 0
  m1.PostScale(5, 6);
  m2 = TransformationMatrix::MakeScale(3, 4) * m2;
  EXPECT_EQ(m1, m2);
#endif
}

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

TEST(TransformationMatrixTest, IsIdentityOr2DTranslation) {
  TransformationMatrix matrix;
  EXPECT_TRUE(matrix.IsIdentityOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Translate(10, 0);
  EXPECT_TRUE(matrix.IsIdentityOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Translate(0, -20);
  EXPECT_TRUE(matrix.IsIdentityOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Translate3d(0, 0, 1);
  EXPECT_FALSE(matrix.IsIdentityOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Rotate(40 /* degrees */);
  EXPECT_FALSE(matrix.IsIdentityOr2DTranslation());

  matrix.MakeIdentity();
  matrix.SkewX(30 /* degrees */);
  EXPECT_FALSE(matrix.IsIdentityOr2DTranslation());
}

TEST(TransformationMatrixTest, Is2DProportionalUpscaleAndOr2DTranslation) {
  TransformationMatrix matrix;
  EXPECT_TRUE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Translate(10, 0);
  EXPECT_TRUE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Scale(1.3);
  EXPECT_TRUE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Translate(0, -20);
  matrix.Scale(1.7);
  EXPECT_TRUE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Scale(0.99);
  EXPECT_FALSE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Translate3d(0, 0, 1);
  EXPECT_FALSE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Rotate(40 /* degrees */);
  EXPECT_FALSE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());

  matrix.MakeIdentity();
  matrix.SkewX(30 /* degrees */);
  EXPECT_FALSE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());
}

TEST(TransformationMatrixTest, To2DTranslation) {
  TransformationMatrix matrix;
  EXPECT_EQ(gfx::Vector2dF(), matrix.To2DTranslation());
  matrix.Translate(30, -40);
  EXPECT_EQ(gfx::Vector2dF(30, -40), matrix.To2DTranslation());
}

TEST(TransformationMatrixTest, To3dTranslation) {
  TransformationMatrix matrix;
  EXPECT_EQ(gfx::Vector3dF(), matrix.To3dTranslation());
  matrix.Translate3d(30, -40, -10);
  EXPECT_EQ(gfx::Vector3dF(30, -40, -10), matrix.To3dTranslation());
}

TEST(TransformationMatrixTest, ApplyTransformOrigin) {
  TransformationMatrix matrix;

  // (0,0,0) is a fixed point of this scale.
  // (1,1,1) should be scaled appropriately.
  matrix.Scale3d(2, 3, 4);
  EXPECT_EQ(gfx::Point3F(0, 0, 0), matrix.MapPoint(gfx::Point3F(0, 0, 0)));
  EXPECT_EQ(gfx::Point3F(2, 3, -4), matrix.MapPoint(gfx::Point3F(1, 1, -1)));

  // With the transform origin applied, (1,2,3) is the fixed point.
  // (0,0,0) should be scaled according to its distance from (1,2,3).
  matrix.ApplyTransformOrigin(1, 2, 3);
  EXPECT_EQ(gfx::Point3F(1, 2, 3), matrix.MapPoint(gfx::Point3F(1, 2, 3)));
  EXPECT_EQ(gfx::Point3F(-1, -4, -9), matrix.MapPoint(gfx::Point3F(0, 0, 0)));
}

TEST(TransformationMatrixTest, Multiplication) {
  // clang-format off
  auto a = TransformationMatrix::ColMajor(1, 2, 3, 4,
                                          2, 3, 4, 5,
                                          3, 4, 5, 6,
                                          4, 5, 6, 7);
  auto b = TransformationMatrix::ColMajor(1, 3, 5, 7,
                                          2, 4, 6, 8,
                                          3, 5, 7, 9,
                                          4, 6, 8, 10);
  auto expected_a_times_b = TransformationMatrix::ColMajor(50, 66, 82, 98,
                                                           60, 80, 100, 120,
                                                           70, 94, 118, 142,
                                                           80, 108, 136, 164);
  // clang-format on

  EXPECT_EQ(expected_a_times_b, a * b) << (a * b).ToString(true);

  a.PreConcat(b);
  EXPECT_EQ(expected_a_times_b, a) << a.ToString(true);
}

TEST(TransformationMatrixTest, MultiplicationSelf) {
  // clang-format off
  auto a = TransformationMatrix::ColMajor(1, 2, 3, 4,
                                          5, 6, 7, 8,
                                          9, 10, 11, 12,
                                          13, 14, 15, 16);
  auto expected_a_times_a = TransformationMatrix::ColMajor(90, 100, 110, 120,
                                                           202, 228, 254, 280,
                                                           314, 356, 398, 440,
                                                           426, 484, 542, 600);
  // clang-format on

  a.PreConcat(a);
  EXPECT_EQ(expected_a_times_a, a) << a.ToString(true);
}

TEST(TransformationMatrixTest, ValidRangedMatrix) {
  double entries[][2] = {
      /*
        first entry is initial matrix value
        second entry is a factor to use transformation operations
      */
      {std::numeric_limits<double>::max(),
       std::numeric_limits<double>::infinity()},
      {1, std::numeric_limits<double>::infinity()},
      {-1, std::numeric_limits<double>::infinity()},
      {1, -std::numeric_limits<double>::infinity()},
      {
          std::numeric_limits<double>::max(),
          std::numeric_limits<double>::max(),
      },
      {
          std::numeric_limits<double>::lowest(),
          -std::numeric_limits<double>::infinity(),
      },
  };

  for (double* entry : entries) {
    const double mv = entry[0];
    const double factor = entry[1];

    auto is_valid_point = [&](const gfx::PointF& p) -> bool {
      return std::isfinite(p.x()) && std::isfinite(p.y());
    };
    auto is_valid_point3 = [&](const gfx::Point3F& p) -> bool {
      return std::isfinite(p.x()) && std::isfinite(p.y()) &&
             std::isfinite(p.z());
    };
    auto is_valid_rect = [&](const gfx::RectF& r) -> bool {
      return is_valid_point(r.origin()) && std::isfinite(r.width()) &&
             std::isfinite(r.height());
    };
    auto is_valid_quad = [&](const gfx::QuadF& q) -> bool {
      return is_valid_point(q.p1()) && is_valid_point(q.p2()) &&
             is_valid_point(q.p3()) && is_valid_point(q.p4());
    };
    auto is_valid_array16 = [&](const float* a) -> bool {
      for (int i = 0; i < 16; i++) {
        if (!std::isfinite(a[i]))
          return false;
      }
      return true;
    };

    auto test = [&](const TransformationMatrix& m) {
      SCOPED_TRACE(String::Format("m: %s factor: %lg",
                                  m.ToString().Utf8().data(), factor));
      auto p = m.MapPoint(gfx::PointF(factor, factor));
      EXPECT_TRUE(is_valid_point(p)) << p.ToString();
      p = m.ProjectPoint(gfx::PointF(factor, factor));
      EXPECT_TRUE(is_valid_point(p)) << p.ToString();
      auto p3 = m.MapPoint(gfx::Point3F(factor, factor, factor));
      EXPECT_TRUE(is_valid_point3(p3)) << p3.ToString();
      auto r = m.MapRect(gfx::RectF(factor, factor, factor, factor));
      EXPECT_TRUE(is_valid_rect(r)) << r.ToString();

      gfx::QuadF q0(gfx::RectF(factor, factor, factor, factor));
      auto q = m.MapQuad(q0);
      EXPECT_TRUE(is_valid_quad(q)) << q.ToString();
      q = m.ProjectQuad(q0);
      EXPECT_TRUE(is_valid_quad(q)) << q.ToString();
      // This should not trigger DCHECK.
      LayoutRect layout_rect = m.ClampedBoundsOfProjectedQuad(q0);
      // This is just to avoid unused variable warning.
      EXPECT_TRUE(layout_rect.IsEmpty() || !layout_rect.IsEmpty());

      float a[16];
      m.ToTransform().GetColMajorF(a);
      EXPECT_TRUE(is_valid_array16(a));
      m.ToSkM44().getColMajor(a);
      EXPECT_TRUE(is_valid_array16(a));
    };

    test(TransformationMatrix::ColMajor(mv, mv, mv, mv, mv, mv, mv, mv, mv, mv,
                                        mv, mv, mv, mv, mv, mv));
    test(MakeTranslationMatrix(mv, mv));
  }
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
    gfx::Point3F origin(5.f, 6.f, 7.f);
    TransformationMatrix n = m;
    n.ApplyTransformOrigin(origin);
    EXPECT_EQ(
        m.MapPoint(p - origin.OffsetFromOrigin()) + origin.OffsetFromOrigin(),
        n.MapPoint(p));
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

TEST(TransformationMatrixTest, ToString) {
  auto zeros = TransformationMatrix::ColMajor(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                              0, 0, 0, 0, 0);
  EXPECT_EQ("[0,0,0,0,\n0,0,0,0,\n0,0,0,0,\n0,0,0,0] (degenerate)",
            zeros.ToString());
  EXPECT_EQ("[0,0,0,0,\n0,0,0,0,\n0,0,0,0,\n0,0,0,0]", zeros.ToString(true));

  TransformationMatrix identity;
  EXPECT_EQ("identity", identity.ToString());
  EXPECT_EQ("[1,0,0,0,\n0,1,0,0,\n0,0,1,0,\n0,0,0,1]", identity.ToString(true));

  TransformationMatrix translation;
  translation.Translate3d(3, 5, 7);
  EXPECT_EQ("translation(3,5,7)", translation.ToString());
  EXPECT_EQ("[1,0,0,3,\n0,1,0,5,\n0,0,1,7,\n0,0,0,1]",
            translation.ToString(true));

  auto column_major_constructor = TransformationMatrix::ColMajor(
      1, 1, 1, 6, 2, 2, 0, 7, 3, 3, 3, 8, 4, 4, 4, 9);
  // [ 1 2 3 4 ]
  // [ 1 2 3 4 ]
  // [ 1 0 3 4 ]
  // [ 6 7 8 9 ]
  EXPECT_EQ("[1,2,3,4,\n1,2,3,4,\n1,0,3,4,\n6,7,8,9]",
            column_major_constructor.ToString(true));
}

TEST(TransformationMatrix, IsInvertible) {
  EXPECT_FALSE(TransformationMatrix::ColMajor(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                              0, 0, 0, 0, 0)
                   .IsInvertible());
  EXPECT_TRUE(TransformationMatrix().IsInvertible());
  TransformationMatrix t;
  t.Translate3d(10, 20, 30);
  EXPECT_TRUE(t.IsInvertible());
  EXPECT_TRUE(MakeScaleMatrix(1e-8).IsInvertible());
  EXPECT_FALSE(MakeScaleMatrix(0).IsInvertible());
  EXPECT_FALSE(
      MakeScaleMatrix(std::numeric_limits<double>::quiet_NaN()).IsInvertible());
  EXPECT_FALSE(
      MakeScaleMatrix(std::numeric_limits<double>::min()).IsInvertible());
}

TEST(TransformationMatrix, Inverse) {
  EXPECT_EQ(TransformationMatrix(), MakeScaleMatrix(0).Inverse());
  EXPECT_EQ(TransformationMatrix(), TransformationMatrix().Inverse());

  auto t1 = MakeTranslationMatrix(-10, 20, -30);
  auto t2 = MakeTranslationMatrix(10, -20, 30);
  EXPECT_EQ(t1, t2.Inverse());
  EXPECT_EQ(t2, t1.Inverse());

  auto s1 = MakeScaleMatrix(2, -4, 0.5);
  auto s2 = MakeScaleMatrix(0.5, -0.25, 2);
  EXPECT_EQ(s1, s2.Inverse());
  EXPECT_EQ(s2, s1.Inverse());

  TransformationMatrix m1;
  m1.RotateAboutZAxis(-30);
  m1.RotateAboutYAxis(10);
  m1.RotateAboutXAxis(20);
  m1.ApplyPerspectiveDepth(100);
  TransformationMatrix m2;
  m2.ApplyPerspectiveDepth(-100);
  m2.RotateAboutXAxis(-20);
  m2.RotateAboutYAxis(-10);
  m2.RotateAboutZAxis(30);
  EXPECT_TRANSFORMATION_MATRIX(m1, m2.Inverse());
  EXPECT_TRANSFORMATION_MATRIX(m2, m1.Inverse());
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

double ComputeDecompRecompError(const TransformationMatrix& transform_matrix) {
  TransformationMatrix::DecomposedType decomp;
  EXPECT_TRUE(transform_matrix.Decompose(decomp));

  TransformationMatrix composed;
  composed.Recompose(decomp);

  double sse = 0;
  for (int i = 0; i < 16; i++) {
    double diff =
        transform_matrix.ColMajorData()[i] - composed.ColMajorData()[i];
    sse += diff * diff;
  }
  return sse;
}

TEST(TransformationMatrixTest, DecomposeRecompose) {
  // Result of Recompose(Decompose(identity)) should be exactly identity.
  EXPECT_EQ(0, ComputeDecompRecompError(TransformationMatrix()));

  // rotateZ(90deg)
  EXPECT_NEAR(
      0,
      ComputeDecompRecompError(TransformationMatrix::Affine(0, 1, -1, 0, 0, 0)),
      1e-6);

  // rotateZ(180deg)
  // Edge case where w = 0.
  EXPECT_NEAR(0,
              ComputeDecompRecompError(
                  TransformationMatrix::Affine(-1, 0, 0, -1, 0, 0)),
              1e-6);

  // rotateX(90deg) rotateY(90deg) rotateZ(90deg)
  // [1  0   0][ 0 0 1][0 -1 0]   [0 0 1][0 -1 0]   [0  0 1]
  // [0  0  -1][ 0 1 0][1  0 0] = [1 0 0][1  0 0] = [0 -1 0]
  // [0  1   0][-1 0 0][0  0 1]   [0 1 0][0  0 1]   [1  0 0]
  // This test case leads to Gimbal lock when using Euler angles.
  EXPECT_NEAR(0,
              ComputeDecompRecompError(TransformationMatrix::ColMajor(
                  0, 0, 1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1)),
              1e-6);

  // Quaternion matrices with 0 off-diagonal elements, and negative trace.
  // Stress tests handling of degenerate cases in computing quaternions.
  // Validates fix for https://crbug.com/647554.
  EXPECT_NEAR(
      0,
      ComputeDecompRecompError(TransformationMatrix::Affine(1, 1, 1, 0, 0, 0)),
      1e-6);
  EXPECT_NEAR(0,
              ComputeDecompRecompError(TransformationMatrix::ColMajor(
                  -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)),
              1e-6);
  EXPECT_NEAR(0,
              ComputeDecompRecompError(TransformationMatrix::ColMajor(
                  1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)),
              1e-6);
  EXPECT_NEAR(0,
              ComputeDecompRecompError(TransformationMatrix::ColMajor(
                  1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1)),
              1e-6);
}

TEST(TransformationMatrixTest, QuaternionFromRotationMatrixTest) {
  double cos30deg = std::cos(M_PI / 6);
  double sin30deg = 0.5;
  double root2 = std::sqrt(2);

  // Test rotation around each axis.

  TransformationMatrix m;
  m.RotateAbout(1, 0, 0, 60);
  TransformationMatrix::DecomposedType decomp;
  EXPECT_TRUE(m.Decompose(decomp));

  EXPECT_NEAR(sin30deg, decomp.quaternion_x, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_y, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_z, 1e-6);
  EXPECT_NEAR(cos30deg, decomp.quaternion_w, 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 1, 0, 60);
  EXPECT_TRUE(m.Decompose(decomp));
  EXPECT_NEAR(0, decomp.quaternion_x, 1e-6);
  EXPECT_NEAR(sin30deg, decomp.quaternion_y, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_z, 1e-6);
  EXPECT_NEAR(cos30deg, decomp.quaternion_w, 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 0, 1, 60);
  EXPECT_TRUE(m.Decompose(decomp));
  EXPECT_NEAR(0, decomp.quaternion_x, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_y, 1e-6);
  EXPECT_NEAR(sin30deg, decomp.quaternion_z, 1e-6);
  EXPECT_NEAR(cos30deg, decomp.quaternion_w, 1e-6);

  // Test rotation around non-axis aligned vector.

  m.MakeIdentity();
  m.RotateAbout(1, 1, 0, 60);
  EXPECT_TRUE(m.Decompose(decomp));
  EXPECT_NEAR(sin30deg / root2, decomp.quaternion_x, 1e-6);
  EXPECT_NEAR(sin30deg / root2, decomp.quaternion_y, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_z, 1e-6);
  EXPECT_NEAR(cos30deg, decomp.quaternion_w, 1e-6);

  // Test edge tests.

  // Cases where q_w = 0. In such cases we resort to basing the calculations on
  // the largest diagonal element in the rotation matrix to ensure numerical
  // stability.

  m.MakeIdentity();
  m.RotateAbout(1, 0, 0, 180);
  EXPECT_TRUE(m.Decompose(decomp));
  EXPECT_NEAR(1, decomp.quaternion_x, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_y, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_z, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_w, 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 1, 0, 180);
  EXPECT_TRUE(m.Decompose(decomp));
  EXPECT_NEAR(0, decomp.quaternion_x, 1e-6);
  EXPECT_NEAR(1, decomp.quaternion_y, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_z, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_w, 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 0, 1, 180);
  EXPECT_TRUE(m.Decompose(decomp));
  EXPECT_NEAR(0, decomp.quaternion_x, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_y, 1e-6);
  EXPECT_NEAR(1, decomp.quaternion_z, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_w, 1e-6);

  // No rotation.

  m.MakeIdentity();
  EXPECT_TRUE(m.Decompose(decomp));
  EXPECT_NEAR(0, decomp.quaternion_x, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_y, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_z, 1e-6);
  EXPECT_NEAR(1, decomp.quaternion_w, 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 0, 1, 360);
  EXPECT_TRUE(m.Decompose(decomp));
  EXPECT_NEAR(0, decomp.quaternion_x, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_y, 1e-6);
  EXPECT_NEAR(0, decomp.quaternion_z, 1e-6);
  EXPECT_NEAR(1, decomp.quaternion_w, 1e-6);
}

TEST(TransformationMatrixTest, QuaternionToRotationMatrixTest) {
  double cos30deg = std::cos(M_PI / 6);
  double sin30deg = 0.5;
  double cos60deg = 0.5;
  double sin60deg = std::sin(M_PI / 3);
  double root2 = std::sqrt(2);

  TransformationMatrix m;
  TransformationMatrix::DecomposedType decomp;

  // Test rotation about each axis.

  SetRotationDecomp(sin30deg, 0, 0, cos30deg, decomp);
  m.Recompose(decomp);
  auto rotate_x_60deg =
      TransformationMatrix::ColMajor(1, 0, 0, 0,                 // column 1
                                     0, cos60deg, sin60deg, 0,   // column 2
                                     0, -sin60deg, cos60deg, 0,  // column 3
                                     0, 0, 0, 1);                // column 4
  EXPECT_TRANSFORMATION_MATRIX(rotate_x_60deg, m);

  SetRotationDecomp(0, sin30deg, 0, cos30deg, decomp);
  m.Recompose(decomp);
  auto rotate_y_60deg =
      TransformationMatrix::ColMajor(cos60deg, 0, -sin60deg, 0,  // column 1
                                     0, 1, 0, 0,                 // column 2
                                     sin60deg, 0, cos60deg, 0,   // column 3
                                     0, 0, 0, 1);                // column 4
  EXPECT_TRANSFORMATION_MATRIX(rotate_y_60deg, m);

  SetRotationDecomp(0, 0, sin30deg, cos30deg, decomp);
  m.Recompose(decomp);
  auto rotate_z_60deg =
      TransformationMatrix::ColMajor(cos60deg, sin60deg, 0, 0,   // column 1
                                     -sin60deg, cos60deg, 0, 0,  // column 2
                                     0, 0, 1, 0,                 // column 3
                                     0, 0, 0, 1);                // column 4
  EXPECT_TRANSFORMATION_MATRIX(rotate_z_60deg, m);

  // Test non-axis aligned rotation
  SetRotationDecomp(sin30deg / root2, sin30deg / root2, 0, cos30deg, decomp);
  m.Recompose(decomp);
  TransformationMatrix rotate_xy_60deg;
  rotate_xy_60deg.RotateAbout(1, 1, 0, 60);
  EXPECT_TRANSFORMATION_MATRIX(rotate_xy_60deg, m);

  // Test 180deg rotation.
  SetRotationDecomp(0, 0, 1, 0, decomp);
  m.Recompose(decomp);
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
  TransformationMatrix::DecomposedType decomp;
  to_matrix.MakeIdentity();
  to_matrix.RotateAbout(0, 0, 1, 90);
  EXPECT_TRUE(to_matrix.Decompose(decomp));
  to_matrix.Blend(from_matrix, 0.5);
  TransformationMatrix expected;
  expected.RotateAbout(1 / root2, 0, 1 / root2, 70.528778372);
  EXPECT_TRANSFORMATION_MATRIX(expected, to_matrix);
}

TEST(TransformationMatrixTest, IsInteger2DTranslation) {
  EXPECT_TRUE(TransformationMatrix().IsInteger2DTranslation());
  EXPECT_TRUE(MakeTranslationMatrix(1, 2).IsInteger2DTranslation());
  EXPECT_FALSE(MakeTranslationMatrix(1.00001, 2).IsInteger2DTranslation());
  EXPECT_FALSE(MakeTranslationMatrix(1, 2.00002).IsInteger2DTranslation());
  EXPECT_FALSE(MakeRotationMatrix(2).IsInteger2DTranslation());
  EXPECT_FALSE(MakeTranslationMatrix(1, 2, 3).IsInteger2DTranslation());
  EXPECT_FALSE(MakeTranslationMatrix(1e20, 0).IsInteger2DTranslation());
  EXPECT_FALSE(MakeTranslationMatrix(0, 1e20).IsInteger2DTranslation());
}

}  // namespace blink
