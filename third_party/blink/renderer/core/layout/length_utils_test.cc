// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/length_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

static ConstraintSpace ConstructConstraintSpace(
    int inline_size,
    int block_size,
    bool fixed_inline = false,
    bool fixed_block = false,
    WritingMode writing_mode = WritingMode::kHorizontalTb) {
  LogicalSize size = {LayoutUnit(inline_size), LayoutUnit(block_size)};

  ConstraintSpaceBuilder builder(writing_mode,
                                 {writing_mode, TextDirection::kLtr},
                                 /* is_new_fc */ false);
  builder.SetAvailableSize(size);
  builder.SetPercentageResolutionSize(size);
  builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  builder.SetIsFixedInlineSize(fixed_inline);
  builder.SetIsFixedBlockSize(fixed_block);
  return builder.ToConstraintSpace();
}

class LengthUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    initial_style_ = ComputedStyle::GetInitialStyleSingleton();
  }

  LayoutUnit ResolveMainInlineLength(
      const Length& length,
      const std::optional<MinMaxSizes>& sizes = std::nullopt,
      ConstraintSpace constraint_space = ConstructConstraintSpace(200, 300)) {
    return ::blink::ResolveMainInlineLength(
        constraint_space, *initial_style_, /* border_padding */ BoxStrut(),
        [&](SizeType) -> MinMaxSizesResult {
          return {*sizes, /* depends_on_block_constraints */ false};
        },
        length, /* auto_length */ nullptr);
  }

  LayoutUnit ResolveMinInlineLength(
      const Length& length,
      const std::optional<MinMaxSizes>& sizes = std::nullopt,
      ConstraintSpace constraint_space = ConstructConstraintSpace(200, 300)) {
    return ::blink::ResolveMinInlineLength(
        constraint_space, *initial_style_, /* border_padding */ BoxStrut(),
        [&](SizeType) -> MinMaxSizesResult {
          return {*sizes, /* depends_on_block_constraints */ false};
        },
        length);
  }

  LayoutUnit ResolveMaxInlineLength(
      const Length& length,
      const std::optional<MinMaxSizes>& sizes = std::nullopt,
      ConstraintSpace constraint_space = ConstructConstraintSpace(200, 300)) {
    return ::blink::ResolveMaxInlineLength(
        constraint_space, *initial_style_, /* border_padding */ BoxStrut(),
        [&](SizeType) -> MinMaxSizesResult {
          return {*sizes, /* depends_on_block_constraints */ false};
        },
        length);
  }

  LayoutUnit ResolveMainBlockLength(const Length& length,
                                    LayoutUnit content_size = LayoutUnit()) {
    ConstraintSpace constraint_space = ConstructConstraintSpace(200, 300);
    return ::blink::ResolveMainBlockLength(constraint_space, *initial_style_,
                                           /* border_padding */ BoxStrut(),
                                           length, /* auto_length */ nullptr,
                                           content_size);
  }

  Persistent<const ComputedStyle> initial_style_;
  test::TaskEnvironment task_environment_;
};

class LengthUtilsTestWithNode : public RenderingTest {
 public:
  LayoutUnit ComputeInlineSizeForFragment(
      const BlockNode& node,
      ConstraintSpace constraint_space = ConstructConstraintSpace(200, 300),
      const MinMaxSizes& sizes = MinMaxSizes()) {
    BoxStrut border_padding = ComputeBorders(constraint_space, node) +
                              ComputePadding(constraint_space, node.Style());
    return ::blink::ComputeInlineSizeForFragment(constraint_space, node,
                                                 border_padding, &sizes);
  }

  LayoutUnit ComputeBlockSizeForFragment(
      const BlockNode& node,
      ConstraintSpace constraint_space = ConstructConstraintSpace(200, 300),
      LayoutUnit content_size = LayoutUnit(),
      LayoutUnit inline_size = kIndefiniteSize) {
    BoxStrut border_padding = ComputeBorders(constraint_space, node) +
                              ComputePadding(constraint_space, node.Style());
    return ::blink::ComputeBlockSizeForFragment(
        constraint_space, node, border_padding, content_size, inline_size);
  }
};

