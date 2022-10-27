// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

TEST(LayoutRectTest, ToString) {
  LayoutRect empty_rect = LayoutRect();
  EXPECT_EQ("0,0 0x0", empty_rect.ToString());

  LayoutRect rect(1, 2, 3, 4);
  EXPECT_EQ("1,2 3x4", rect.ToString());

  LayoutRect granular_rect(LayoutUnit(1.6f), LayoutUnit(2.7f), LayoutUnit(3.8f),
                           LayoutUnit(4.9f));
  EXPECT_EQ("1.59375,2.6875 3.79688x4.89063", granular_rect.ToString());
}

TEST(LayoutRectTest, InclusiveIntersect) {
  LayoutRect rect(11, 12, 0, 0);
  EXPECT_TRUE(rect.InclusiveIntersect(LayoutRect(11, 12, 13, 14)));
  EXPECT_EQ(rect, LayoutRect(11, 12, 0, 0));

  rect = LayoutRect(11, 12, 13, 14);
  EXPECT_TRUE(rect.InclusiveIntersect(LayoutRect(24, 8, 0, 7)));
  EXPECT_EQ(rect, LayoutRect(24, 12, 0, 3));

  rect = LayoutRect(11, 12, 13, 14);
  EXPECT_TRUE(rect.InclusiveIntersect(LayoutRect(9, 15, 4, 0)));
  EXPECT_EQ(rect, LayoutRect(11, 15, 2, 0));

  rect = LayoutRect(11, 12, 0, 14);
  EXPECT_FALSE(rect.InclusiveIntersect(LayoutRect(12, 13, 15, 16)));
  EXPECT_EQ(rect, LayoutRect());
}

TEST(LayoutRectTest, IntersectsInclusively) {
  LayoutRect a(11, 12, 0, 0);
  LayoutRect b(11, 12, 13, 14);
  // An empty rect can have inclusive intersection.
  EXPECT_TRUE(a.IntersectsInclusively(b));
  EXPECT_TRUE(b.IntersectsInclusively(a));

  a = LayoutRect(11, 12, 13, 14);
  b = LayoutRect(24, 8, 0, 7);
  // Intersecting left side is sufficient for inclusive intersection.
  EXPECT_TRUE(a.IntersectsInclusively(b));
  EXPECT_TRUE(b.IntersectsInclusively(a));

  a = LayoutRect(11, 12, 13, 14);
  b = LayoutRect(0, 26, 13, 8);
  // Intersecting bottom side is sufficient for inclusive intersection.
  EXPECT_TRUE(a.IntersectsInclusively(b));
  EXPECT_TRUE(b.IntersectsInclusively(a));

  a = LayoutRect(11, 12, 0, 0);
  b = LayoutRect(11, 12, 0, 0);
  // Two empty rects can intersect inclusively.
  EXPECT_TRUE(a.IntersectsInclusively(b));
  EXPECT_TRUE(b.IntersectsInclusively(a));

  a = LayoutRect(10, 10, 10, 10);
  b = LayoutRect(20, 20, 10, 10);
  // Two rects can intersect inclusively at a single point.
  EXPECT_TRUE(a.IntersectsInclusively(b));
  EXPECT_TRUE(b.IntersectsInclusively(a));

  a = LayoutRect(11, 12, 0, 0);
  b = LayoutRect(20, 21, 0, 0);
  // Two empty rects that do not touch do not intersect.
  EXPECT_FALSE(a.IntersectsInclusively(b));
  EXPECT_FALSE(b.IntersectsInclusively(a));

  a = LayoutRect(11, 12, 5, 5);
  b = LayoutRect(20, 21, 0, 0);
  // A rect that does not touch a point does not intersect.
  EXPECT_FALSE(a.IntersectsInclusively(b));
  EXPECT_FALSE(b.IntersectsInclusively(a));
}

