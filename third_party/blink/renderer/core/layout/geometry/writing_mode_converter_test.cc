// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

TEST(WritingModeConverterTest, ConvertLogicalOffsetToPhysicalOffset) {
  test::TaskEnvironment task_environment;
  LogicalOffset logical_offset(20, 30);
  PhysicalSize outer_size(300, 400);
  PhysicalSize inner_size(5, 65);
  PhysicalOffset offset;

  offset = WritingModeConverter(
               {WritingMode::kHorizontalTb, TextDirection::kLtr}, outer_size)
               .ToPhysical(logical_offset, inner_size);
  EXPECT_EQ(PhysicalOffset(20, 30), offset);

  offset = WritingModeConverter(
               {WritingMode::kHorizontalTb, TextDirection::kRtl}, outer_size)
               .ToPhysical(logical_offset, inner_size);
  EXPECT_EQ(PhysicalOffset(275, 30), offset);

  offset = WritingModeConverter({WritingMode::kVerticalRl, TextDirection::kLtr},
                                outer_size)
               .ToPhysical(logical_offset, inner_size);
  EXPECT_EQ(PhysicalOffset(265, 20), offset);

  offset = WritingModeConverter({WritingMode::kVerticalRl, TextDirection::kRtl},
                                outer_size)
               .ToPhysical(logical_offset, inner_size);
  EXPECT_EQ(PhysicalOffset(265, 315), offset);

  offset = WritingModeConverter({WritingMode::kSidewaysRl, TextDirection::kLtr},
                                outer_size)
               .ToPhysical(logical_offset, inner_size);
  EXPECT_EQ(PhysicalOffset(265, 20), offset);

  offset = WritingModeConverter({WritingMode::kSidewaysRl, TextDirection::kRtl},
                                outer_size)
               .ToPhysical(logical_offset, inner_size);
  EXPECT_EQ(PhysicalOffset(265, 315), offset);

  offset = WritingModeConverter({WritingMode::kVerticalLr, TextDirection::kLtr},
                                outer_size)
               .ToPhysical(logical_offset, inner_size);
  EXPECT_EQ(PhysicalOffset(30, 20), offset);

  offset = WritingModeConverter({WritingMode::kVerticalLr, TextDirection::kRtl},
                                outer_size)
               .ToPhysical(logical_offset, inner_size);
  EXPECT_EQ(PhysicalOffset(30, 315), offset);

  offset = WritingModeConverter({WritingMode::kSidewaysLr, TextDirection::kLtr},
                                outer_size)
               .ToPhysical(logical_offset, inner_size);
  EXPECT_EQ(PhysicalOffset(30, 315), offset);

  offset = WritingModeConverter({WritingMode::kSidewaysLr, TextDirection::kRtl},
                                outer_size)
               .ToPhysical(logical_offset, inner_size);
  EXPECT_EQ(PhysicalOffset(30, 20), offset);
}

TEST(WritingModeConverterTest, ConvertPhysicalOffsetToLogicalOffset) {
  test::TaskEnvironment task_environment;
  PhysicalOffset physical_offset(20, 30);
  PhysicalSize outer_size(300, 400);
  PhysicalSize inner_size(5, 65);
  LogicalOffset offset;

  offset = WritingModeConverter(
               {WritingMode::kHorizontalTb, TextDirection::kLtr}, outer_size)
               .ToLogical(physical_offset, inner_size);
  EXPECT_EQ(LogicalOffset(20, 30), offset);

  offset = WritingModeConverter(
               {WritingMode::kHorizontalTb, TextDirection::kRtl}, outer_size)
               .ToLogical(physical_offset, inner_size);
  EXPECT_EQ(LogicalOffset(275, 30), offset);

  offset = WritingModeConverter({WritingMode::kVerticalRl, TextDirection::kLtr},
                                outer_size)
               .ToLogical(physical_offset, inner_size);
  EXPECT_EQ(LogicalOffset(30, 275), offset);

  offset = WritingModeConverter({WritingMode::kVerticalRl, TextDirection::kRtl},
                                outer_size)
               .ToLogical(physical_offset, inner_size);
  EXPECT_EQ(LogicalOffset(305, 275), offset);

  offset = WritingModeConverter({WritingMode::kSidewaysRl, TextDirection::kLtr},
                                outer_size)
               .ToLogical(physical_offset, inner_size);
  EXPECT_EQ(LogicalOffset(30, 275), offset);

  offset = WritingModeConverter({WritingMode::kSidewaysRl, TextDirection::kRtl},
                                outer_size)
               .ToLogical(physical_offset, inner_size);
  EXPECT_EQ(LogicalOffset(305, 275), offset);

  offset = WritingModeConverter({WritingMode::kVerticalLr, TextDirection::kLtr},
                                outer_size)
               .ToLogical(physical_offset, inner_size);
  EXPECT_EQ(LogicalOffset(30, 20), offset);

  offset = WritingModeConverter({WritingMode::kVerticalLr, TextDirection::kRtl},
                                outer_size)
               .ToLogical(physical_offset, inner_size);
  EXPECT_EQ(LogicalOffset(305, 20), offset);

  offset = WritingModeConverter({WritingMode::kSidewaysLr, TextDirection::kLtr},
                                outer_size)
               .ToLogical(physical_offset, inner_size);
  EXPECT_EQ(LogicalOffset(305, 20), offset);

  offset = WritingModeConverter({WritingMode::kSidewaysLr, TextDirection::kRtl},
                                outer_size)
               .ToLogical(physical_offset, inner_size);
  EXPECT_EQ(LogicalOffset(30, 20), offset);
}

}  // namespace

}  // namespace blink