TEST_F(LengthUtilsTest, TestResolveInlineLength) {
  EXPECT_EQ(LayoutUnit(60), ResolveMainInlineLength(Length::Percent(30)));
  EXPECT_EQ(LayoutUnit(150), ResolveMainInlineLength(Length::Fixed(150)));
  EXPECT_EQ(LayoutUnit(200), ResolveMainInlineLength(Length::Stretch()));

  MinMaxSizes sizes;
  sizes.min_size = LayoutUnit(30);
  sizes.max_size = LayoutUnit(40);
  EXPECT_EQ(LayoutUnit(30),
            ResolveMainInlineLength(Length::MinContent(), sizes));
  EXPECT_EQ(LayoutUnit(40),
            ResolveMainInlineLength(Length::MaxContent(), sizes));
  EXPECT_EQ(LayoutUnit(40),
            ResolveMainInlineLength(Length::FitContent(), sizes));
  sizes.max_size = LayoutUnit(800);
  EXPECT_EQ(LayoutUnit(200),
            ResolveMainInlineLength(Length::FitContent(), sizes));

#if DCHECK_IS_ON()
  // This should fail a DCHECK.
  EXPECT_DEATH_IF_SUPPORTED(ResolveMainInlineLength(Length::FitContent()), "");
#endif
}

TEST_F(LengthUtilsTest, TestIndefiniteResolveInlineLength) {
  const ConstraintSpace space = ConstructConstraintSpace(-1, -1);

  EXPECT_EQ(LayoutUnit(0),
            ResolveMinInlineLength(Length::Auto(), std::nullopt, space));
  EXPECT_EQ(LayoutUnit::Max(),
            ResolveMaxInlineLength(Length::Percent(30), std::nullopt, space));
  EXPECT_EQ(LayoutUnit::Max(),
            ResolveMaxInlineLength(Length::Stretch(), std::nullopt, space));
}

TEST_F(LengthUtilsTest, TestResolveBlockLength) {
  EXPECT_EQ(LayoutUnit(90), ResolveMainBlockLength(Length::Percent(30)));
  EXPECT_EQ(LayoutUnit(150), ResolveMainBlockLength(Length::Fixed(150)));
  EXPECT_EQ(LayoutUnit(300), ResolveMainBlockLength(Length::Stretch()));
}

