// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/fallback_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"

namespace {

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSampleOne = kItemTypeEnumZero,
  ItemTypeSampleTwo,
  ItemTypeSampleThree,
  ItemTypeSampleFour
};

}  // namespace

class FallbackViewControllerTest : public LegacyChromeTableViewControllerTest {
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
TEST_F(FallbackViewControllerTest, CheckDataAndActionAndHeaderItems) {
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

  FallbackViewController* fallbackViewController =
      base::apple::ObjCCastStrict<FallbackViewController>(controller());

  [fallbackViewController presentDataItems:dataItems];
  [fallbackViewController presentActionItems:actionItems];
  [fallbackViewController presentHeaderItem:itemFour];

  EXPECT_EQ(NumberOfSections(), 3);
  // Header section stays at the top and has no items other than a header.
  EXPECT_EQ(NumberOfItemsInSection(0), 0);
  EXPECT_EQ(NumberOfItemsInSection(1), 2);
  EXPECT_EQ(NumberOfItemsInSection(2), 1);

  EXPECT_EQ(GetHeaderItemType(/*section=*/0), ItemTypeSampleFour);
  EXPECT_EQ(GetTableViewItemType(/*section=*/1, /*item=*/0), ItemTypeSampleOne);
  EXPECT_EQ(GetTableViewItemType(/*section=*/1, /*item=*/1), ItemTypeSampleTwo);
  EXPECT_EQ(GetTableViewItemType(/*section=*/2, /*item=*/0),
            ItemTypeSampleThree);
}