TEST(LayoutRectTest, ToEnclosingRect) {
  LayoutUnit small;
  small.SetRawValue(1);
  LayoutRect small_dimensions_rect(LayoutUnit(42.5f), LayoutUnit(84.5f), small,
                                   small);
  EXPECT_EQ(gfx::Rect(42, 84, 1, 1), ToEnclosingRect(small_dimensions_rect));

  LayoutRect integral_rect(gfx::Rect(100, 150, 200, 350));
  EXPECT_EQ(gfx::Rect(100, 150, 200, 350), ToEnclosingRect(integral_rect));

  LayoutRect fractional_pos_rect(LayoutUnit(100.6f), LayoutUnit(150.8f),
                                 LayoutUnit(200), LayoutUnit(350));
  EXPECT_EQ(gfx::Rect(100, 150, 201, 351),
            ToEnclosingRect(fractional_pos_rect));

  LayoutRect fractional_dimensions_rect(LayoutUnit(100), LayoutUnit(150),
                                        LayoutUnit(200.6f), LayoutUnit(350.4f));
  EXPECT_EQ(gfx::Rect(100, 150, 201, 351),
            ToEnclosingRect(fractional_dimensions_rect));

  LayoutRect fractional_both_rect1(LayoutUnit(100.6f), LayoutUnit(150.8f),
                                   LayoutUnit(200.4f), LayoutUnit(350.2f));
  EXPECT_EQ(gfx::Rect(100, 150, 201, 351),
            ToEnclosingRect(fractional_both_rect1));

  LayoutRect fractional_both_rect2(LayoutUnit(100.5f), LayoutUnit(150.7f),
                                   LayoutUnit(200.3f), LayoutUnit(350.3f));
  EXPECT_EQ(gfx::Rect(100, 150, 201, 351),
            ToEnclosingRect(fractional_both_rect2));

  LayoutRect fractional_both_rect3(LayoutUnit(100.3f), LayoutUnit(150.2f),
                                   LayoutUnit(200.8f), LayoutUnit(350.9f));
  EXPECT_EQ(gfx::Rect(100, 150, 202, 352),
            ToEnclosingRect(fractional_both_rect3));

  LayoutRect fractional_negpos_rect1(LayoutUnit(-100.4f), LayoutUnit(-150.8f),
                                     LayoutUnit(200), LayoutUnit(350));
  EXPECT_EQ(gfx::Rect(-101, -151, 201, 351),
            ToEnclosingRect(fractional_negpos_rect1));

  LayoutRect fractional_negpos_rect2(LayoutUnit(-100.5f), LayoutUnit(-150.7f),
                                     LayoutUnit(199.4f), LayoutUnit(350.3f));
  EXPECT_EQ(gfx::Rect(-101, -151, 200, 351),
            ToEnclosingRect(fractional_negpos_rect2));

  LayoutRect fractional_negpos_rect3(LayoutUnit(-100.3f), LayoutUnit(-150.2f),
                                     LayoutUnit(199.6f), LayoutUnit(350.3f));
  EXPECT_EQ(gfx::Rect(-101, -151, 201, 352),
            ToEnclosingRect(fractional_negpos_rect3));
}

