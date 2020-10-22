// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

class NGAbsoluteUtilsTest : public RenderingTest {
 protected:
  NGConstraintSpace CreateConstraintSpace(TextDirection direction,
                                          WritingMode out_writing_mode) {
    NGConstraintSpaceBuilder builder(
        WritingMode::kHorizontalTb, out_writing_mode,
        /* is_new_fc */ true);
    builder.SetAvailableSize({LayoutUnit(200), LayoutUnit(300)});
    builder.SetTextDirection(direction);
    return builder.ToConstraintSpace();
  }

  void SetUp() override {
    RenderingTest::SetUp();
    SetBodyInnerHTML(R"HTML(
      <style>
        #target {
          position: absolute;
          border: solid;
          border-width: 9px 17px 17px 9px;
          padding: 11px 19px 19px 11px;
        }
      </style>
      <div id=target></div>
    )HTML");
    RunDocumentLifecycle();

    element_ = GetDocument().getElementById("target");
    ltr_space_ =
        CreateConstraintSpace(TextDirection::kLtr, WritingMode::kHorizontalTb);
    rtl_space_ =
        CreateConstraintSpace(TextDirection::kRtl, WritingMode::kHorizontalTb);
    vlr_space_ =
        CreateConstraintSpace(TextDirection::kLtr, WritingMode::kVerticalLr);
    vrl_space_ =
        CreateConstraintSpace(TextDirection::kLtr, WritingMode::kVerticalRl);
  }

  void SetHorizontalStyle(const String& left,
                          const String& margin_left,
                          const String& width,
                          const String& margin_right,
                          const String& right,
                          const String& writing_mode = "horizontal-tb",
                          const String& box_sizing = "border-box") {
    element_->SetInlineStyleProperty(CSSPropertyID::kLeft, left);
    element_->SetInlineStyleProperty(CSSPropertyID::kMarginLeft, margin_left);
    element_->SetInlineStyleProperty(CSSPropertyID::kWidth, width);
    element_->SetInlineStyleProperty(CSSPropertyID::kMarginRight, margin_right);
    element_->SetInlineStyleProperty(CSSPropertyID::kRight, right);
    element_->SetInlineStyleProperty(CSSPropertyID::kWritingMode, writing_mode);
    element_->SetInlineStyleProperty(CSSPropertyID::kBoxSizing, box_sizing);
    RunDocumentLifecycle();
  }

  void SetVerticalStyle(const String& top,
                        const String& margin_top,
                        const String& height,
                        const String& margin_bottom,
                        const String& bottom,
                        const String& writing_mode = "horizontal-tb",
                        const String& box_sizing = "border-box") {
    element_->SetInlineStyleProperty(CSSPropertyID::kTop, top);
    element_->SetInlineStyleProperty(CSSPropertyID::kMarginTop, margin_top);
    element_->SetInlineStyleProperty(CSSPropertyID::kHeight, height);
    element_->SetInlineStyleProperty(CSSPropertyID::kMarginBottom,
                                     margin_bottom);
    element_->SetInlineStyleProperty(CSSPropertyID::kBottom, bottom);
    element_->SetInlineStyleProperty(CSSPropertyID::kWritingMode, writing_mode);
    element_->SetInlineStyleProperty(CSSPropertyID::kBoxSizing, box_sizing);
    RunDocumentLifecycle();
  }

  Persistent<Element> element_;
  NGConstraintSpace ltr_space_;
  NGConstraintSpace rtl_space_;
  NGConstraintSpace vlr_space_;
  NGConstraintSpace vrl_space_;
};

