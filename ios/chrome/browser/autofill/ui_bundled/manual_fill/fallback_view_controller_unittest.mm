// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/fallback_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/with_feature_override.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_text_cell.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"

namespace {

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSampleOne = kItemTypeEnumZero,
  ItemTypeSampleTwo,
  ItemTypeSampleThree,
  ItemTypeSampleFour
};

}  // namespace

class FallbackViewControllerTest : public LegacyChromeTableViewControllerTest,
                                   public base::test::WithFeatureOverride {
 public:
  FallbackViewControllerTest()
      : base::test::WithFeatureOverride(kIOSKeyboardAccessoryUpgrade) {}

 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    CreateController();
    CheckController();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    FallbackViewController* view_controller =
        [[FallbackViewController alloc] init];
    [view_controller loadModel];
    return view_controller;
  }

  FallbackViewController* GetFallbackViewController() {
    return base::apple::ObjCCastStrict<FallbackViewController>(controller());
  }

  // Returns the header item at `section`.
  id GetHeaderItem(int section) {
    return [controller().tableViewModel headerForSectionIndex:section];
  }

  // Returns the type of the header item at `section`.
  NSInteger GetHeaderItemType(int section) {
    return base::apple::ObjCCastStrict<TableViewHeaderFooterItem>(
               GetHeaderItem(section))
        .type;
  }

  // Returns the type of the table view item at `item` in `section`.
  NSInteger GetTableViewItemType(int section, int item) {
    return base::apple::ObjCCastStrict<TableViewItem>(
               GetTableViewItem(section, item))
        .type;
  }
};

// Tests the order of the elements in the view when all of data, action and
// header items are initialized.
TEST_P(FallbackViewControllerTest, CheckItems) {
  TableViewItem* item_one =
      [[TableViewItem alloc] initWithType:ItemTypeSampleOne];
  TableViewItem* item_two =
      [[TableViewItem alloc] initWithType:ItemTypeSampleTwo];
  NSArray<TableViewItem*>* data_items = @[ item_one, item_two ];

  TableViewItem* item_three =
      [[TableViewItem alloc] initWithType:ItemTypeSampleThree];
  NSArray<TableViewItem*>* action_items = @[ item_three ];

  TableViewHeaderFooterItem* item_four =
      [[TableViewHeaderFooterItem alloc] initWithType:ItemTypeSampleFour];

  FallbackViewController* fallbackViewController = GetFallbackViewController();

  [fallbackViewController presentDataItems:data_items];
  [fallbackViewController presentActionItems:action_items];
  [fallbackViewController presentHeaderItem:item_four];

  // When the kIOSKeyboardAccessoryUpgrade feature is enabled, data items each
  // have their own section. When disabled, data items are grouped in the same
  // section.
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    EXPECT_EQ(NumberOfSections(), 4);

    // Header section stays at the top and has no items other than a header.
    EXPECT_EQ(NumberOfItemsInSection(0), 0);
    EXPECT_EQ(NumberOfItemsInSection(1), 1);
    EXPECT_EQ(NumberOfItemsInSection(2), 1);
    EXPECT_EQ(NumberOfItemsInSection(3), 1);

    EXPECT_EQ(GetHeaderItemType(/*section=*/0), ItemTypeSampleFour);
    EXPECT_EQ(GetTableViewItemType(/*section=*/1, /*item=*/0),
              ItemTypeSampleOne);
    EXPECT_EQ(GetTableViewItemType(/*section=*/2, /*item=*/0),
              ItemTypeSampleTwo);
    EXPECT_EQ(GetTableViewItemType(/*section=*/3, /*item=*/0),
              ItemTypeSampleThree);
  } else {
    EXPECT_EQ(NumberOfSections(), 3);

    // Header section stays at the top and has no items other than a header.
    EXPECT_EQ(NumberOfItemsInSection(0), 0);
    EXPECT_EQ(NumberOfItemsInSection(1), 2);
    EXPECT_EQ(NumberOfItemsInSection(2), 1);

    EXPECT_EQ(GetHeaderItemType(/*section=*/0), ItemTypeSampleFour);
    EXPECT_EQ(GetTableViewItemType(/*section=*/1, /*item=*/0),
              ItemTypeSampleOne);
    EXPECT_EQ(GetTableViewItemType(/*section=*/1, /*item=*/1),
              ItemTypeSampleTwo);
    EXPECT_EQ(GetTableViewItemType(/*section=*/2, /*item=*/0),
              ItemTypeSampleThree);
  }
}

// Tests that unused data item sections are deleted as expected.
TEST_P(FallbackViewControllerTest, RemoveUnusedDataItemSections) {
  TableViewItem* item_one =
      [[TableViewItem alloc] initWithType:ItemTypeSampleOne];
  TableViewItem* item_two =
      [[TableViewItem alloc] initWithType:ItemTypeSampleTwo];
  TableViewItem* item_three =
      [[TableViewItem alloc] initWithType:ItemTypeSampleThree];

  FallbackViewController* fallback_view_controller =
      GetFallbackViewController();

  // Present three data items.
  [fallback_view_controller
      presentDataItems:@[ item_one, item_two, item_three ]];
  // When the kIOSKeyboardAccessoryUpgrade feature is enabled, data items each
  // have their own section. When disabled, data items are grouped in the same
  // section.
  EXPECT_EQ(NumberOfSections(), IsKeyboardAccessoryUpgradeEnabled() ? 3 : 1);

  // Present 2 data items.
  [fallback_view_controller presentDataItems:@[ item_one, item_two ]];
  EXPECT_EQ(NumberOfSections(), IsKeyboardAccessoryUpgradeEnabled() ? 2 : 1);

  // Present no data items.
  [fallback_view_controller presentDataItems:@[]];
  EXPECT_EQ(NumberOfSections(), 0);
}

