// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/infinite_int_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

#define TEST_INTERCEPTS(roundedRect, yCoordinate, expectedMinXIntercept,     \
                        expectedMaxXIntercept)                               \
  {                                                                          \
    float min_x_intercept;                                                   \
    float max_x_intercept;                                                   \
    EXPECT_TRUE(                                                             \
        ContouredRect(roundedRect)                                           \
            .XInterceptsAtY(yCoordinate, min_x_intercept, max_x_intercept)); \
    EXPECT_FLOAT_EQ(expectedMinXIntercept, min_x_intercept);                 \
    EXPECT_FLOAT_EQ(expectedMaxXIntercept, max_x_intercept);                 \
  }

TEST(ContouredRectTest, zeroRadii) {
  FloatRoundedRect r = FloatRoundedRect(1, 2, 3, 4);

  EXPECT_EQ(gfx::RectF(1, 2, 3, 4), r.Rect());
  EXPECT_EQ(gfx::SizeF(), r.GetRadii().TopLeft());
  EXPECT_EQ(gfx::SizeF(), r.GetRadii().TopRight());
  EXPECT_EQ(gfx::SizeF(), r.GetRadii().BottomLeft());
  EXPECT_EQ(gfx::SizeF(), r.GetRadii().BottomRight());
  EXPECT_TRUE(r.GetRadii().IsZero());
  EXPECT_FALSE(r.IsRounded());
  EXPECT_FALSE(r.IsEmpty());

  EXPECT_EQ(gfx::RectF(1, 2, 0, 0), r.TopLeftCorner());
  EXPECT_EQ(gfx::RectF(4, 2, 0, 0), r.TopRightCorner());
  EXPECT_EQ(gfx::RectF(4, 6, 0, 0), r.BottomRightCorner());
  EXPECT_EQ(gfx::RectF(1, 6, 0, 0), r.BottomLeftCorner());

  TEST_INTERCEPTS(r, 2, r.Rect().x(), r.Rect().right());
  TEST_INTERCEPTS(r, 4, r.Rect().x(), r.Rect().right());
  TEST_INTERCEPTS(r, 6, r.Rect().x(), r.Rect().right());

  float min_x_intercept;
  float max_x_intercept;

  EXPECT_FALSE(
      ContouredRect(r).XInterceptsAtY(1, min_x_intercept, max_x_intercept));
  EXPECT_FALSE(
      ContouredRect(r).XInterceptsAtY(7, min_x_intercept, max_x_intercept));

  // The FloatRoundedRect::Outset() and Inset() don't change zero radii.
  r.Outset(20);
  EXPECT_TRUE(r.GetRadii().IsZero());
  r.Inset(10);
  EXPECT_TRUE(r.GetRadii().IsZero());
}

TEST(ContouredRectTest, circle) {
  gfx::SizeF corner_radii(50, 50);
  FloatRoundedRect r(gfx::RectF(0, 0, 100, 100), corner_radii, corner_radii,
                     corner_radii, corner_radii);

  EXPECT_EQ(gfx::RectF(0, 0, 100, 100), r.Rect());
  EXPECT_EQ(corner_radii, r.GetRadii().TopLeft());
  EXPECT_EQ(corner_radii, r.GetRadii().TopRight());
  EXPECT_EQ(corner_radii, r.GetRadii().BottomLeft());
  EXPECT_EQ(corner_radii, r.GetRadii().BottomRight());
  EXPECT_FALSE(r.GetRadii().IsZero());
  EXPECT_TRUE(r.IsRounded());
  EXPECT_FALSE(r.IsEmpty());

  EXPECT_EQ(gfx::RectF(0, 0, 50, 50), r.TopLeftCorner());
  EXPECT_EQ(gfx::RectF(50, 0, 50, 50), r.TopRightCorner());
  EXPECT_EQ(gfx::RectF(0, 50, 50, 50), r.BottomLeftCorner());
  EXPECT_EQ(gfx::RectF(50, 50, 50, 50), r.BottomRightCorner());

  TEST_INTERCEPTS(r, 0, 50, 50);
  TEST_INTERCEPTS(r, 25, 6.69873, 93.3013);
  TEST_INTERCEPTS(r, 50, 0, 100);
  TEST_INTERCEPTS(r, 75, 6.69873, 93.3013);
  TEST_INTERCEPTS(r, 100, 50, 50);

  float min_x_intercept;
  float max_x_intercept;

  EXPECT_FALSE(
      ContouredRect(r).XInterceptsAtY(-1, min_x_intercept, max_x_intercept));
  EXPECT_FALSE(
      ContouredRect(r).XInterceptsAtY(101, min_x_intercept, max_x_intercept));
}

