// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/float_rect.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/geometry_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

TEST(FloatRectTest, SquaredDistanceToTest) {
  //
  //  O--x
  //  |
  //  y
  //
  //     FloatRect.x()   FloatRect.maxX()
  //            |          |
  //        1   |    2     |  3
  //      ======+==========+======   --FloatRect.y()
  //        4   |    5(in) |  6
  //      ======+==========+======   --FloatRect.maxY()
  //        7   |    8     |  9
  //

  FloatRect r1(100, 100, 250, 150);

  // `1` case
  gfx::PointF p1(80, 80);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p1), 800.f);

  gfx::PointF p2(-10, -10);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p2), 24200.f);

  gfx::PointF p3(80, -10);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p3), 12500.f);

  // `2` case
  gfx::PointF p4(110, 80);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p4), 400.f);

  gfx::PointF p5(150, 0);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p5), 10000.f);

  gfx::PointF p6(180, -10);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p6), 12100.f);

  // `3` case
  gfx::PointF p7(400, 80);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p7), 2900.f);

  gfx::PointF p8(360, -10);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p8), 12200.f);

  // `4` case
  gfx::PointF p9(80, 110);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p9), 400.f);

  gfx::PointF p10(-10, 180);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p10), 12100.f);

  // `5`(& In) case
  gfx::PointF p11(100, 100);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p11), 0.f);

  gfx::PointF p12(150, 100);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p12), 0.f);

  gfx::PointF p13(350, 100);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p13), 0.f);

  gfx::PointF p14(350, 150);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p14), 0.f);

  gfx::PointF p15(350, 250);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p15), 0.f);

  gfx::PointF p16(150, 250);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p16), 0.f);

  gfx::PointF p17(100, 250);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p17), 0.f);

  gfx::PointF p18(100, 150);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p18), 0.f);

  gfx::PointF p19(150, 150);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p19), 0.f);

  // `6` case
  gfx::PointF p20(380, 150);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p20), 900.f);

  // `7` case
  gfx::PointF p21(80, 280);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p21), 1300.f);

  gfx::PointF p22(-10, 300);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p22), 14600.f);

  // `8` case
  gfx::PointF p23(180, 300);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p23), 2500.f);

  // `9` case
  gfx::PointF p24(450, 450);
  EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual,
                      r1.SquaredDistanceTo(p24), 50000.f);
}

