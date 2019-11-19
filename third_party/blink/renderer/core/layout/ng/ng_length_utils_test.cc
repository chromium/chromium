// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {
namespace {

static NGConstraintSpace ConstructConstraintSpace(
    int inline_size,
    int block_size,
    bool fixed_inline = false,
    bool fixed_block = false,
    WritingMode writing_mode = WritingMode::kHorizontalTb) {
  LogicalSize size = {LayoutUnit(inline_size), LayoutUnit(block_size)};

  NGConstraintSpaceBuilder builder(writing_mode, writing_mode,
                                   /* is_new_fc */ false);
  builder.SetAvailableSize(size);
  builder.SetPercentageResolutionSize(size);
  builder.SetIsFixedInlineSize(fixed_inline);
  builder.SetIsFixedBlockSize(fixed_block);
  return builder.ToConstraintSpace();
}

class NGLengthUtilsTest : public testing::Test {
 protected:
  void SetUp() override { style_ = ComputedStyle::Create(); }

  LayoutUnit ResolveMainInlineLength(
      const Length& length,
      const base::Optional<MinMaxSize>& sizes = base::nullopt) {
    NGConstraintSpace constraint_space = ConstructConstraintSpace(200, 300);
    NGBoxStrut border_padding = ComputeBordersForTest(*style_) +
                                ComputePadding(constraint_space, *style_);

    return ::blink::ResolveMainInlineLength(constraint_space, *style_,
                                            border_padding, sizes, length);
  }

  LayoutUnit ResolveMinInlineLength(
      const Length& length,
      LengthResolvePhase phase = LengthResolvePhase::kLayout,
      const base::Optional<MinMaxSize>& sizes = base::nullopt) {
    NGConstraintSpace constraint_space = ConstructConstraintSpace(200, 300);
    NGBoxStrut border_padding = ComputeBordersForTest(*style_) +
                                ComputePadding(constraint_space, *style_);

    return ::blink::ResolveMinInlineLength(
        constraint_space, *style_, border_padding, sizes, length, phase);
  }

  LayoutUnit ResolveMaxInlineLength(
      const Length& length,
      LengthResolvePhase phase = LengthResolvePhase::kLayout,
      const base::Optional<MinMaxSize>& sizes = base::nullopt) {
    NGConstraintSpace constraint_space = ConstructConstraintSpace(200, 300);
    NGBoxStrut border_padding = ComputeBordersForTest(*style_) +
                                ComputePadding(constraint_space, *style_);

    return ::blink::ResolveMaxInlineLength(
        constraint_space, *style_, border_padding, sizes, length, phase);
  }

  LayoutUnit ResolveMainBlockLength(const Length& length,
                                    LayoutUnit content_size = LayoutUnit()) {
    NGConstraintSpace constraint_space = ConstructConstraintSpace(200, 300);
    NGBoxStrut border_padding = ComputeBordersForTest(*style_) +
                                ComputePadding(constraint_space, *style_);

    return ::blink::ResolveMainBlockLength(constraint_space, *style_,
                                           border_padding, length, content_size,
                                           LengthResolvePhase::kLayout);
  }

  scoped_refptr<ComputedStyle> style_;
};

class NGLengthUtilsTestWithNode : public NGLayoutTest {
 public:
  void SetUp() override {
    NGLayoutTest::SetUp();
    style_ = ComputedStyle::Create();
  }

  LayoutUnit ComputeInlineSizeForFragment(
      NGConstraintSpace constraint_space = ConstructConstraintSpace(200, 300),
      const MinMaxSize& sizes = MinMaxSize()) {
    LayoutBox* body = ToLayoutBox(GetDocument().body()->GetLayoutObject());
    body->SetStyle(style_);
    body->SetPreferredLogicalWidthsDirty();
    NGBlockNode node(body);

    NGBoxStrut border_padding = ComputeBordersForTest(*style_) +
                                ComputePadding(constraint_space, *style_);
    return ::blink::ComputeInlineSizeForFragment(constraint_space, node,
                                                 border_padding, &sizes);
  }

