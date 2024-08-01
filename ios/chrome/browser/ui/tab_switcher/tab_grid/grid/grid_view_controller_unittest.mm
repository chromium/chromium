// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller+Testing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/numerics/safe_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/root_view_controller_test.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class BaseGridViewControllerTest : public RootViewControllerTest {
 public:
  BaseGridViewControllerTest()
      : identifier_a_(web::WebStateID::NewUnique()),
        identifier_b_(web::WebStateID::NewUnique()) {
    view_controller_ = [[BaseGridViewController alloc] init];
    // Load the view and notify its content will appear. This sets the data
    // source and loads the initial snapshot.
    [view_controller_ loadView];
    [view_controller_ contentWillAppearAnimated:NO];
    TabSwitcherItem* item_a =
        [[TabSwitcherItem alloc] initWithIdentifier:identifier_a_];
    TabSwitcherItem* item_b =
        [[TabSwitcherItem alloc] initWithIdentifier:identifier_b_];

    NSArray* items = @[
      [[GridItemIdentifier alloc] initWithTabItem:item_a],
      [[GridItemIdentifier alloc] initWithTabItem:item_b],
    ];
    [view_controller_ populateItems:items
             selectedItemIdentifier:[[GridItemIdentifier alloc]
                                        initWithTabItem:item_a]];
  }

 protected:
  TabSwitcherItem* TabItemForIndex(NSInteger index) {
    GridDiffableDataSource* dataSource = view_controller_.diffableDataSource;
    return [dataSource
               itemIdentifierForIndexPath:[NSIndexPath indexPathForItem:index
                                                              inSection:0]]
        .tabSwitcherItem;
  }

  web::WebStateID IdentifierForIndex(NSInteger index) {
    return TabItemForIndex(index).identifier;
  }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  BaseGridViewController* view_controller_;
  const web::WebStateID identifier_a_;
  const web::WebStateID identifier_b_;
};

// Tests that items are initialized.
TEST_F(BaseGridViewControllerTest, InitializeItems) {
  // Previously: The grid had 2 items and selectedIndex was 0.
  web::WebStateID newItemID = web::WebStateID::NewUnique();
  TabSwitcherItem* item =
      [[TabSwitcherItem alloc] initWithIdentifier:newItemID];
  [view_controller_
               populateItems:@[ [[GridItemIdentifier alloc]
                                 initWithTabItem:item] ]
      selectedItemIdentifier:[[GridItemIdentifier alloc] initWithTabItem:item]];
  EXPECT_EQ(newItemID, IdentifierForIndex(0));
  EXPECT_EQ(1U, [[view_controller_.diffableDataSource snapshot] numberOfItems]);
  EXPECT_EQ(0U, view_controller_.selectedIndex);
}

// Tests that an item is inserted.
TEST_F(BaseGridViewControllerTest, InsertItem) {
  // Previously: The grid had 2 items and selectedIndex was 0.
  web::WebStateID newItemID = web::WebStateID::NewUnique();
  TabSwitcherItem* item =
      [[TabSwitcherItem alloc] initWithIdentifier:newItemID];
  [view_controller_
                  insertItem:[[GridItemIdentifier alloc] initWithTabItem:item]
                beforeItemID:nil
      selectedItemIdentifier:[[GridItemIdentifier alloc] initWithTabItem:item]];
  EXPECT_EQ(3U, [[view_controller_.diffableDataSource snapshot] numberOfItems]);
  EXPECT_EQ(2U, view_controller_.selectedIndex);
}

// Tests that an item is removed.
TEST_F(BaseGridViewControllerTest, RemoveItem) {
  // Previously: The grid had 2 items and selectedIndex was 0.
  TabSwitcherItem* item_a =
      [[TabSwitcherItem alloc] initWithIdentifier:identifier_a_];
  TabSwitcherItem* item_b =
      [[TabSwitcherItem alloc] initWithIdentifier:identifier_b_];

  [view_controller_ removeItemWithIdentifier:[[GridItemIdentifier alloc]
                                                 initWithTabItem:item_a]
                      selectedItemIdentifier:[[GridItemIdentifier alloc]
                                                 initWithTabItem:item_b]];
  EXPECT_EQ(1U, [[view_controller_.diffableDataSource snapshot] numberOfItems]);
  EXPECT_EQ(0U, view_controller_.selectedIndex);
}