// TODO(crbug.com/851414): Reenable this.
#if defined(OS_ANDROID)
#define MAYBE_ToString DISABLED_ToString
#else
#define MAYBE_ToString ToString
#endif
TEST(FloatRectTest, MAYBE_ToString) {
  FloatRect empty_rect = FloatRect();
  EXPECT_EQ("0,0 0x0", empty_rect.ToString());

  FloatRect rect(1, 2, 3, 4);
  EXPECT_EQ("1,2 3x4", rect.ToString());

  FloatRect granular_rect(1.6f, 2.7f, 3.8f, 4.9f);
  EXPECT_EQ("1.6,2.7 3.8x4.9", granular_rect.ToString());

  FloatRect min_max_rect(
      std::numeric_limits<float>::min(), std::numeric_limits<float>::max(),
      std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
  EXPECT_EQ("1.17549e-38,3.40282e+38 1.17549e-38x3.40282e+38",
            min_max_rect.ToString());

  FloatRect infinite_rect(-std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::infinity());
  EXPECT_EQ("-inf,-inf infxinf", infinite_rect.ToString());

  FloatRect nan_rect(0, 0, std::numeric_limits<float>::signaling_NaN(),
                     std::numeric_limits<float>::signaling_NaN());
  EXPECT_EQ("0,0 nanxnan", nan_rect.ToString());
}

TEST(FloatRectTest, ToEnclosingRect) {
  FloatRect small_dimensions_rect(42.5f, 84.5f,
                                  std::numeric_limits<float>::epsilon(),
                                  std::numeric_limits<float>::epsilon());
  EXPECT_EQ(gfx::Rect(42, 84, 1, 1), ToEnclosingRect(small_dimensions_rect));

  FloatRect integral_rect(100, 150, 200, 350);
  EXPECT_EQ(gfx::Rect(100, 150, 200, 350), ToEnclosingRect(integral_rect));

  FloatRect fractional_pos_rect(100.6f, 150.8f, 200, 350);
  EXPECT_EQ(gfx::Rect(100, 150, 201, 351),
            ToEnclosingRect(fractional_pos_rect));

  FloatRect fractional_dimensions_rect(100, 150, 200.6f, 350.4f);
  EXPECT_EQ(gfx::Rect(100, 150, 201, 351),
            ToEnclosingRect(fractional_dimensions_rect));

  FloatRect fractional_both_rect1(100.6f, 150.8f, 200.4f, 350.2f);
  EXPECT_EQ(gfx::Rect(100, 150, 201, 351),
            ToEnclosingRect(fractional_both_rect1));

  FloatRect fractional_both_rect2(100.6f, 150.8f, 200.3f, 350.3f);
  EXPECT_EQ(gfx::Rect(100, 150, 201, 352),
            ToEnclosingRect(fractional_both_rect2));

  FloatRect fractional_both_rect3(100.6f, 150.8f, 200.5f, 350.3f);
  EXPECT_EQ(gfx::Rect(100, 150, 202, 352),
            ToEnclosingRect(fractional_both_rect3));

  FloatRect fractional_negpos_rect1(-100.4f, -150.8f, 200, 350);
  EXPECT_EQ(gfx::Rect(-101, -151, 201, 351),
            ToEnclosingRect(fractional_negpos_rect1));

  FloatRect fractional_negpos_rect2(-100.4f, -150.8f, 199.4f, 350.3f);
  EXPECT_EQ(gfx::Rect(-101, -151, 200, 351),
            ToEnclosingRect(fractional_negpos_rect2));

  FloatRect fractional_negpos_rect3(-100.6f, -150.8f, 199.6f, 350.3f);
  EXPECT_EQ(gfx::Rect(-101, -151, 201, 351),
            ToEnclosingRect(fractional_negpos_rect3));

  FloatRect max_rect(-std::numeric_limits<float>::max() / 2,
                     -std::numeric_limits<float>::max() / 2,
                     std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max());
  EXPECT_EQ(gfx::Rect(INT_MIN, INT_MIN, INT_MAX, INT_MAX),
            ToEnclosingRect(max_rect));
}

TEST(FloatRectTest, ToEnclosedRect) {
  FloatRect small_dimensions_rect(42.5f, 84.5f,
                                  std::numeric_limits<float>::epsilon(),
                                  std::numeric_limits<float>::epsilon());
  EXPECT_EQ(gfx::Rect(43, 85, 0, 0), ToEnclosedRect(small_dimensions_rect));

  FloatRect integral_rect(100, 150, 200, 350);
  EXPECT_EQ(gfx::Rect(100, 150, 200, 350), ToEnclosedRect(integral_rect));

  FloatRect fractional_pos_rect(100.6f, 150.8f, 200, 350);
  EXPECT_EQ(gfx::Rect(101, 151, 199, 349), ToEnclosedRect(fractional_pos_rect));

  FloatRect fractional_dimensions_rect(100, 150, 200.6f, 350.4f);
  EXPECT_EQ(gfx::Rect(100, 150, 200, 350),
            ToEnclosedRect(fractional_dimensions_rect));

  FloatRect fractional_both_rect1(100.6f, 150.8f, 200.4f, 350.2f);
  EXPECT_EQ(gfx::Rect(101, 151, 200, 350),
            ToEnclosedRect(fractional_both_rect1));

  FloatRect fractional_both_rect2(100.6f, 150.8f, 200.3f, 350.3f);
  EXPECT_EQ(gfx::Rect(101, 151, 199, 350),
            ToEnclosedRect(fractional_both_rect2));

  FloatRect fractional_both_rect3(100.6f, 150.8f, 200.5f, 350.3f);
  EXPECT_EQ(gfx::Rect(101, 151, 200, 350),
            ToEnclosedRect(fractional_both_rect3));

  FloatRect fractional_negpos_rect1(-100.4f, -150.8f, 200, 350);
  EXPECT_EQ(gfx::Rect(-100, -150, 199, 349),
            ToEnclosedRect(fractional_negpos_rect1));

  FloatRect fractional_negpos_rect2(-100.4f, -150.8f, 199.5f, 350.3f);
  EXPECT_EQ(gfx::Rect(-100, -150, 199, 349),
            ToEnclosedRect(fractional_negpos_rect2));

  FloatRect fractional_negpos_rect3(-100.6f, -150.8f, 199.6f, 350.3f);
  EXPECT_EQ(gfx::Rect(-100, -150, 199, 349),
            ToEnclosedRect(fractional_negpos_rect3));

  FloatRect max_rect(-std::numeric_limits<float>::max() / 2,
                     -std::numeric_limits<float>::max() / 2,
                     std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max());
  // Location of the result is not (INT_MIN,INT_MIN) because gfx::Rect also
  // clamps right and bottom in int range.
  EXPECT_EQ(gfx::Rect(INT_MIN / 2 - 1, INT_MIN / 2 - 1, INT_MAX, INT_MAX),
            ToEnclosedRect(max_rect));
}

}  // namespace blink