  LayoutUnit ComputeBlockSizeForFragment(
      NGConstraintSpace constraint_space = ConstructConstraintSpace(200, 300),
      LayoutUnit content_size = LayoutUnit()) {
    LayoutBox* body = ToLayoutBox(GetDocument().body()->GetLayoutObject());
    body->SetStyle(style_);
    body->SetPreferredLogicalWidthsDirty();

    NGBoxStrut border_padding = ComputeBordersForTest(*style_) +
                                ComputePadding(constraint_space, *style_);
    return ::blink::ComputeBlockSizeForFragment(constraint_space, *style_,
                                                border_padding, content_size);
  }

  scoped_refptr<ComputedStyle> style_;
};

TEST_F(NGLengthUtilsTest, testResolveInlineLength) {
  EXPECT_EQ(LayoutUnit(60), ResolveMainInlineLength(Length::Percent(30)));
  EXPECT_EQ(LayoutUnit(150), ResolveMainInlineLength(Length::Fixed(150)));
  EXPECT_EQ(LayoutUnit(0), ResolveMinInlineLength(
                               Length::Auto(), LengthResolvePhase::kIntrinsic));
  EXPECT_EQ(LayoutUnit(200), ResolveMainInlineLength(Length::Auto()));
  EXPECT_EQ(LayoutUnit(200), ResolveMainInlineLength(Length::FillAvailable()));

  EXPECT_EQ(LayoutUnit::Max(),
            ResolveMaxInlineLength(Length::Percent(30),
                                   LengthResolvePhase::kIntrinsic));
  EXPECT_EQ(LayoutUnit::Max(),
            ResolveMaxInlineLength(Length::FillAvailable(),
                                   LengthResolvePhase::kIntrinsic));
  MinMaxSize sizes;
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

TEST_F(NGLengthUtilsTest, testResolveBlockLength) {
  EXPECT_EQ(LayoutUnit(90), ResolveMainBlockLength(Length::Percent(30)));
  EXPECT_EQ(LayoutUnit(150), ResolveMainBlockLength(Length::Fixed(150)));
  EXPECT_EQ(LayoutUnit(0), ResolveMainBlockLength(Length::Auto()));
  EXPECT_EQ(LayoutUnit(300), ResolveMainBlockLength(Length::FillAvailable()));

  EXPECT_EQ(LayoutUnit(300), ResolveMainBlockLength(Length::FillAvailable()));
}

TEST_F(NGLengthUtilsTest, testComputeContentContribution) {
  MinMaxSize sizes;
  sizes.min_size = LayoutUnit(30);
  sizes.max_size = LayoutUnit(40);

  NGBoxStrut border_padding;

  MinMaxSize expected = sizes;
  style_->SetLogicalWidth(Length::Percent(30));
  EXPECT_EQ(expected,
            ComputeMinAndMaxContentContribution(
                style_->GetWritingMode(), *style_, border_padding, sizes));

  style_->SetLogicalWidth(Length::FillAvailable());
  EXPECT_EQ(expected,
            ComputeMinAndMaxContentContribution(
                style_->GetWritingMode(), *style_, border_padding, sizes));

  expected = MinMaxSize{LayoutUnit(150), LayoutUnit(150)};
  style_->SetLogicalWidth(Length::Fixed(150));
  EXPECT_EQ(expected,
            ComputeMinAndMaxContentContribution(
                style_->GetWritingMode(), *style_, border_padding, sizes));

  expected = sizes;
  style_->SetLogicalWidth(Length::Auto());
  EXPECT_EQ(expected,
            ComputeMinAndMaxContentContribution(
                style_->GetWritingMode(), *style_, border_padding, sizes));

  expected = MinMaxSize{LayoutUnit(430), LayoutUnit(440)};
  style_->SetPaddingLeft(Length::Fixed(400));
  auto sizes_padding400 = sizes;
  sizes_padding400 += LayoutUnit(400);
  NGBoxStrut border_padding400 = {LayoutUnit(400), LayoutUnit(), LayoutUnit(),
                                  LayoutUnit()};
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(
                          style_->GetWritingMode(), *style_, border_padding400,
                          sizes_padding400));

  expected = MinMaxSize{LayoutUnit(30), LayoutUnit(40)};
  style_->SetPaddingLeft(Length::Fixed(0));
  style_->SetLogicalWidth(Length(CalculationValue::Create(
      PixelsAndPercent(100, -10), kValueRangeNonNegative)));
  EXPECT_EQ(expected,
            ComputeMinAndMaxContentContribution(
                style_->GetWritingMode(), *style_, border_padding, sizes));

  expected = MinMaxSize{LayoutUnit(30), LayoutUnit(35)};
  style_->SetLogicalWidth(Length::Auto());
  style_->SetMaxWidth(Length::Fixed(35));
  EXPECT_EQ(expected,
            ComputeMinAndMaxContentContribution(
                style_->GetWritingMode(), *style_, border_padding, sizes));

  expected = MinMaxSize{LayoutUnit(80), LayoutUnit(80)};
  style_->SetLogicalWidth(Length::Fixed(50));
  style_->SetMinWidth(Length::Fixed(80));
  EXPECT_EQ(expected,
            ComputeMinAndMaxContentContribution(
                style_->GetWritingMode(), *style_, border_padding, sizes));

  expected = MinMaxSize{LayoutUnit(150), LayoutUnit(150)};
  style_ = ComputedStyle::Create();
  style_->SetLogicalWidth(Length::Fixed(100));
  style_->SetPaddingLeft(Length::Fixed(50));
  auto sizes_padding50 = sizes;
  NGBoxStrut border_padding50 = {LayoutUnit(50), LayoutUnit(), LayoutUnit(),
                                 LayoutUnit()};
  sizes_padding50 += LayoutUnit(50);
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(
                          style_->GetWritingMode(), *style_, border_padding50,
                          sizes_padding50));

