// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

TEST(AffineTransformTest, IsIdentity) {
  EXPECT_TRUE(AffineTransform().IsIdentity());

  AffineTransform a;
  EXPECT_TRUE(a.IsIdentity());
  a.SetA(2);
  EXPECT_FALSE(a.IsIdentity());
  EXPECT_NE(a, AffineTransform());

  a.MakeIdentity();
  EXPECT_TRUE(a.IsIdentity());
  a.SetB(2);
  EXPECT_FALSE(a.IsIdentity());
  EXPECT_NE(a, AffineTransform());

  a.MakeIdentity();
  EXPECT_TRUE(a.IsIdentity());
  a.SetC(2);
  EXPECT_FALSE(a.IsIdentity());
  EXPECT_NE(a, AffineTransform());

  a.MakeIdentity();
  EXPECT_TRUE(a.IsIdentity());
  a.SetD(2);
  EXPECT_FALSE(a.IsIdentity());
  EXPECT_NE(a, AffineTransform());

  a.MakeIdentity();
  EXPECT_TRUE(a.IsIdentity());
  a.SetE(2);
  EXPECT_FALSE(a.IsIdentity());
  EXPECT_NE(a, AffineTransform());

  a.MakeIdentity();
  EXPECT_TRUE(a.IsIdentity());
  a.SetF(2);
  EXPECT_FALSE(a.IsIdentity());
  EXPECT_NE(a, AffineTransform());
}

TEST(AffineTransformTest, IsIdentityOrTranslation) {
  EXPECT_TRUE(AffineTransform().IsIdentityOrTranslation());
  AffineTransform a;
  EXPECT_TRUE(a.IsIdentityOrTranslation());
  a.Translate(1, 2);
  EXPECT_TRUE(a.IsIdentityOrTranslation());
  a.Scale(2);
  EXPECT_FALSE(a.IsIdentityOrTranslation());
  a.MakeIdentity();
  a.Rotate(1);
  EXPECT_FALSE(a.IsIdentityOrTranslation());
}

TEST(AffineTransformTest, Multiply) {
  AffineTransform a(1, 2, 3, 4, 5, 6);
  AffineTransform b(10, 20, 30, 40, 50, 60);
  AffineTransform c = a * b;
  AffineTransform d = b * a;
  EXPECT_EQ(AffineTransform(70, 100, 150, 220, 235, 346), c);
  EXPECT_EQ(AffineTransform(70, 100, 150, 220, 280, 400), d);
  AffineTransform a1 = a;
  a.PreConcat(b);
  b.PreConcat(a1);
  EXPECT_EQ(c, a);
  EXPECT_EQ(d, b);
}

TEST(AffineTransformTest, PreMultiply) {
  AffineTransform a(1, 2, 3, 4, 5, 6);
  AffineTransform b(10, 20, 30, 40, 50, 60);
  AffineTransform a1 = a;
  a.PostConcat(b);
  b.PostConcat(a1);
  EXPECT_EQ(AffineTransform(70, 100, 150, 220, 280, 400), a);
  EXPECT_EQ(AffineTransform(70, 100, 150, 220, 235, 346), b);
}

TEST(AffineTransformTest, MultiplyOneTranslation) {
  AffineTransform a(1, 2, 3, 4, 5, 6);
  AffineTransform b(1, 0, 0, 1, 50, 60);
  EXPECT_EQ(AffineTransform(1, 2, 3, 4, 235, 346), a * b);
  EXPECT_EQ(AffineTransform(1, 2, 3, 4, 55, 66), b * a);
}

TEST(AffineTransformTest, IsInvertible) {
  EXPECT_TRUE(AffineTransform().IsInvertible());
  EXPECT_TRUE(AffineTransform().Translate(1, 2).IsInvertible());
  EXPECT_TRUE(AffineTransform().Rotate(10).IsInvertible());
  EXPECT_FALSE(AffineTransform().Scale(0, 1).IsInvertible());
  EXPECT_FALSE(AffineTransform().Scale(1, 0).IsInvertible());
  EXPECT_FALSE(AffineTransform(2, 1, 2, 1, 0, 0).IsInvertible());
}

TEST(AffineTransformTest, Inverse) {
  EXPECT_EQ(AffineTransform(), AffineTransform().Inverse());
  EXPECT_EQ(AffineTransform().Translate(1, -2),
            AffineTransform().Translate(-1, 2).Inverse());
  EXPECT_EQ(AffineTransform().Translate(1, -2),
            AffineTransform().Translate(-1, 2).Inverse());
  EXPECT_EQ(AffineTransform().Scale(2, -0.25),
            AffineTransform().Scale(0.5, -4).Inverse());
  EXPECT_EQ(AffineTransform().Scale(2, -0.25).Translate(1, -2),
            AffineTransform().Translate(-1, 2).Scale(0.5, -4).Inverse());
}

TEST(AffineTransformTest, MultiplySelf) {
  AffineTransform a(1, 2, 3, 4, 5, 6);
  auto b = a;
  a.PreConcat(a);
  EXPECT_EQ(AffineTransform(7, 10, 15, 22, 28, 40), a);
  b.PostConcat(b);
  EXPECT_EQ(a, b);
}

TEST(AffineTransformTest, ValidRangedMatrix) {
  double entries[][2] = {
      // The first entry is initial matrix value.
      // The second entry is a factor to use transformation operations.
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
    auto is_valid_rect = [&](const gfx::RectF& r) -> bool {
      return is_valid_point(r.origin()) && std::isfinite(r.width()) &&
             std::isfinite(r.height());
    };
    auto is_valid_quad = [&](const gfx::QuadF& q) -> bool {
      return is_valid_point(q.p1()) && is_valid_point(q.p2()) &&
             is_valid_point(q.p3()) && is_valid_point(q.p4());
    };

    auto test = [&](const AffineTransform& m) {
      SCOPED_TRACE(String::Format("m: %s factor: %lg",
                                  m.ToString().Utf8().data(), factor));
      auto p = m.MapPoint(gfx::PointF(factor, factor));
      EXPECT_TRUE(is_valid_point(p)) << p.ToString();
      auto r = m.MapRect(gfx::RectF(factor, factor, factor, factor));
      EXPECT_TRUE(is_valid_rect(r)) << r.ToString();

      gfx::QuadF q0(gfx::RectF(factor, factor, factor, factor));
      auto q = m.MapQuad(q0);
      EXPECT_TRUE(is_valid_quad(q)) << q.ToString();
    };

    test(AffineTransform(mv, mv, mv, mv, mv, mv));
    test(AffineTransform().Translate(mv, mv));
  }
}

TEST(AffineTransformTest, ToString) {
  AffineTransform identity;
  EXPECT_EQ("identity", identity.ToString());
  EXPECT_EQ("[1,0,0,\n0,1,0]", identity.ToString(true));

  AffineTransform translation = AffineTransform::Translation(7, 9);
  EXPECT_EQ("translation(7,9)", translation.ToString());
  EXPECT_EQ("[1,0,7,\n0,1,9]", translation.ToString(true));

  AffineTransform rotation;
  rotation.Rotate(180);
  EXPECT_EQ("translation(0,0), scale(1,1), angle(180deg), skewxy(0)",
            rotation.ToString());
  EXPECT_EQ("[-1,-1.22465e-16,0,\n1.22465e-16,-1,0]", rotation.ToString(true));

  AffineTransform column_major_constructor(1, 4, 2, 5, 3, 6);
  EXPECT_EQ("[1,2,3,\n4,5,6]", column_major_constructor.ToString(true));
}

}  // namespace blink
