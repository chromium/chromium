// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_cell_utils.h"

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/chip_button.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_labeled_chip.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Button whose intrinsic content size mathes that of its frame. Used for
// testing purposes only.
@interface FixedSizeButton : UIButton
@end

@implementation FixedSizeButton

- (CGSize)intrinsicContentSize {
  return self.frame.size;
}

@end

namespace {

using ManualFillTestUtilsTest = PlatformTest;

// Creates a ManualFillCellView with the given `view` and `type`.
ManualFillCellView CreateManualFillCellView(
    UIView* view,
    ManualFillCellView::ElementType type) {
  return (struct ManualFillCellView){view, type};
}

// Creates a button and sizes it with the given `width`.
UIButton* CreateButtonOfWidth(CGFloat width) {
  return [[FixedSizeButton alloc] initWithFrame:CGRectMake(0, 0, width, 0)];
}

// Creates a layout guide and sizes it with the given `width`.
UILayoutGuide* CreateLayoutGuideOfWidth(CGFloat width) {
  UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, width, 0)];
  UILayoutGuide* layout_guide = [[UILayoutGuide alloc] init];
  [view addLayoutGuide:layout_guide];
  AddSameConstraintsToSides(layout_guide, view,
                            LayoutSides::kTop | LayoutSides::kBottom |
                                LayoutSides::kLeading | LayoutSides::kTrailing);

  [view setNeedsLayout];
  [view layoutIfNeeded];

  return layout_guide;
}

// Tests that `AddChipGroupsToVerticalLeadViews` adds the expected
// ManualFillCellViews to the `vertical_lead_views` array.
TEST_F(ManualFillTestUtilsTest, TestAddChipGroupsToVerticalLeadViews) {
  // Create group with unlabeled chip buttons.
  ChipButton* chip_button_1 = [[ChipButton alloc] initWithFrame:CGRectZero];
  ChipButton* chip_button_2 = [[ChipButton alloc] initWithFrame:CGRectZero];
  NSArray<UIView*>* group_1 = @[ chip_button_1, chip_button_2 ];

  // Create empty group.
  NSArray<UIView*>* group_2 = @[];

  // Create group with only one unlabeled chip button.
  ChipButton* chip_button_3 = [[ChipButton alloc] initWithFrame:CGRectZero];
  NSArray<UIView*>* group_3 = @[ chip_button_3 ];

  // Create group with labeled chip buttons.
  ManualFillLabeledChip* chip_button_4 =
      [[ManualFillLabeledChip alloc] initSingleChipWithTarget:0 selector:0];
  ManualFillLabeledChip* chip_button_5 =
      [[ManualFillLabeledChip alloc] initSingleChipWithTarget:0 selector:0];
  ManualFillLabeledChip* chip_button_6 =
      [[ManualFillLabeledChip alloc] initSingleChipWithTarget:0 selector:0];
  NSArray<UIView*>* group_4 = @[ chip_button_4, chip_button_5, chip_button_6 ];

  std::vector<ManualFillCellView> vertical_lead_views;
  AddChipGroupsToVerticalLeadViews(@[ group_1, group_2, group_3, group_4 ],
                                   vertical_lead_views);

  std::vector<ManualFillCellView> expected_vertical_lead_views = {
      CreateManualFillCellView(
          chip_button_1,
          ManualFillCellView::ElementType::kFirstChipButtonOfGroup),
      CreateManualFillCellView(
          chip_button_2, ManualFillCellView::ElementType::kOtherChipButton),
      CreateManualFillCellView(
          chip_button_3,
          ManualFillCellView::ElementType::kFirstChipButtonOfGroup),
      CreateManualFillCellView(
          chip_button_4,
          ManualFillCellView::ElementType::kFirstChipButtonOfGroup),
      CreateManualFillCellView(
          chip_button_5, ManualFillCellView::ElementType::kLabeledChipButton),
      CreateManualFillCellView(
          chip_button_6, ManualFillCellView::ElementType::kLabeledChipButton)};

  EXPECT_THAT(vertical_lead_views,
              ::testing::ElementsAreArray(expected_vertical_lead_views));
}

// Tests that `LayViewsHorizontallyWhenPossible` lays as many views as possible
// horizontally, and starts a new row of views whenever there's not enough
// space in the current row.
TEST_F(ManualFillTestUtilsTest, TestLayViewsHorizontallyWhenPossible) {
  CGFloat max_width = 100;

  // Create the layout guide. Used in `LayViewsHorizontallyWhenPossible` to, in
  // part, determine the max width per row of views.
  UILayoutGuide* guide = CreateLayoutGuideOfWidth(max_width);

  // Generate views.
  std::vector<CGFloat> view_widths = {30, 20, 19, 39, 10, 120, 30};
  NSMutableArray<UIView*>* views = [[NSMutableArray alloc] init];
  for (CGFloat width : view_widths) {
    [views addObject:CreateButtonOfWidth(width)];
  }

  // Will contain every first view of a row.
  NSMutableArray<UIView*>* vertical_lead_views = [[NSMutableArray alloc] init];

  LayViewsHorizontallyWhenPossible(
      views, guide, /*constraints=*/[[NSMutableArray alloc] init],
      vertical_lead_views);

  // Generate `expected_vertical_lead_views`.
  NSMutableArray<UIView*>* expected_vertical_lead_views =
      [[NSMutableArray alloc] init];
  [expected_vertical_lead_views addObject:views[0]];

  // The width in the current row that's occupied.
  CGFloat row_width = 0;
  for (uint i = 0; i < views.count; i++) {
    row_width += view_widths[i];
    // Check if a new row should be started.
    if (row_width > max_width) {
      [expected_vertical_lead_views addObject:views[i]];
      row_width = view_widths[i];
    }
    row_width += GetHorizontalSpacingBetweenChips();
  }

  EXPECT_NSEQ(vertical_lead_views, expected_vertical_lead_views);
}

// Tests that `LayViewsHorizontallyWhenPossible` leaves `constraints` and
// `vertical_lead_views` empty when called with an empty array of views.
TEST_F(ManualFillTestUtilsTest,
       TestLayViewsHorizontallyWhenPossibleWithoutViews) {
  UILayoutGuide* guide = CreateLayoutGuideOfWidth(100);
  NSMutableArray<NSLayoutConstraint*>* constraints =
      [[NSMutableArray alloc] init];
  NSMutableArray<UIView*>* vertical_lead_views = [[NSMutableArray alloc] init];

  LayViewsHorizontallyWhenPossible(@[], guide, constraints,
                                   vertical_lead_views);

  EXPECT_EQ(constraints.count, 0u);
  EXPECT_EQ(vertical_lead_views.count, 0u);
}

}  // namespace
