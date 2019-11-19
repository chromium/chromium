// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

struct PhysicalOffsetRectUniteTestData {
  const char* test_case;
  PhysicalRect a;
  PhysicalRect b;
  PhysicalRect expected;
} physical_offset_rect_unite_test_data[] = {
    {"all_empty", {}, {}, {}},
    {"a empty", {}, {1, 2, 3, 4}, {1, 2, 3, 4}},
    {"b empty", {1, 2, 3, 4}, {}, {1, 2, 3, 4}},
    {"a larger", {100, 50, 300, 200}, {200, 50, 200, 200}, {100, 50, 300, 200}},
    {"b larger", {200, 50, 200, 200}, {100, 50, 300, 200}, {100, 50, 300, 200}},
};

std::ostream& operator<<(std::ostream& os,
                         const PhysicalOffsetRectUniteTestData& data) {
  return os << "Unite " << data.test_case;
}

class PhysicalRectUniteTest
    : public testing::Test,
      public testing::WithParamInterface<PhysicalOffsetRectUniteTestData> {};

INSTANTIATE_TEST_SUITE_P(
    GeometryUnitsTest,
    PhysicalRectUniteTest,
    testing::ValuesIn(physical_offset_rect_unite_test_data));

TEST_P(PhysicalRectUniteTest, Data) {
  const auto& data = GetParam();
  PhysicalRect actual = data.a;
  actual.Unite(data.b);
  EXPECT_EQ(data.expected, actual);
}

TEST(PhysicalRectTest, InclusiveIntersect) {
  PhysicalRect rect(11, 12, 0, 0);
  EXPECT_TRUE(rect.InclusiveIntersect(PhysicalRect(11, 12, 13, 14)));
  EXPECT_EQ(rect, PhysicalRect(11, 12, 0, 0));

  rect = PhysicalRect(11, 12, 13, 14);
  EXPECT_TRUE(rect.InclusiveIntersect(PhysicalRect(24, 8, 0, 7)));
  EXPECT_EQ(rect, PhysicalRect(24, 12, 0, 3));

  rect = PhysicalRect(11, 12, 13, 14);
  EXPECT_TRUE(rect.InclusiveIntersect(PhysicalRect(9, 15, 4, 0)));
  EXPECT_EQ(rect, PhysicalRect(11, 15, 2, 0));

  rect = PhysicalRect(11, 12, 0, 14);
  EXPECT_FALSE(rect.InclusiveIntersect(PhysicalRect(12, 13, 15, 16)));
  EXPECT_EQ(rect, PhysicalRect());
}

TEST(PhysicalRectTest, IntersectsInclusively) {
  PhysicalRect a(11, 12, 0, 0);
  PhysicalRect b(11, 12, 13, 14);
  // An empty rect can have inclusive intersection.
  EXPECT_TRUE(a.IntersectsInclusively(b));
  EXPECT_TRUE(b.IntersectsInclusively(a));

  a = PhysicalRect(11, 12, 13, 14);
  b = PhysicalRect(24, 8, 0, 7);
  // Intersecting left side is sufficient for inclusive intersection.
  EXPECT_TRUE(a.IntersectsInclusively(b));
  EXPECT_TRUE(b.IntersectsInclusively(a));

  a = PhysicalRect(11, 12, 13, 14);
  b = PhysicalRect(0, 26, 13, 8);
  // Intersecting bottom side is sufficient for inclusive intersection.
  EXPECT_TRUE(a.IntersectsInclusively(b));
  EXPECT_TRUE(b.IntersectsInclusively(a));

  a = PhysicalRect(11, 12, 0, 0);
  b = PhysicalRect(11, 12, 0, 0);
  // Two empty rects can intersect inclusively.
  EXPECT_TRUE(a.IntersectsInclusively(b));
  EXPECT_TRUE(b.IntersectsInclusively(a));

  a = PhysicalRect(10, 10, 10, 10);
  b = PhysicalRect(20, 20, 10, 10);
  // Two rects can intersect inclusively at a single point.
  EXPECT_TRUE(a.IntersectsInclusively(b));
  EXPECT_TRUE(b.IntersectsInclusively(a));

  a = PhysicalRect(11, 12, 0, 0);
  b = PhysicalRect(20, 21, 0, 0);
  // Two empty rects that do not touch do not intersect.
  EXPECT_FALSE(a.IntersectsInclusively(b));
  EXPECT_FALSE(b.IntersectsInclusively(a));

  a = PhysicalRect(11, 12, 5, 5);
  b = PhysicalRect(20, 21, 0, 0);
  // A rect that does not touch a point does not intersect.
  EXPECT_FALSE(a.IntersectsInclusively(b));
  EXPECT_FALSE(b.IntersectsInclusively(a));
}

