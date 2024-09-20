// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/absolute_utils.h"

#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/geometry/static_position.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
namespace {

class AbsoluteUtilsTest : public RenderingTest {
 public:
  AbsoluteUtilsTest()
      : ltr_space_(CreateConstraintSpace(
            {WritingMode::kHorizontalTb, TextDirection::kLtr})),
        rtl_space_(CreateConstraintSpace(
            {WritingMode::kHorizontalTb, TextDirection::kRtl})),
        vlr_space_(CreateConstraintSpace(
            {WritingMode::kVerticalLr, TextDirection::kLtr})),
        vrl_space_(CreateConstraintSpace(
            {WritingMode::kVerticalRl, TextDirection::kLtr})) {}

 protected:
  ConstraintSpace CreateConstraintSpace(
      WritingDirectionMode writing_direction) {
    ConstraintSpaceBuilder builder(WritingMode::kHorizontalTb,
                                   writing_direction,
                                   /* is_new_fc */ true);
    builder.SetAvailableSize({LayoutUnit(200), LayoutUnit(300)});
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
      <div id=target>
        <!-- Use a compressible element to simulate min/max sizes of {0, N} -->
        <textarea style="width: 100%; height: 88px;">
          xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
        </div>
      </div>
    )HTML");
    RunDocumentLifecycle();

    element_ = GetElementById("target");
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

  void ComputeOutOfFlowInlineDimensions(
      const BlockNode& node,
      const ConstraintSpace& space,
      const BoxStrut& border_padding,
      const LogicalStaticPosition& static_position,
      const WritingDirectionMode container_writing_direction,
      LogicalOofDimensions* dimensions) {
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInPerformLayout);
    WritingModeConverter container_converter(
        container_writing_direction,
        ToPhysicalSize(space.AvailableSize(),
                       container_writing_direction.GetWritingMode()));
    LogicalAnchorQuery* anchor_query =
        MakeGarbageCollected<LogicalAnchorQuery>();
    AnchorEvaluatorImpl anchor_evaluator(
        *node.GetLayoutBox(), *anchor_query,
        /* implicit_anchor */ nullptr, container_converter,
        /* self_writing_direction */
        {WritingMode::kHorizontalTb, TextDirection::kLtr},
        /* offset_to_padding_box */
        PhysicalOffset(),
        /* available_size */
        PhysicalSize());
    WritingDirectionMode self_writing_direction =
        node.Style().GetWritingDirection();
    const LogicalOofInsets insets =
        ComputeOutOfFlowInsets(node.Style(), space.AvailableSize(),
                               LogicalAlignment(), self_writing_direction);
    const InsetModifiedContainingBlock imcb =
        ComputeInsetModifiedContainingBlock(
            node, space.AvailableSize(), LogicalAlignment(), insets,
            static_position, container_writing_direction,
            node.Style().GetWritingDirection());
    ComputeOofInlineDimensions(
        node, node.Style(), space, imcb, LogicalAnchorCenterPosition(),
        LogicalAlignment(), border_padding, std::nullopt, BoxStrut(),
        container_writing_direction, dimensions);
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kAfterPerformLayout);
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kLayoutClean);
  }

  void ComputeOutOfFlowBlockDimensions(
      const BlockNode& node,
      const ConstraintSpace& space,
      const BoxStrut& border_padding,
      const LogicalStaticPosition& static_position,
      const WritingDirectionMode container_writing_direction,
      LogicalOofDimensions* dimensions) {
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInPerformLayout);
    WritingModeConverter container_converter(
        container_writing_direction,
        ToPhysicalSize(space.AvailableSize(),
                       container_writing_direction.GetWritingMode()));
    LogicalAnchorQuery* anchor_query =
        MakeGarbageCollected<LogicalAnchorQuery>();
    AnchorEvaluatorImpl anchor_evaluator(
        *node.GetLayoutBox(), *anchor_query,
        /* implicit_anchor */ nullptr, container_converter,
        /* self_writing_direction */
        {WritingMode::kHorizontalTb, TextDirection::kLtr},
        /* offset_to_padding_box */
        PhysicalOffset(),
        /* available_size */
        PhysicalSize());
    WritingDirectionMode self_writing_direction =
        node.Style().GetWritingDirection();
    const LogicalOofInsets insets =
        ComputeOutOfFlowInsets(node.Style(), space.AvailableSize(),
                               LogicalAlignment(), self_writing_direction);
    const InsetModifiedContainingBlock imcb =
        ComputeInsetModifiedContainingBlock(
            node, space.AvailableSize(), LogicalAlignment(), insets,
            static_position, container_writing_direction,
            node.Style().GetWritingDirection());
    ComputeOofBlockDimensions(node, node.Style(), space, imcb,
                              LogicalAnchorCenterPosition(), LogicalAlignment(),
                              border_padding, std::nullopt, BoxStrut(),
                              container_writing_direction, dimensions);
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kAfterPerformLayout);
    GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kLayoutClean);
  }

  Persistent<Element> element_;
  ConstraintSpace ltr_space_;
  ConstraintSpace rtl_space_;
  ConstraintSpace vlr_space_;
  ConstraintSpace vrl_space_;
};

