// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/axis.h"

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(AxisTest, StreamOperators) {
  test::TaskEnvironment task_environment;

  {
    std::stringstream ss;
    ss << kLogicalAxesNone;
    EXPECT_EQ("\"kLogicalAxesNone\"", ss.str());
  }
  {
    std::stringstream ss;
    ss << kLogicalAxesInline;
    EXPECT_EQ("\"kLogicalAxesInline\"", ss.str());
  }
  {
    std::stringstream ss;
    ss << kLogicalAxesBlock;
    EXPECT_EQ("\"kLogicalAxesBlock\"", ss.str());
  }
  {
    std::stringstream ss;
    ss << kLogicalAxesBoth;
    EXPECT_EQ("\"kLogicalAxesBoth\"", ss.str());
  }

  {
    std::stringstream ss;
    ss << kPhysicalAxesNone;
    EXPECT_EQ("\"kPhysicalAxesNone\"", ss.str());
  }
  {
    std::stringstream ss;
    ss << kPhysicalAxesHorizontal;
    EXPECT_EQ("\"kPhysicalAxesHorizontal\"", ss.str());
  }
  {
    std::stringstream ss;
    ss << kPhysicalAxesVertical;
    EXPECT_EQ("\"kPhysicalAxesVertical\"", ss.str());
  }
  {
    std::stringstream ss;
    ss << kPhysicalAxesBoth;
    EXPECT_EQ("\"kPhysicalAxesBoth\"", ss.str());
  }

  // Test fallback
  {
    std::stringstream ss;
    ss << LogicalAxes(100);
    EXPECT_EQ("\"LogicalAxes(100)\"", ss.str());
  }
  {
    std::stringstream ss;
    ss << PhysicalAxes(100);
    EXPECT_EQ("\"PhysicalAxes(100)\"", ss.str());
  }
}

TEST(AxisTest, LogicalAxesOperators) {
  test::TaskEnvironment task_environment;
  // operator |
  EXPECT_EQ(kLogicalAxesNone, (kLogicalAxesNone | kLogicalAxesNone));
  EXPECT_EQ(kLogicalAxesInline, (kLogicalAxesNone | kLogicalAxesInline));
  EXPECT_EQ(kLogicalAxesBoth, (kLogicalAxesInline | kLogicalAxesBlock));

  // operator |=
  {
    LogicalAxes axes(kLogicalAxesNone);
    EXPECT_EQ(kLogicalAxesNone, axes);
    axes |= kLogicalAxesInline;
    EXPECT_EQ(kLogicalAxesInline, axes);
    axes |= kLogicalAxesBlock;
    EXPECT_EQ(kLogicalAxesBoth, axes);
  }

  // operator &
  EXPECT_EQ(kLogicalAxesNone, (kLogicalAxesBoth & kLogicalAxesNone));
  EXPECT_EQ(kLogicalAxesInline, (kLogicalAxesInline & kLogicalAxesInline));
  EXPECT_EQ(kLogicalAxesInline, (kLogicalAxesBoth & kLogicalAxesInline));
  EXPECT_EQ(kLogicalAxesNone, (kLogicalAxesBlock & kLogicalAxesInline));

  // operator &=
  {
    LogicalAxes axes(kLogicalAxesBoth);
    EXPECT_EQ(kLogicalAxesBoth, axes);
    axes &= kLogicalAxesInline;
    EXPECT_EQ(kLogicalAxesInline, axes);
    axes &= kLogicalAxesBlock;
    EXPECT_EQ(kLogicalAxesNone, axes);
  }

  // operator ^
  EXPECT_EQ(kLogicalAxesNone, (kLogicalAxesNone ^ kLogicalAxesNone));
  EXPECT_EQ(kLogicalAxesNone, (kLogicalAxesBoth ^ kLogicalAxesBoth));
  EXPECT_EQ(kLogicalAxesBoth, (kLogicalAxesInline ^ kLogicalAxesBlock));
  EXPECT_EQ(kLogicalAxesBlock, (kLogicalAxesBoth ^ kLogicalAxesInline));

  // operator ^=
  {
    LogicalAxes axes(kLogicalAxesNone);
    EXPECT_EQ(kLogicalAxesNone, axes);
    axes ^= kLogicalAxesInline;
    EXPECT_EQ(kLogicalAxesInline, axes);
    axes ^= kLogicalAxesBoth;
    EXPECT_EQ(kLogicalAxesBlock, axes);
  }

  // operator -
  EXPECT_EQ(kLogicalAxesNone, (kLogicalAxesNone - kLogicalAxesNone));
  EXPECT_EQ(kLogicalAxesNone, (kLogicalAxesInline - kLogicalAxesBoth));
  EXPECT_EQ(kLogicalAxesInline, (kLogicalAxesBoth - kLogicalAxesBlock));
  EXPECT_EQ(kLogicalAxesInline, (kLogicalAxesInline - kLogicalAxesBlock));

  // operator -=
  {
    LogicalAxes axes(kLogicalAxesBoth);
    EXPECT_EQ(kLogicalAxesBoth, axes);
    axes -= kLogicalAxesInline;
    EXPECT_EQ(kLogicalAxesBlock, axes);
    axes -= kLogicalAxesBlock;
    EXPECT_EQ(kLogicalAxesNone, axes);
  }
}

