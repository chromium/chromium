// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

TEST(GeometryUnitsTest, ConvertPhysicalOffsetToLogicalOffset) {
  PhysicalOffset physical_offset(20, 30);
  PhysicalSize outer_size(300, 400);
  PhysicalSize inner_size(5, 65);
  LogicalOffset offset;

  offset = physical_offset.ConvertToLogical(
      WritingMode::kHorizontalTb, TextDirection::kLtr, outer_size, inner_size);
  EXPECT_EQ(20, offset.inline_offset);
  EXPECT_EQ(30, offset.block_offset);

  offset = physical_offset.ConvertToLogical(
      WritingMode::kHorizontalTb, TextDirection::kRtl, outer_size, inner_size);
  EXPECT_EQ(275, offset.inline_offset);
  EXPECT_EQ(30, offset.block_offset);

  offset = physical_offset.ConvertToLogical(
      WritingMode::kVerticalRl, TextDirection::kLtr, outer_size, inner_size);
  EXPECT_EQ(30, offset.inline_offset);
  EXPECT_EQ(275, offset.block_offset);

  offset = physical_offset.ConvertToLogical(
      WritingMode::kVerticalRl, TextDirection::kRtl, outer_size, inner_size);
  EXPECT_EQ(305, offset.inline_offset);
  EXPECT_EQ(275, offset.block_offset);

  offset = physical_offset.ConvertToLogical(
      WritingMode::kSidewaysRl, TextDirection::kLtr, outer_size, inner_size);
  EXPECT_EQ(30, offset.inline_offset);
  EXPECT_EQ(275, offset.block_offset);

  offset = physical_offset.ConvertToLogical(
      WritingMode::kSidewaysRl, TextDirection::kRtl, outer_size, inner_size);
  EXPECT_EQ(305, offset.inline_offset);
  EXPECT_EQ(275, offset.block_offset);

  offset = physical_offset.ConvertToLogical(
      WritingMode::kVerticalLr, TextDirection::kLtr, outer_size, inner_size);
  EXPECT_EQ(30, offset.inline_offset);
  EXPECT_EQ(20, offset.block_offset);

  offset = physical_offset.ConvertToLogical(
      WritingMode::kVerticalLr, TextDirection::kRtl, outer_size, inner_size);
  EXPECT_EQ(305, offset.inline_offset);
  EXPECT_EQ(20, offset.block_offset);

  offset = physical_offset.ConvertToLogical(
      WritingMode::kSidewaysLr, TextDirection::kLtr, outer_size, inner_size);
  EXPECT_EQ(305, offset.inline_offset);
  EXPECT_EQ(20, offset.block_offset);

  offset = physical_offset.ConvertToLogical(
      WritingMode::kSidewaysLr, TextDirection::kRtl, outer_size, inner_size);
  EXPECT_EQ(30, offset.inline_offset);
  EXPECT_EQ(20, offset.block_offset);
}

}  // namespace

}  // namespace blink
