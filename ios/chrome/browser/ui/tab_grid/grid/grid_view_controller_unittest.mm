// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/grid/grid_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/numerics/safe_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_item.h"
#import "ios/chrome/test/root_view_controller_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test object that exposes the inner state for test verification.
@interface TestGridViewController : GridViewController
@property(nonatomic, readonly) NSMutableArray<GridItem*>* items;
@property(nonatomic, readonly) NSUInteger selectedIndex;
@property(nonatomic, readonly) UICollectionView* collectionView;
@property(nonatomic, assign, getter=isViewAppeared) BOOL viewAppeared;
@end
@implementation TestGridViewController
@dynamic items;
@dynamic selectedIndex;
@dynamic collectionView;
@dynamic viewAppeared;
@end

// Fake object that conforms to GridViewControllerDelegate.
@interface FakeGridViewControllerDelegate : NSObject<GridViewControllerDelegate>
@property(nonatomic, assign) NSUInteger itemCount;
@end
@implementation FakeGridViewControllerDelegate
@synthesize itemCount = _itemCount;
- (void)gridViewController:(GridViewController*)gridViewController
        didChangeItemCount:(NSUInteger)count {
  self.itemCount = count;
}
- (void)gridViewController:(GridViewController*)gridViewController
       didSelectItemWithID:(NSString*)itemID {
  // No-op for unittests. This is only called when a user taps on a cell, not
  // generically when selectedIndex is updated.
}
- (void)gridViewController:(GridViewController*)gridViewController
         didMoveItemWithID:(NSString*)itemID
                   toIndex:(NSUInteger)destinationIndex {
  // No-op for unittests. This is only called when a user interactively moves
  // an item, not generically when items are moved in the data source.
}
- (void)gridViewController:(GridViewController*)gridViewController
        didCloseItemWithID:(NSString*)itemID {
  // No-op for unittests. This is only called when a user taps to close a cell,
  // not generically when items are removed from the data source.
}
@end

class GridViewControllerTest : public RootViewControllerTest {
 public:
  GridViewControllerTest() {
    view_controller_ = [[TestGridViewController alloc] init];
    NSArray* items = @[
      [[GridItem alloc] initWithIdentifier:@"A"],
      [[GridItem alloc] initWithIdentifier:@"B"]
    ];
    [view_controller_ populateItems:items selectedItemID:@"A"];
    delegate_ = [[FakeGridViewControllerDelegate alloc] init];
    delegate_.itemCount = 2;
    view_controller_.delegate = delegate_;
  }

 protected:
  TestGridViewController* view_controller_;
  FakeGridViewControllerDelegate* delegate_;
};

// Tests that items are initialized and delegate is updated with a new
// itemCount.
TEST_F(GridViewControllerTest, InitializeItems) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  GridItem* item = [[GridItem alloc] initWithIdentifier:@"NEW-ITEM"];
  [view_controller_ populateItems:@[ item ] selectedItemID:@"NEW-ITEM"];
  EXPECT_NSEQ(@"NEW-ITEM", view_controller_.items[0].identifier);
  EXPECT_EQ(1U, view_controller_.items.count);
  EXPECT_EQ(0U, view_controller_.selectedIndex);
  EXPECT_EQ(1U, delegate_.itemCount);
}

// Tests that an item is inserted and delegate is updated with a new itemCount.
TEST_F(GridViewControllerTest, InsertItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  [view_controller_ insertItem:[[GridItem alloc] initWithIdentifier:@"NEW-ITEM"]
                       atIndex:2
                selectedItemID:@"NEW-ITEM"];
  EXPECT_EQ(3U, view_controller_.items.count);
  EXPECT_EQ(2U, view_controller_.selectedIndex);
  EXPECT_EQ(3U, delegate_.itemCount);
}

// Tests that an item is removed and delegate is updated with a new itemCount.
TEST_F(GridViewControllerTest, RemoveItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  [view_controller_ removeItemWithID:@"A" selectedItemID:@"B"];
  EXPECT_EQ(1U, view_controller_.items.count);
  EXPECT_EQ(0U, view_controller_.selectedIndex);
  EXPECT_EQ(1U, delegate_.itemCount);
}