TEST(LayoutRectTest, EdgesOnPixelBoundaries) {
  EXPECT_TRUE(LayoutRect().EdgesOnPixelBoundaries());
  EXPECT_TRUE(
      LayoutRect(LayoutUnit(1), LayoutUnit(1), LayoutUnit(1), LayoutUnit(1))
          .EdgesOnPixelBoundaries());
  EXPECT_TRUE(
      LayoutRect(LayoutUnit(1), LayoutUnit(-1), LayoutUnit(1), LayoutUnit(1))
          .EdgesOnPixelBoundaries());
  EXPECT_TRUE(
      LayoutRect(LayoutUnit(-1), LayoutUnit(10), LayoutUnit(10), LayoutUnit(0))
          .EdgesOnPixelBoundaries());
  EXPECT_TRUE(
      LayoutRect(LayoutUnit(-5), LayoutUnit(-7), LayoutUnit(10), LayoutUnit(7))
          .EdgesOnPixelBoundaries());
  EXPECT_TRUE(
      LayoutRect(LayoutUnit(10), LayoutUnit(5), LayoutUnit(-2), LayoutUnit(-3))
          .EdgesOnPixelBoundaries());
  EXPECT_TRUE(
      LayoutRect(LayoutUnit(1.0f), LayoutUnit(5), LayoutUnit(10), LayoutUnit(3))
          .EdgesOnPixelBoundaries());

  EXPECT_FALSE(
      LayoutRect(LayoutUnit(9.3f), LayoutUnit(5), LayoutUnit(10), LayoutUnit(3))
          .EdgesOnPixelBoundaries());
  EXPECT_FALSE(
      LayoutRect(LayoutUnit(0.5f), LayoutUnit(5), LayoutUnit(10), LayoutUnit(3))
          .EdgesOnPixelBoundaries());
  EXPECT_FALSE(LayoutRect(LayoutUnit(-0.5f), LayoutUnit(-5), LayoutUnit(10),
                          LayoutUnit(3))
                   .EdgesOnPixelBoundaries());
  EXPECT_FALSE(LayoutRect(LayoutUnit(-0.5f), LayoutUnit(-2), LayoutUnit(10),
                          LayoutUnit(3))
                   .EdgesOnPixelBoundaries());
  EXPECT_FALSE(LayoutRect(LayoutUnit(-0.5f), LayoutUnit(5.1f), LayoutUnit(10),
                          LayoutUnit(3))
                   .EdgesOnPixelBoundaries());
  EXPECT_FALSE(
      LayoutRect(LayoutUnit(3), LayoutUnit(5.1f), LayoutUnit(10), LayoutUnit(3))
          .EdgesOnPixelBoundaries());
  EXPECT_FALSE(
      LayoutRect(LayoutUnit(3), LayoutUnit(5), LayoutUnit(10.2f), LayoutUnit(3))
          .EdgesOnPixelBoundaries());
  EXPECT_FALSE(
      LayoutRect(LayoutUnit(3), LayoutUnit(5), LayoutUnit(10), LayoutUnit(0.3f))
          .EdgesOnPixelBoundaries());
}

TEST(LayoutRectTest, ExpandEdgesToPixelBoundaries) {
  LayoutUnit small;
  small.SetRawValue(1);
  LayoutRect small_dimensions_rect(LayoutUnit(42.5f), LayoutUnit(84.5f), small,
                                   small);
  small_dimensions_rect.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(LayoutRect(gfx::Rect(42, 84, 1, 1)), small_dimensions_rect);

  LayoutRect integral_rect(gfx::Rect(100, 150, 200, 350));
  integral_rect.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(LayoutRect(gfx::Rect(100, 150, 200, 350)), integral_rect);

  LayoutRect fractional_pos_rect(LayoutUnit(100.6f), LayoutUnit(150.8f),
                                 LayoutUnit(200), LayoutUnit(350));
  fractional_pos_rect.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(LayoutRect(gfx::Rect(100, 150, 201, 351)), fractional_pos_rect);

  LayoutRect fractional_dimensions_rect(LayoutUnit(100), LayoutUnit(150),
                                        LayoutUnit(200.6f), LayoutUnit(350.4f));
  fractional_dimensions_rect.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(LayoutRect(gfx::Rect(100, 150, 201, 351)),
            fractional_dimensions_rect);

  LayoutRect fractional_both_rect1(LayoutUnit(100.6f), LayoutUnit(150.8f),
                                   LayoutUnit(200.4f), LayoutUnit(350.2f));
  fractional_both_rect1.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(LayoutRect(gfx::Rect(100, 150, 201, 351)), fractional_both_rect1);

  LayoutRect fractional_both_rect2(LayoutUnit(100.5f), LayoutUnit(150.7f),
                                   LayoutUnit(200.3f), LayoutUnit(350.3f));
  fractional_both_rect2.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(LayoutRect(gfx::Rect(100, 150, 201, 351)), fractional_both_rect2);

  LayoutRect fractional_both_rect3(LayoutUnit(100.3f), LayoutUnit(150.2f),
                                   LayoutUnit(200.8f), LayoutUnit(350.9f));
  fractional_both_rect3.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(LayoutRect(gfx::Rect(100, 150, 202, 352)), fractional_both_rect3);

  LayoutRect fractional_negpos_rect1(LayoutUnit(-100.4f), LayoutUnit(-150.8f),
                                     LayoutUnit(200), LayoutUnit(350));
  fractional_negpos_rect1.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(LayoutRect(gfx::Rect(-101, -151, 201, 351)),
            fractional_negpos_rect1);

  LayoutRect fractional_negpos_rect2(LayoutUnit(-100.5f), LayoutUnit(-150.7f),
                                     LayoutUnit(199.4f), LayoutUnit(350.3f));
  fractional_negpos_rect2.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(LayoutRect(gfx::Rect(-101, -151, 200, 351)),
            fractional_negpos_rect2);

  LayoutRect fractional_negpos_rect3(LayoutUnit(-100.3f), LayoutUnit(-150.2f),
                                     LayoutUnit(199.6f), LayoutUnit(350.3f));
  fractional_negpos_rect3.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(LayoutRect(gfx::Rect(-101, -151, 201, 352)),
            fractional_negpos_rect3);
}