TEST(AxisTest, PhysicalAxesOperators) {
  test::TaskEnvironment task_environment;
  // operator |
  EXPECT_EQ(kPhysicalAxesNone, (kPhysicalAxesNone | kPhysicalAxesNone));
  EXPECT_EQ(kPhysicalAxesHorizontal,
            (kPhysicalAxesNone | kPhysicalAxesHorizontal));
  EXPECT_EQ(kPhysicalAxesBoth,
            (kPhysicalAxesHorizontal | kPhysicalAxesVertical));

  // operator |=
  {
    PhysicalAxes axes(kPhysicalAxesNone);
    EXPECT_EQ(kPhysicalAxesNone, axes);
    axes |= kPhysicalAxesHorizontal;
    EXPECT_EQ(kPhysicalAxesHorizontal, axes);
    axes |= kPhysicalAxesVertical;
    EXPECT_EQ(kPhysicalAxesBoth, axes);
  }

  // operator &
  EXPECT_EQ(kPhysicalAxesNone, (kPhysicalAxesBoth & kPhysicalAxesNone));
  EXPECT_EQ(kPhysicalAxesHorizontal,
            (kPhysicalAxesHorizontal & kPhysicalAxesHorizontal));
  EXPECT_EQ(kPhysicalAxesHorizontal,
            (kPhysicalAxesBoth & kPhysicalAxesHorizontal));
  EXPECT_EQ(kPhysicalAxesNone,
            (kPhysicalAxesVertical & kPhysicalAxesHorizontal));

  // operator &=
  {
    PhysicalAxes axes(kPhysicalAxesBoth);
    EXPECT_EQ(kPhysicalAxesBoth, axes);
    axes &= kPhysicalAxesHorizontal;
    EXPECT_EQ(kPhysicalAxesHorizontal, axes);
    axes &= kPhysicalAxesVertical;
    EXPECT_EQ(kPhysicalAxesNone, axes);
  }

  // operator ^
  EXPECT_EQ(kPhysicalAxesNone, (kPhysicalAxesNone ^ kPhysicalAxesNone));
  EXPECT_EQ(kPhysicalAxesNone, (kPhysicalAxesBoth ^ kPhysicalAxesBoth));
  EXPECT_EQ(kPhysicalAxesBoth,
            (kPhysicalAxesHorizontal ^ kPhysicalAxesVertical));
  EXPECT_EQ(kPhysicalAxesVertical,
            (kPhysicalAxesBoth ^ kPhysicalAxesHorizontal));

  // operator ^=
  {
    PhysicalAxes axes(kPhysicalAxesNone);
    EXPECT_EQ(kPhysicalAxesNone, axes);
    axes ^= kPhysicalAxesHorizontal;
    EXPECT_EQ(kPhysicalAxesHorizontal, axes);
    axes ^= kPhysicalAxesBoth;
    EXPECT_EQ(kPhysicalAxesVertical, axes);
  }

  // operator -
  EXPECT_EQ(kPhysicalAxesNone, (kPhysicalAxesNone - kPhysicalAxesNone));
  EXPECT_EQ(kPhysicalAxesNone, (kPhysicalAxesHorizontal - kPhysicalAxesBoth));
  EXPECT_EQ(kPhysicalAxesHorizontal,
            (kPhysicalAxesBoth - kPhysicalAxesVertical));
  EXPECT_EQ(kPhysicalAxesHorizontal,
            (kPhysicalAxesHorizontal - kPhysicalAxesVertical));

  // operator -=
  {
    PhysicalAxes axes(kPhysicalAxesBoth);
    EXPECT_EQ(kPhysicalAxesBoth, axes);
    axes -= kPhysicalAxesHorizontal;
    EXPECT_EQ(kPhysicalAxesVertical, axes);
    axes -= kPhysicalAxesVertical;
    EXPECT_EQ(kPhysicalAxesNone, axes);
  }
}