// Tests that the "no data items to show" message is displayed in the right
// place in the table view.
TEST_P(FallbackViewControllerTest, CheckNoDataItemsMessage) {
  FallbackViewController* fallback_view_controller =
      GetFallbackViewController();

  NSArray<TableViewItem*>* data_items;
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    // Set `noRegularDataItemsToShowHeaderItem`.
    TableViewTextHeaderFooterItem* header_item =
        [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeSampleOne];
    fallback_view_controller.noRegularDataItemsToShowHeaderItem = header_item;
    data_items = @[];
  } else {
    ManualFillTextItem* empty_credential_item =
        [[ManualFillTextItem alloc] initWithType:ItemTypeSampleTwo];
    data_items = @[ empty_credential_item ];
  }

  TableViewItem* action_item =
      [[TableViewItem alloc] initWithType:ItemTypeSampleThree];
  NSArray<TableViewItem*>* action_items = @[ action_item ];

  [fallback_view_controller presentDataItems:data_items];
  [fallback_view_controller presentActionItems:action_items];

  // When the kIOSKeyboardAccessoryUpgrade feature is enabled, the "no data
  // items to show" message is displayed as a header for the actions
  // section. When disabled, it is displayed as a regular table view item in the
  // data items section.
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    // Only the actions section and no data item section should be present in
    // the table view.
    EXPECT_EQ(NumberOfSections(), 2);

    // The actions section should have a header and an item.
    EXPECT_TRUE(GetHeaderItem(/*section=*/0));
    EXPECT_EQ(NumberOfItemsInSection(0), 0);
    EXPECT_EQ(NumberOfItemsInSection(1), 1);

    EXPECT_EQ(GetHeaderItemType(/*section=*/0), ItemTypeSampleOne);
    EXPECT_EQ(GetTableViewItemType(/*section=*/1, /*item=*/0),
              ItemTypeSampleThree);
  } else {
    // Both the data items and the actions sections should be present in the
    // table view.
    EXPECT_EQ(NumberOfSections(), 2);

    // Both sections should have one item each and no header.
    EXPECT_FALSE(GetHeaderItem(/*section=*/0));
    EXPECT_EQ(NumberOfItemsInSection(0), 1);
    EXPECT_FALSE(GetHeaderItem(/*section=*/1));
    EXPECT_EQ(NumberOfItemsInSection(1), 1);

    EXPECT_EQ(GetTableViewItemType(/*section=*/0, /*item=*/0),
              ItemTypeSampleTwo);
    EXPECT_EQ(GetTableViewItemType(/*section=*/1, /*item=*/0),
              ItemTypeSampleThree);
  }
}

// Tests that the "no data items to show" message is displayed in the right
// place in the table view even if there are no action items to show.
TEST_P(FallbackViewControllerTest, CheckNoDataItemsMessageWhenNoActions) {
  // This test is only relevant when the Keyboard Accessory Upgrade feature is
  // enabled.
  if (!IsKeyboardAccessoryUpgradeEnabled()) {
    return;
  }

  FallbackViewController* fallback_view_controller =
      GetFallbackViewController();

  // Set `noRegularDataItemsToShowHeaderItem`.
  TableViewTextHeaderFooterItem* header_item =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeSampleOne];
  fallback_view_controller.noRegularDataItemsToShowHeaderItem = header_item;

  [fallback_view_controller presentDataItems:@[]];
  [fallback_view_controller presentActionItems:@[]];

  // Only the actions section should be present in the table view.
  EXPECT_EQ(NumberOfSections(), 1);

  // The actions section should only have a header.
  EXPECT_TRUE(GetHeaderItem(/*section=*/0));
  EXPECT_EQ(NumberOfItemsInSection(0), 0);

  EXPECT_EQ(GetHeaderItemType(/*section=*/0), ItemTypeSampleOne);
}

// Tests that the actions are separated by sections if they belong to different
// types.
TEST_P(FallbackViewControllerTest,
       CheckDifferentSectionsForActionsOfDifferentTypes) {
  TableViewItem* item_one =
      [[TableViewItem alloc] initWithType:ItemTypeSampleOne];
  TableViewItem* item_two =
      [[TableViewItem alloc] initWithType:ItemTypeSampleTwo];

  FallbackViewController* fallbackViewController = GetFallbackViewController();

  [fallbackViewController presentActionItems:@[ item_one ]];
  [fallbackViewController presentPlusAddressActionItems:@[ item_two ]];

  EXPECT_EQ(NumberOfSections(), 2);

  EXPECT_EQ(NumberOfItemsInSection(0), 1);
  EXPECT_EQ(NumberOfItemsInSection(1), 1);

  EXPECT_EQ(GetTableViewItemType(/*section=*/0, /*item=*/0), ItemTypeSampleOne);
  EXPECT_EQ(GetTableViewItemType(/*section=*/1, /*item=*/0), ItemTypeSampleTwo);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(FallbackViewControllerTest);
