// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace {

#define NGAuto LayoutUnit(-1)

class NGAbsoluteUtilsTest : public testing::Test {
 protected:
  NGConstraintSpace CreateConstraintSpace(TextDirection direction,
                                          WritingMode out_writing_mode) {
    NGConstraintSpaceBuilder builder(
        WritingMode::kHorizontalTb, out_writing_mode,
        /* is_new_fc */ true);
    builder.SetAvailableSize(container_size_);
    builder.SetTextDirection(direction);
    return builder.ToConstraintSpace();
  }

  void SetUp() override {
    style_ = ComputedStyle::Create();
    // If not set, border widths will always be 0.
    style_->SetBorderLeftStyle(EBorderStyle::kSolid);
    style_->SetBorderRightStyle(EBorderStyle::kSolid);
    style_->SetBorderTopStyle(EBorderStyle::kSolid);
    style_->SetBorderBottomStyle(EBorderStyle::kSolid);
    style_->SetBoxSizing(EBoxSizing::kBorderBox);
    container_size_ = LogicalSize(LayoutUnit(200), LayoutUnit(300));

    ltr_space_ =
        CreateConstraintSpace(TextDirection::kLtr, WritingMode::kHorizontalTb);
    rtl_space_ =
        CreateConstraintSpace(TextDirection::kRtl, WritingMode::kHorizontalTb);
    vlr_space_ =
        CreateConstraintSpace(TextDirection::kLtr, WritingMode::kVerticalLr);
    vrl_space_ =
        CreateConstraintSpace(TextDirection::kLtr, WritingMode::kVerticalRl);
  }

  void SetHorizontalStyle(
      LayoutUnit left,
      LayoutUnit margin_left,
      LayoutUnit width,
      LayoutUnit margin_right,
      LayoutUnit right,
      WritingMode writing_mode = WritingMode::kHorizontalTb) {
    style_->SetLeft(left == NGAuto ? Length::Auto()
                                   : Length::Fixed(left.ToInt()));
    style_->SetMarginLeft(margin_left == NGAuto
                              ? Length::Auto()
                              : Length::Fixed(margin_left.ToInt()));
    style_->SetWidth(width == NGAuto ? Length::Auto()
                                     : Length::Fixed(width.ToInt()));
    style_->SetMarginRight(margin_right == NGAuto
                               ? Length::Auto()
                               : Length::Fixed(margin_right.ToInt()));
    style_->SetRight(right == NGAuto ? Length::Auto()
                                     : Length::Fixed(right.ToInt()));
    style_->SetWritingMode(writing_mode);
  }

  void SetVerticalStyle(LayoutUnit top,
                        LayoutUnit margin_top,
                        LayoutUnit height,
                        LayoutUnit margin_bottom,
                        LayoutUnit bottom,
                        WritingMode writing_mode = WritingMode::kHorizontalTb) {
    style_->SetTop(top == NGAuto ? Length::Auto() : Length::Fixed(top.ToInt()));
    style_->SetMarginTop(margin_top == NGAuto
                             ? Length::Auto()
                             : Length::Fixed(margin_top.ToInt()));
    style_->SetHeight(height == NGAuto ? Length::Auto()
                                       : Length::Fixed(height.ToInt()));
    style_->SetMarginBottom(margin_bottom == NGAuto
                                ? Length::Auto()
                                : Length::Fixed(margin_bottom.ToInt()));
    style_->SetBottom(bottom == NGAuto ? Length::Auto()
                                       : Length::Fixed(bottom.ToInt()));
    style_->SetWritingMode(writing_mode);
  }

  scoped_refptr<ComputedStyle> style_;
  LogicalSize container_size_;
  NGConstraintSpace ltr_space_;
  NGConstraintSpace rtl_space_;
  NGConstraintSpace vlr_space_;
  NGConstraintSpace vrl_space_;
};