  expected = MinMaxSize{LayoutUnit(100), LayoutUnit(100)};
  style_->SetBoxSizing(EBoxSizing::kBorderBox);
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(
                          style_->GetWritingMode(), *style_, border_padding50,
                          sizes_padding50));

  // Content size should never be below zero, even with box-sizing: border-box
  // and a large padding...
  expected = MinMaxSize{LayoutUnit(400), LayoutUnit(400)};
  style_->SetPaddingLeft(Length::Fixed(400));
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(
                          style_->GetWritingMode(), *style_, border_padding400,
                          sizes_padding400));

  expected.min_size = expected.max_size = sizes.min_size + LayoutUnit(400);
  style_->SetLogicalWidth(Length::MinContent());
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(
                          style_->GetWritingMode(), *style_, border_padding400,
                          sizes_padding400));
  style_->SetLogicalWidth(Length::Fixed(100));
  style_->SetMaxWidth(Length::MaxContent());
  // Due to padding and box-sizing, width computes to 400px and max-width to
  // 440px, so the result is 400.
  expected = MinMaxSize{LayoutUnit(400), LayoutUnit(400)};
  EXPECT_EQ(expected, ComputeMinAndMaxContentContribution(
                          style_->GetWritingMode(), *style_, border_padding400,
                          sizes_padding400));
  expected = MinMaxSize{LayoutUnit(40), LayoutUnit(40)};
  style_->SetPaddingLeft(Length::Fixed(0));
  EXPECT_EQ(expected,
            ComputeMinAndMaxContentContribution(
                style_->GetWritingMode(), *style_, border_padding, sizes));
}

