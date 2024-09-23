// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/position_area.h"

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
  PositionArea logical;
  PositionArea expected_physical;
  WritingDirectionMode container_writing_direction;
  WritingDirectionMode self_writing_direction;
};

ToPhysicalTestCase to_physical_test_cases[] = {
    {
        {PositionAreaRegion::kAll, PositionAreaRegion::kAll, PositionAreaRegion::kTop,
         PositionAreaRegion::kCenter},
        {PositionAreaRegion::kTop, PositionAreaRegion::kCenter,
         PositionAreaRegion::kLeft, PositionAreaRegion::kRight},
        horizontal_tb_ltr,
        horizontal_tb_ltr,
    },
    {
        {PositionAreaRegion::kXStart, PositionAreaRegion::kXStart,
         PositionAreaRegion::kYStart, PositionAreaRegion::kYStart},
        {PositionAreaRegion::kTop, PositionAreaRegion::kTop, PositionAreaRegion::kRight,
         PositionAreaRegion::kRight},
        horizontal_tb_rtl,
        horizontal_tb_ltr,
    },
    {
        {PositionAreaRegion::kXSelfEnd, PositionAreaRegion::kXSelfEnd,
         PositionAreaRegion::kYSelfEnd, PositionAreaRegion::kYSelfEnd},
        {PositionAreaRegion::kBottom, PositionAreaRegion::kBottom,
         PositionAreaRegion::kLeft, PositionAreaRegion::kLeft},
        horizontal_tb_ltr,
        horizontal_tb_rtl,
    },
    {
        // block-axis (containing block) / inline-axis (containing block) since
        // both are neutral. First span becomes physical "center right" because
        // of vertical-rl / ltr. Second becomes "center bottom" because of
        // horizontal-tb / rtl.
        {PositionAreaRegion::kStart, PositionAreaRegion::kCenter,
         PositionAreaRegion::kCenter, PositionAreaRegion::kSelfEnd},
        {PositionAreaRegion::kCenter, PositionAreaRegion::kBottom,
         PositionAreaRegion::kCenter, PositionAreaRegion::kRight},
        vertical_rl_ltr,
        horizontal_tb_rtl,
    },
    {
        // block-axis (self) / inline-axis (self) since both are neutral. First
        // span becomes physical "right" because of vertical-lr. Second becomes
        // "bottom" because of rtl.
        {PositionAreaRegion::kSelfStart, PositionAreaRegion::kSelfStart,
         PositionAreaRegion::kSelfEnd, PositionAreaRegion::kSelfEnd},
        {PositionAreaRegion::kBottom, PositionAreaRegion::kBottom,
         PositionAreaRegion::kRight, PositionAreaRegion::kRight},
        horizontal_tb_ltr,
        vertical_lr_rtl,
    },
};

class PositionAreaToPhysicalTest
    : public testing::Test,
      public testing::WithParamInterface<ToPhysicalTestCase> {};

INSTANTIATE_TEST_SUITE_P(All,
                         PositionAreaToPhysicalTest,
                         testing::ValuesIn(to_physical_test_cases));

TEST_P(PositionAreaToPhysicalTest, All) {
  const ToPhysicalTestCase& test_case = GetParam();
  EXPECT_EQ(test_case.logical.ToPhysical(test_case.container_writing_direction,
                                         test_case.self_writing_direction),
            test_case.expected_physical);
}

enum class ExpectedInset {
  kZero,  // 0px
  kTop,
  kBottom,
  kLeft,
  kRight
};

struct UsedInsetsTestCase {
  PositionArea physical;
  ExpectedInset expected_top;
  ExpectedInset expected_bottom;
  ExpectedInset expected_left;
  ExpectedInset expected_right;
};

namespace {

std::optional<AnchorQuery> ToAnchorQuery(ExpectedInset inset) {
  switch (inset) {
    case ExpectedInset::kZero:
      return std::nullopt;  // 0px
    case ExpectedInset::kTop:
      return PositionArea::AnchorTop();
    case ExpectedInset::kBottom:
      return PositionArea::AnchorBottom();
    case ExpectedInset::kLeft:
      return PositionArea::AnchorLeft();
    case ExpectedInset::kRight:
      return PositionArea::AnchorRight();
  }
}

}  // namespace

// Note that we use ExpectedInset to express the expected results
// instead of calling PositionArea::AnchorTop() (etc) directly here,
// because PositionArea::InitializeAnchors may not have happened yet.
UsedInsetsTestCase used_insets_test_cases[] = {
    {{PositionAreaRegion::kTop, PositionAreaRegion::kTop, PositionAreaRegion::kLeft,
      PositionAreaRegion::kLeft},
     ExpectedInset::kZero,
     ExpectedInset::kTop,
     ExpectedInset::kZero,
     ExpectedInset::kLeft},
    {{PositionAreaRegion::kCenter, PositionAreaRegion::kCenter,
      PositionAreaRegion::kCenter, PositionAreaRegion::kCenter},
     ExpectedInset::kTop,
     ExpectedInset::kBottom,
     ExpectedInset::kLeft,
     ExpectedInset::kRight},
    {{PositionAreaRegion::kBottom, PositionAreaRegion::kBottom,
      PositionAreaRegion::kRight, PositionAreaRegion::kRight},
     ExpectedInset::kBottom,
     ExpectedInset::kZero,
     ExpectedInset::kRight,
     ExpectedInset::kZero},
    {{PositionAreaRegion::kTop, PositionAreaRegion::kCenter, PositionAreaRegion::kLeft,
      PositionAreaRegion::kCenter},
     ExpectedInset::kZero,
     ExpectedInset::kBottom,
     ExpectedInset::kZero,
     ExpectedInset::kRight},
    {{PositionAreaRegion::kCenter, PositionAreaRegion::kBottom,
      PositionAreaRegion::kCenter, PositionAreaRegion::kRight},
     ExpectedInset::kTop,
     ExpectedInset::kZero,
     ExpectedInset::kLeft,
     ExpectedInset::kZero},
    {{PositionAreaRegion::kTop, PositionAreaRegion::kBottom, PositionAreaRegion::kLeft,
      PositionAreaRegion::kRight},
     ExpectedInset::kZero,
     ExpectedInset::kZero,
     ExpectedInset::kZero,
     ExpectedInset::kZero},
};

class PositionAreaUsedInsetsTest
    : public testing::Test,
      public testing::WithParamInterface<UsedInsetsTestCase> {};

INSTANTIATE_TEST_SUITE_P(All,
                         PositionAreaUsedInsetsTest,
                         testing::ValuesIn(used_insets_test_cases));

TEST_P(PositionAreaUsedInsetsTest, All) {
  const UsedInsetsTestCase& test_case = GetParam();
  EXPECT_EQ(test_case.physical.UsedTop(),
            ToAnchorQuery(test_case.expected_top));
  EXPECT_EQ(test_case.physical.UsedBottom(),
            ToAnchorQuery(test_case.expected_bottom));
  EXPECT_EQ(test_case.physical.UsedLeft(),
            ToAnchorQuery(test_case.expected_left));
  EXPECT_EQ(test_case.physical.UsedRight(),
            ToAnchorQuery(test_case.expected_right));
}

}  // namespace blink