TEST_F(AbsoluteUtilsTest, Horizontal) {
  BlockNode node(element_->GetLayoutBox());
  element_->SetInlineStyleProperty(CSSPropertyID::kContain, "size");
  element_->SetInlineStyleProperty(CSSPropertyID::kContainIntrinsicSize,
                                   "60px 4px");

  BoxStrut ltr_border_padding = ComputeBorders(ltr_space_, node) +
                                ComputePadding(ltr_space_, node.Style());
  BoxStrut rtl_border_padding = ComputeBorders(rtl_space_, node) +
                                ComputePadding(rtl_space_, node.Style());
  BoxStrut vlr_border_padding = ComputeBorders(vlr_space_, node) +
                                ComputePadding(vlr_space_, node.Style());
  BoxStrut vrl_border_padding = ComputeBorders(vrl_space_, node) +
                                ComputePadding(vrl_space_, node.Style());

  LogicalStaticPosition static_position = {{LayoutUnit(), LayoutUnit()},
                                           LogicalStaticPosition::kInlineStart,
                                           LogicalStaticPosition::kBlockStart};
  // Same as regular static position, but with the inline-end edge.
  LogicalStaticPosition static_position_inline_end = {
      {LayoutUnit(), LayoutUnit()},
      LogicalStaticPosition::kInlineEnd,
      LogicalStaticPosition::kBlockStart};

  LogicalOofDimensions dimensions;

  // All auto => width is content, left is 0.
  SetHorizontalStyle("auto", "auto", "auto", "auto", "auto");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(116, dimensions.size.inline_size);
  EXPECT_EQ(0, dimensions.inset.inline_start);

  // All auto => width is content, static_position is right
  SetHorizontalStyle("auto", "auto", "auto", "auto", "auto");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position_inline_end,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(116, dimensions.size.inline_size);
  EXPECT_EQ(200, dimensions.inset.inline_end);

  // All auto + RTL.
  SetHorizontalStyle("auto", "auto", "auto", "auto", "auto");
  ComputeOutOfFlowInlineDimensions(
      node, rtl_space_, rtl_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(116, dimensions.size.inline_size);
  // 200 = 0 + 0 + 116 + 84 + 0
  EXPECT_EQ(84, dimensions.inset.inline_end);

  // left, right, and left are known, compute margins.
  SetHorizontalStyle("5px", "auto", "160px", "auto", "13px");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  // 200 = 5 + 11 + 160 + 11 + 13
  EXPECT_EQ(16, dimensions.inset.inline_start);
  EXPECT_EQ(24, dimensions.inset.inline_end);

  // left, right, and left are known, compute margins, writing mode vertical_lr.
  SetHorizontalStyle("5px", "auto", "160px", "auto", "13px", "vertical-lr");
  ComputeOutOfFlowBlockDimensions(
      node, vlr_space_, vlr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(16, dimensions.inset.block_start);
  EXPECT_EQ(24, dimensions.inset.block_end);

  // left, right, and left are known, compute margins, writing mode vertical_rl.
  SetHorizontalStyle("5px", "auto", "160px", "auto", "13px", "vertical-rl");
  ComputeOutOfFlowBlockDimensions(
      node, vrl_space_, vrl_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(16, dimensions.inset.block_end);
  EXPECT_EQ(24, dimensions.inset.block_start);

  // left, right, and width are known, not enough space for margins LTR.
  SetHorizontalStyle("5px", "auto", "200px", "auto", "13px");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(5, dimensions.inset.inline_start);
  EXPECT_EQ(-5, dimensions.inset.inline_end);

  // left, right, and left are known, not enough space for margins RTL.
  SetHorizontalStyle("5px", "auto", "200px", "auto", "13px");
  ComputeOutOfFlowInlineDimensions(
      node, rtl_space_, rtl_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kRtl}, &dimensions);
  EXPECT_EQ(-13, dimensions.inset.inline_start);
  EXPECT_EQ(13, dimensions.inset.inline_end);

  // Rule 1 left and width are auto.
  SetHorizontalStyle("auto", "7px", "auto", "15px", "13px");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(116, dimensions.size.inline_size);

  // Rule 2 left and right are auto LTR.
  SetHorizontalStyle("auto", "7px", "160px", "15px", "auto");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  // 200 = 0 + 7 + 160 + 15 + 18
  EXPECT_EQ(0 + 7, dimensions.inset.inline_start);
  EXPECT_EQ(15 + 18, dimensions.inset.inline_end);

  // Rule 2 left and right are auto RTL.
  SetHorizontalStyle("auto", "7px", "160px", "15px", "auto");
  ComputeOutOfFlowInlineDimensions(
      node, rtl_space_, rtl_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kRtl}, &dimensions);
  // 200 = 0 + 7 + 160 + 15 + 18
  EXPECT_EQ(0 + 7, dimensions.inset.inline_start);
  EXPECT_EQ(15 + 18, dimensions.inset.inline_end);

  // Rule 3 width and right are auto.
  SetHorizontalStyle("5px", "7px", "auto", "15px", "auto");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  // 200 = 5 + 7 + 116 + 15 + 57
  EXPECT_EQ(116, dimensions.size.inline_size);
  EXPECT_EQ(15 + 57, dimensions.inset.inline_end);

  // Rule 4: left is auto.
  SetHorizontalStyle("auto", "7px", "160px", "15px", "13px");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  // 200 = 5 + 7 + 160 + 15 + 13
  EXPECT_EQ(5 + 7, dimensions.inset.inline_start);

  // Rule 4: left is auto, "box-sizing: content-box".
  SetHorizontalStyle("auto", "7px", "104px", "15px", "13px", "horizontal-tb",
                     "content-box");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  // 200 = 5 + 7 + 160 + 15 + 13
  EXPECT_EQ(5 + 7, dimensions.inset.inline_start);

  // Rule 5: right is auto.
  SetHorizontalStyle("5px", "7px", "160px", "15px", "auto");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  // 200 = 5 + 7 + 160 + 15 + 13
  EXPECT_EQ(15 + 13, dimensions.inset.inline_end);

  // Rule 6: width is auto.
  SetHorizontalStyle("5px", "7px", "auto", "15px", "13px");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  // 200 = 5 + 7 + 160 + 15 + 13
  EXPECT_EQ(160, dimensions.size.inline_size);
}

TEST_F(AbsoluteUtilsTest, Vertical) {
  element_->SetInlineStyleProperty(CSSPropertyID::kContain, "size");
  element_->SetInlineStyleProperty(CSSPropertyID::kContainIntrinsicSize,
                                   "60px 4px");

  BlockNode node(element_->GetLayoutBox());

  BoxStrut ltr_border_padding = ComputeBorders(ltr_space_, node) +
                                ComputePadding(ltr_space_, node.Style());
  BoxStrut vlr_border_padding = ComputeBorders(vlr_space_, node) +
                                ComputePadding(vlr_space_, node.Style());
  BoxStrut vrl_border_padding = ComputeBorders(vrl_space_, node) +
                                ComputePadding(vrl_space_, node.Style());

  LogicalStaticPosition static_position = {{LayoutUnit(), LayoutUnit()},
                                           LogicalStaticPosition::kInlineStart,
                                           LogicalStaticPosition::kBlockStart};
  LogicalStaticPosition static_position_block_end = {
      {LayoutUnit(), LayoutUnit()},
      LogicalStaticPosition::kInlineStart,
      LogicalStaticPosition::kBlockEnd};

  LogicalOofDimensions dimensions;

  // Set inline-dimensions in-case any block dimensions require it.
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);

  // All auto, compute margins.
  SetVerticalStyle("auto", "auto", "auto", "auto", "auto");
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(60, dimensions.size.block_size);
  EXPECT_EQ(0, dimensions.inset.block_start);

  // All auto, static position bottom.
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position_block_end,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(300, dimensions.inset.block_end);

  // If top, bottom, and height are known, compute margins.
  SetVerticalStyle("5px", "auto", "260px", "auto", "13px");
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  // 300 = 5 + 11 + 260 + 11 + 13
  EXPECT_EQ(5 + 11, dimensions.inset.block_start);
  EXPECT_EQ(11 + 13, dimensions.inset.block_end);

  // If top, bottom, and height are known, "writing-mode: vertical-lr".
  SetVerticalStyle("5px", "auto", "260px", "auto", "13px", "vertical-lr");
  ComputeOutOfFlowInlineDimensions(
      node, vlr_space_, vlr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  // 300 = 5 + 11 + 260 + 11 + 13
  EXPECT_EQ(5 + 11, dimensions.inset.inline_start);
  EXPECT_EQ(11 + 13, dimensions.inset.inline_end);

  // If top, bottom, and height are known, "writing-mode: vertical-rl".
  SetVerticalStyle("5px", "auto", "260px", "auto", "13px", "vertical-rl");
  ComputeOutOfFlowInlineDimensions(
      node, vrl_space_, vrl_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  // 300 = 5 + 11 + 260 + 11 + 13
  EXPECT_EQ(5 + 11, dimensions.inset.inline_start);
  EXPECT_EQ(11 + 13, dimensions.inset.inline_end);

  // If top, bottom, and height are known, negative auto margins.
  SetVerticalStyle("5px", "auto", "300px", "auto", "13px");
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  // 300 = 5 + (-9) + 300 + (-9) + 13
  EXPECT_EQ(5 - 9, dimensions.inset.block_start);
  EXPECT_EQ(-9 + 13, dimensions.inset.block_end);

  // Rule 1: top and height are unknown.
  SetVerticalStyle("auto", "7px", "auto", "15px", "13px");
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(60, dimensions.size.block_size);

  // Rule 2: top and bottom are unknown.
  SetVerticalStyle("auto", "7px", "260px", "15px", "auto");
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  // 300 = 0 + 7 + 260 + 15 + 18
  EXPECT_EQ(0 + 7, dimensions.inset.block_start);
  EXPECT_EQ(15 + 18, dimensions.inset.block_end);

  // Rule 3: height and bottom are unknown.
  SetVerticalStyle("5px", "7px", "auto", "15px", "auto");
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(60, dimensions.size.block_size);

  // Rule 4: top is unknown.
  SetVerticalStyle("auto", "7px", "260px", "15px", "13px");
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  // 300 = 5 + 7 + 260 + 15 + 13
  EXPECT_EQ(5 + 7, dimensions.inset.block_start);

  // Rule 5: bottom is unknown.
  SetVerticalStyle("5px", "7px", "260px", "15px", "auto");
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(260, dimensions.size.block_size);
}

TEST_F(AbsoluteUtilsTest, CenterStaticPosition) {
  BlockNode node(element_->GetLayoutBox());
  LogicalStaticPosition static_position = {{LayoutUnit(150), LayoutUnit(200)},
                                           LogicalStaticPosition::kInlineCenter,
                                           LogicalStaticPosition::kBlockCenter};

  SetHorizontalStyle("auto", "auto", "auto", "auto", "auto");
  SetVerticalStyle("auto", "auto", "auto", "auto", "auto");

  BoxStrut border_padding;
  LogicalOofDimensions dimensions;

  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(100, dimensions.size.inline_size);
  EXPECT_EQ(100, dimensions.inset.inline_start);
  EXPECT_EQ(0, dimensions.inset.inline_end);

  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kRtl}, &dimensions);
  EXPECT_EQ(100, dimensions.size.inline_size);
  EXPECT_EQ(100, dimensions.inset.inline_start);
  EXPECT_EQ(0, dimensions.inset.inline_end);

  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(150, dimensions.size.block_size);
  EXPECT_EQ(125, dimensions.inset.block_start);
  EXPECT_EQ(25, dimensions.inset.block_end);
}