// Tests that an item is selected.
TEST_F(BaseGridViewControllerTest, SelectItem) {
  TabSwitcherItem* item_b =
      [[TabSwitcherItem alloc] initWithIdentifier:identifier_b_];
  // Previously: The grid had 2 items and selectedIndex was 0.
  [view_controller_ selectItemWithIdentifier:[[GridItemIdentifier alloc]
                                                 initWithTabItem:item_b]];
  EXPECT_EQ(1U, view_controller_.selectedIndex);
}

// Tests that when a nonexistent item is selected, the selected item index is
// NSNotFound
TEST_F(BaseGridViewControllerTest, SelectNonexistentItem) {
  TabSwitcherItem* item =
      [[TabSwitcherItem alloc] initWithIdentifier:web::WebStateID::NewUnique()];

  // Previously: The grid had 2 items and selectedIndex was 0.
  [view_controller_ selectItemWithIdentifier:[[GridItemIdentifier alloc]
                                                 initWithTabItem:item]];
  EXPECT_EQ(base::checked_cast<NSUInteger>(NSNotFound),
            view_controller_.selectedIndex);
}

// Tests that an item is replaced with a new identifier.
TEST_F(BaseGridViewControllerTest, ReplaceItem) {
  // Previously: The grid had 2 items and selectedIndex was 0.
  web::WebStateID newItemID = web::WebStateID::NewUnique();

  TabSwitcherItem* item_a =
      [[TabSwitcherItem alloc] initWithIdentifier:identifier_a_];
  TabSwitcherItem* item =
      [[TabSwitcherItem alloc] initWithIdentifier:newItemID];
  [view_controller_
              replaceItem:[[GridItemIdentifier alloc] initWithTabItem:item_a]
      withReplacementItem:[[GridItemIdentifier alloc] initWithTabItem:item]];
  EXPECT_EQ(newItemID, IdentifierForIndex(0));
}

// Tests that an item is replaced with same identifier.
TEST_F(BaseGridViewControllerTest, ReplaceItemSameIdentifier) {
  // This test requires that the collection view be placed on the screen.
  SetRootViewController(view_controller_);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return view_controller_.collectionView.visibleCells.count > 0;
      }));
  // Previously: The grid had 2 items and selectedIndex was 0.
  TabSwitcherItem* existingItem = TabItemForIndex(0);
  id mock_item = OCMPartialMock(existingItem);
  OCMStub([mock_item title]).andReturn(@"NEW-ITEM-TITLE");
  TabSwitcherItem* itemForReplace =
      [[TabSwitcherItem alloc] initWithIdentifier:identifier_a_];
  [view_controller_ replaceItem:[[GridItemIdentifier alloc]
                                    initWithTabItem:itemForReplace]
            withReplacementItem:[[GridItemIdentifier alloc]
                                    initWithTabItem:itemForReplace]];
  NSString* identifier_cell_a =
      [NSString stringWithFormat:@"%@0", kGridCellIdentifierPrefix];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        for (GridCell* cell in view_controller_.collectionView.visibleCells) {
          if ([cell.accessibilityIdentifier isEqual:identifier_cell_a]) {
            return [cell.title isEqual:@"NEW-ITEM-TITLE"];
          }
        }
        return false;
      }));
  EXPECT_EQ(identifier_a_, IdentifierForIndex(0));
}

