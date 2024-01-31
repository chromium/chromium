// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/fallback_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/with_feature_override.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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
    FallbackViewController* viewController =
        [[FallbackViewController alloc] init];
    [viewController loadModel];
    return viewController;
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
  TableViewItem* itemOne =
      [[TableViewItem alloc] initWithType:ItemTypeSampleOne];
  TableViewItem* itemTwo =
      [[TableViewItem alloc] initWithType:ItemTypeSampleTwo];
  NSArray<TableViewItem*>* dataItems = @[ itemOne, itemTwo ];

  TableViewItem* itemThree =
      [[TableViewItem alloc] initWithType:ItemTypeSampleThree];
  NSArray<TableViewItem*>* actionItems = @[ itemThree ];

  TableViewHeaderFooterItem* itemFour =
      [[TableViewHeaderFooterItem alloc] initWithType:ItemTypeSampleFour];

  FallbackViewController* fallbackViewController = GetFallbackViewController();

  [fallbackViewController presentDataItems:dataItems];
  [fallbackViewController presentActionItems:actionItems];
  [fallbackViewController presentHeaderItem:itemFour];

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
  TableViewItem* itemOne =
      [[TableViewItem alloc] initWithType:ItemTypeSampleOne];
  TableViewItem* itemTwo =
      [[TableViewItem alloc] initWithType:ItemTypeSampleTwo];
  TableViewItem* itemThree =
      [[TableViewItem alloc] initWithType:ItemTypeSampleThree];

  FallbackViewController* fallbackViewController = GetFallbackViewController();

  // Present three data items.
  [fallbackViewController presentDataItems:@[ itemOne, itemTwo, itemThree ]];
  // When the kIOSKeyboardAccessoryUpgrade feature is enabled, data items each
  // have their own section. When disabled, data items are grouped in the same
  // section.
  EXPECT_EQ(NumberOfSections(), IsKeyboardAccessoryUpgradeEnabled() ? 3 : 1);

  // Present 2 data items.
  [fallbackViewController presentDataItems:@[ itemOne, itemTwo ]];
  EXPECT_EQ(NumberOfSections(), IsKeyboardAccessoryUpgradeEnabled() ? 2 : 1);

  // Present no data items.
  [fallbackViewController presentDataItems:@[]];
  EXPECT_EQ(NumberOfSections(), 0);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(FallbackViewControllerTest);