TEST_F(NGAbsoluteUtilsTest, Horizontal) {
  // Test that the equation is computed correctly:
  // left + margin-left + border-left + padding-left +
  // width +
  // right + margin-right + border-right + padding-right = container-width
  MinMaxSizes min_max_60{LayoutUnit(60) + LayoutUnit(56),
                         LayoutUnit(60) + LayoutUnit(56)};

  NGBlockNode node(element_->GetLayoutBox());

  NGBoxStrut ltr_border_padding = ComputeBorders(ltr_space_, node) +
                                  ComputePadding(ltr_space_, node.Style());
  NGBoxStrut rtl_border_padding = ComputeBorders(rtl_space_, node) +
                                  ComputePadding(rtl_space_, node.Style());
  NGBoxStrut vlr_border_padding = ComputeBorders(vlr_space_, node) +
                                  ComputePadding(vlr_space_, node.Style());
  NGBoxStrut vrl_border_padding = ComputeBorders(vrl_space_, node) +
                                  ComputePadding(vrl_space_, node.Style());

  NGLogicalStaticPosition static_position = {
      {LayoutUnit(), LayoutUnit()},
      NGLogicalStaticPosition::kInlineStart,
      NGLogicalStaticPosition::kBlockStart};
  // Same as regular static position, but with the inline-end edge.
  NGLogicalStaticPosition static_position_inline_end = {
      {LayoutUnit(), LayoutUnit()},
      NGLogicalStaticPosition::kInlineEnd,
      NGLogicalStaticPosition::kBlockStart};

  NGLogicalOutOfFlowDimensions dimensions;

  // All auto => width is min_max_60, left is 0.
  SetHorizontalStyle("auto", "auto", "auto", "auto", "auto");
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(node), true);
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position, min_max_60,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  EXPECT_EQ(min_max_60.min_size, dimensions.size.inline_size);
  EXPECT_EQ(0, dimensions.inset.inline_start);

  // All auto => width is min_max_60, static_position is right
  SetHorizontalStyle("auto", "auto", "auto", "auto", "auto");
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(node), true);
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position_inline_end,
      min_max_60, base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  EXPECT_EQ(min_max_60.min_size, dimensions.size.inline_size);
  EXPECT_EQ(200, dimensions.inset.inline_end);

  // All auto + RTL.
  SetHorizontalStyle("auto", "auto", "auto", "auto", "auto");
  ComputeOutOfFlowInlineDimensions(
      node, rtl_space_, rtl_border_padding, static_position, min_max_60,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  EXPECT_EQ(min_max_60.min_size, dimensions.size.inline_size);
  // 200 = 0 + 0 + 116 + 84 + 0
  EXPECT_EQ(84, dimensions.inset.inline_end);

  // left, right, and left are known, compute margins.
  SetHorizontalStyle("5px", "auto", "160px", "auto", "13px");
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(node), false);
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  // 200 = 5 + 11 + 160 + 11 + 13
  EXPECT_EQ(16, dimensions.inset.inline_start);
  EXPECT_EQ(24, dimensions.inset.inline_end);

  // left, right, and left are known, compute margins, writing mode vertical_lr.
  SetHorizontalStyle("5px", "auto", "160px", "auto", "13px", "vertical-lr");
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(node), false);
  ComputeOutOfFlowBlockDimensions(
      node, vlr_space_, vlr_border_padding, static_position, base::nullopt,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  EXPECT_EQ(16, dimensions.inset.block_start);
  EXPECT_EQ(24, dimensions.inset.block_end);

  // left, right, and left are known, compute margins, writing mode vertical_rl.
  SetHorizontalStyle("5px", "auto", "160px", "auto", "13px", "vertical-rl");
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(node), false);
  ComputeOutOfFlowBlockDimensions(
      node, vrl_space_, vrl_border_padding, static_position, base::nullopt,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  EXPECT_EQ(16, dimensions.inset.block_end);
  EXPECT_EQ(24, dimensions.inset.block_start);

  // left, right, and width are known, not enough space for margins LTR.
  SetHorizontalStyle("5px", "auto", "200px", "auto", "13px");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  EXPECT_EQ(5, dimensions.inset.inline_start);
  EXPECT_EQ(-5, dimensions.inset.inline_end);

  // left, right, and left are known, not enough space for margins RTL.
  SetHorizontalStyle("5px", "auto", "200px", "auto", "13px");
  ComputeOutOfFlowInlineDimensions(
      node, rtl_space_, rtl_border_padding, static_position, base::nullopt,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kRtl, &dimensions);
  EXPECT_EQ(-13, dimensions.inset.inline_start);
  EXPECT_EQ(13, dimensions.inset.inline_end);

  // Rule 1 left and width are auto.
  SetHorizontalStyle("auto", "7px", "auto", "15px", "13px");
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(node), true);
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position, min_max_60,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  EXPECT_EQ(min_max_60.min_size, dimensions.size.inline_size);

  // Rule 2 left and right are auto LTR.
  SetHorizontalStyle("auto", "7px", "160px", "15px", "auto");
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(node), false);
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  // 200 = 0 + 7 + 160 + 15 + 18
  EXPECT_EQ(0 + 7, dimensions.inset.inline_start);
  EXPECT_EQ(15 + 18, dimensions.inset.inline_end);

  // Rule 2 left and right are auto RTL.
  SetHorizontalStyle("auto", "7px", "160px", "15px", "auto");
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(node), false);
  ComputeOutOfFlowInlineDimensions(
      node, rtl_space_, rtl_border_padding, static_position, base::nullopt,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kRtl, &dimensions);
  // 200 = 0 + 7 + 160 + 15 + 18
  EXPECT_EQ(0 + 7, dimensions.inset.inline_start);
  EXPECT_EQ(15 + 18, dimensions.inset.inline_end);

  // Rule 3 width and right are auto.
  SetHorizontalStyle("5px", "7px", "auto", "15px", "auto");
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(node), true);
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position, min_max_60,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  // 200 = 5 + 7 + 116 + 15 + 57
  EXPECT_EQ(min_max_60.min_size, dimensions.size.inline_size);
  EXPECT_EQ(15 + 57, dimensions.inset.inline_end);

  // Rule 4: left is auto.
  SetHorizontalStyle("auto", "7px", "160px", "15px", "13px");
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(node), false);
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  // 200 = 5 + 7 + 160 + 15 + 13
  EXPECT_EQ(5 + 7, dimensions.inset.inline_start);

  // Rule 4: left is auto, "box-sizing: content-box".
  SetHorizontalStyle("auto", "7px", "104px", "15px", "13px", "horizontal-tb",
                     "content-box");
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(node), false);
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  // 200 = 5 + 7 + 160 + 15 + 13
  EXPECT_EQ(5 + 7, dimensions.inset.inline_start);

  // Rule 5: right is auto.
  SetHorizontalStyle("5px", "7px", "160px", "15px", "auto");
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(node), false);
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  // 200 = 5 + 7 + 160 + 15 + 13
  EXPECT_EQ(15 + 13, dimensions.inset.inline_end);

  // Rule 6: width is auto.
  SetHorizontalStyle("5px", "7px", "auto", "15px", "13px");
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(node), false);
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  // 200 = 5 + 7 + 160 + 15 + 13
  EXPECT_EQ(160, dimensions.size.inline_size);
}

