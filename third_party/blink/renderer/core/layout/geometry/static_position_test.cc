// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/static_position.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

using InlineEdge = LogicalStaticPosition::InlineEdge;
using BlockEdge = LogicalStaticPosition::BlockEdge;
using HorizontalEdge = PhysicalStaticPosition::HorizontalEdge;
using VerticalEdge = PhysicalStaticPosition::VerticalEdge;

struct StaticPositionTestData {
  LogicalStaticPosition logical;
  PhysicalStaticPosition physical;
  WritingMode writing_mode;
  TextDirection direction;

} ng_static_position_test_data[] = {
    // |WritingMode::kHorizontalTb|, |TextDirection::kLtr|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(20, 30), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kHorizontalTb,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(20, 30), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kHorizontalTb,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(20, 30), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kHorizontalTb,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(20, 30), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kHorizontalTb,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(20, 30), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kTop},
     WritingMode::kHorizontalTb,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(20, 30), HorizontalEdge::kLeft,
      VerticalEdge::kVerticalCenter},
     WritingMode::kHorizontalTb,
     TextDirection::kLtr},
    // |WritingMode::kHorizontalTb|, |TextDirection::kRtl|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(80, 30), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kHorizontalTb,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(80, 30), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kHorizontalTb,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(80, 30), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kHorizontalTb,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(80, 30), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kHorizontalTb,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(80, 30), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kTop},
     WritingMode::kHorizontalTb,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(80, 30), HorizontalEdge::kRight,
      VerticalEdge::kVerticalCenter},
     WritingMode::kHorizontalTb,
     TextDirection::kRtl},
    // |WritingMode::kVerticalRl|, |TextDirection::kLtr|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(70, 20), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kVerticalRl,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(70, 20), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kVerticalRl,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(70, 20), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kVerticalRl,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(70, 20), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kVerticalRl,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(70, 20), HorizontalEdge::kRight,
      VerticalEdge::kVerticalCenter},
     WritingMode::kVerticalRl,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(70, 20), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kTop},
     WritingMode::kVerticalRl,
     TextDirection::kLtr},
    // |WritingMode::kVerticalRl|, |TextDirection::kRtl|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(70, 80), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kVerticalRl,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(70, 80), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kVerticalRl,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(70, 80), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kVerticalRl,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(70, 80), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kVerticalRl,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(70, 80), HorizontalEdge::kRight,
      VerticalEdge::kVerticalCenter},
     WritingMode::kVerticalRl,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(70, 80), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kBottom},
     WritingMode::kVerticalRl,
     TextDirection::kRtl},
    // |WritingMode::kVerticalLr|, |TextDirection::kLtr|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 20), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kVerticalLr,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 20), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kVerticalLr,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 20), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kVerticalLr,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 20), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kVerticalLr,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 20), HorizontalEdge::kLeft,
      VerticalEdge::kVerticalCenter},
     WritingMode::kVerticalLr,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(30, 20), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kTop},
     WritingMode::kVerticalLr,
     TextDirection::kLtr},
    // |WritingMode::kVerticalLr|, |TextDirection::kRtl|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 80), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kVerticalLr,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 80), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kVerticalLr,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 80), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kVerticalLr,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 80), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kVerticalLr,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 80), HorizontalEdge::kLeft,
      VerticalEdge::kVerticalCenter},
     WritingMode::kVerticalLr,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(30, 80), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kBottom},
     WritingMode::kVerticalLr,
     TextDirection::kRtl},
    // |WritingMode::kSidewaysLr|, |TextDirection::kLtr|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 80), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kSidewaysLr,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 80), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kSidewaysLr,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 80), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kSidewaysLr,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 80), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kSidewaysLr,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 80), HorizontalEdge::kLeft,
      VerticalEdge::kVerticalCenter},
     WritingMode::kSidewaysLr,
     TextDirection::kLtr},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(30, 80), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kBottom},
     WritingMode::kSidewaysLr,
     TextDirection::kLtr},
    // |WritingMode::kSidewaysLr|, |TextDirection::kRtl|
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 20), HorizontalEdge::kLeft, VerticalEdge::kTop},
     WritingMode::kSidewaysLr,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 20), HorizontalEdge::kLeft, VerticalEdge::kBottom},
     WritingMode::kSidewaysLr,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 20), HorizontalEdge::kRight, VerticalEdge::kTop},
     WritingMode::kSidewaysLr,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineEnd, BlockEdge::kBlockEnd},
     {PhysicalOffset(30, 20), HorizontalEdge::kRight, VerticalEdge::kBottom},
     WritingMode::kSidewaysLr,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineCenter, BlockEdge::kBlockStart},
     {PhysicalOffset(30, 20), HorizontalEdge::kLeft,
      VerticalEdge::kVerticalCenter},
     WritingMode::kSidewaysLr,
     TextDirection::kRtl},
    {{LogicalOffset(20, 30), InlineEdge::kInlineStart, BlockEdge::kBlockCenter},
     {PhysicalOffset(30, 20), HorizontalEdge::kHorizontalCenter,
      VerticalEdge::kTop},
     WritingMode::kSidewaysLr,
     TextDirection::kRtl},
};

class StaticPositionTest
    : public testing::Test,
      public testing::WithParamInterface<StaticPositionTestData> {};

TEST_P(StaticPositionTest, Convert) {
  const auto& data = GetParam();

  // These tests take the logical static-position, and convert it to a physical
  // static-position with a 100x100 rect.
  //
  // It asserts that it is the same as the expected physical static-position,
  // then performs the same operation in reverse.

  const WritingModeConverter converter({data.writing_mode, data.direction},
                                       PhysicalSize(100, 100));
  PhysicalStaticPosition physical_result =
      data.logical.ConvertToPhysical(converter);
  EXPECT_EQ(physical_result.offset, data.physical.offset);
  EXPECT_EQ(physical_result.horizontal_edge, data.physical.horizontal_edge);
  EXPECT_EQ(physical_result.vertical_edge, data.physical.vertical_edge);

  LogicalStaticPosition logical_result =
      data.physical.ConvertToLogical(converter);
  EXPECT_EQ(logical_result.offset, data.logical.offset);
  EXPECT_EQ(logical_result.inline_edge, data.logical.inline_edge);
  EXPECT_EQ(logical_result.block_edge, data.logical.block_edge);
}

INSTANTIATE_TEST_SUITE_P(StaticPositionTest,
                         StaticPositionTest,
                         testing::ValuesIn(ng_static_position_test_data));

}  // namespace
}  // namespace blink
