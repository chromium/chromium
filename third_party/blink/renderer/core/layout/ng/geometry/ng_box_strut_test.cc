// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

// Ideally, this would be tested by NGBoxStrut::ConvertToPhysical, but
// this has not been implemented yet.
TEST(NGGeometryUnitsTest, ConvertPhysicalStrutToLogical) {
  LayoutUnit left{5}, right{10}, top{15}, bottom{20};
  NGPhysicalBoxStrut physical{top, right, bottom, left};

  NGBoxStrut logical = physical.ConvertToLogical(
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

TEST(NGGeometryUnitsTest, ConvertLogicalStrutToPhysical) {
  LayoutUnit left{5}, right{10}, top{15}, bottom{20};
  NGBoxStrut logical(left, right, top, bottom);
  NGBoxStrut converted =
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

}  // namespace

}  // namespace blink
