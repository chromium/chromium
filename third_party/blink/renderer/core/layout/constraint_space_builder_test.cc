// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

using ConstraintSpaceBuilderTest = RenderingTest;

// Asserts that indefinite inline length becomes initial containing
// block width for horizontal-tb inside vertical document.
TEST(ConstraintSpaceBuilderTest, AvailableSizeFromHorizontalICB) {
  test::TaskEnvironment task_environment;
  PhysicalSize icb_size{kIndefiniteSize, LayoutUnit(51)};

  ConstraintSpaceBuilder horizontal_builder(
      WritingMode::kHorizontalTb,
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      /* is_new_fc */ true);
  LogicalSize fixed_size{LayoutUnit(100), LayoutUnit(200)};
  LogicalSize indefinite_size{kIndefiniteSize, kIndefiniteSize};

  horizontal_builder.SetOrthogonalFallbackInlineSize(icb_size.height);
  horizontal_builder.SetAvailableSize(fixed_size);
  horizontal_builder.SetPercentageResolutionSize(fixed_size);

  ConstraintSpaceBuilder vertical_builder(
      horizontal_builder.ToConstraintSpace(),
      {WritingMode::kVerticalLr, TextDirection::kLtr},
      /* is_new_fc */ true);

  vertical_builder.SetOrthogonalFallbackInlineSize(icb_size.height);
  vertical_builder.SetAvailableSize(indefinite_size);
  vertical_builder.SetPercentageResolutionSize(indefinite_size);

  ConstraintSpace space = vertical_builder.ToConstraintSpace();

  EXPECT_EQ(space.AvailableSize().inline_size, icb_size.height);
  EXPECT_EQ(space.PercentageResolutionInlineSize(), icb_size.height);
}

// Asserts that indefinite inline length becomes initial containing
// block height for vertical-lr inside horizontal document.
TEST(ConstraintSpaceBuilderTest, AvailableSizeFromVerticalICB) {
  test::TaskEnvironment task_environment;
  PhysicalSize icb_size{LayoutUnit(51), kIndefiniteSize};

  ConstraintSpaceBuilder horizontal_builder(
      WritingMode::kVerticalLr, {WritingMode::kVerticalLr, TextDirection::kLtr},
      /* is_new_fc */ true);
  LogicalSize fixed_size{LayoutUnit(100), LayoutUnit(200)};
  LogicalSize indefinite_size{kIndefiniteSize, kIndefiniteSize};

  horizontal_builder.SetOrthogonalFallbackInlineSize(icb_size.width);
  horizontal_builder.SetAvailableSize(fixed_size);
  horizontal_builder.SetPercentageResolutionSize(fixed_size);

  ConstraintSpaceBuilder vertical_builder(
      horizontal_builder.ToConstraintSpace(),
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      /* is_new_fc */ true);

  vertical_builder.SetOrthogonalFallbackInlineSize(icb_size.width);
  vertical_builder.SetAvailableSize(indefinite_size);
  vertical_builder.SetPercentageResolutionSize(indefinite_size);

  ConstraintSpace space = vertical_builder.ToConstraintSpace();

  EXPECT_EQ(space.AvailableSize().inline_size, icb_size.width);
  EXPECT_EQ(space.PercentageResolutionInlineSize(), icb_size.width);
}

}  // namespace

}  // namespace blink