TEST_F(NGAbsoluteUtilsTest, Horizontal) {
  // Test that the equation is computed correctly:
  // left + marginLeft + borderLeft  + paddingLeft +
  // width +
  // right + marginRight + borderRight + paddingRight = container width

  // Common setup.
  LayoutUnit left(5);
  LayoutUnit margin_left(7);
  LayoutUnit border_left(9);
  LayoutUnit padding_left(11);
  LayoutUnit right(13);
  LayoutUnit margin_right(15);
  LayoutUnit border_right(17);
  LayoutUnit padding_right(19);

  LayoutUnit horizontal_border_padding =
      border_left + padding_left + padding_right + border_right;
  LayoutUnit width =
      container_size_.inline_size - left - margin_left - right - margin_right;

  base::Optional<MinMaxSize> estimated_inline;
  base::Optional<LayoutUnit> estimated_block;
  MinMaxSize minmax_60{LayoutUnit(60) + horizontal_border_padding,
                       LayoutUnit(60) + horizontal_border_padding};

  style_->SetBorderLeftWidth(border_left.ToInt());
  style_->SetBorderRightWidth(border_right.ToInt());
  style_->SetPaddingLeft(Length::Fixed(padding_left.ToInt()));
  style_->SetPaddingRight(Length::Fixed(padding_right.ToInt()));

  // These default to 3 which is not what we want.
  style_->SetBorderBottomWidth(0);
  style_->SetBorderTopWidth(0);

  NGBoxStrut ltr_border_padding =
      ComputeBordersForTest(*style_) + ComputePadding(ltr_space_, *style_);
  NGBoxStrut rtl_border_padding =
      ComputeBordersForTest(*style_) + ComputePadding(rtl_space_, *style_);
  NGBoxStrut vlr_border_padding =
      ComputeBordersForTest(*style_) + ComputePadding(vlr_space_, *style_);
  NGBoxStrut vrl_border_padding =
      ComputeBordersForTest(*style_) + ComputePadding(vrl_space_, *style_);

  NGLogicalStaticPosition static_position = {
      {LayoutUnit(), LayoutUnit()},
      NGLogicalStaticPosition::kInlineStart,
      NGLogicalStaticPosition::kBlockStart};
  // Same as regular static position, but with the inline-end edge.
  NGLogicalStaticPosition static_position_inline_end = {
      {LayoutUnit(), LayoutUnit()},
      NGLogicalStaticPosition::kInlineEnd,
      NGLogicalStaticPosition::kBlockStart};
  //
  // Tests.
  //

  NGLogicalOutOfFlowPosition p;

  // All auto => width is estimated_inline, left is 0.
  SetHorizontalStyle(NGAuto, NGAuto, NGAuto, NGAuto, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), true);
  estimated_inline = minmax_60;
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, ltr_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(minmax_60.min_size, p.size.inline_size);
  EXPECT_EQ(LayoutUnit(0), p.inset.inline_start);

  // All auto => width is estimated_inline, static_position is right
  SetHorizontalStyle(NGAuto, NGAuto, NGAuto, NGAuto, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), true);
  estimated_inline = minmax_60;
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, ltr_border_padding, static_position_inline_end,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(minmax_60.min_size, p.size.inline_size);
  EXPECT_EQ(container_size_.inline_size, p.inset.inline_end);

  // All auto + RTL.
  p = ComputePartialAbsoluteWithChildInlineSize(
      rtl_space_, *style_, rtl_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(minmax_60.min_size, p.size.inline_size);
  EXPECT_EQ(container_size_.inline_size - minmax_60.min_size,
            p.inset.inline_end);

  // left, right, and left are known, compute margins.
  SetHorizontalStyle(left, NGAuto, width, NGAuto, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, ltr_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  LayoutUnit margin_space =
      (container_size_.inline_size - left - right - p.size.inline_size) / 2;
  EXPECT_EQ(left + margin_space, p.inset.inline_start);
  EXPECT_EQ(right + margin_space, p.inset.inline_end);

  // left, right, and left are known, compute margins, writing mode vertical_lr.
  SetHorizontalStyle(left, NGAuto, width, NGAuto, right,
                     WritingMode::kVerticalLr);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  estimated_inline.reset();
  ComputeFullAbsoluteWithChildBlockSize(
      vlr_space_, *style_, vlr_border_padding, static_position, estimated_block,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(left + margin_space, p.inset.block_start);
  EXPECT_EQ(right + margin_space, p.inset.block_end);

  // left, right, and left are known, compute margins, writing mode vertical_rl.
  SetHorizontalStyle(left, NGAuto, width, NGAuto, right,
                     WritingMode::kVerticalRl);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  estimated_inline.reset();
  ComputeFullAbsoluteWithChildBlockSize(
      vrl_space_, *style_, vrl_border_padding, static_position, estimated_block,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(left + margin_space, p.inset.block_end);
  EXPECT_EQ(right + margin_space, p.inset.block_start);

  // left, right, and width are known, not enough space for margins LTR.
  SetHorizontalStyle(left, NGAuto, LayoutUnit(200), NGAuto, right);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, ltr_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(left, p.inset.inline_start);
  EXPECT_EQ(-left, p.inset.inline_end);

  // left, right, and left are known, not enough space for margins RTL.
  SetHorizontalStyle(left, NGAuto, LayoutUnit(200), NGAuto, right,
                     WritingMode::kHorizontalTb);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      rtl_space_, *style_, rtl_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kRtl);
  EXPECT_EQ(-right, p.inset.inline_start);
  EXPECT_EQ(right, p.inset.inline_end);

  // Rule 1 left and width are auto.
  SetHorizontalStyle(NGAuto, margin_left, NGAuto, margin_right, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), true);
  estimated_inline = minmax_60;
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, ltr_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(minmax_60.min_size, p.size.inline_size);

  // Rule 2 left and right are auto LTR.
  SetHorizontalStyle(NGAuto, margin_left, width, margin_right, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, ltr_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(margin_left, p.inset.inline_start);
  EXPECT_EQ(container_size_.inline_size - margin_left - width,
            p.inset.inline_end);

  // Rule 2 left and right are auto RTL.
  SetHorizontalStyle(NGAuto, margin_left, width, margin_right, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      rtl_space_, *style_, rtl_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(margin_left, p.inset.inline_start);
  EXPECT_EQ(container_size_.inline_size - margin_left - width,
            p.inset.inline_end);

  // Rule 3 width and right are auto.
  SetHorizontalStyle(left, margin_left, NGAuto, margin_right, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), true);
  estimated_inline = minmax_60;
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, ltr_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(
      container_size_.inline_size - minmax_60.min_size - left - margin_left,
      p.inset.inline_end);
  EXPECT_EQ(minmax_60.min_size, p.size.inline_size);

  // Rule 4: left is auto.
  SetHorizontalStyle(NGAuto, margin_left, width, margin_right, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, ltr_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(left + margin_left, p.inset.inline_start);

  // Rule 4: left is auto, EBoxSizing::kContentBox
  style_->SetBoxSizing(EBoxSizing::kContentBox);
  SetHorizontalStyle(NGAuto, margin_left, width - border_left - border_right -
                                              padding_left - padding_right,
                     margin_right, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, ltr_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(left + margin_left, p.inset.inline_start);
  style_->SetBoxSizing(EBoxSizing::kBorderBox);

  // Rule 5: right is auto.
  SetHorizontalStyle(left, margin_left, width, margin_right, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, ltr_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(right + margin_right, p.inset.inline_end);

  // Rule 6: width is auto.
  SetHorizontalStyle(left, margin_left, NGAuto, margin_right, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, ltr_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(width, p.size.inline_size);
}

TEST_F(NGAbsoluteUtilsTest, Vertical) {
  // Test that the equation is computed correctly:
  // top + marginTop + borderTop  + paddingTop +
  // height +
  // bottom + marginBottom + borderBottom + paddingBottom = container height

  // Common setup
  LayoutUnit top(5);
  LayoutUnit margin_top(7);
  LayoutUnit border_top(9);
  LayoutUnit padding_top(11);
  LayoutUnit bottom(13);
  LayoutUnit margin_bottom(15);
  LayoutUnit border_bottom(17);
  LayoutUnit padding_bottom(19);

  LayoutUnit horizontal_border_padding =
      border_top + padding_top + padding_bottom + border_bottom;
  LayoutUnit height =
      container_size_.block_size - top - margin_top - bottom - margin_bottom;

  style_->SetBorderTopWidth(border_top.ToInt());
  style_->SetBorderBottomWidth(border_bottom.ToInt());
  style_->SetPaddingTop(Length::Fixed(padding_top.ToInt()));
  style_->SetPaddingBottom(Length::Fixed(padding_bottom.ToInt()));
  // These default to 3 which is not what we want.
  style_->SetBorderLeftWidth(0);
  style_->SetBorderRightWidth(0);

  base::Optional<LayoutUnit> auto_height;
  MinMaxSize minmax_60{LayoutUnit(60), LayoutUnit(60)};

  NGBoxStrut ltr_border_padding =
      ComputeBordersForTest(*style_) + ComputePadding(ltr_space_, *style_);
  NGBoxStrut vlr_border_padding =
      ComputeBordersForTest(*style_) + ComputePadding(vlr_space_, *style_);
  NGBoxStrut vrl_border_padding =
      ComputeBordersForTest(*style_) + ComputePadding(vrl_space_, *style_);

  NGLogicalStaticPosition static_position = {
      {LayoutUnit(), LayoutUnit()},
      NGLogicalStaticPosition::kInlineStart,
      NGLogicalStaticPosition::kBlockStart};
  NGLogicalStaticPosition static_position_block_end = {
      {LayoutUnit(), LayoutUnit()},
      NGLogicalStaticPosition::kInlineStart,
      NGLogicalStaticPosition::kBlockEnd};

  //
  // Tests
  //

  NGLogicalOutOfFlowPosition p;

  // All auto, compute margins.
  SetVerticalStyle(NGAuto, NGAuto, NGAuto, NGAuto, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), true);
  auto_height = LayoutUnit(60);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, ltr_border_padding, static_position, auto_height,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(*auto_height, p.size.block_size);
  EXPECT_EQ(LayoutUnit(0), p.inset.block_start);

  // All auto, static position bottom
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, ltr_border_padding, static_position_block_end,
      auto_height, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr, &p);
  EXPECT_EQ(container_size_.block_size, p.inset.block_end);

  // If top, bottom, and height are known, compute margins.
  SetVerticalStyle(top, NGAuto, height, NGAuto, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, ltr_border_padding, static_position, auto_height,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  LayoutUnit margin_space =
      (container_size_.block_size - top - height - bottom) / 2;
  EXPECT_EQ(top + margin_space, p.inset.block_start);
  EXPECT_EQ(bottom + margin_space, p.inset.block_end);

  // If top, bottom, and height are known, writing mode vertical_lr.
  SetVerticalStyle(top, NGAuto, height, NGAuto, bottom,
                   WritingMode::kVerticalLr);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  p = ComputePartialAbsoluteWithChildInlineSize(
      vlr_space_, *style_, vlr_border_padding, static_position, minmax_60,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(top + margin_space, p.inset.inline_start);
  EXPECT_EQ(bottom + margin_space, p.inset.inline_end);

  // If top, bottom, and height are known, writing mode vertical_rl.
  SetVerticalStyle(top, NGAuto, height, NGAuto, bottom,
                   WritingMode::kVerticalRl);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  p = ComputePartialAbsoluteWithChildInlineSize(
      vrl_space_, *style_, vrl_border_padding, static_position, minmax_60,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(top + margin_space, p.inset.inline_start);
  EXPECT_EQ(bottom + margin_space, p.inset.inline_end);

  // If top, bottom, and height are known, negative auto margins.
  LayoutUnit negative_margin_space =
      (container_size_.block_size - top - LayoutUnit(300) - bottom) / 2;
  SetVerticalStyle(top, NGAuto, LayoutUnit(300), NGAuto, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, ltr_border_padding, static_position, auto_height,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(top + negative_margin_space, p.inset.block_start);
  EXPECT_EQ(bottom + negative_margin_space, p.inset.block_end);

  // Rule 1: top and height are unknown.
  SetVerticalStyle(NGAuto, margin_top, NGAuto, margin_bottom, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), true);
  auto_height = LayoutUnit(60);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, ltr_border_padding, static_position, auto_height,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(*auto_height, p.size.block_size);

  // Rule 2: top and bottom are unknown.
  SetVerticalStyle(NGAuto, margin_top, height, margin_bottom, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, ltr_border_padding, static_position, auto_height,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(margin_top, p.inset.block_start);
  EXPECT_EQ(container_size_.block_size - margin_top - height,
            p.inset.block_end);

  // Rule 3: height and bottom are unknown, auto_height <
  // horizontal_border_padding.
  SetVerticalStyle(top, margin_top, NGAuto, margin_bottom, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), true);
  auto_height = LayoutUnit(20);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, ltr_border_padding, static_position, auto_height,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(horizontal_border_padding, p.size.block_size);

  // Rule 3: height and bottom are unknown.
  SetVerticalStyle(top, margin_top, NGAuto, margin_bottom, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), true);
  auto_height = LayoutUnit(70);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, ltr_border_padding, static_position, auto_height,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(*auto_height, p.size.block_size);

  // Rule 4: top is unknown.
  SetVerticalStyle(NGAuto, margin_top, height, margin_bottom, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, ltr_border_padding, static_position, auto_height,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(top + margin_top, p.inset.block_start);

  // Rule 5: bottom is unknown.
  SetVerticalStyle(top, margin_top, height, margin_bottom, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, ltr_border_padding, static_position, auto_height,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(bottom + margin_bottom, p.inset.block_end);

  // Rule 6: height is unknown.
  SetVerticalStyle(top, margin_top, NGAuto, margin_bottom, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, ltr_border_padding, static_position, auto_height,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(height, p.size.block_size);
}

TEST_F(NGAbsoluteUtilsTest, MinMax) {
  LayoutUnit min{50};
  LayoutUnit max{150};

  style_->SetMinWidth(Length::Fixed(min.ToInt()));
  style_->SetMaxWidth(Length::Fixed(max.ToInt()));
  style_->SetMinHeight(Length::Fixed(min.ToInt()));
  style_->SetMaxHeight(Length::Fixed(max.ToInt()));

  NGBoxStrut ltr_border_padding =
      ComputeBordersForTest(*style_) + ComputePadding(ltr_space_, *style_);

  NGLogicalStaticPosition static_position = {
      {LayoutUnit(), LayoutUnit()},
      NGLogicalStaticPosition::kInlineStart,
      NGLogicalStaticPosition::kBlockStart};
  MinMaxSize estimated_inline{LayoutUnit(20), LayoutUnit(20)};
  NGLogicalOutOfFlowPosition p;

  // WIDTH TESTS

  // width < min gets set to min.
  SetHorizontalStyle(NGAuto, NGAuto, LayoutUnit(5), NGAuto, NGAuto);
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, ltr_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(min, p.size.inline_size);

  // width > max gets set to max.
  SetHorizontalStyle(NGAuto, NGAuto, LayoutUnit(200), NGAuto, NGAuto);
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, ltr_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(max, p.size.inline_size);

  // Unspecified width becomes minmax, gets clamped to min.
  SetHorizontalStyle(NGAuto, NGAuto, NGAuto, NGAuto, NGAuto);
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, ltr_border_padding, static_position,
      estimated_inline, base::nullopt, WritingMode::kHorizontalTb,
      TextDirection::kLtr);
  EXPECT_EQ(min, p.size.inline_size);

  // HEIGHT TESTS

  base::Optional<LayoutUnit> auto_height;

  // height < min gets set to min.
  SetVerticalStyle(NGAuto, NGAuto, LayoutUnit(5), NGAuto, NGAuto);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, ltr_border_padding, static_position, auto_height,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(min, p.size.block_size);

  // height > max gets set to max.
  SetVerticalStyle(NGAuto, NGAuto, LayoutUnit(200), NGAuto, NGAuto);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, ltr_border_padding, static_position, auto_height,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(max, p.size.block_size);

  // // Unspecified height becomes estimated, gets clamped to min.
  SetVerticalStyle(NGAuto, NGAuto, NGAuto, NGAuto, NGAuto);
  auto_height = LayoutUnit(20);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, ltr_border_padding, static_position, auto_height,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(min, p.size.block_size);
}

}  // namespace
}  // namespace blink