TEST_F(LengthUtilsTestWithNode, TestComputeContentContribution) {
  SetBodyInnerHTML(R"HTML(
    <div id="test1" style="width:30%;"></div>
    <div id="test2" style="width:-webkit-fill-available;"></div>
    <div id="test3" style="width:150px;"></div>
    <div id="test4" style="width:auto;"></div>
    <div id="test5" style="width:auto; padding-left:400px;"></div>
    <div id="test6" style="width:calc(100px + 10%);"></div>
    <div id="test7" style="max-width:35px;"></div>
    <div id="test8" style="min-width:80px; max-width: 35px"></div>
    <div id="test9" style="width:100px; padding-left:50px;"></div>
    <div id="test10" style="width:100px; padding-left:50px; box-sizing:border-box;"></div>
    <div id="test11" style="width:100px; padding-left:400px; box-sizing:border-box;"></div>
    <div id="test12" style="width:min-content; padding-left:400px; box-sizing:border-box;"></div>
    <div id="test13" style="width:100px; max-width:max-content; padding-left:400px; box-sizing:border-box;"></div>
    <div id="test14" style="width:100px; max-width:max-content; box-sizing:border-box;"></div>
  )HTML");

  MinMaxSizes sizes = {LayoutUnit(30), LayoutUnit(40)};
  const auto space =
      ConstraintSpaceBuilder(WritingMode::kHorizontalTb,
                             {WritingMode::kHorizontalTb, TextDirection::kLtr},
                             /* is_new_fc */ false)
          .ToConstraintSpace();

  MinMaxSizes expected = sizes;
  BlockNode node(To<LayoutBox>(GetLayoutObjectByElementId("test1")));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContributionForTest(
                          WritingMode::kHorizontalTb, node, space, sizes));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test2")));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContributionForTest(
                          WritingMode::kHorizontalTb, node, space, sizes));

  expected = MinMaxSizes{LayoutUnit(150), LayoutUnit(150)};
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test3")));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContributionForTest(
                          WritingMode::kHorizontalTb, node, space, sizes));

  expected = sizes;
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test4")));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContributionForTest(
                          WritingMode::kHorizontalTb, node, space, sizes));

  expected = MinMaxSizes{LayoutUnit(430), LayoutUnit(440)};
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test5")));
  auto sizes_padding400 = sizes;
  sizes_padding400 += LayoutUnit(400);
  EXPECT_EQ(expected,
            ComputeMinAndMaxContentContributionForTest(
                WritingMode::kHorizontalTb, node, space, sizes_padding400));

  expected = MinMaxSizes{LayoutUnit(30), LayoutUnit(40)};
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test6")));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContributionForTest(
                          WritingMode::kHorizontalTb, node, space, sizes));

  expected = MinMaxSizes{LayoutUnit(30), LayoutUnit(35)};
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test7")));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContributionForTest(
                          WritingMode::kHorizontalTb, node, space, sizes));

  expected = MinMaxSizes{LayoutUnit(80), LayoutUnit(80)};
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test8")));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContributionForTest(
                          WritingMode::kHorizontalTb, node, space, sizes));

  expected = MinMaxSizes{LayoutUnit(150), LayoutUnit(150)};
  auto sizes_padding50 = sizes;
  sizes_padding50 += LayoutUnit(50);
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test9")));
  EXPECT_EQ(expected,
            ComputeMinAndMaxContentContributionForTest(
                WritingMode::kHorizontalTb, node, space, sizes_padding50));

  expected = MinMaxSizes{LayoutUnit(100), LayoutUnit(100)};
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test10")));
  EXPECT_EQ(expected,
            ComputeMinAndMaxContentContributionForTest(
                WritingMode::kHorizontalTb, node, space, sizes_padding50));

  // Content size should never be below zero, even with box-sizing: border-box
  // and a large padding...
  expected = MinMaxSizes{LayoutUnit(400), LayoutUnit(400)};
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test11")));
  EXPECT_EQ(expected,
            ComputeMinAndMaxContentContributionForTest(
                WritingMode::kHorizontalTb, node, space, sizes_padding400));

  expected.min_size = expected.max_size = sizes.min_size + LayoutUnit(400);
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test12")));
  EXPECT_EQ(expected,
            ComputeMinAndMaxContentContributionForTest(
                WritingMode::kHorizontalTb, node, space, sizes_padding400));

  // Due to padding and box-sizing, width computes to 400px and max-width to
  // 440px, so the result is 400.
  expected = MinMaxSizes{LayoutUnit(400), LayoutUnit(400)};
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test13")));
  EXPECT_EQ(expected,
            ComputeMinAndMaxContentContributionForTest(
                WritingMode::kHorizontalTb, node, space, sizes_padding400));

  expected = MinMaxSizes{LayoutUnit(40), LayoutUnit(40)};
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test14")));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContributionForTest(
                          WritingMode::kHorizontalTb, node, space, sizes));
}