// Tests that an item is selected.
TEST_F(GridViewControllerTest, SelectItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  [view_controller_ selectItemWithID:@"B"];
  EXPECT_EQ(1U, view_controller_.selectedIndex);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that when a nonexistent item is selected, the selected item index is
// NSNotFound
TEST_F(GridViewControllerTest, SelectNonexistentItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  [view_controller_ selectItemWithID:@"NOT-A-KNOWN-ITEM"];
  EXPECT_EQ(base::checked_cast<NSUInteger>(NSNotFound),
            view_controller_.selectedIndex);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that an item is replaced with a new identifier.
TEST_F(GridViewControllerTest, ReplaceItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  GridItem* item = [[GridItem alloc] initWithIdentifier:@"NEW-ITEM"];
  [view_controller_ replaceItemID:@"A" withItem:item];
  EXPECT_NSEQ(@"NEW-ITEM", view_controller_.items[0].identifier);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that an item is replaced with same identifier.
TEST_F(GridViewControllerTest, ReplaceItemSameIdentifier) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  GridItem* item = [[GridItem alloc] initWithIdentifier:@"A"];
  item.title = @"NEW-ITEM-TITLE";
  [view_controller_ replaceItemID:@"A" withItem:item];
  EXPECT_NSEQ(@"A", view_controller_.items[0].identifier);
  EXPECT_NSEQ(@"NEW-ITEM-TITLE", view_controller_.items[0].title);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that an item is not replaced if it doesn't exist.
TEST_F(GridViewControllerTest, ReplaceItemNotFound) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  GridItem* item = [[GridItem alloc] initWithIdentifier:@"NOT-FOUND"];
  [view_controller_ replaceItemID:@"NOT-FOUND" withItem:item];
  EXPECT_NSNE(@"NOT-FOUND", view_controller_.items[0].identifier);
  EXPECT_NSNE(@"NOT-FOUND", view_controller_.items[1].identifier);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that the selected item is moved.
TEST_F(GridViewControllerTest, MoveSelectedItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  [view_controller_ moveItemWithID:@"A" toIndex:1];
  EXPECT_NSEQ(@"A", view_controller_.items[1].identifier);
  EXPECT_EQ(1U, view_controller_.selectedIndex);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that a non-selected item is moved.
TEST_F(GridViewControllerTest, MoveUnselectedItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  [view_controller_ moveItemWithID:@"B" toIndex:0];
  EXPECT_NSEQ(@"A", view_controller_.items[1].identifier);
  EXPECT_EQ(1U, view_controller_.selectedIndex);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that |-replaceItemID:withItem:| does not crash when updating an item
// that is scrolled offscreen.
TEST_F(GridViewControllerTest, ReplaceScrolledOffScreenCell) {
  // This test requires that the collection view be placed on the screen.
  SetRootViewController(view_controller_);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return view_controller_.collectionView.visibleCells.count > 0;
      }));
  NSArray* items = view_controller_.items;
  // Keep adding items until we get an item that is offscreen. Since device
  // sizes may vary, this is better than creating a fixed number of items that
  // we think will overflow to offscreen items.
  NSUInteger visibleCellsCount =
      view_controller_.collectionView.visibleCells.count;
  while (visibleCellsCount >= items.count) {
    NSString* uniqueID =
        [NSString stringWithFormat:@"%d", base::checked_cast<int>(items.count)];
    GridItem* item = [[GridItem alloc] initWithIdentifier:uniqueID];
    [view_controller_ insertItem:item atIndex:0 selectedItemID:@"A"];
    visibleCellsCount = view_controller_.collectionView.visibleCells.count;
  }
  // The last item ("B") is scrolled off screen.
  GridItem* item = [[GridItem alloc] initWithIdentifier:@"NEW-ITEM"];
  // Do not crash due to cell being nil.
  [view_controller_ replaceItemID:@"B" withItem:item];
}
