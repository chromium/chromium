// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/inset_area.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"

namespace blink {

const WritingDirectionMode vertical_lr_rtl(WritingMode::kVerticalLr,
                                           TextDirection::kRtl);
const WritingDirectionMode vertical_rl_ltr(WritingMode::kVerticalRl,
                                           TextDirection::kLtr);
const WritingDirectionMode horizontal_tb_ltr(WritingMode::kHorizontalTb,
                                             TextDirection::kLtr);
const WritingDirectionMode horizontal_tb_rtl(WritingMode::kHorizontalTb,
                                             TextDirection::kRtl);

struct ToPhysicalTestCase {
  InsetArea logical;
  InsetArea expected_physical;
  WritingDirectionMode container_writing_direction;
  WritingDirectionMode self_writing_direction;
};

ToPhysicalTestCase to_physical_test_cases[] = {
    {
        {InsetAreaRegion::kAll, InsetAreaRegion::kAll, InsetAreaRegion::kTop,
         InsetAreaRegion::kCenter},
        {InsetAreaRegion::kTop, InsetAreaRegion::kCenter,
         InsetAreaRegion::kLeft, InsetAreaRegion::kRight},
        horizontal_tb_ltr,
        horizontal_tb_ltr,
    },
    {
        {InsetAreaRegion::kXStart, InsetAreaRegion::kXStart,
         InsetAreaRegion::kYStart, InsetAreaRegion::kYStart},
        {InsetAreaRegion::kTop, InsetAreaRegion::kTop, InsetAreaRegion::kRight,
         InsetAreaRegion::kRight},
        horizontal_tb_rtl,
        horizontal_tb_ltr,
    },
    {
        {InsetAreaRegion::kXSelfEnd, InsetAreaRegion::kXSelfEnd,
         InsetAreaRegion::kYSelfEnd, InsetAreaRegion::kYSelfEnd},
        {InsetAreaRegion::kBottom, InsetAreaRegion::kBottom,
         InsetAreaRegion::kLeft, InsetAreaRegion::kLeft},
        horizontal_tb_ltr,
        horizontal_tb_rtl,
    },
    {
        // block-axis (containing block) / inline-axis (containing block) since
        // both are neutral. First span becomes physical "center right" because
        // of vertical-rl / ltr. Second becomes "center bottom" because of
        // horizontal-tb / rtl.
        {InsetAreaRegion::kStart, InsetAreaRegion::kCenter,
         InsetAreaRegion::kCenter, InsetAreaRegion::kSelfEnd},
        {InsetAreaRegion::kCenter, InsetAreaRegion::kBottom,
         InsetAreaRegion::kCenter, InsetAreaRegion::kRight},
        vertical_rl_ltr,
        horizontal_tb_rtl,
    },
    {
        // block-axis (self) / inline-axis (self) since both are neutral. First
        // span becomes physical "right" because of vertical-lr. Second becomes
        // "bottom" because of rtl.
        {InsetAreaRegion::kSelfStart, InsetAreaRegion::kSelfStart,
         InsetAreaRegion::kSelfEnd, InsetAreaRegion::kSelfEnd},
        {InsetAreaRegion::kBottom, InsetAreaRegion::kBottom,
         InsetAreaRegion::kRight, InsetAreaRegion::kRight},
        horizontal_tb_ltr,
        vertical_lr_rtl,
    },
};

class InsetAreaToPhysicalTest
    : public testing::Test,
      public testing::WithParamInterface<ToPhysicalTestCase> {};

INSTANTIATE_TEST_SUITE_P(All,
                         InsetAreaToPhysicalTest,
                         testing::ValuesIn(to_physical_test_cases));

TEST_P(InsetAreaToPhysicalTest, All) {
  const ToPhysicalTestCase& test_case = GetParam();
  EXPECT_EQ(test_case.logical.ToPhysical(test_case.container_writing_direction,
                                         test_case.self_writing_direction),
            test_case.expected_physical);
}

struct UsedInsetsTestCase {
  InsetArea physical;
  const Length& expected_top;
  const Length& expected_bottom;
  const Length& expected_left;
  const Length& expected_right;
};

UsedInsetsTestCase used_insets_test_cases[] = {
    {{InsetAreaRegion::kTop, InsetAreaRegion::kTop, InsetAreaRegion::kLeft,
      InsetAreaRegion::kLeft},
     Length::FixedZero(),
     InsetArea::AnchorTop(),
     Length::FixedZero(),
     InsetArea::AnchorLeft()},
    {{InsetAreaRegion::kCenter, InsetAreaRegion::kCenter,
      InsetAreaRegion::kCenter, InsetAreaRegion::kCenter},
     InsetArea::AnchorTop(),
     InsetArea::AnchorBottom(),
     InsetArea::AnchorLeft(),
     InsetArea::AnchorRight()},
    {{InsetAreaRegion::kBottom, InsetAreaRegion::kBottom,
      InsetAreaRegion::kRight, InsetAreaRegion::kRight},
     InsetArea::AnchorBottom(),
     Length::FixedZero(),
     InsetArea::AnchorRight(),
     Length::FixedZero()},
    {{InsetAreaRegion::kTop, InsetAreaRegion::kCenter, InsetAreaRegion::kLeft,
      InsetAreaRegion::kCenter},
     Length::FixedZero(),
     InsetArea::AnchorBottom(),
     Length::FixedZero(),
     InsetArea::AnchorRight()},
    {{InsetAreaRegion::kCenter, InsetAreaRegion::kBottom,
      InsetAreaRegion::kCenter, InsetAreaRegion::kRight},
     InsetArea::AnchorTop(),
     Length::FixedZero(),
     InsetArea::AnchorLeft(),
     Length::FixedZero()},
    {{InsetAreaRegion::kTop, InsetAreaRegion::kBottom, InsetAreaRegion::kLeft,
      InsetAreaRegion::kRight},
     Length::FixedZero(),
     Length::FixedZero(),
     Length::FixedZero(),
     Length::FixedZero()},
};

class InsetAreaUsedInsetsTest
    : public testing::Test,
      public testing::WithParamInterface<UsedInsetsTestCase> {};

INSTANTIATE_TEST_SUITE_P(All,
                         InsetAreaUsedInsetsTest,
                         testing::ValuesIn(used_insets_test_cases));

TEST_P(InsetAreaUsedInsetsTest, All) {
  const UsedInsetsTestCase& test_case = GetParam();
  EXPECT_EQ(test_case.physical.UsedTop(), test_case.expected_top);
  EXPECT_EQ(test_case.physical.UsedBottom(), test_case.expected_bottom);
  EXPECT_EQ(test_case.physical.UsedLeft(), test_case.expected_left);
  EXPECT_EQ(test_case.physical.UsedRight(), test_case.expected_right);
}

}  // namespace blink