TEST_F(NGAbsoluteUtilsTest, Vertical) {
  // Test that the equation is computed correctly:
  // top + margin-top + border-top + padding-top +
  // height +
  // bottom + margin-top + border-bottom + padding-bottom = container-height
  MinMaxSizes min_max_60{LayoutUnit(60), LayoutUnit(60)};
  base::Optional<LayoutUnit> auto_60 = LayoutUnit(60);

  NGBlockNode node(element_->GetLayoutBox());

  NGBoxStrut ltr_border_padding = ComputeBorders(ltr_space_, node) +
                                  ComputePadding(ltr_space_, node.Style());
  NGBoxStrut vlr_border_padding = ComputeBorders(vlr_space_, node) +
                                  ComputePadding(vlr_space_, node.Style());
  NGBoxStrut vrl_border_padding = ComputeBorders(vrl_space_, node) +
                                  ComputePadding(vrl_space_, node.Style());

  NGLogicalStaticPosition static_position = {
      {LayoutUnit(), LayoutUnit()},
      NGLogicalStaticPosition::kInlineStart,
      NGLogicalStaticPosition::kBlockStart};
  NGLogicalStaticPosition static_position_block_end = {
      {LayoutUnit(), LayoutUnit()},
      NGLogicalStaticPosition::kInlineStart,
      NGLogicalStaticPosition::kBlockEnd};

  NGLogicalOutOfFlowDimensions dimensions;

  // All auto, compute margins.
  SetVerticalStyle("auto", "auto", "auto", "auto", "auto");
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(node), true);
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position, auto_60,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  EXPECT_EQ(60, dimensions.size.block_size);
  EXPECT_EQ(0, dimensions.inset.block_start);

  // All auto, static position bottom.
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position_block_end, auto_60,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  EXPECT_EQ(300, dimensions.inset.block_end);

  // If top, bottom, and height are known, compute margins.
  SetVerticalStyle("5px", "auto", "260px", "auto", "13px");
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(node), false);
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  // 300 = 5 + 11 + 260 + 11 + 13
  EXPECT_EQ(5 + 11, dimensions.inset.block_start);
  EXPECT_EQ(11 + 13, dimensions.inset.block_end);

  // If top, bottom, and height are known, "writing-mode: vertical-lr".
  SetVerticalStyle("5px", "auto", "260px", "auto", "13px", "vertical-lr");
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(node), false);
  ComputeOutOfFlowInlineDimensions(
      node, vlr_space_, vlr_border_padding, static_position, min_max_60,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  // 300 = 5 + 11 + 260 + 11 + 13
  EXPECT_EQ(5 + 11, dimensions.inset.inline_start);
  EXPECT_EQ(11 + 13, dimensions.inset.inline_end);

  // If top, bottom, and height are known, "writing-mode: vertical-rl".
  SetVerticalStyle("5px", "auto", "260px", "auto", "13px", "vertical-rl");
  EXPECT_EQ(AbsoluteNeedsChildInlineSize(node), false);
  ComputeOutOfFlowInlineDimensions(
      node, vrl_space_, vrl_border_padding, static_position, min_max_60,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  // 300 = 5 + 11 + 260 + 11 + 13
  EXPECT_EQ(5 + 11, dimensions.inset.inline_start);
  EXPECT_EQ(11 + 13, dimensions.inset.inline_end);

  // If top, bottom, and height are known, negative auto margins.
  SetVerticalStyle("5px", "auto", "300px", "auto", "13px");
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(node), false);
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  // 300 = 5 + (-9) + 300 + (-9) + 13
  EXPECT_EQ(5 - 9, dimensions.inset.block_start);
  EXPECT_EQ(-9 + 13, dimensions.inset.block_end);

  // Rule 1: top and height are unknown.
  SetVerticalStyle("auto", "7px", "auto", "15px", "13px");
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(node), true);
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position, auto_60,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  EXPECT_EQ(*auto_60, dimensions.size.block_size);

  // Rule 2: top and bottom are unknown.
  SetVerticalStyle("auto", "7px", "260px", "15px", "auto");
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(node), false);
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  // 300 = 0 + 7 + 260 + 15 + 18
  EXPECT_EQ(0 + 7, dimensions.inset.block_start);
  EXPECT_EQ(15 + 18, dimensions.inset.block_end);

  // Rule 3: height and bottom are unknown, auto_height <
  // horizontal_border_padding.
  SetVerticalStyle("5px", "7px", "auto", "15px", "auto");
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(node), true);
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position, LayoutUnit(20),
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  EXPECT_EQ(56, dimensions.size.block_size);

  // Rule 3: height and bottom are unknown.
  SetVerticalStyle("5px", "7px", "auto", "15px", "auto");
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(node), true);
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position, auto_60,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  EXPECT_EQ(*auto_60, dimensions.size.block_size);

  // Rule 4: top is unknown.
  SetVerticalStyle("auto", "7px", "260px", "15px", "13px");
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(node), false);
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  // 300 = 5 + 7 + 260 + 15 + 13
  EXPECT_EQ(5 + 7, dimensions.inset.block_start);

  // Rule 5: bottom is unknown.
  SetVerticalStyle("5px", "7px", "260px", "15px", "auto");
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(node), false);
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  // 300 = 5 + 7 + 260 + 15 + 13
  EXPECT_EQ(15 + 13, dimensions.inset.block_end);

  // Rule 6: height is unknown.
  SetVerticalStyle("5px", "7px", "auto", "15px", "13px");
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(node), false);
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  EXPECT_EQ(260, dimensions.size.block_size);
}

