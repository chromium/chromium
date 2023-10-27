// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/numerics/safe_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller+private.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/root_view_controller_test.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Fake object that conforms to GridViewControllerDelegate.
@interface FakeGridViewControllerDelegate
    : NSObject <GridViewControllerDelegate>
@property(nonatomic, assign) NSUInteger itemCount;
@end
@implementation FakeGridViewControllerDelegate
@synthesize itemCount = _itemCount;

- (void)gridViewController:(BaseGridViewController*)gridViewController
    contentNeedsAuthenticationChanged:(BOOL)needsAuth {
}
- (void)gridViewController:(BaseGridViewController*)gridViewController
        didChangeItemCount:(NSUInteger)count {
  self.itemCount = count;
}
- (void)gridViewController:(BaseGridViewController*)gridViewController
       didSelectItemWithID:(web::WebStateID)itemID {
  // No-op for unittests. This is only called when a user taps on a cell, not
  // generically when selectedIndex is updated.
}
- (void)gridViewController:(BaseGridViewController*)gridViewController
         didMoveItemWithID:(web::WebStateID)itemID
                   toIndex:(NSUInteger)destinationIndex {
  // No-op for unittests. This is only called when a user interactively moves
  // an item, not generically when items are moved in the data source.
}
- (void)gridViewController:(BaseGridViewController*)gridViewController
        didCloseItemWithID:(web::WebStateID)itemID {
  // No-op for unittests. This is only called when a user taps to close a cell,
  // not generically when items are removed from the data source.
}
- (void)gridViewController:(BaseGridViewController*)gridViewController
       didRemoveItemWIthID:(web::WebStateID)itemID {
  // No-op for unittests. This is only called when an item has been removed.
}
- (void)didChangeLastItemVisibilityInGridViewController:
    (BaseGridViewController*)gridViewController {
  // No-op for unittests.
}
- (void)gridViewControllerWillBeginDragging:
    (BaseGridViewController*)gridViewController {
  // No-op for unittests.
}

- (void)gridViewControllerDragSessionWillBegin:
    (BaseGridViewController*)gridViewController {
  // No-op for unittests.
}

- (void)gridViewControllerDragSessionDidEnd:
    (BaseGridViewController*)gridViewController {
  // No-op for unittests.
}

- (void)gridViewControllerScrollViewDidScroll:
    (BaseGridViewController*)gridViewController {
  // No-op for unittests.
}

- (void)gridViewControllerDropAnimationWillBegin:
    (BaseGridViewController*)gridViewController {
  // No-op for unittests.
}

- (void)gridViewControllerDropAnimationDidEnd:
    (BaseGridViewController*)gridViewController {
  // No-op for unittests.
}

- (void)didTapInactiveTabsButtonInGridViewController:
    (BaseGridViewController*)gridViewController {
  // No-op for unittests.
}

- (void)didTapInactiveTabsSettingsLinkInGridViewController:
    (BaseGridViewController*)gridViewController {
  // No-op for unittests.
}

@end

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
    NSArray* items = @[
      [[TabSwitcherItem alloc] initWithIdentifier:identifier_a_],
      [[TabSwitcherItem alloc] initWithIdentifier:identifier_b_],
    ];
    [view_controller_ populateItems:items selectedItemID:identifier_a_];
    delegate_ = [[FakeGridViewControllerDelegate alloc] init];
    delegate_.itemCount = 2;
    view_controller_.delegate = delegate_;
  }

 protected:
  IOSChromeScopedTestingLocalState local_state_;
  BaseGridViewController* view_controller_;
  FakeGridViewControllerDelegate* delegate_;
  const web::WebStateID identifier_a_;
  const web::WebStateID identifier_b_;
};

// Tests that items are initialized and delegate is updated with a new
// itemCount.
TEST_F(BaseGridViewControllerTest, InitializeItems) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  web::WebStateID newItemID = web::WebStateID::NewUnique();
  TabSwitcherItem* item =
      [[TabSwitcherItem alloc] initWithIdentifier:newItemID];
  [view_controller_ populateItems:@[ item ] selectedItemID:newItemID];
  EXPECT_EQ(newItemID, view_controller_.items[0].identifier);
  EXPECT_EQ(1U, view_controller_.items.count);
  EXPECT_EQ(0U, view_controller_.selectedIndex);
  EXPECT_EQ(1U, delegate_.itemCount);
}

// Tests that an item is inserted and delegate is updated with a new itemCount.
TEST_F(BaseGridViewControllerTest, InsertItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  web::WebStateID newItemID = web::WebStateID::NewUnique();
  [view_controller_
          insertItem:[[TabSwitcherItem alloc] initWithIdentifier:newItemID]
             atIndex:2
      selectedItemID:newItemID];
  EXPECT_EQ(3U, view_controller_.items.count);
  EXPECT_EQ(2U, view_controller_.selectedIndex);
  EXPECT_EQ(3U, delegate_.itemCount);
}

