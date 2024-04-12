// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/chip_button.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_labeled_chip.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

namespace {

using ManualFillTestUtilsTest = PlatformTest;

ManualFillCellView CreateManualFillCellView(
    UIView* view,
    ManualFillCellView::ElementType type) {
  return (struct ManualFillCellView){view, type};
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

}  // namespace
