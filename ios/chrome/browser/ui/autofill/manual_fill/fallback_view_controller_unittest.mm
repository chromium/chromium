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

}

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
};

// Test the order of the elements in the view when all of data, action and
// header items are initialised.
TEST_F(FallbackViewControllerTest, CheckDataAndActionAndHeaderItems) {
  TableViewItem* itemOne =
      [[TableViewItem alloc] initWithType:ItemTypeSampleOne];
  TableViewItem* itemTwo =
      [[TableViewItem alloc] initWithType:ItemTypeSampleTwo];

  NSArray<TableViewItem*>* dataItems = @[ itemOne, itemTwo ];
  FallbackViewController* fallbackViewController =
      base::apple::ObjCCastStrict<FallbackViewController>(controller());

  TableViewItem* itemThree =
      [[TableViewItem alloc] initWithType:ItemTypeSampleThree];
  NSArray<TableViewItem*>* actionItems = @[ itemThree ];

  TableViewItem* itemFour =
      [[TableViewItem alloc] initWithType:ItemTypeSampleFour];

  [fallbackViewController presentDataItems:dataItems];
  [fallbackViewController presentActionItems:actionItems];
  [fallbackViewController presentHeaderItem:itemFour];

  EXPECT_EQ(NumberOfSections(), 3);
  // Section for header stays at the top.
  EXPECT_EQ(NumberOfItemsInSection(0), 1);
  EXPECT_EQ(NumberOfItemsInSection(1), 2);
  EXPECT_EQ(NumberOfItemsInSection(2), 1);

  EXPECT_EQ(
      base::apple::ObjCCastStrict<TableViewItem>(GetTableViewItem(0, 0)).type,
      ItemTypeSampleFour);
  EXPECT_EQ(
      base::apple::ObjCCastStrict<TableViewItem>(GetTableViewItem(1, 0)).type,
      ItemTypeSampleOne);
  EXPECT_EQ(
      base::apple::ObjCCastStrict<TableViewItem>(GetTableViewItem(1, 1)).type,
      ItemTypeSampleTwo);
  EXPECT_EQ(
      base::apple::ObjCCastStrict<TableViewItem>(GetTableViewItem(2, 0)).type,
      ItemTypeSampleThree);
}
