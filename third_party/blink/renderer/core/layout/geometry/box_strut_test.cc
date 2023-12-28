// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

// Ideally, this would be tested by BoxStrut::ConvertToPhysical, but
// this has not been implemented yet.
TEST(GeometryUnitsTest, ConvertPhysicalStrutToLogical) {
  test::TaskEnvironment task_environment;
  LayoutUnit left{5}, right{10}, top{15}, bottom{20};
  PhysicalBoxStrut physical{top, right, bottom, left};

  BoxStrut logical = physical.ConvertToLogical(
      {WritingMode::kHorizontalTb, TextDirection::kLtr});
  EXPECT_EQ(left, logical.inline_start);
  EXPECT_EQ(top, logical.block_start);

  logical = physical.ConvertToLogical(
      {WritingMode::kHorizontalTb, TextDirection::kRtl});
  EXPECT_EQ(right, logical.inline_start);
  EXPECT_EQ(top, logical.block_start);

  logical = physical.ConvertToLogical(
      {WritingMode::kVerticalLr, TextDirection::kLtr});
  EXPECT_EQ(top, logical.inline_start);
  EXPECT_EQ(left, logical.block_start);

  logical = physical.ConvertToLogical(
      {WritingMode::kVerticalLr, TextDirection::kRtl});
  EXPECT_EQ(bottom, logical.inline_start);
  EXPECT_EQ(left, logical.block_start);

  logical = physical.ConvertToLogical(
      {WritingMode::kVerticalRl, TextDirection::kLtr});
  EXPECT_EQ(top, logical.inline_start);
  EXPECT_EQ(right, logical.block_start);

  logical = physical.ConvertToLogical(
      {WritingMode::kVerticalRl, TextDirection::kRtl});
  EXPECT_EQ(bottom, logical.inline_start);
  EXPECT_EQ(right, logical.block_start);
}

TEST(GeometryUnitsTest, ConvertLogicalStrutToPhysical) {
  test::TaskEnvironment task_environment;
  LayoutUnit left{5}, right{10}, top{15}, bottom{20};
  BoxStrut logical(left, right, top, bottom);
  BoxStrut converted =
      logical
          .ConvertToPhysical({WritingMode::kHorizontalTb, TextDirection::kLtr})
          .ConvertToLogical({WritingMode::kHorizontalTb, TextDirection::kLtr});
  EXPECT_EQ(logical, converted);
  converted =
      logical
          .ConvertToPhysical({WritingMode::kHorizontalTb, TextDirection::kRtl})
          .ConvertToLogical({WritingMode::kHorizontalTb, TextDirection::kRtl});
  EXPECT_EQ(logical, converted);
  converted =
      logical.ConvertToPhysical({WritingMode::kVerticalLr, TextDirection::kLtr})
          .ConvertToLogical({WritingMode::kVerticalLr, TextDirection::kLtr});
  EXPECT_EQ(logical, converted);
  converted =
      logical.ConvertToPhysical({WritingMode::kVerticalLr, TextDirection::kRtl})
          .ConvertToLogical({WritingMode::kVerticalLr, TextDirection::kRtl});
  EXPECT_EQ(logical, converted);
  converted =
      logical.ConvertToPhysical({WritingMode::kVerticalRl, TextDirection::kLtr})
          .ConvertToLogical({WritingMode::kVerticalRl, TextDirection::kLtr});
  EXPECT_EQ(logical, converted);
  converted =
      logical.ConvertToPhysical({WritingMode::kVerticalRl, TextDirection::kRtl})
          .ConvertToLogical({WritingMode::kVerticalRl, TextDirection::kRtl});
  EXPECT_EQ(logical, converted);
  converted =
      logical.ConvertToPhysical({WritingMode::kSidewaysRl, TextDirection::kLtr})
          .ConvertToLogical({WritingMode::kSidewaysRl, TextDirection::kLtr});
  EXPECT_EQ(logical, converted);
  converted =
      logical.ConvertToPhysical({WritingMode::kSidewaysRl, TextDirection::kRtl})
          .ConvertToLogical({WritingMode::kSidewaysRl, TextDirection::kRtl});
  EXPECT_EQ(logical, converted);
  converted =
      logical.ConvertToPhysical({WritingMode::kSidewaysLr, TextDirection::kLtr})
          .ConvertToLogical({WritingMode::kSidewaysLr, TextDirection::kLtr});
  EXPECT_EQ(logical, converted);
  converted =
      logical.ConvertToPhysical({WritingMode::kSidewaysLr, TextDirection::kRtl})
          .ConvertToLogical({WritingMode::kSidewaysLr, TextDirection::kRtl});
  EXPECT_EQ(logical, converted);
}

TEST(PhysicalBoxStrutTest, Constructors) {
  test::TaskEnvironment task_environment;
  PhysicalBoxStrut result(0, std::numeric_limits<int>::max(), -1,
                          std::numeric_limits<int>::min());
  EXPECT_EQ(LayoutUnit(), result.top);
  EXPECT_EQ(LayoutUnit::FromRawValue(GetMaxSaturatedSetResultForTesting()),
            result.right);
  EXPECT_EQ(LayoutUnit(-1), result.bottom);
  EXPECT_EQ(LayoutUnit::Min(), result.left);
}

TEST(PhysicalBoxStrutTest, Enclosing) {
  test::TaskEnvironment task_environment;
  ASSERT_LT(0.01f, LayoutUnit::Epsilon());
  auto result = PhysicalBoxStrut::Enclosing(
      gfx::OutsetsF()
          .set_top(3.00f)
          .set_right(5.01f)
          .set_bottom(-7.001f)
          .set_left(LayoutUnit::Max().ToFloat() + 1));
  EXPECT_EQ(LayoutUnit(3), result.top);
  EXPECT_EQ(LayoutUnit(5 + LayoutUnit::Epsilon()), result.right);
  EXPECT_EQ(LayoutUnit(-7), result.bottom);
  EXPECT_EQ(LayoutUnit::Max(), result.left);
}

TEST(PhysicalBoxStrutTest, Unite) {
  test::TaskEnvironment task_environment;
  PhysicalBoxStrut strut(LayoutUnit(10));
  strut.Unite(
      {LayoutUnit(10), LayoutUnit(11), LayoutUnit(0), LayoutUnit::Max()});
  EXPECT_EQ(LayoutUnit(10), strut.top);
  EXPECT_EQ(LayoutUnit(11), strut.right);
  EXPECT_EQ(LayoutUnit(10), strut.bottom);
  EXPECT_EQ(LayoutUnit::Max(), strut.left);
}

}  // namespace

}  // namespace blink