TEST_F(NGLengthUtilsTestWithNode, testComputeInlineSizeForFragment) {
  MinMaxSize sizes;
  sizes.min_size = LayoutUnit(30);
  sizes.max_size = LayoutUnit(40);

  style_->SetLogicalWidth(Length::Percent(30));
  EXPECT_EQ(LayoutUnit(60), ComputeInlineSizeForFragment());

  style_->SetLogicalWidth(Length::Fixed(150));
  EXPECT_EQ(LayoutUnit(150), ComputeInlineSizeForFragment());

  style_->SetLogicalWidth(Length::Auto());
  EXPECT_EQ(LayoutUnit(200), ComputeInlineSizeForFragment());

  style_->SetLogicalWidth(Length::FillAvailable());
  EXPECT_EQ(LayoutUnit(200), ComputeInlineSizeForFragment());

  style_->SetLogicalWidth(Length(CalculationValue::Create(
      PixelsAndPercent(100, -10), kValueRangeNonNegative)));
  EXPECT_EQ(LayoutUnit(80), ComputeInlineSizeForFragment());

  NGConstraintSpace constraint_space =
      ConstructConstraintSpace(120, 120, true, true);
  style_->SetLogicalWidth(Length::Fixed(150));
  EXPECT_EQ(LayoutUnit(120), ComputeInlineSizeForFragment(constraint_space));

  style_->SetLogicalWidth(Length::Fixed(200));
  style_->SetMaxWidth(Length::Percent(80));
  EXPECT_EQ(LayoutUnit(160), ComputeInlineSizeForFragment());

  style_->SetLogicalWidth(Length::Fixed(100));
  style_->SetMinWidth(Length::Percent(80));
  EXPECT_EQ(LayoutUnit(160), ComputeInlineSizeForFragment());

  style_ = ComputedStyle::Create();
  style_->SetMarginRight(Length::Fixed(20));
  EXPECT_EQ(LayoutUnit(180), ComputeInlineSizeForFragment());

  style_->SetLogicalWidth(Length::Fixed(100));
  style_->SetPaddingLeft(Length::Fixed(50));
  EXPECT_EQ(LayoutUnit(150), ComputeInlineSizeForFragment());

  style_->SetBoxSizing(EBoxSizing::kBorderBox);
  EXPECT_EQ(LayoutUnit(100), ComputeInlineSizeForFragment());

  // Content size should never be below zero, even with box-sizing: border-box
  // and a large padding...
  style_->SetPaddingLeft(Length::Fixed(400));
  EXPECT_EQ(LayoutUnit(400), ComputeInlineSizeForFragment());
  auto sizes_padding400 = sizes;
  sizes_padding400 += LayoutUnit(400);

  // ...and the same goes for fill-available with a large padding.
  style_->SetLogicalWidth(Length::FillAvailable());
  EXPECT_EQ(LayoutUnit(400), ComputeInlineSizeForFragment());

  constraint_space = ConstructConstraintSpace(120, 140);
  style_->SetLogicalWidth(Length::MinContent());
  EXPECT_EQ(LayoutUnit(430),
            ComputeInlineSizeForFragment(constraint_space, sizes_padding400));
  style_->SetLogicalWidth(Length::Fixed(100));
  style_->SetMaxWidth(Length::MaxContent());
  // Due to padding and box-sizing, width computes to 400px and max-width to
  // 440px, so the result is 400.
  EXPECT_EQ(LayoutUnit(400),
            ComputeInlineSizeForFragment(constraint_space, sizes_padding400));
  style_->SetPaddingLeft(Length::Fixed(0));
  EXPECT_EQ(LayoutUnit(40),
            ComputeInlineSizeForFragment(constraint_space, sizes));
}

