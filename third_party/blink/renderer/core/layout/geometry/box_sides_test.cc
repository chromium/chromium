// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/box_sides.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(GeometryUnitsTest, LogicalBoxSidesToLineLogical) {
  LogicalBoxSides sides;
  sides.block_start = false;
  sides.inline_start = false;

  {
    LineLogicalBoxSides result(sides, TextDirection::kRtl);
    EXPECT_FALSE(result.block_start);
    EXPECT_TRUE(result.block_end);
    EXPECT_TRUE(result.line_left);
    EXPECT_FALSE(result.line_right);
  }

  {
    LineLogicalBoxSides result(sides, TextDirection::kLtr);
    EXPECT_FALSE(result.block_start);
    EXPECT_TRUE(result.block_end);
    EXPECT_FALSE(result.line_left);
    EXPECT_TRUE(result.line_right);
  }
}

TEST(GeometryUnitsTest, PhysicalBoxSidesToLogical) {
  PhysicalBoxSides sides;
  sides.bottom = false;
  sides.left = false;

  {
    LogicalBoxSides result = sides.ToLogical(
        WritingDirectionMode(WritingMode::kVerticalRl, TextDirection::kRtl));
    EXPECT_TRUE(result.block_start);
    EXPECT_FALSE(result.block_end);
    EXPECT_FALSE(result.inline_start);
    EXPECT_TRUE(result.inline_end);
  }

  {
    LogicalBoxSides result = sides.ToLogical(
        WritingDirectionMode(WritingMode::kVerticalRl, TextDirection::kLtr));
    EXPECT_TRUE(result.block_start);
    EXPECT_FALSE(result.block_end);
    EXPECT_TRUE(result.inline_start);
    EXPECT_FALSE(result.inline_end);
  }

  {
    LogicalBoxSides result = sides.ToLogical(
        WritingDirectionMode(WritingMode::kVerticalLr, TextDirection::kRtl));
    EXPECT_FALSE(result.block_start);
    EXPECT_TRUE(result.block_end);
    EXPECT_FALSE(result.inline_start);
    EXPECT_TRUE(result.inline_end);
  }

  {
    LogicalBoxSides result = sides.ToLogical(
        WritingDirectionMode(WritingMode::kVerticalLr, TextDirection::kLtr));
    EXPECT_FALSE(result.block_start);
    EXPECT_TRUE(result.block_end);
    EXPECT_TRUE(result.inline_start);
    EXPECT_FALSE(result.inline_end);
  }

  {
    LogicalBoxSides result = sides.ToLogical(
        WritingDirectionMode(WritingMode::kSidewaysRl, TextDirection::kRtl));
    EXPECT_TRUE(result.block_start);
    EXPECT_FALSE(result.block_end);
    EXPECT_FALSE(result.inline_start);
    EXPECT_TRUE(result.inline_end);
  }

  {
    LogicalBoxSides result = sides.ToLogical(
        WritingDirectionMode(WritingMode::kSidewaysRl, TextDirection::kLtr));
    EXPECT_TRUE(result.block_start);
    EXPECT_FALSE(result.block_end);
    EXPECT_TRUE(result.inline_start);
    EXPECT_FALSE(result.inline_end);
  }

  {
    LogicalBoxSides result = sides.ToLogical(
        WritingDirectionMode(WritingMode::kSidewaysLr, TextDirection::kRtl));
    EXPECT_FALSE(result.block_start);
    EXPECT_TRUE(result.block_end);
    EXPECT_TRUE(result.inline_start);
    EXPECT_FALSE(result.inline_end);
  }

  {
    LogicalBoxSides result = sides.ToLogical(
        WritingDirectionMode(WritingMode::kSidewaysLr, TextDirection::kLtr));
    EXPECT_FALSE(result.block_start);
    EXPECT_TRUE(result.block_end);
    EXPECT_FALSE(result.inline_start);
    EXPECT_TRUE(result.inline_end);
  }

  {
    LogicalBoxSides result = sides.ToLogical(
        WritingDirectionMode(WritingMode::kHorizontalTb, TextDirection::kRtl));
    EXPECT_TRUE(result.block_start);
    EXPECT_FALSE(result.block_end);
    EXPECT_TRUE(result.inline_start);
    EXPECT_FALSE(result.inline_end);
  }

  {
    LogicalBoxSides result = sides.ToLogical(
        WritingDirectionMode(WritingMode::kHorizontalTb, TextDirection::kLtr));
    EXPECT_TRUE(result.block_start);
    EXPECT_FALSE(result.block_end);
    EXPECT_FALSE(result.inline_start);
    EXPECT_TRUE(result.inline_end);
  }
}

}  // namespace

}  // namespace blink
