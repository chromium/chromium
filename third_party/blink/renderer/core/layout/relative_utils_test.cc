// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/relative_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/physical_offset.h"
#include "third_party/blink/renderer/platform/geometry/physical_size.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

const LayoutUnit kLeft{3};
const LayoutUnit kRight{5};
const LayoutUnit kTop{7};
const LayoutUnit kBottom{9};
const LayoutUnit kAuto{-1};

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
  PhysicalBoxStrut insets;

  // Everything auto defaults to 0
  const ComputedStyle* style = CreateStyle(kAuto, kAuto, kAuto, kAuto);
  insets = ComputeRelativeInsets(
      *style, {WritingMode::kHorizontalTb, TextDirection::kLtr},
      container_size_);
  EXPECT_EQ(insets, PhysicalBoxStrut());

  // Set all sides
  style = CreateStyle(kTop, kRight, kBottom, kLeft);
  insets = ComputeRelativeInsets(
      *style, {WritingMode::kHorizontalTb, TextDirection::kLtr},
      container_size_);
  EXPECT_EQ(insets, PhysicalBoxStrut(kTop, kRight, kBottom, kLeft));

  // Set only opposing sides
  style = CreateStyle(kAuto, kRight, kBottom, kAuto);
  insets = ComputeRelativeInsets(
      *style, {WritingMode::kHorizontalTb, TextDirection::kLtr},
      container_size_);
  EXPECT_EQ(insets, PhysicalBoxStrut(-kBottom, kRight, kBottom, -kRight));

  style = CreateStyle(kTop, kAuto, kAuto, kLeft);
  insets = ComputeRelativeInsets(
      *style, {WritingMode::kHorizontalTb, TextDirection::kLtr},
      container_size_);
  EXPECT_EQ(insets, PhysicalBoxStrut(kTop, -kLeft, -kTop, kLeft));
}

}  // namespace
}  // namespace blink