TEST(AxisTest, ToPhysicalAxes) {
  test::TaskEnvironment task_environment;
  ASSERT_TRUE(IsHorizontalWritingMode(WritingMode::kHorizontalTb));
  ASSERT_FALSE(IsHorizontalWritingMode(WritingMode::kVerticalRl));

  EXPECT_EQ(kPhysicalAxesNone,
            ToPhysicalAxes(kLogicalAxesNone, WritingMode::kHorizontalTb));
  EXPECT_EQ(kPhysicalAxesNone,
            ToPhysicalAxes(kLogicalAxesNone, WritingMode::kVerticalRl));

  EXPECT_EQ(kPhysicalAxesBoth,
            ToPhysicalAxes(kLogicalAxesBoth, WritingMode::kHorizontalTb));
  EXPECT_EQ(kPhysicalAxesBoth,
            ToPhysicalAxes(kLogicalAxesBoth, WritingMode::kVerticalRl));

  EXPECT_EQ(kPhysicalAxesHorizontal,
            ToPhysicalAxes(kLogicalAxesInline, WritingMode::kHorizontalTb));
  EXPECT_EQ(kPhysicalAxesVertical,
            ToPhysicalAxes(kLogicalAxesInline, WritingMode::kVerticalRl));

  EXPECT_EQ(kPhysicalAxesVertical,
            ToPhysicalAxes(kLogicalAxesBlock, WritingMode::kHorizontalTb));
  EXPECT_EQ(kPhysicalAxesHorizontal,
            ToPhysicalAxes(kLogicalAxesBlock, WritingMode::kVerticalRl));
}

TEST(AxisTest, ToLogicalAxes) {
  test::TaskEnvironment task_environment;
  ASSERT_TRUE(IsHorizontalWritingMode(WritingMode::kHorizontalTb));
  ASSERT_FALSE(IsHorizontalWritingMode(WritingMode::kVerticalRl));

  EXPECT_EQ(kLogicalAxesNone,
            ToLogicalAxes(kPhysicalAxesNone, WritingMode::kHorizontalTb));
  EXPECT_EQ(kLogicalAxesNone,
            ToLogicalAxes(kPhysicalAxesNone, WritingMode::kVerticalRl));

  EXPECT_EQ(kLogicalAxesBoth,
            ToLogicalAxes(kPhysicalAxesBoth, WritingMode::kHorizontalTb));
  EXPECT_EQ(kLogicalAxesBoth,
            ToLogicalAxes(kPhysicalAxesBoth, WritingMode::kVerticalRl));

  EXPECT_EQ(kLogicalAxesInline,
            ToLogicalAxes(kPhysicalAxesHorizontal, WritingMode::kHorizontalTb));
  EXPECT_EQ(kLogicalAxesBlock,
            ToLogicalAxes(kPhysicalAxesHorizontal, WritingMode::kVerticalRl));

  EXPECT_EQ(kLogicalAxesBlock,
            ToLogicalAxes(kPhysicalAxesVertical, WritingMode::kHorizontalTb));
  EXPECT_EQ(kLogicalAxesInline,
            ToLogicalAxes(kPhysicalAxesVertical, WritingMode::kVerticalRl));
}

}  // namespace blink