TEST_F(AbsoluteUtilsTest, MinMax) {
  element_->SetInlineStyleProperty(CSSPropertyID::kMinWidth, "70px");
  element_->SetInlineStyleProperty(CSSPropertyID::kMaxWidth, "150px");
  element_->SetInlineStyleProperty(CSSPropertyID::kMinHeight, "70px");
  element_->SetInlineStyleProperty(CSSPropertyID::kMaxHeight, "150px");
  element_->SetInlineStyleProperty(CSSPropertyID::kContain, "size");

  BlockNode node(element_->GetLayoutBox());

  BoxStrut ltr_border_padding = ComputeBorders(ltr_space_, node) +
                                ComputePadding(ltr_space_, node.Style());

  LogicalStaticPosition static_position = {{LayoutUnit(), LayoutUnit()},
                                           LogicalStaticPosition::kInlineStart,
                                           LogicalStaticPosition::kBlockStart};

  LogicalOofDimensions dimensions;

  // WIDTH TESTS

  // width < min gets set to min.
  SetHorizontalStyle("auto", "auto", "5px", "auto", "auto");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(70, dimensions.size.inline_size);

  // width > max gets set to max.
  SetHorizontalStyle("auto", "auto", "200px", "auto", "auto");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(150, dimensions.size.inline_size);

  // Unspecified width becomes min_max, gets clamped to min.
  SetHorizontalStyle("auto", "auto", "auto", "auto", "auto");
  ComputeOutOfFlowInlineDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(70, dimensions.size.inline_size);

  // HEIGHT TESTS

  // height < min gets set to min.
  SetVerticalStyle("auto", "auto", "5px", "auto", "auto");
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(70, dimensions.size.block_size);

  // height > max gets set to max.
  SetVerticalStyle("auto", "auto", "200px", "auto", "auto");
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(150, dimensions.size.block_size);

  // // Unspecified height becomes estimated, gets clamped to min.
  SetVerticalStyle("auto", "auto", "auto", "auto", "auto");
  ComputeOutOfFlowBlockDimensions(
      node, ltr_space_, ltr_border_padding, static_position,
      {WritingMode::kHorizontalTb, TextDirection::kLtr}, &dimensions);
  EXPECT_EQ(70, dimensions.size.block_size);
}

}  // namespace
}  // namespace blink
