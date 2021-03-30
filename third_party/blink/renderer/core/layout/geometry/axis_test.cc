// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/axis.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(AxisTest, LogicalAxesOperators) {
  // operator |
  EXPECT_EQ(LogicalAxes(kLogicalAxisNone),
            (LogicalAxes(kLogicalAxisNone) | LogicalAxes(kLogicalAxisNone)));
  EXPECT_EQ(LogicalAxes(kLogicalAxisInline),
            (LogicalAxes(kLogicalAxisNone) | LogicalAxes(kLogicalAxisInline)));
  EXPECT_EQ(LogicalAxes(kLogicalAxisBoth),
            (LogicalAxes(kLogicalAxisInline) | LogicalAxes(kLogicalAxisBlock)));

  // operator |=
  {
    LogicalAxes axes(kLogicalAxisNone);
    EXPECT_EQ(LogicalAxes(kLogicalAxisNone), axes);
    axes |= LogicalAxes(kLogicalAxisInline);
    EXPECT_EQ(LogicalAxes(kLogicalAxisInline), axes);
    axes |= LogicalAxes(kLogicalAxisBlock);
    EXPECT_EQ(LogicalAxes(kLogicalAxisBoth), axes);
  }

  // operator &
  EXPECT_EQ(LogicalAxes(kLogicalAxisNone),
            (LogicalAxes(kLogicalAxisBoth) & LogicalAxes(kLogicalAxisNone)));
  EXPECT_EQ(LogicalAxes(kLogicalAxisInline), (LogicalAxes(kLogicalAxisInline) &
                                              LogicalAxes(kLogicalAxisInline)));
  EXPECT_EQ(LogicalAxes(kLogicalAxisInline),
            (LogicalAxes(kLogicalAxisBoth) & LogicalAxes(kLogicalAxisInline)));
  EXPECT_EQ(LogicalAxes(kLogicalAxisNone),
            (LogicalAxes(kLogicalAxisBlock) & LogicalAxes(kLogicalAxisInline)));

  // operator &=
  {
    LogicalAxes axes(kLogicalAxisBoth);
    EXPECT_EQ(LogicalAxes(kLogicalAxisBoth), axes);
    axes &= LogicalAxes(kLogicalAxisInline);
    EXPECT_EQ(LogicalAxes(kLogicalAxisInline), axes);
    axes &= LogicalAxes(kLogicalAxisBlock);
    EXPECT_EQ(LogicalAxes(kLogicalAxisNone), axes);
  }
}

TEST(AxisTest, PhysicalAxesOperators) {
  // operator |
  EXPECT_EQ(PhysicalAxes(kPhysicalAxisNone), (PhysicalAxes(kPhysicalAxisNone) |
                                              PhysicalAxes(kPhysicalAxisNone)));
  EXPECT_EQ(PhysicalAxes(kPhysicalAxisHorizontal),
            (PhysicalAxes(kPhysicalAxisNone) |
             PhysicalAxes(kPhysicalAxisHorizontal)));
  EXPECT_EQ(PhysicalAxes(kPhysicalAxisBoth),
            (PhysicalAxes(kPhysicalAxisHorizontal) |
             PhysicalAxes(kPhysicalAxisVertical)));

  // operator |=
  {
    PhysicalAxes axes(kPhysicalAxisNone);
    EXPECT_EQ(PhysicalAxes(kPhysicalAxisNone), axes);
    axes |= PhysicalAxes(kPhysicalAxisHorizontal);
    EXPECT_EQ(PhysicalAxes(kPhysicalAxisHorizontal), axes);
    axes |= PhysicalAxes(kPhysicalAxisVertical);
    EXPECT_EQ(PhysicalAxes(kPhysicalAxisBoth), axes);
  }

  // operator &
  EXPECT_EQ(PhysicalAxes(kPhysicalAxisNone), (PhysicalAxes(kPhysicalAxisBoth) &
                                              PhysicalAxes(kPhysicalAxisNone)));
  EXPECT_EQ(PhysicalAxes(kPhysicalAxisHorizontal),
            (PhysicalAxes(kPhysicalAxisHorizontal) &
             PhysicalAxes(kPhysicalAxisHorizontal)));
  EXPECT_EQ(PhysicalAxes(kPhysicalAxisHorizontal),
            (PhysicalAxes(kPhysicalAxisBoth) &
             PhysicalAxes(kPhysicalAxisHorizontal)));
  EXPECT_EQ(PhysicalAxes(kPhysicalAxisNone),
            (PhysicalAxes(kPhysicalAxisVertical) &
             PhysicalAxes(kPhysicalAxisHorizontal)));

  // operator &=
  {
    PhysicalAxes axes(kPhysicalAxisBoth);
    EXPECT_EQ(PhysicalAxes(kPhysicalAxisBoth), axes);
    axes &= PhysicalAxes(kPhysicalAxisHorizontal);
    EXPECT_EQ(PhysicalAxes(kPhysicalAxisHorizontal), axes);
    axes &= PhysicalAxes(kPhysicalAxisVertical);
    EXPECT_EQ(PhysicalAxes(kPhysicalAxisNone), axes);
  }
}

TEST(AxisTest, ToPhysicalAxes) {
  ASSERT_TRUE(IsHorizontalWritingMode(WritingMode::kHorizontalTb));
  ASSERT_FALSE(IsHorizontalWritingMode(WritingMode::kVerticalRl));

  EXPECT_EQ(PhysicalAxes(kPhysicalAxisNone),
            ToPhysicalAxes(LogicalAxes(kLogicalAxisNone),
                           WritingMode::kHorizontalTb));
  EXPECT_EQ(
      PhysicalAxes(kPhysicalAxisNone),
      ToPhysicalAxes(LogicalAxes(kLogicalAxisNone), WritingMode::kVerticalRl));

  EXPECT_EQ(PhysicalAxes(kPhysicalAxisBoth),
            ToPhysicalAxes(LogicalAxes(kLogicalAxisBoth),
                           WritingMode::kHorizontalTb));
  EXPECT_EQ(
      PhysicalAxes(kPhysicalAxisBoth),
      ToPhysicalAxes(LogicalAxes(kLogicalAxisBoth), WritingMode::kVerticalRl));

  EXPECT_EQ(PhysicalAxes(kPhysicalAxisHorizontal),
            ToPhysicalAxes(LogicalAxes(kLogicalAxisInline),
                           WritingMode::kHorizontalTb));
  EXPECT_EQ(PhysicalAxes(kPhysicalAxisVertical),
            ToPhysicalAxes(LogicalAxes(kLogicalAxisInline),
                           WritingMode::kVerticalRl));

  EXPECT_EQ(PhysicalAxes(kPhysicalAxisVertical),
            ToPhysicalAxes(LogicalAxes(kLogicalAxisBlock),
                           WritingMode::kHorizontalTb));
  EXPECT_EQ(
      PhysicalAxes(kPhysicalAxisHorizontal),
      ToPhysicalAxes(LogicalAxes(kLogicalAxisBlock), WritingMode::kVerticalRl));
}

}  // namespace blink