// Tests that an item is removed and delegate is updated with a new itemCount.
TEST_F(BaseGridViewControllerTest, RemoveItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  [view_controller_ removeItemWithID:identifier_a_
                      selectedItemID:identifier_b_];
  EXPECT_EQ(1U, view_controller_.items.count);
  EXPECT_EQ(0U, view_controller_.selectedIndex);
  EXPECT_EQ(1U, delegate_.itemCount);
}

// Tests that an item is selected.
TEST_F(BaseGridViewControllerTest, SelectItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  [view_controller_ selectItemWithID:identifier_b_];
  EXPECT_EQ(1U, view_controller_.selectedIndex);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that when a nonexistent item is selected, the selected item index is
// NSNotFound
TEST_F(BaseGridViewControllerTest, SelectNonexistentItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  [view_controller_ selectItemWithID:web::WebStateID::NewUnique()];
  EXPECT_EQ(base::checked_cast<NSUInteger>(NSNotFound),
            view_controller_.selectedIndex);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that an item is replaced with a new identifier.
TEST_F(BaseGridViewControllerTest, ReplaceItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  web::WebStateID newItemID = web::WebStateID::NewUnique();
  TabSwitcherItem* item =
      [[TabSwitcherItem alloc] initWithIdentifier:newItemID];
  [view_controller_ replaceItemID:identifier_a_ withItem:item];
  EXPECT_EQ(newItemID, view_controller_.items[0].identifier);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that an item is replaced with same identifier.
TEST_F(BaseGridViewControllerTest, ReplaceItemSameIdentifier) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  TabSwitcherItem* item =
      [[TabSwitcherItem alloc] initWithIdentifier:identifier_a_];
  id mock_item = OCMPartialMock(item);
  OCMStub([mock_item title]).andReturn(@"NEW-ITEM-TITLE");
  [view_controller_ replaceItemID:identifier_a_ withItem:mock_item];
  EXPECT_EQ(identifier_a_, view_controller_.items[0].identifier);
  EXPECT_NSEQ(@"NEW-ITEM-TITLE", view_controller_.items[0].title);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that an item is not replaced if it doesn't exist.
TEST_F(BaseGridViewControllerTest, ReplaceItemNotFound) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  web::WebStateID notFoundItemID = web::WebStateID::NewUnique();
  TabSwitcherItem* item =
      [[TabSwitcherItem alloc] initWithIdentifier:notFoundItemID];
  [view_controller_ replaceItemID:notFoundItemID withItem:item];
  EXPECT_NE(notFoundItemID, view_controller_.items[0].identifier);
  EXPECT_NE(notFoundItemID, view_controller_.items[1].identifier);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that the selected item is moved.
TEST_F(BaseGridViewControllerTest, MoveSelectedItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  [view_controller_ moveItemWithID:identifier_a_ toIndex:1];
  EXPECT_EQ(identifier_a_, view_controller_.items[1].identifier);
  EXPECT_EQ(1U, view_controller_.selectedIndex);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that a non-selected item is moved.
TEST_F(BaseGridViewControllerTest, MoveUnselectedItem) {
  // Previously: The grid had 2 items and selectedIndex was 0. The delegate had
  // an itemCount of 2.
  [view_controller_ moveItemWithID:identifier_b_ toIndex:0];
  EXPECT_EQ(identifier_a_, view_controller_.items[1].identifier);
  EXPECT_EQ(1U, view_controller_.selectedIndex);
  EXPECT_EQ(2U, delegate_.itemCount);
}

// Tests that `-replaceItemID:withItem:` does not crash when updating an item
// that is scrolled offscreen.
TEST_F(BaseGridViewControllerTest, ReplaceScrolledOffScreenCell) {
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
    web::WebStateID uniqueID = web::WebStateID::NewUnique();
    TabSwitcherItem* item =
        [[TabSwitcherItem alloc] initWithIdentifier:uniqueID];
    [view_controller_ insertItem:item atIndex:0 selectedItemID:identifier_a_];
    // Spin the runloop to make sure that the visible cells are updated.
    base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(1));
    visibleCellsCount = view_controller_.collectionView.visibleCells.count;
  }
  // The last item ("B") is scrolled off screen.
  TabSwitcherItem* item =
      [[TabSwitcherItem alloc] initWithIdentifier:web::WebStateID::NewUnique()];
  // Do not crash due to cell being nil.
  [view_controller_ replaceItemID:identifier_b_ withItem:item];
}