// Tests that an item is not replaced if it doesn't exist.
TEST_F(BaseGridViewControllerTest, ReplaceItemNotFound) {
  // Previously: The grid had 2 items and selectedIndex was 0.
  web::WebStateID notFoundItemID = web::WebStateID::NewUnique();
  TabSwitcherItem* item =
      [[TabSwitcherItem alloc] initWithIdentifier:notFoundItemID];
  [view_controller_
              replaceItem:[[GridItemIdentifier alloc] initWithTabItem:item]
      withReplacementItem:[[GridItemIdentifier alloc] initWithTabItem:item]];
  EXPECT_NE(notFoundItemID, IdentifierForIndex(0));
  EXPECT_NE(notFoundItemID, IdentifierForIndex(1));
}

// Tests that the selected item is moved.
TEST_F(BaseGridViewControllerTest, MoveSelectedItem) {
  TabSwitcherItem* item_a =
      [[TabSwitcherItem alloc] initWithIdentifier:identifier_a_];
  // Previously: The grid had 2 items and selectedIndex was 0.
  [view_controller_ moveItem:[[GridItemIdentifier alloc] initWithTabItem:item_a]
                  beforeItem:nil];
  EXPECT_EQ(identifier_a_, IdentifierForIndex(1));
  EXPECT_EQ(1U, view_controller_.selectedIndex);
}

// Tests that a non-selected item is moved.
TEST_F(BaseGridViewControllerTest, MoveUnselectedItem) {
  TabSwitcherItem* item_a =
      [[TabSwitcherItem alloc] initWithIdentifier:identifier_a_];
  TabSwitcherItem* item_b =
      [[TabSwitcherItem alloc] initWithIdentifier:identifier_b_];
  // Previously: The grid had 2 items and selectedIndex was 0.
  [view_controller_
        moveItem:[[GridItemIdentifier alloc] initWithTabItem:item_b]
      beforeItem:[[GridItemIdentifier alloc] initWithTabItem:item_a]];
  EXPECT_EQ(identifier_a_, IdentifierForIndex(1));
  EXPECT_EQ(1U, view_controller_.selectedIndex);
}

// Tests that `replaceItem:withReplacementItem:` does not crash when updating an
// item that is scrolled offscreen.
TEST_F(BaseGridViewControllerTest, ReplaceScrolledOffScreenCell) {
  // This test requires that the collection view be placed on the screen.
  SetRootViewController(view_controller_);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return view_controller_.collectionView.visibleCells.count > 0;
      }));
  GridDiffableDataSource* dataSource = view_controller_.diffableDataSource;
  // Keep adding items until we get an item that is offscreen. Since device
  // sizes may vary, this is better than creating a fixed number of items that
  // we think will overflow to offscreen items.
  NSUInteger visibleCellsCount =
      view_controller_.collectionView.visibleCells.count;
  while (visibleCellsCount >=
         static_cast<NSUInteger>([[dataSource snapshot] numberOfItems])) {
    web::WebStateID uniqueID = web::WebStateID::NewUnique();
    TabSwitcherItem* item =
        [[TabSwitcherItem alloc] initWithIdentifier:uniqueID];
    TabSwitcherItem* selectedItem =
        [[TabSwitcherItem alloc] initWithIdentifier:identifier_a_];
    [view_controller_
                    insertItem:[[GridItemIdentifier alloc] initWithTabItem:item]
                  beforeItemID:nil
        selectedItemIdentifier:[[GridItemIdentifier alloc]
                                   initWithTabItem:selectedItem]];
    // Spin the runloop to make sure that the visible cells are updated.
    base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(1));
    visibleCellsCount = view_controller_.collectionView.visibleCells.count;
  }
  TabSwitcherItem* item_b =
      [[TabSwitcherItem alloc] initWithIdentifier:identifier_b_];
  // The last item ("B") is scrolled off screen.
  TabSwitcherItem* item =
      [[TabSwitcherItem alloc] initWithIdentifier:web::WebStateID::NewUnique()];
  // Do not crash due to cell being nil.
  [view_controller_
              replaceItem:[[GridItemIdentifier alloc] initWithTabItem:item_b]
      withReplacementItem:[[GridItemIdentifier alloc] initWithTabItem:item]];
}