/*
 * FloatRoundedRect geometry for this test. Corner radii are in parens, x and y
 * intercepts for the elliptical corners are noted. The rectangle itself is at
 * 0,0 with width and height 100.
 *
 *         (10, 15)  x=10      x=90 (10, 20)
 *                (--+---------+--)
 *           y=15 +--|         |-+ y=20
 *                |               |
 *                |               |
 *           y=85 + -|         |- + y=70
 *                (--+---------+--)
 *       (25, 15)  x=25      x=80  (20, 30)
 */
TEST(ContouredRectTest, ellipticalCorners) {
  FloatRoundedRect::Radii corner_radii;
  corner_radii.SetTopLeft(gfx::SizeF(10, 15));
  corner_radii.SetTopRight(gfx::SizeF(10, 20));
  corner_radii.SetBottomLeft(gfx::SizeF(25, 15));
  corner_radii.SetBottomRight(gfx::SizeF(20, 30));

  FloatRoundedRect r(gfx::RectF(0, 0, 100, 100), corner_radii);

  EXPECT_EQ(r.GetRadii(),
            FloatRoundedRect::Radii(gfx::SizeF(10, 15), gfx::SizeF(10, 20),
                                    gfx::SizeF(25, 15), gfx::SizeF(20, 30)));
  EXPECT_EQ(r, FloatRoundedRect(gfx::RectF(0, 0, 100, 100), corner_radii));

  EXPECT_EQ(gfx::RectF(0, 0, 10, 15), r.TopLeftCorner());
  EXPECT_EQ(gfx::RectF(90, 0, 10, 20), r.TopRightCorner());
  EXPECT_EQ(gfx::RectF(0, 85, 25, 15), r.BottomLeftCorner());
  EXPECT_EQ(gfx::RectF(80, 70, 20, 30), r.BottomRightCorner());

  TEST_INTERCEPTS(r, 5, 2.5464401, 96.61438);
  TEST_INTERCEPTS(r, 15, 0, 99.682457);
  TEST_INTERCEPTS(r, 20, 0, 100);
  TEST_INTERCEPTS(r, 50, 0, 100);
  TEST_INTERCEPTS(r, 70, 0, 100);
  TEST_INTERCEPTS(r, 85, 0, 97.320511);
  TEST_INTERCEPTS(r, 95, 6.3661003, 91.05542);

  float min_x_intercept;
  float max_x_intercept;

  EXPECT_FALSE(
      ContouredRect(r).XInterceptsAtY(-1, min_x_intercept, max_x_intercept));
  EXPECT_FALSE(
      ContouredRect(r).XInterceptsAtY(101, min_x_intercept, max_x_intercept));
}

TEST(ContouredRectTest, ToString) {
  gfx::SizeF corner_rect(1, 2);
  ContouredRect rect_with_curvature(
      FloatRoundedRect(gfx::RectF(1, 3, 5, 7),
                       FloatRoundedRect::Radii(corner_rect, corner_rect,
                                               corner_rect, corner_rect)),
      ContouredRect::CornerCurvature(1, 0.2222, 0, 3000));
  EXPECT_EQ(
      "1,3 5x7 radii:(tl:1x2; tr:1x2; bl:1x2; br:1x2) curvature:(tl:1.00; "
      "tr:0.22; bl:3000.00; br:0.00)",
      rect_with_curvature.ToString());
}

}  // namespace blink