TEST_F(LengthUtilsTestWithNode, TestComputeInlineSizeForFragment) {
  SetBodyInnerHTML(R"HTML(
    <div id="test1" style="width:30%;"></div>
    <div id="test2" style="width:-webkit-fill-available;"></div>
    <div id="test3" style="width:150px;"></div>
    <div id="test4" style="width:auto;"></div>
    <div id="test5" style="width:calc(100px - 10%);"></div>
    <div id="test6" style="width:150px;"></div>
    <div id="test7" style="width:200px; max-width:80%;"></div>
    <div id="test8" style="min-width:80%; width:100px; max-width:80%;"></div>
    <div id="test9" style="margin-right:20px;"></div>
    <div id="test10" style="width:100px; padding-left:50px; margin-right:20px;"></div>
    <div id="test11" style="width:100px; padding-left:50px; margin-right:20px; box-sizing:border-box;"></div>
    <div id="test12" style="width:100px; padding-left:400px; margin-right:20px; box-sizing:border-box;"></div>
    <div id="test13" style="width:-webkit-fill-available; padding-left:400px; margin-right:20px; box-sizing:border-box;"></div>
    <div id="test14" style="width:min-content; padding-left:400px; margin-right:20px; box-sizing:border-box;"></div>
    <div id="test15" style="width:100px; max-width:max-content; padding-left:400px; margin-right:20px; box-sizing:border-box;"></div>
    <div id="test16" style="width:100px; max-width:max-content; margin-right:20px; box-sizing:border-box;"></div>
  )HTML");

  MinMaxSizes sizes = {LayoutUnit(30), LayoutUnit(40)};

  BlockNode node(To<LayoutBox>(GetLayoutObjectByElementId("test1")));
  EXPECT_EQ(LayoutUnit(60), ComputeInlineSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test2")));
  EXPECT_EQ(LayoutUnit(200), ComputeInlineSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test3")));
  EXPECT_EQ(LayoutUnit(150), ComputeInlineSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test4")));
  EXPECT_EQ(LayoutUnit(200), ComputeInlineSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test5")));
  EXPECT_EQ(LayoutUnit(80), ComputeInlineSizeForFragment(node));

  ConstraintSpace constraint_space =
      ConstructConstraintSpace(120, 120, true, true);
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test6")));
  EXPECT_EQ(LayoutUnit(120),
            ComputeInlineSizeForFragment(node, constraint_space));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test7")));
  EXPECT_EQ(LayoutUnit(160), ComputeInlineSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test8")));
  EXPECT_EQ(LayoutUnit(160), ComputeInlineSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test9")));
  EXPECT_EQ(LayoutUnit(180), ComputeInlineSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test10")));
  EXPECT_EQ(LayoutUnit(150), ComputeInlineSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test11")));
  EXPECT_EQ(LayoutUnit(100), ComputeInlineSizeForFragment(node));

  // Content size should never be below zero, even with box-sizing: border-box
  // and a large padding...
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test12")));
  EXPECT_EQ(LayoutUnit(400), ComputeInlineSizeForFragment(node));
  auto sizes_padding400 = sizes;
  sizes_padding400 += LayoutUnit(400);

  // ...and the same goes for fill-available with a large padding.
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test13")));
  EXPECT_EQ(LayoutUnit(400), ComputeInlineSizeForFragment(node));

  constraint_space = ConstructConstraintSpace(120, 140);
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test14")));
  EXPECT_EQ(LayoutUnit(430), ComputeInlineSizeForFragment(
                                 node, constraint_space, sizes_padding400));

  //  Due to padding and box-sizing, width computes to 400px and max-width to
  //  440px, so the result is 400.
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test15")));
  EXPECT_EQ(LayoutUnit(400), ComputeInlineSizeForFragment(
                                 node, constraint_space, sizes_padding400));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test16")));
  EXPECT_EQ(LayoutUnit(40),
            ComputeInlineSizeForFragment(node, constraint_space, sizes));
}

TEST_F(LengthUtilsTestWithNode, TestComputeBlockSizeForFragment) {
  SetBodyInnerHTML(R"HTML(
    <div id="test1" style="height:30%;"></div>
    <div id="test2" style="height:-webkit-fill-available;"></div>
    <div id="test3" style="height:150px;"></div>
    <div id="test4" style="height:auto;"></div>
    <div id="test5" style="height:calc(100px - 10%);"></div>
    <div id="test6" style="height:150px;"></div>
    <div id="test7" style="height:300px; max-height:80%;"></div>
    <div id="test8" style="min-height:80%; height:100px; max-height:80%;"></div>
    <div id="test9" style="height:-webkit-fill-available; margin-top:20px;"></div>
    <div id="test10" style="height:100px; padding-bottom:50px;"></div>
    <div id="test11" style="height:100px; padding-bottom:50px; box-sizing:border-box;"></div>
    <div id="test12" style="height:100px; padding-bottom:400px; box-sizing:border-box;"></div>
    <div id="test13" style="height:-webkit-fill-available; padding-bottom:400px; box-sizing:border-box;"></div>
    <div id="test14" style="width:100px; aspect-ratio:2/1;"></div>
    <div id="test15" style="width:100px; aspect-ratio:2/1; padding-right:10px; padding-bottom:20px;"></div>
    <div id="test16" style="width:100px; aspect-ratio:2/1; padding-right:10px; padding-bottom:20px; box-sizing:border-box;"></div>
  )HTML");

  BlockNode node(To<LayoutBox>(GetLayoutObjectByElementId("test1")));
  EXPECT_EQ(LayoutUnit(90), ComputeBlockSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test2")));
  EXPECT_EQ(LayoutUnit(300), ComputeBlockSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test3")));
  EXPECT_EQ(LayoutUnit(150), ComputeBlockSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test4")));
  EXPECT_EQ(LayoutUnit(0), ComputeBlockSizeForFragment(node));

  ConstraintSpace constraint_space = ConstructConstraintSpace(200, 300);
  EXPECT_EQ(LayoutUnit(120), ComputeBlockSizeForFragment(node, constraint_space,
                                                         LayoutUnit(120)));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test5")));
  EXPECT_EQ(LayoutUnit(70), ComputeBlockSizeForFragment(node));

  constraint_space = ConstructConstraintSpace(200, 200, true, true);
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test6")));
  EXPECT_EQ(LayoutUnit(200),
            ComputeBlockSizeForFragment(node, constraint_space));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test7")));
  EXPECT_EQ(LayoutUnit(240), ComputeBlockSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test8")));
  EXPECT_EQ(LayoutUnit(240), ComputeBlockSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test9")));
  EXPECT_EQ(LayoutUnit(280), ComputeBlockSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test10")));
  EXPECT_EQ(LayoutUnit(150), ComputeBlockSizeForFragment(node));

  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test11")));
  EXPECT_EQ(LayoutUnit(100), ComputeBlockSizeForFragment(node));

  // Content size should never be below zero, even with box-sizing: border-box
  // and a large padding...
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test12")));
  EXPECT_EQ(LayoutUnit(400), ComputeBlockSizeForFragment(node));

  // ...and the same goes for fill-available with a large padding.
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test13")));
  EXPECT_EQ(LayoutUnit(400), ComputeBlockSizeForFragment(node));

  // Now check aspect-ratio.
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test14")));
  EXPECT_EQ(LayoutUnit(50), ComputeBlockSizeForFragment(
                                node, ConstructConstraintSpace(200, 300),
                                LayoutUnit(), LayoutUnit(100)));

  // Default box-sizing
  // Should be (100 - 10) / 2 + 20 = 65.
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test15")));
  EXPECT_EQ(LayoutUnit(65), ComputeBlockSizeForFragment(
                                node, ConstructConstraintSpace(200, 300),
                                LayoutUnit(20), LayoutUnit(100)));

  // With box-sizing: border-box, should be 50.
  node = BlockNode(To<LayoutBox>(GetLayoutObjectByElementId("test16")));
  EXPECT_EQ(LayoutUnit(50), ComputeBlockSizeForFragment(
                                node, ConstructConstraintSpace(200, 300),
                                LayoutUnit(20), LayoutUnit(100)));
}

TEST_F(LengthUtilsTestWithNode, TestIndefinitePercentages) {
  SetBodyInnerHTML(R"HTML(
    <div id="test" style="min-height:20px; height:20%;"></div>
  )HTML");

  BlockNode node(To<LayoutBox>(GetLayoutObjectByElementId("test")));
  EXPECT_EQ(kIndefiniteSize,
            ComputeBlockSizeForFragment(node, ConstructConstraintSpace(200, -1),
                                        LayoutUnit(-1)));
  EXPECT_EQ(LayoutUnit(20),
            ComputeBlockSizeForFragment(node, ConstructConstraintSpace(200, -1),
                                        LayoutUnit(10)));
  EXPECT_EQ(LayoutUnit(120),
            ComputeBlockSizeForFragment(node, ConstructConstraintSpace(200, -1),
                                        LayoutUnit(120)));
}

TEST_F(LengthUtilsTestWithNode, ComputeReplacedSizeSvgNoScaling) {
  SetBodyInnerHTML(R"HTML(
<style>
svg {
  width: 100%;
  margin-left: 9223372036854775807in;
}
span {
  display: inline-flex;
}
</style>
<span><svg></svg></span>)HTML");
  // Pass if no DCHECK failures in BlockNode::FinishLayout().
}

TEST_F(LengthUtilsTest, TestMargins) {
  ComputedStyleBuilder builder(*initial_style_);
  builder.SetMarginTop(Length::Percent(10));
  builder.SetMarginRight(Length::Fixed(52));
  builder.SetMarginBottom(Length::Auto());
  builder.SetMarginLeft(Length::Percent(11));
  const ComputedStyle* style = builder.TakeStyle();

  ConstraintSpace constraint_space = ConstructConstraintSpace(200, 300);

  PhysicalBoxStrut margins = ComputePhysicalMargins(constraint_space, *style);

  EXPECT_EQ(LayoutUnit(20), margins.top);
  EXPECT_EQ(LayoutUnit(52), margins.right);
  EXPECT_EQ(LayoutUnit(), margins.bottom);
  EXPECT_EQ(LayoutUnit(22), margins.left);
}

TEST_F(LengthUtilsTest, TestBorders) {
  ComputedStyleBuilder builder(*initial_style_);
  builder.SetBorderTopWidth(1);
  builder.SetBorderRightWidth(2);
  builder.SetBorderBottomWidth(3);
  builder.SetBorderLeftWidth(4);
  builder.SetBorderTopStyle(EBorderStyle::kSolid);
  builder.SetBorderRightStyle(EBorderStyle::kSolid);
  builder.SetBorderBottomStyle(EBorderStyle::kSolid);
  builder.SetBorderLeftStyle(EBorderStyle::kSolid);
  builder.SetWritingMode(WritingMode::kVerticalLr);
  const ComputedStyle* style = builder.TakeStyle();

  BoxStrut borders = ComputeBordersForTest(*style);

  EXPECT_EQ(LayoutUnit(4), borders.block_start);
  EXPECT_EQ(LayoutUnit(3), borders.inline_end);
  EXPECT_EQ(LayoutUnit(2), borders.block_end);
  EXPECT_EQ(LayoutUnit(1), borders.inline_start);
}

TEST_F(LengthUtilsTest, TestPadding) {
  ComputedStyleBuilder builder(*initial_style_);
  builder.SetPaddingTop(Length::Percent(10));
  builder.SetPaddingRight(Length::Fixed(52));
  builder.SetPaddingBottom(Length::Auto());
  builder.SetPaddingLeft(Length::Percent(11));
  builder.SetWritingMode(WritingMode::kVerticalRl);
  const ComputedStyle* style = builder.TakeStyle();

  ConstraintSpace constraint_space = ConstructConstraintSpace(
      200, 300, false, false, WritingMode::kVerticalRl);

  BoxStrut padding = ComputePadding(constraint_space, *style);

  EXPECT_EQ(LayoutUnit(52), padding.block_start);
  EXPECT_EQ(LayoutUnit(), padding.inline_end);
  EXPECT_EQ(LayoutUnit(22), padding.block_end);
  EXPECT_EQ(LayoutUnit(20), padding.inline_start);
}

TEST_F(LengthUtilsTest, TestAutoMargins) {
  ComputedStyleBuilder builder(*initial_style_);
  builder.SetMarginRight(Length::Auto());
  builder.SetMarginLeft(Length::Auto());
  const ComputedStyle* style = builder.TakeStyle();

  LayoutUnit kInlineSize(150);
  LayoutUnit kAvailableInlineSize(200);

  BoxStrut margins;
  ResolveInlineAutoMargins(*style, *style, kAvailableInlineSize, kInlineSize,
                           &margins);

  EXPECT_EQ(LayoutUnit(), margins.block_start);
  EXPECT_EQ(LayoutUnit(), margins.block_end);
  EXPECT_EQ(LayoutUnit(25), margins.inline_start);
  EXPECT_EQ(LayoutUnit(25), margins.inline_end);

  builder = ComputedStyleBuilder(*style);
  builder.SetMarginLeft(Length::Fixed(0));
  style = builder.TakeStyle();
  margins = BoxStrut();
  ResolveInlineAutoMargins(*style, *style, kAvailableInlineSize, kInlineSize,
                           &margins);
  EXPECT_EQ(LayoutUnit(0), margins.inline_start);
  EXPECT_EQ(LayoutUnit(50), margins.inline_end);

  builder = ComputedStyleBuilder(*style);
  builder.SetMarginLeft(Length::Auto());
  builder.SetMarginRight(Length::Fixed(0));
  style = builder.TakeStyle();
  margins = BoxStrut();
  ResolveInlineAutoMargins(*style, *style, kAvailableInlineSize, kInlineSize,
                           &margins);
  EXPECT_EQ(LayoutUnit(50), margins.inline_start);
  EXPECT_EQ(LayoutUnit(0), margins.inline_end);

  // Test that we don't end up with negative "auto" margins when the box is too
  // big.
  builder = ComputedStyleBuilder(*style);
  builder.SetMarginLeft(Length::Auto());
  builder.SetMarginRight(Length::Fixed(5000));
  style = builder.TakeStyle();
  margins = BoxStrut();
  margins.inline_end = LayoutUnit(5000);
  ResolveInlineAutoMargins(*style, *style, kAvailableInlineSize, kInlineSize,
                           &margins);
  EXPECT_EQ(LayoutUnit(0), margins.inline_start);
  EXPECT_EQ(LayoutUnit(5000), margins.inline_end);
}

// Simple wrappers that don't use LayoutUnit(). Their only purpose is to make
// the tests below humanly readable (to make the expectation expressions fit on
// one line each). Passing 0 for column width or column count means "auto".
int GetUsedColumnWidth(int computed_column_count,
                       int computed_column_width,
                       int used_column_gap,
                       int available_inline_size) {
  LayoutUnit column_width(computed_column_width);
  if (!computed_column_width)
    column_width = LayoutUnit(kIndefiniteSize);
  return ResolveUsedColumnInlineSize(computed_column_count, column_width,
                                     LayoutUnit(used_column_gap),
                                     LayoutUnit(available_inline_size))
      .ToInt();
}
int GetUsedColumnCount(int computed_column_count,
                       int computed_column_width,
                       int used_column_gap,
                       int available_inline_size) {
  LayoutUnit column_width(computed_column_width);
  if (!computed_column_width)
    column_width = LayoutUnit(kIndefiniteSize);
  return ResolveUsedColumnCount(computed_column_count, column_width,
                                LayoutUnit(used_column_gap),
                                LayoutUnit(available_inline_size));
}

TEST_F(LengthUtilsTest, TestColumnWidthAndCount) {
  EXPECT_EQ(100, GetUsedColumnWidth(0, 100, 0, 300));
  EXPECT_EQ(3, GetUsedColumnCount(0, 100, 0, 300));
  EXPECT_EQ(150, GetUsedColumnWidth(0, 101, 0, 300));
  EXPECT_EQ(2, GetUsedColumnCount(0, 101, 0, 300));
  EXPECT_EQ(300, GetUsedColumnWidth(0, 151, 0, 300));
  EXPECT_EQ(1, GetUsedColumnCount(0, 151, 0, 300));
  EXPECT_EQ(300, GetUsedColumnWidth(0, 1000, 0, 300));
  EXPECT_EQ(1, GetUsedColumnCount(0, 1000, 0, 300));

  EXPECT_EQ(100, GetUsedColumnWidth(0, 100, 10, 320));
  EXPECT_EQ(3, GetUsedColumnCount(0, 100, 10, 320));
  EXPECT_EQ(150, GetUsedColumnWidth(0, 101, 10, 310));
  EXPECT_EQ(2, GetUsedColumnCount(0, 101, 10, 310));
  EXPECT_EQ(300, GetUsedColumnWidth(0, 151, 10, 300));
  EXPECT_EQ(1, GetUsedColumnCount(0, 151, 10, 300));
  EXPECT_EQ(300, GetUsedColumnWidth(0, 1000, 10, 300));
  EXPECT_EQ(1, GetUsedColumnCount(0, 1000, 10, 300));

  EXPECT_EQ(125, GetUsedColumnWidth(4, 0, 0, 500));
  EXPECT_EQ(4, GetUsedColumnCount(4, 0, 0, 500));
  EXPECT_EQ(125, GetUsedColumnWidth(4, 100, 0, 500));
  EXPECT_EQ(4, GetUsedColumnCount(4, 100, 0, 500));
  EXPECT_EQ(100, GetUsedColumnWidth(6, 100, 0, 500));
  EXPECT_EQ(5, GetUsedColumnCount(6, 100, 0, 500));
  EXPECT_EQ(100, GetUsedColumnWidth(0, 100, 0, 500));
  EXPECT_EQ(5, GetUsedColumnCount(0, 100, 0, 500));

  EXPECT_EQ(125, GetUsedColumnWidth(4, 0, 10, 530));
  EXPECT_EQ(4, GetUsedColumnCount(4, 0, 10, 530));
  EXPECT_EQ(125, GetUsedColumnWidth(4, 100, 10, 530));
  EXPECT_EQ(4, GetUsedColumnCount(4, 100, 10, 530));
  EXPECT_EQ(100, GetUsedColumnWidth(6, 100, 10, 540));
  EXPECT_EQ(5, GetUsedColumnCount(6, 100, 10, 540));
  EXPECT_EQ(100, GetUsedColumnWidth(0, 100, 10, 540));
  EXPECT_EQ(5, GetUsedColumnCount(0, 100, 10, 540));

  EXPECT_EQ(0, GetUsedColumnWidth(3, 0, 10, 10));
  EXPECT_EQ(3, GetUsedColumnCount(3, 0, 10, 10));
}

LayoutUnit ComputeInlineSize(LogicalSize aspect_ratio, LayoutUnit block_size) {
  return InlineSizeFromAspectRatio(BoxStrut(), aspect_ratio,
                                   EBoxSizing::kBorderBox, block_size);
}
TEST_F(LengthUtilsTest, AspectRatio) {
  EXPECT_EQ(LayoutUnit(8000),
            ComputeInlineSize(LogicalSize(8000, 8000), LayoutUnit(8000)));
  EXPECT_EQ(LayoutUnit(1),
            ComputeInlineSize(LogicalSize(1, 10000), LayoutUnit(10000)));
  EXPECT_EQ(LayoutUnit(4),
            ComputeInlineSize(LogicalSize(1, 1000000), LayoutUnit(4000000)));
  EXPECT_EQ(LayoutUnit(0),
            ComputeInlineSize(LogicalSize(3, 5000000), LayoutUnit(5)));
  // The literals are 8 million, 20 million, 10 million, 4 million.
  EXPECT_EQ(
      LayoutUnit(8000000),
      ComputeInlineSize(LogicalSize(20000000, 10000000), LayoutUnit(4000000)));
  // If you specify an aspect ratio of 10000:1 with a large block size,
  // LayoutUnit saturates.
  EXPECT_EQ(LayoutUnit::Max(),
            ComputeInlineSize(LogicalSize(10000, 1), LayoutUnit(10000)));
}

}  // namespace
}  // namespace blink