TEST_F(NGLengthUtilsTestWithNode, testComputeBlockSizeForFragment) {
  style_->SetLogicalHeight(Length::Percent(30));
  EXPECT_EQ(LayoutUnit(90), ComputeBlockSizeForFragment());

  style_->SetLogicalHeight(Length::Fixed(150));
  EXPECT_EQ(LayoutUnit(150), ComputeBlockSizeForFragment());

  style_->SetLogicalHeight(Length::Auto());
  EXPECT_EQ(LayoutUnit(0), ComputeBlockSizeForFragment());

  NGConstraintSpace constraint_space = ConstructConstraintSpace(200, 300);
  style_->SetLogicalHeight(Length::Auto());
  EXPECT_EQ(LayoutUnit(120),
            ComputeBlockSizeForFragment(constraint_space, LayoutUnit(120)));

  style_->SetLogicalHeight(Length::FillAvailable());
  EXPECT_EQ(LayoutUnit(300), ComputeBlockSizeForFragment());

  style_->SetLogicalHeight(Length(CalculationValue::Create(
      PixelsAndPercent(100, -10), kValueRangeNonNegative)));
  EXPECT_EQ(LayoutUnit(70), ComputeBlockSizeForFragment());

  constraint_space = ConstructConstraintSpace(200, 200, true, true);
  style_->SetLogicalHeight(Length::Fixed(150));
  EXPECT_EQ(LayoutUnit(200), ComputeBlockSizeForFragment(constraint_space));

  style_->SetLogicalHeight(Length::Fixed(300));
  style_->SetMaxHeight(Length::Percent(80));
  EXPECT_EQ(LayoutUnit(240), ComputeBlockSizeForFragment());

  style_->SetLogicalHeight(Length::Fixed(100));
  style_->SetMinHeight(Length::Percent(80));
  EXPECT_EQ(LayoutUnit(240), ComputeBlockSizeForFragment());

  style_ = ComputedStyle::Create();
  style_->SetMarginTop(Length::Fixed(20));
  style_->SetLogicalHeight(Length::FillAvailable());
  EXPECT_EQ(LayoutUnit(280), ComputeBlockSizeForFragment());

  style_->SetLogicalHeight(Length::Fixed(100));
  style_->SetPaddingBottom(Length::Fixed(50));
  EXPECT_EQ(LayoutUnit(150), ComputeBlockSizeForFragment());

  style_->SetBoxSizing(EBoxSizing::kBorderBox);
  EXPECT_EQ(LayoutUnit(100), ComputeBlockSizeForFragment());

  // Content size should never be below zero, even with box-sizing: border-box
  // and a large padding...
  style_->SetPaddingBottom(Length::Fixed(400));
  EXPECT_EQ(LayoutUnit(400), ComputeBlockSizeForFragment());

  // ...and the same goes for fill-available with a large padding.
  style_->SetLogicalHeight(Length::FillAvailable());
  EXPECT_EQ(LayoutUnit(400), ComputeBlockSizeForFragment());

  // TODO(layout-ng): test {min,max}-content on max-height.
}

TEST_F(NGLengthUtilsTestWithNode, testIndefinitePercentages) {
  style_->SetMinHeight(Length::Fixed(20));
  style_->SetHeight(Length::Percent(20));

  EXPECT_EQ(kIndefiniteSize,
            ComputeBlockSizeForFragment(ConstructConstraintSpace(200, -1),
                                        LayoutUnit(-1)));
  EXPECT_EQ(LayoutUnit(20),
            ComputeBlockSizeForFragment(ConstructConstraintSpace(200, -1),
                                        LayoutUnit(10)));
  EXPECT_EQ(LayoutUnit(120),
            ComputeBlockSizeForFragment(ConstructConstraintSpace(200, -1),
                                        LayoutUnit(120)));
}

TEST_F(NGLengthUtilsTest, testMargins) {
  style_->SetMarginTop(Length::Percent(10));
  style_->SetMarginRight(Length::Fixed(52));
  style_->SetMarginBottom(Length::Auto());
  style_->SetMarginLeft(Length::Percent(11));

  NGConstraintSpace constraint_space = ConstructConstraintSpace(200, 300);

  NGPhysicalBoxStrut margins =
      ComputePhysicalMargins(constraint_space, *style_);

  EXPECT_EQ(LayoutUnit(20), margins.top);
  EXPECT_EQ(LayoutUnit(52), margins.right);
  EXPECT_EQ(LayoutUnit(), margins.bottom);
  EXPECT_EQ(LayoutUnit(22), margins.left);
}