TEST(LayoutRectTest, InfiniteIntRect) {
  gfx::Rect r = LayoutRect::InfiniteIntRect();
  EXPECT_TRUE(r.Contains(gfx::Rect(-8000000, -8000000, 16000000, 16000000)));
  // The rect can be converted to LayoutRect and back without loss of accuracy.
  EXPECT_EQ(ToEnclosingRect(LayoutRect(r)), r);
  EXPECT_EQ(ToPixelSnappedRect(LayoutRect(r)), r);
  for (int i = 0; i < 50; i++) {
    // Modified rect with visible right/bottom can be converted to gfx::RectF
    // or LayoutRect and back without loss of accuracy.
    r.set_width(r.x() + i);
    r.set_height(r.y() + i + 2000);
    EXPECT_EQ(gfx::ToEnclosingRect(gfx::RectF(r)), r);
    EXPECT_EQ(gfx::ToEnclosedRect(gfx::RectF(r)), r);
    EXPECT_EQ(ToEnclosingRect(LayoutRect(r)), r);
    EXPECT_EQ(ToPixelSnappedRect(LayoutRect(r)), r);
  }
}

struct LayoutRectUniteTestData {
  const char* test_case;
  LayoutRect a;
  LayoutRect b;
  LayoutRect expected;
} layout_rect_unite_test_data[] = {
    {"empty", {}, {}, {}},
    {"a empty", {}, {1, 2, 3, 4}, {1, 2, 3, 4}},
    {"b empty", {1, 2, 3, 4}, {}, {1, 2, 3, 4}},
    {"a larger", {100, 50, 300, 200}, {200, 50, 200, 200}, {100, 50, 300, 200}},
    {"b larger", {200, 50, 200, 200}, {100, 50, 300, 200}, {100, 50, 300, 200}},
    {"saturated width",
     {-1000, 0, 200, 200},
     {33554402, 500, 30, 100},
     {0, 0, 99999999, 600}},
    {"saturated height",
     {0, -1000, 200, 200},
     {0, 33554402, 100, 30},
     {0, 0, 200, 99999999}},
};

std::ostream& operator<<(std::ostream& os,
                         const LayoutRectUniteTestData& data) {
  return os << "Unite " << data.test_case;
}

class LayoutRectUniteTest
    : public testing::Test,
      public testing::WithParamInterface<LayoutRectUniteTestData> {};

INSTANTIATE_TEST_SUITE_P(LayoutRectTest,
                         LayoutRectUniteTest,
                         testing::ValuesIn(layout_rect_unite_test_data));

TEST_P(LayoutRectUniteTest, Data) {
  const auto& data = GetParam();
  LayoutRect actual = data.a;
  actual.Unite(data.b);

  LayoutRect expected = data.expected;
  constexpr int kExtraForSaturation = 2000;
  // On arm, you cannot actually get the true saturated value just by
  // setting via LayoutUnit constructor. Instead, add to the expected
  // value to actually get a saturated expectation (which is what happens in
  // the Unite operation).
  if (data.expected.Width() == GetMaxSaturatedSetResultForTesting()) {
    expected.Expand(LayoutSize(kExtraForSaturation, 0));
  }

  if (data.expected.Height() == GetMaxSaturatedSetResultForTesting()) {
    expected.Expand(LayoutSize(0, kExtraForSaturation));
  }
  EXPECT_EQ(expected, actual);
}

}  // namespace blink