TEST(PhysicalRectTest, EnclosingIntRect) {
  LayoutUnit small;
  small.SetRawValue(1);
  PhysicalRect small_dimensions_rect(LayoutUnit(42.5f), LayoutUnit(84.5f),
                                     small, small);
  EXPECT_EQ(IntRect(42, 84, 1, 1), EnclosingIntRect(small_dimensions_rect));

  PhysicalRect integral_rect(100, 150, 200, 350);
  EXPECT_EQ(IntRect(100, 150, 200, 350), EnclosingIntRect(integral_rect));

  PhysicalRect fractional_pos_rect(LayoutUnit(100.6f), LayoutUnit(150.8f),
                                   LayoutUnit(200), LayoutUnit(350));
  EXPECT_EQ(IntRect(100, 150, 201, 351), EnclosingIntRect(fractional_pos_rect));

  PhysicalRect fractional_dimensions_rect(
      LayoutUnit(100), LayoutUnit(150), LayoutUnit(200.6f), LayoutUnit(350.4f));
  EXPECT_EQ(IntRect(100, 150, 201, 351),
            EnclosingIntRect(fractional_dimensions_rect));

  PhysicalRect fractional_both_rect1(LayoutUnit(100.6f), LayoutUnit(150.8f),
                                     LayoutUnit(200.4f), LayoutUnit(350.2f));
  EXPECT_EQ(IntRect(100, 150, 201, 351),
            EnclosingIntRect(fractional_both_rect1));

  PhysicalRect fractional_both_rect2(LayoutUnit(100.5f), LayoutUnit(150.7f),
                                     LayoutUnit(200.3f), LayoutUnit(350.3f));
  EXPECT_EQ(IntRect(100, 150, 201, 351),
            EnclosingIntRect(fractional_both_rect2));

  PhysicalRect fractional_both_rect3(LayoutUnit(100.3f), LayoutUnit(150.2f),
                                     LayoutUnit(200.8f), LayoutUnit(350.9f));
  EXPECT_EQ(IntRect(100, 150, 202, 352),
            EnclosingIntRect(fractional_both_rect3));

  PhysicalRect fractional_negpos_rect1(LayoutUnit(-100.4f), LayoutUnit(-150.8f),
                                       LayoutUnit(200), LayoutUnit(350));
  EXPECT_EQ(IntRect(-101, -151, 201, 351),
            EnclosingIntRect(fractional_negpos_rect1));

  PhysicalRect fractional_negpos_rect2(LayoutUnit(-100.5f), LayoutUnit(-150.7f),
                                       LayoutUnit(199.4f), LayoutUnit(350.3f));
  EXPECT_EQ(IntRect(-101, -151, 200, 351),
            EnclosingIntRect(fractional_negpos_rect2));

  PhysicalRect fractional_negpos_rect3(LayoutUnit(-100.3f), LayoutUnit(-150.2f),
                                       LayoutUnit(199.6f), LayoutUnit(350.3f));
  EXPECT_EQ(IntRect(-101, -151, 201, 352),
            EnclosingIntRect(fractional_negpos_rect3));
}