TEST_F(NGLengthUtilsTest, testBorders) {
  style_->SetBorderTopWidth(1);
  style_->SetBorderRightWidth(2);
  style_->SetBorderBottomWidth(3);
  style_->SetBorderLeftWidth(4);
  style_->SetBorderTopStyle(EBorderStyle::kSolid);
  style_->SetBorderRightStyle(EBorderStyle::kSolid);
  style_->SetBorderBottomStyle(EBorderStyle::kSolid);
  style_->SetBorderLeftStyle(EBorderStyle::kSolid);
  style_->SetWritingMode(WritingMode::kVerticalLr);

  NGBoxStrut borders = ComputeBordersForTest(*style_);

  EXPECT_EQ(LayoutUnit(4), borders.block_start);
  EXPECT_EQ(LayoutUnit(3), borders.inline_end);
  EXPECT_EQ(LayoutUnit(2), borders.block_end);
  EXPECT_EQ(LayoutUnit(1), borders.inline_start);
}

TEST_F(NGLengthUtilsTest, testPadding) {
  style_->SetPaddingTop(Length::Percent(10));
  style_->SetPaddingRight(Length::Fixed(52));
  style_->SetPaddingBottom(Length::Auto());
  style_->SetPaddingLeft(Length::Percent(11));
  style_->SetWritingMode(WritingMode::kVerticalRl);

  NGConstraintSpace constraint_space = ConstructConstraintSpace(
      200, 300, false, false, WritingMode::kVerticalRl);

  NGBoxStrut padding = ComputePadding(constraint_space, *style_);

  EXPECT_EQ(LayoutUnit(52), padding.block_start);
  EXPECT_EQ(LayoutUnit(), padding.inline_end);
  EXPECT_EQ(LayoutUnit(22), padding.block_end);
  EXPECT_EQ(LayoutUnit(20), padding.inline_start);
}

TEST_F(NGLengthUtilsTest, testAutoMargins) {
  style_->SetMarginRight(Length::Auto());
  style_->SetMarginLeft(Length::Auto());

  LayoutUnit kInlineSize(150);
  LayoutUnit kAvailableInlineSize(200);

  NGBoxStrut margins;
  ResolveInlineMargins(*style_, *style_, kAvailableInlineSize, kInlineSize,
                       &margins);

  EXPECT_EQ(LayoutUnit(), margins.block_start);
  EXPECT_EQ(LayoutUnit(), margins.block_end);
  EXPECT_EQ(LayoutUnit(25), margins.inline_start);
  EXPECT_EQ(LayoutUnit(25), margins.inline_end);

  style_->SetMarginLeft(Length::Fixed(0));
  margins = NGBoxStrut();
  ResolveInlineMargins(*style_, *style_, kAvailableInlineSize, kInlineSize,
                       &margins);
  EXPECT_EQ(LayoutUnit(0), margins.inline_start);
  EXPECT_EQ(LayoutUnit(50), margins.inline_end);

  style_->SetMarginLeft(Length::Auto());
  style_->SetMarginRight(Length::Fixed(0));
  margins = NGBoxStrut();
  ResolveInlineMargins(*style_, *style_, kAvailableInlineSize, kInlineSize,
                       &margins);
  EXPECT_EQ(LayoutUnit(50), margins.inline_start);
  EXPECT_EQ(LayoutUnit(0), margins.inline_end);

  // Test that we don't end up with negative "auto" margins when the box is too
  // big.
  style_->SetMarginLeft(Length::Auto());
  style_->SetMarginRight(Length::Fixed(5000));
  margins = NGBoxStrut();
  margins.inline_end = LayoutUnit(5000);
  ResolveInlineMargins(*style_, *style_, kAvailableInlineSize, kInlineSize,
                       &margins);
  EXPECT_EQ(LayoutUnit(0), margins.inline_start);
  EXPECT_EQ(LayoutUnit(50), margins.inline_end);
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

TEST_F(NGLengthUtilsTest, testColumnWidthAndCount) {
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

}  // namespace
}  // namespace blink
