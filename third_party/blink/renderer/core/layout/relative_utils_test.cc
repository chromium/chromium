// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/relative_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

const LayoutUnit kLeft{3};
const LayoutUnit kRight{5};
const LayoutUnit kTop{7};
const LayoutUnit kBottom{9};
const LayoutUnit kAuto{-1};
const LayoutUnit kZero{0};

class RelativeUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    initial_style_ = ComputedStyle::GetInitialStyleSingleton();
  }

  const ComputedStyle* CreateStyle(LayoutUnit top,
                                   LayoutUnit right,
                                   LayoutUnit bottom,
                                   LayoutUnit left) {
    ComputedStyleBuilder builder(*initial_style_);
    builder.SetPosition(EPosition::kRelative);
    builder.SetTop(top == kAuto ? Length::Auto() : Length::Fixed(top.ToInt()));
    builder.SetRight(right == kAuto ? Length::Auto()
                                    : Length::Fixed(right.ToInt()));
    builder.SetBottom(bottom == kAuto ? Length::Auto()
                                      : Length::Fixed(bottom.ToInt()));
    builder.SetLeft(left == kAuto ? Length::Auto()
                                  : Length::Fixed(left.ToInt()));
    return builder.TakeStyle();
  }

  Persistent<const ComputedStyle> initial_style_;
  test::TaskEnvironment task_environment_;
  LogicalSize container_size_;
};

TEST_F(RelativeUtilsTest, HorizontalTB) {
  LogicalOffset offset;

  // Everything auto defaults to kZero,kZero
  const ComputedStyle* style = CreateStyle(kAuto, kAuto, kAuto, kAuto);
  offset = ComputeRelativeOffset(
      *style, {WritingMode::kHorizontalTb, TextDirection::kLtr},
      container_size_);
  EXPECT_EQ(offset.inline_offset, kZero);
  EXPECT_EQ(offset.block_offset, kZero);

  // Set all sides
  style = CreateStyle(kTop, kRight, kBottom, kLeft);

  // kLtr
  offset = ComputeRelativeOffset(
      *style, {WritingMode::kHorizontalTb, TextDirection::kLtr},
      container_size_);
  EXPECT_EQ(offset.inline_offset, kLeft);
  EXPECT_EQ(offset.block_offset, kTop);

  // kRtl
  offset = ComputeRelativeOffset(
      *style, {WritingMode::kHorizontalTb, TextDirection::kRtl},
      container_size_);
  EXPECT_EQ(offset.inline_offset, kRight);
  EXPECT_EQ(offset.block_offset, kTop);

  // Set only non-default sides
  style = CreateStyle(kAuto, kRight, kBottom, kAuto);
  offset = ComputeRelativeOffset(
      *style, {WritingMode::kHorizontalTb, TextDirection::kLtr},
      container_size_);
  EXPECT_EQ(offset.inline_offset, -kRight);
  EXPECT_EQ(offset.block_offset, -kBottom);
}

TEST_F(RelativeUtilsTest, VerticalRightLeft) {
  LogicalOffset offset;

  // Set all sides
  const ComputedStyle* style = CreateStyle(kTop, kRight, kBottom, kLeft);

  // kLtr
  offset = ComputeRelativeOffset(
      *style, {WritingMode::kVerticalRl, TextDirection::kLtr}, container_size_);
  EXPECT_EQ(offset.inline_offset, kTop);
  EXPECT_EQ(offset.block_offset, kRight);

  // kRtl
  offset = ComputeRelativeOffset(
      *style, {WritingMode::kVerticalRl, TextDirection::kRtl}, container_size_);
  EXPECT_EQ(offset.inline_offset, kBottom);
  EXPECT_EQ(offset.block_offset, kRight);

  // Set only non-default sides
  style = CreateStyle(kAuto, kAuto, kBottom, kLeft);
  offset = ComputeRelativeOffset(
      *style, {WritingMode::kVerticalRl, TextDirection::kLtr}, container_size_);
  EXPECT_EQ(offset.inline_offset, -kBottom);
  EXPECT_EQ(offset.block_offset, -kLeft);
}

TEST_F(RelativeUtilsTest, VerticalLeftRight) {
  LogicalOffset offset;

  // Set all sides
  const ComputedStyle* style = CreateStyle(kTop, kRight, kBottom, kLeft);

  // kLtr
  offset = ComputeRelativeOffset(
      *style, {WritingMode::kVerticalLr, TextDirection::kLtr}, container_size_);
  EXPECT_EQ(offset.inline_offset, kTop);
  EXPECT_EQ(offset.block_offset, kLeft);

  // kRtl
  offset = ComputeRelativeOffset(
      *style, {WritingMode::kVerticalLr, TextDirection::kRtl}, container_size_);
  EXPECT_EQ(offset.inline_offset, kBottom);
  EXPECT_EQ(offset.block_offset, kLeft);

  // Set only non-default sides
  style = CreateStyle(kAuto, kRight, kBottom, kAuto);
  offset = ComputeRelativeOffset(
      *style, {WritingMode::kVerticalLr, TextDirection::kLtr}, container_size_);
  EXPECT_EQ(offset.inline_offset, -kBottom);
  EXPECT_EQ(offset.block_offset, -kRight);
}

}  // namespace
}  // namespace blink