TEST_F(NGAbsoluteUtilsTest, CenterStaticPosition) {
  NGBlockNode node(element_->GetLayoutBox());
  NGLogicalStaticPosition static_position = {
      {LayoutUnit(150), LayoutUnit(200)},
      NGLogicalStaticPosition::kInlineCenter,
      NGLogicalStaticPosition::kBlockCenter};

  SetHorizontalStyle("auto", "auto", "auto", "auto", "auto");
  SetVerticalStyle("auto", "auto", "auto", "auto", "auto");

  EXPECT_EQ(AbsoluteNeedsChildInlineSize(node), true);
  EXPECT_EQ(AbsoluteNeedsChildBlockSize(node), true);

  NGBoxStrut border_padding;
  NGLogicalOutOfFlowDimensions dimensions;

  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, border_padding, static_position,
      MinMaxSizes{LayoutUnit(), LayoutUnit(1000)}, base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  EXPECT_EQ(100, dimensions.size.inline_size);
  EXPECT_EQ(100, dimensions.inset.inline_start);
  EXPECT_EQ(0, dimensions.inset.inline_end);

  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, border_padding, static_position,
      MinMaxSizes{LayoutUnit(), LayoutUnit(1000)}, base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kRtl, &dimensions);
  EXPECT_EQ(100, dimensions.size.inline_size);
  EXPECT_EQ(100, dimensions.inset.inline_start);
  EXPECT_EQ(0, dimensions.inset.inline_end);

  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, border_padding, static_position, LayoutUnit(150),
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  EXPECT_EQ(150, dimensions.size.block_size);
  EXPECT_EQ(125, dimensions.inset.block_start);
  EXPECT_EQ(25, dimensions.inset.block_end);
}

