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
  void SetUp() override {
    style_ = ComputedStyle::Create();
    // If not set, border widths will always be 0.
    style_->SetBorderLeftStyle(EBorderStyle::kSolid);
    style_->SetBorderRightStyle(EBorderStyle::kSolid);
    style_->SetBorderTopStyle(EBorderStyle::kSolid);
    style_->SetBorderBottomStyle(EBorderStyle::kSolid);
    style_->SetBoxSizing(EBoxSizing::kBorderBox);
    container_size_ = NGLogicalSize(LayoutUnit(200), LayoutUnit(300));
    NGConstraintSpaceBuilder builder(
        WritingMode::kHorizontalTb,
        /* icb_size */ container_size_.ConvertToPhysical(
            WritingMode::kHorizontalTb));
    builder.SetAvailableSize(container_size_);
    ltr_space_ = builder.SetTextDirection(TextDirection::kLtr)
                     .ToConstraintSpace(WritingMode::kHorizontalTb);
    rtl_space_ = builder.SetTextDirection(TextDirection::kRtl)
                     .ToConstraintSpace(WritingMode::kHorizontalTb);
    vertical_lr_space_ = builder.SetTextDirection(TextDirection::kLtr)
                             .ToConstraintSpace(WritingMode::kVerticalLr);
    vertical_rl_space_ = builder.SetTextDirection(TextDirection::kLtr)
                             .ToConstraintSpace(WritingMode::kVerticalRl);
  }

  void SetHorizontalStyle(
      LayoutUnit left,
      LayoutUnit margin_left,
      LayoutUnit width,
      LayoutUnit margin_right,
      LayoutUnit right,
      WritingMode writing_mode = WritingMode::kHorizontalTb) {
    style_->SetLeft(left == NGAuto ? Length(LengthType::kAuto)
                                   : Length(left.ToInt(), LengthType::kFixed));
    style_->SetMarginLeft(margin_left == NGAuto ? Length(LengthType::kAuto)
                                                : Length(margin_left.ToInt(),
                                                         LengthType::kFixed));
    style_->SetWidth(width == NGAuto
                         ? Length(LengthType::kAuto)
                         : Length(width.ToInt(), LengthType::kFixed));
    style_->SetMarginRight(margin_right == NGAuto ? Length(LengthType::kAuto)
                                                  : Length(margin_right.ToInt(),
                                                           LengthType::kFixed));
    style_->SetRight(right == NGAuto
                         ? Length(LengthType::kAuto)
                         : Length(right.ToInt(), LengthType::kFixed));
    style_->SetWritingMode(writing_mode);
  }

  void SetVerticalStyle(LayoutUnit top,
                        LayoutUnit margin_top,
                        LayoutUnit height,
                        LayoutUnit margin_bottom,
                        LayoutUnit bottom,
                        WritingMode writing_mode = WritingMode::kHorizontalTb) {
    style_->SetTop(top == NGAuto ? Length(LengthType::kAuto)
                                 : Length(top.ToInt(), LengthType::kFixed));
    style_->SetMarginTop(margin_top == NGAuto
                             ? Length(LengthType::kAuto)
                             : Length(margin_top.ToInt(), LengthType::kFixed));
    style_->SetHeight(height == NGAuto
                          ? Length(LengthType::kAuto)
                          : Length(height.ToInt(), LengthType::kFixed));
    style_->SetMarginBottom(
        margin_bottom == NGAuto
            ? Length(LengthType::kAuto)
            : Length(margin_bottom.ToInt(), LengthType::kFixed));
    style_->SetBottom(bottom == NGAuto
                          ? Length(LengthType::kAuto)
                          : Length(bottom.ToInt(), LengthType::kFixed));
    style_->SetWritingMode(writing_mode);
  }

  scoped_refptr<ComputedStyle> style_;
  NGLogicalSize container_size_;
  NGConstraintSpace ltr_space_;
  NGConstraintSpace rtl_space_;
  NGConstraintSpace vertical_lr_space_;
  NGConstraintSpace vertical_rl_space_;
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

  LayoutUnit border_padding =
      border_left + padding_left + padding_right + border_right;
  LayoutUnit width =
      container_size_.inline_size - left - margin_left - right - margin_right;

  base::Optional<MinMaxSize> estimated_inline;
  base::Optional<LayoutUnit> estimated_block;
  MinMaxSize minmax_60{LayoutUnit(60) + border_padding,
                       LayoutUnit(60) + border_padding};

  style_->SetBorderLeftWidth(border_left.ToInt());
  style_->SetBorderRightWidth(border_right.ToInt());
  style_->SetPaddingLeft(Length(padding_left.ToInt(), LengthType::kFixed));
  style_->SetPaddingRight(Length(padding_right.ToInt(), LengthType::kFixed));

  // These default to 3 which is not what we want.
  style_->SetBorderBottomWidth(0);
  style_->SetBorderTopWidth(0);

  NGAbsolutePhysicalPosition p;

  NGStaticPosition static_position{NGStaticPosition::kTopLeft,
                                   {LayoutUnit(), LayoutUnit()}};
  // Same as regular static position, but with right edge.
  NGStaticPosition static_right_position{NGStaticPosition::kTopRight,
                                         {LayoutUnit(), LayoutUnit()}};
  //
  // Tests.
  //

  // All auto => width is estimated_inline, left is 0.
  SetHorizontalStyle(NGAuto, NGAuto, NGAuto, NGAuto, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), true);
  estimated_inline = minmax_60;
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(minmax_60.min_size, p.size.width);
  EXPECT_EQ(LayoutUnit(0), p.inset.left);

  // All auto => width is estimated_inline, static_position is right
  SetHorizontalStyle(NGAuto, NGAuto, NGAuto, NGAuto, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), true);
  estimated_inline = minmax_60;
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, static_right_position, estimated_inline,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(minmax_60.min_size, p.size.width);
  EXPECT_EQ(container_size_.inline_size, p.inset.right);

  // All auto + RTL.
  p = ComputePartialAbsoluteWithChildInlineSize(
      rtl_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(minmax_60.min_size, p.size.width);
  EXPECT_EQ(container_size_.inline_size - minmax_60.min_size, p.inset.right);

  // left, right, and left are known, compute margins.
  SetHorizontalStyle(left, NGAuto, width, NGAuto, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  LayoutUnit margin_space =
      (container_size_.inline_size - left - right - p.size.width) / 2;
  EXPECT_EQ(left + margin_space, p.inset.left);
  EXPECT_EQ(right + margin_space, p.inset.right);

  // left, right, and left are known, compute margins, writing mode vertical_lr.
  SetHorizontalStyle(left, NGAuto, width, NGAuto, right,
                     WritingMode::kVerticalLr);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  estimated_inline.reset();
  ComputeFullAbsoluteWithChildBlockSize(
      vertical_lr_space_, *style_, static_position, estimated_block,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(left + margin_space, p.inset.left);
  EXPECT_EQ(right + margin_space, p.inset.right);

  // left, right, and left are known, compute margins, writing mode vertical_rl.
  SetHorizontalStyle(left, NGAuto, width, NGAuto, right,
                     WritingMode::kVerticalRl);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  estimated_inline.reset();
  ComputeFullAbsoluteWithChildBlockSize(
      vertical_rl_space_, *style_, static_position, estimated_block,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(left + margin_space, p.inset.left);
  EXPECT_EQ(right + margin_space, p.inset.right);

  // left, right, and width are known, not enough space for margins LTR.
  SetHorizontalStyle(left, NGAuto, LayoutUnit(200), NGAuto, right);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(left, p.inset.left);
  EXPECT_EQ(-left, p.inset.right);

  // left, right, and left are known, not enough space for margins RTL.
  SetHorizontalStyle(left, NGAuto, LayoutUnit(200), NGAuto, right,
                     WritingMode::kHorizontalTb);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      rtl_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kRtl);
  EXPECT_EQ(-right, p.inset.left);
  EXPECT_EQ(right, p.inset.right);

  // Rule 1 left and width are auto.
  SetHorizontalStyle(NGAuto, margin_left, NGAuto, margin_right, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), true);
  estimated_inline = minmax_60;
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(minmax_60.min_size, p.size.width);

  // Rule 2 left and right are auto LTR.
  SetHorizontalStyle(NGAuto, margin_left, width, margin_right, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(margin_left, p.inset.left);
  EXPECT_EQ(container_size_.inline_size - margin_left - width, p.inset.right);

  // Rule 2 left and right are auto RTL.
  SetHorizontalStyle(NGAuto, margin_left, width, margin_right, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      rtl_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(margin_left, p.inset.left);
  EXPECT_EQ(container_size_.inline_size - margin_left - width, p.inset.right);

  // Rule 3 width and right are auto.
  SetHorizontalStyle(left, margin_left, NGAuto, margin_right, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), true);
  estimated_inline = minmax_60;
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(
      container_size_.inline_size - minmax_60.min_size - left - margin_left,
      p.inset.right);
  EXPECT_EQ(minmax_60.min_size, p.size.width);

  // Rule 4: left is auto.
  SetHorizontalStyle(NGAuto, margin_left, width, margin_right, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(left + margin_left, p.inset.left);

  // Rule 4: left is auto, EBoxSizing::kContentBox
  style_->SetBoxSizing(EBoxSizing::kContentBox);
  SetHorizontalStyle(NGAuto, margin_left, width - border_left - border_right -
                                              padding_left - padding_right,
                     margin_right, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(left + margin_left, p.inset.left);
  style_->SetBoxSizing(EBoxSizing::kBorderBox);

  // Rule 5: right is auto.
  SetHorizontalStyle(left, margin_left, width, margin_right, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(right + margin_right, p.inset.right);

  // Rule 6: width is auto.
  SetHorizontalStyle(left, margin_left, NGAuto, margin_right, right);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  estimated_inline.reset();
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(width, p.size.width);
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

  LayoutUnit border_padding =
      border_top + padding_top + padding_bottom + border_bottom;
  LayoutUnit height =
      container_size_.block_size - top - margin_top - bottom - margin_bottom;

  style_->SetBorderTopWidth(border_top.ToInt());
  style_->SetBorderBottomWidth(border_bottom.ToInt());
  style_->SetPaddingTop(Length(padding_top.ToInt(), LengthType::kFixed));
  style_->SetPaddingBottom(Length(padding_bottom.ToInt(), LengthType::kFixed));
  // These default to 3 which is not what we want.
  style_->SetBorderLeftWidth(0);
  style_->SetBorderRightWidth(0);

  NGAbsolutePhysicalPosition p;
  base::Optional<LayoutUnit> auto_height;
  MinMaxSize minmax_60{LayoutUnit(60), LayoutUnit(60)};

  NGStaticPosition static_position{NGStaticPosition::kTopLeft,
                                   {LayoutUnit(), LayoutUnit()}};
  NGStaticPosition static_position_bottom{NGStaticPosition::kBottomLeft,
                                          {LayoutUnit(), LayoutUnit()}};

  //
  // Tests
  //

  // All auto, compute margins.
  SetVerticalStyle(NGAuto, NGAuto, NGAuto, NGAuto, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), true);
  auto_height = LayoutUnit(60);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, static_position, auto_height, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(*auto_height, p.size.height);
  EXPECT_EQ(LayoutUnit(0), p.inset.top);

  // All auto, static position bottom
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, static_position_bottom, auto_height, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(container_size_.block_size, p.inset.bottom);

  // If top, bottom, and height are known, compute margins.
  SetVerticalStyle(top, NGAuto, height, NGAuto, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, static_position, auto_height, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  LayoutUnit margin_space =
      (container_size_.block_size - top - height - bottom) / 2;
  EXPECT_EQ(top + margin_space, p.inset.top);
  EXPECT_EQ(bottom + margin_space, p.inset.bottom);

  // If top, bottom, and height are known, writing mode vertical_lr.
  SetVerticalStyle(top, NGAuto, height, NGAuto, bottom,
                   WritingMode::kVerticalLr);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  p = ComputePartialAbsoluteWithChildInlineSize(
      vertical_lr_space_, *style_, static_position, minmax_60, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(top + margin_space, p.inset.top);
  EXPECT_EQ(bottom + margin_space, p.inset.bottom);

  // If top, bottom, and height are known, writing mode vertical_rl.
  SetVerticalStyle(top, NGAuto, height, NGAuto, bottom,
                   WritingMode::kVerticalRl);
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(*style_), false);
  p = ComputePartialAbsoluteWithChildInlineSize(
      vertical_rl_space_, *style_, static_position, minmax_60, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(top + margin_space, p.inset.top);
  EXPECT_EQ(bottom + margin_space, p.inset.bottom);

  // If top, bottom, and height are known, negative auto margins.
  LayoutUnit negative_margin_space =
      (container_size_.block_size - top - LayoutUnit(300) - bottom) / 2;
  SetVerticalStyle(top, NGAuto, LayoutUnit(300), NGAuto, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, static_position, auto_height, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(top + negative_margin_space, p.inset.top);
  EXPECT_EQ(bottom + negative_margin_space, p.inset.bottom);

  // Rule 1: top and height are unknown.
  SetVerticalStyle(NGAuto, margin_top, NGAuto, margin_bottom, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), true);
  auto_height = LayoutUnit(60);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, static_position, auto_height, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(*auto_height, p.size.height);

  // Rule 2: top and bottom are unknown.
  SetVerticalStyle(NGAuto, margin_top, height, margin_bottom, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, static_position, auto_height, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(margin_top, p.inset.top);
  EXPECT_EQ(container_size_.block_size - margin_top - height, p.inset.bottom);

  // Rule 3: height and bottom are unknown, auto_height < border_padding.
  SetVerticalStyle(top, margin_top, NGAuto, margin_bottom, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), true);
  auto_height = LayoutUnit(20);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, static_position, auto_height, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(border_padding, p.size.height);

  // Rule 3: height and bottom are unknown.
  SetVerticalStyle(top, margin_top, NGAuto, margin_bottom, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), true);
  auto_height = LayoutUnit(70);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, static_position, auto_height, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(*auto_height, p.size.height);

  // Rule 4: top is unknown.
  SetVerticalStyle(NGAuto, margin_top, height, margin_bottom, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, static_position, auto_height, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(top + margin_top, p.inset.top);

  // Rule 5: bottom is unknown.
  SetVerticalStyle(top, margin_top, height, margin_bottom, NGAuto);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, static_position, auto_height, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(bottom + margin_bottom, p.inset.bottom);

  // Rule 6: height is unknown.
  SetVerticalStyle(top, margin_top, NGAuto, margin_bottom, bottom);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(*style_), false);
  auto_height.reset();
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, static_position, auto_height, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(height, p.size.height);
}

TEST_F(NGAbsoluteUtilsTest, MinMax) {
  LayoutUnit min{50};
  LayoutUnit max{150};

  style_->SetMinWidth(Length(min.ToInt(), LengthType::kFixed));
  style_->SetMaxWidth(Length(max.ToInt(), LengthType::kFixed));
  style_->SetMinHeight(Length(min.ToInt(), LengthType::kFixed));
  style_->SetMaxHeight(Length(max.ToInt(), LengthType::kFixed));

  NGStaticPosition static_position{NGStaticPosition::kTopLeft,
                                   {LayoutUnit(), LayoutUnit()}};
  MinMaxSize estimated_inline{LayoutUnit(20), LayoutUnit(20)};
  NGAbsolutePhysicalPosition p;

  // WIDTH TESTS

  // width < min gets set to min.
  SetHorizontalStyle(NGAuto, NGAuto, LayoutUnit(5), NGAuto, NGAuto);
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(min, p.size.width);

  // width > max gets set to max.
  SetHorizontalStyle(NGAuto, NGAuto, LayoutUnit(200), NGAuto, NGAuto);
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(max, p.size.width);

  // Unspecified width becomes minmax, gets clamped to min.
  SetHorizontalStyle(NGAuto, NGAuto, NGAuto, NGAuto, NGAuto);
  p = ComputePartialAbsoluteWithChildInlineSize(
      ltr_space_, *style_, static_position, estimated_inline, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr);
  EXPECT_EQ(min, p.size.width);

  // HEIGHT TESTS

  base::Optional<LayoutUnit> auto_height;

  // height < min gets set to min.
  SetVerticalStyle(NGAuto, NGAuto, LayoutUnit(5), NGAuto, NGAuto);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, static_position, auto_height, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(min, p.size.height);

  // height > max gets set to max.
  SetVerticalStyle(NGAuto, NGAuto, LayoutUnit(200), NGAuto, NGAuto);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, static_position, auto_height, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(max, p.size.height);

  // // Unspecified height becomes estimated, gets clamped to min.
  SetVerticalStyle(NGAuto, NGAuto, NGAuto, NGAuto, NGAuto);
  auto_height = LayoutUnit(20);
  ComputeFullAbsoluteWithChildBlockSize(
      ltr_space_, *style_, static_position, auto_height, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &p);
  EXPECT_EQ(min, p.size.width);
}

}  // namespace
}  // namespace blink