TEST(LayoutRectTest, EdgesOnPixelBoundaries) {
  EXPECT_TRUE(PhysicalRect().EdgesOnPixelBoundaries());
  EXPECT_TRUE(PhysicalRect(1, 1, 1, 1).EdgesOnPixelBoundaries());
  EXPECT_TRUE(PhysicalRect(1, -1, 1, 1).EdgesOnPixelBoundaries());
  EXPECT_TRUE(PhysicalRect(-1, 10, 10, 0).EdgesOnPixelBoundaries());
  EXPECT_TRUE(PhysicalRect(-5, -7, 10, 7).EdgesOnPixelBoundaries());
  EXPECT_TRUE(PhysicalRect(10, 5, -2, -3).EdgesOnPixelBoundaries());
  EXPECT_TRUE(PhysicalRect(LayoutUnit(1.0f), LayoutUnit(5), LayoutUnit(10),
                           LayoutUnit(3))
                  .EdgesOnPixelBoundaries());

  EXPECT_FALSE(PhysicalRect(LayoutUnit(9.3f), LayoutUnit(5), LayoutUnit(10),
                            LayoutUnit(3))
                   .EdgesOnPixelBoundaries());
  EXPECT_FALSE(PhysicalRect(LayoutUnit(0.5f), LayoutUnit(5), LayoutUnit(10),
                            LayoutUnit(3))
                   .EdgesOnPixelBoundaries());
  EXPECT_FALSE(PhysicalRect(LayoutUnit(-0.5f), LayoutUnit(-5), LayoutUnit(10),
                            LayoutUnit(3))
                   .EdgesOnPixelBoundaries());
  EXPECT_FALSE(PhysicalRect(LayoutUnit(-0.5f), LayoutUnit(-2), LayoutUnit(10),
                            LayoutUnit(3))
                   .EdgesOnPixelBoundaries());
  EXPECT_FALSE(PhysicalRect(LayoutUnit(-0.5f), LayoutUnit(5.1f), LayoutUnit(10),
                            LayoutUnit(3))
                   .EdgesOnPixelBoundaries());
  EXPECT_FALSE(PhysicalRect(LayoutUnit(3), LayoutUnit(5.1f), LayoutUnit(10),
                            LayoutUnit(3))
                   .EdgesOnPixelBoundaries());
  EXPECT_FALSE(PhysicalRect(LayoutUnit(3), LayoutUnit(5), LayoutUnit(10.2f),
                            LayoutUnit(3))
                   .EdgesOnPixelBoundaries());
  EXPECT_FALSE(PhysicalRect(LayoutUnit(3), LayoutUnit(5), LayoutUnit(10),
                            LayoutUnit(0.3f))
                   .EdgesOnPixelBoundaries());
}

TEST(PhysicalRectTest, ExpandEdgesToPixelBoundaries) {
  LayoutUnit small;
  small.SetRawValue(1);
  PhysicalRect small_dimensions_rect(LayoutUnit(42.5f), LayoutUnit(84.5f),
                                     small, small);
  small_dimensions_rect.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(PhysicalRect(42, 84, 1, 1), small_dimensions_rect);

  PhysicalRect integral_rect(100, 150, 200, 350);
  integral_rect.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(PhysicalRect(100, 150, 200, 350), integral_rect);

  PhysicalRect fractional_pos_rect(LayoutUnit(100.6f), LayoutUnit(150.8f),
                                   LayoutUnit(200), LayoutUnit(350));
  fractional_pos_rect.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(PhysicalRect(100, 150, 201, 351), fractional_pos_rect);

  PhysicalRect fractional_dimensions_rect(
      LayoutUnit(100), LayoutUnit(150), LayoutUnit(200.6f), LayoutUnit(350.4f));
  fractional_dimensions_rect.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(PhysicalRect(100, 150, 201, 351), fractional_dimensions_rect);

  PhysicalRect fractional_both_rect1(LayoutUnit(100.6f), LayoutUnit(150.8f),
                                     LayoutUnit(200.4f), LayoutUnit(350.2f));
  fractional_both_rect1.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(PhysicalRect(100, 150, 201, 351), fractional_both_rect1);

  PhysicalRect fractional_both_rect2(LayoutUnit(100.5f), LayoutUnit(150.7f),
                                     LayoutUnit(200.3f), LayoutUnit(350.3f));
  fractional_both_rect2.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(PhysicalRect(100, 150, 201, 351), fractional_both_rect2);

  PhysicalRect fractional_both_rect3(LayoutUnit(100.3f), LayoutUnit(150.2f),
                                     LayoutUnit(200.8f), LayoutUnit(350.9f));
  fractional_both_rect3.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(PhysicalRect(100, 150, 202, 352), fractional_both_rect3);

  PhysicalRect fractional_negpos_rect1(LayoutUnit(-100.4f), LayoutUnit(-150.8f),
                                       LayoutUnit(200), LayoutUnit(350));
  fractional_negpos_rect1.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(PhysicalRect(-101, -151, 201, 351), fractional_negpos_rect1);

  PhysicalRect fractional_negpos_rect2(LayoutUnit(-100.5f), LayoutUnit(-150.7f),
                                       LayoutUnit(199.4f), LayoutUnit(350.3f));
  fractional_negpos_rect2.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(PhysicalRect(-101, -151, 200, 351), fractional_negpos_rect2);

  PhysicalRect fractional_negpos_rect3(LayoutUnit(-100.3f), LayoutUnit(-150.2f),
                                       LayoutUnit(199.6f), LayoutUnit(350.3f));
  fractional_negpos_rect3.ExpandEdgesToPixelBoundaries();
  EXPECT_EQ(PhysicalRect(-101, -151, 201, 352), fractional_negpos_rect3);
}

}  // namespace

}  // namespace blink
