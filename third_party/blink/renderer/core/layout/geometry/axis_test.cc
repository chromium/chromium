// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/axis.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(AxisTest, LogicalAxesOperators) {
  // operator |
  EXPECT_EQ(kLogicalAxisNone, (kLogicalAxisNone | kLogicalAxisNone));
  EXPECT_EQ(kLogicalAxisInline, (kLogicalAxisNone | kLogicalAxisInline));
  EXPECT_EQ(kLogicalAxisBoth, (kLogicalAxisInline | kLogicalAxisBlock));

  // operator |=
  {
    LogicalAxes axes(kLogicalAxisNone);
    EXPECT_EQ(kLogicalAxisNone, axes);
    axes |= kLogicalAxisInline;
    EXPECT_EQ(kLogicalAxisInline, axes);
    axes |= kLogicalAxisBlock;
    EXPECT_EQ(kLogicalAxisBoth, axes);
  }

  // operator &
  EXPECT_EQ(kLogicalAxisNone, (kLogicalAxisBoth & kLogicalAxisNone));
  EXPECT_EQ(kLogicalAxisInline, (kLogicalAxisInline & kLogicalAxisInline));
  EXPECT_EQ(kLogicalAxisInline, (kLogicalAxisBoth & kLogicalAxisInline));
  EXPECT_EQ(kLogicalAxisNone, (kLogicalAxisBlock & kLogicalAxisInline));

  // operator &=
  {
    LogicalAxes axes(kLogicalAxisBoth);
    EXPECT_EQ(kLogicalAxisBoth, axes);
    axes &= kLogicalAxisInline;
    EXPECT_EQ(kLogicalAxisInline, axes);
    axes &= kLogicalAxisBlock;
    EXPECT_EQ(kLogicalAxisNone, axes);
  }
}

TEST(AxisTest, PhysicalAxesOperators) {
  // operator |
  EXPECT_EQ(kPhysicalAxisNone, (kPhysicalAxisNone | kPhysicalAxisNone));
  EXPECT_EQ(kPhysicalAxisHorizontal,
            (kPhysicalAxisNone | kPhysicalAxisHorizontal));
  EXPECT_EQ(kPhysicalAxisBoth,
            (kPhysicalAxisHorizontal | kPhysicalAxisVertical));

  // operator |=
  {
    PhysicalAxes axes(kPhysicalAxisNone);
    EXPECT_EQ(kPhysicalAxisNone, axes);
    axes |= kPhysicalAxisHorizontal;
    EXPECT_EQ(kPhysicalAxisHorizontal, axes);
    axes |= kPhysicalAxisVertical;
    EXPECT_EQ(kPhysicalAxisBoth, axes);
  }

  // operator &
  EXPECT_EQ(kPhysicalAxisNone, (kPhysicalAxisBoth & kPhysicalAxisNone));
  EXPECT_EQ(kPhysicalAxisHorizontal,
            (kPhysicalAxisHorizontal & kPhysicalAxisHorizontal));
  EXPECT_EQ(kPhysicalAxisHorizontal,
            (kPhysicalAxisBoth & kPhysicalAxisHorizontal));
  EXPECT_EQ(kPhysicalAxisNone,
            (kPhysicalAxisVertical & kPhysicalAxisHorizontal));

  // operator &=
  {
    PhysicalAxes axes(kPhysicalAxisBoth);
    EXPECT_EQ(kPhysicalAxisBoth, axes);
    axes &= kPhysicalAxisHorizontal;
    EXPECT_EQ(kPhysicalAxisHorizontal, axes);
    axes &= kPhysicalAxisVertical;
    EXPECT_EQ(kPhysicalAxisNone, axes);
  }
}

TEST(AxisTest, ToPhysicalAxes) {
  ASSERT_TRUE(IsHorizontalWritingMode(WritingMode::kHorizontalTb));
  ASSERT_FALSE(IsHorizontalWritingMode(WritingMode::kVerticalRl));

  EXPECT_EQ(kPhysicalAxisNone,
            ToPhysicalAxes(kLogicalAxisNone, WritingMode::kHorizontalTb));
  EXPECT_EQ(kPhysicalAxisNone,
            ToPhysicalAxes(kLogicalAxisNone, WritingMode::kVerticalRl));

  EXPECT_EQ(kPhysicalAxisBoth,
            ToPhysicalAxes(kLogicalAxisBoth, WritingMode::kHorizontalTb));
  EXPECT_EQ(kPhysicalAxisBoth,
            ToPhysicalAxes(kLogicalAxisBoth, WritingMode::kVerticalRl));

  EXPECT_EQ(kPhysicalAxisHorizontal,
            ToPhysicalAxes(kLogicalAxisInline, WritingMode::kHorizontalTb));
  EXPECT_EQ(kPhysicalAxisVertical,
            ToPhysicalAxes(kLogicalAxisInline, WritingMode::kVerticalRl));

  EXPECT_EQ(kPhysicalAxisVertical,
            ToPhysicalAxes(kLogicalAxisBlock, WritingMode::kHorizontalTb));
  EXPECT_EQ(kPhysicalAxisHorizontal,
            ToPhysicalAxes(kLogicalAxisBlock, WritingMode::kVerticalRl));
}

TEST(AxisTest, ToLogicalAxes) {
  ASSERT_TRUE(IsHorizontalWritingMode(WritingMode::kHorizontalTb));
  ASSERT_FALSE(IsHorizontalWritingMode(WritingMode::kVerticalRl));

  EXPECT_EQ(kLogicalAxisNone,
            ToLogicalAxes(kPhysicalAxisNone, WritingMode::kHorizontalTb));
  EXPECT_EQ(kLogicalAxisNone,
            ToLogicalAxes(kPhysicalAxisNone, WritingMode::kVerticalRl));

  EXPECT_EQ(kLogicalAxisBoth,
            ToLogicalAxes(kPhysicalAxisBoth, WritingMode::kHorizontalTb));
  EXPECT_EQ(kLogicalAxisBoth,
            ToLogicalAxes(kPhysicalAxisBoth, WritingMode::kVerticalRl));

  EXPECT_EQ(kLogicalAxisInline,
            ToLogicalAxes(kPhysicalAxisHorizontal, WritingMode::kHorizontalTb));
  EXPECT_EQ(kLogicalAxisBlock,
            ToLogicalAxes(kPhysicalAxisHorizontal, WritingMode::kVerticalRl));

  EXPECT_EQ(kLogicalAxisBlock,
            ToLogicalAxes(kPhysicalAxisVertical, WritingMode::kHorizontalTb));
  EXPECT_EQ(kLogicalAxisInline,
            ToLogicalAxes(kPhysicalAxisVertical, WritingMode::kVerticalRl));
}

}  // namespace blink
