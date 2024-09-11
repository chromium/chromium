// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/card_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_card_cell.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ui/base/device_form_factor.h"

namespace {

enum ItemType : NSInteger {
  kItemTypeSampleOne = kItemTypeEnumZero,
  kItemTypeSampleTwo,
};

}  // namespace

class CardViewControllerTest : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    CreateController();
    CheckController();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    CardViewController* view_controller = [[CardViewController alloc] init];
    [view_controller loadModel];
    return view_controller;
  }

  // Returns the header item at `section`.
  id GetHeaderItem(int section) {
    return [controller().tableViewModel headerForSectionIndex:section];
  }

  // Returns the type of the table view item at `item` in `section`.
  NSInteger GetTableViewItemType(int section, int item) {
    return base::apple::ObjCCastStrict<TableViewItem>(
               GetTableViewItem(section, item))
        .type;
  }
};

// Tests the following:
// 1. "No card items present" message is shown alongside the action items if
// there are no data items.
// 2. "No card items present" message is removed once there are card items to be
// shown in the view.
TEST_F(CardViewControllerTest, CheckNoDataItemsMessageRemoved) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // TODO(crbug.com/327838014): Fails on iPad.
    return;
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kIOSKeyboardAccessoryUpgrade);

  CardViewController* card_view_controller =
      base::apple::ObjCCastStrict<CardViewController>(controller());

  ManualFillActionItem* action_item =
      [[ManualFillActionItem alloc] initWithTitle:nil action:nil];
  action_item.type = kItemTypeSampleOne;

  // First, send no data items so that the "no card items to show" message is
  // displayed.
  [card_view_controller presentCards:@[]];
  [card_view_controller presentActions:@[ action_item ]];

  // Make sure that the table view content is as expected.
  EXPECT_EQ(NumberOfSections(), 2);
  EXPECT_TRUE(GetHeaderItem(/*section=*/0));
  EXPECT_EQ(NumberOfItemsInSection(0), 0);
  EXPECT_EQ(NumberOfItemsInSection(1), 1);
  EXPECT_EQ(GetTableViewItemType(/*section=*/1, /*item=*/0),
            kItemTypeSampleOne);

  // Add an card data item.
  ManualFillCardItem* cardItem =
      [[ManualFillCardItem alloc] initWithCreditCard:nil
                                     contentInjector:nil
                                  navigationDelegate:nil
                                         menuActions:@[]
                                           cellIndex:0
                         cellIndexAccessibilityLabel:nil
                              showAutofillFormButton:NO];
  [card_view_controller presentCards:@[ cardItem ]];
  // Override the type for the test.
  cardItem.type = kItemTypeSampleTwo;

  // Check that the "no card item present" message is removed.
  EXPECT_EQ(NumberOfSections(), 2);
  EXPECT_FALSE(GetHeaderItem(/*section=*/0));
  EXPECT_FALSE(GetHeaderItem(/*section=*/1));
  EXPECT_EQ(NumberOfItemsInSection(0), 1);
  EXPECT_EQ(NumberOfItemsInSection(1), 1);
  EXPECT_EQ(GetTableViewItemType(/*section=*/0, /*item=*/0),
            kItemTypeSampleTwo);
  EXPECT_EQ(GetTableViewItemType(/*section=*/1, /*item=*/0),
            kItemTypeSampleOne);
}