TEST_F(NGAbsoluteUtilsTest, MinMax) {
  element_->SetInlineStyleProperty(CSSPropertyID::kMinWidth, "70px");
  element_->SetInlineStyleProperty(CSSPropertyID::kMaxWidth, "150px");
  element_->SetInlineStyleProperty(CSSPropertyID::kMinHeight, "70px");
  element_->SetInlineStyleProperty(CSSPropertyID::kMaxHeight, "150px");

  NGBlockNode node(element_->GetLayoutBox());

  NGBoxStrut ltr_border_padding = ComputeBorders(ltr_space_, node) +
                                  ComputePadding(ltr_space_, node.Style());

  NGLogicalStaticPosition static_position = {
      {LayoutUnit(), LayoutUnit()},
      NGLogicalStaticPosition::kInlineStart,
      NGLogicalStaticPosition::kBlockStart};
  MinMaxSizes estimated_inline{LayoutUnit(20), LayoutUnit(20)};
  NGLogicalOutOfFlowDimensions dimensions;

  // WIDTH TESTS

  // width < min gets set to min.
  SetHorizontalStyle("auto", "auto", "5px", "auto", "auto");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position, estimated_inline,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  EXPECT_EQ(70, dimensions.size.inline_size);

  // width > max gets set to max.
  SetHorizontalStyle("auto", "auto", "200px", "auto", "auto");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position, estimated_inline,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  EXPECT_EQ(150, dimensions.size.inline_size);

  // Unspecified width becomes min_max, gets clamped to min.
  SetHorizontalStyle("auto", "auto", "auto", "auto", "auto");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position, estimated_inline,
      base::nullopt, base::nullopt,
      WritingMode::kHorizontalTb, TextDirection::kLtr, &dimensions);
  EXPECT_EQ(70, dimensions.size.inline_size);

  // HEIGHT TESTS

  // height < min gets set to min.
  SetVerticalStyle("auto", "auto", "5px", "auto", "auto");
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  EXPECT_EQ(70, dimensions.size.block_size);

  // height > max gets set to max.
  SetVerticalStyle("auto", "auto", "200px", "auto", "auto");
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position, base::nullopt,
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  EXPECT_EQ(150, dimensions.size.block_size);

  // // Unspecified height becomes estimated, gets clamped to min.
  SetVerticalStyle("auto", "auto", "auto", "auto", "auto");
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position, LayoutUnit(20),
      base::nullopt, WritingMode::kHorizontalTb, TextDirection::kLtr,
      &dimensions);
  EXPECT_EQ(70, dimensions.size.block_size);
}

}  // namespace
}  // namespace blink
