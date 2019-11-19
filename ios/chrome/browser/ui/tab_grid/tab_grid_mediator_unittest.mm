// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_mediator.h"

#import <Foundation/Foundation.h>
#include <memory>

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_item.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#include "ios/web/common/features.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test object that conforms to GridConsumer and exposes inner state for test
// verification.
@interface FakeConsumer : NSObject<GridConsumer>
// The fake consumer only keeps the identifiers of items for simplicity
@property(nonatomic, strong) NSMutableArray<NSString*>* items;
@property(nonatomic, assign) NSString* selectedItemID;
@end
@implementation FakeConsumer
@synthesize items = _items;
@synthesize selectedItemID = _selectedItemID;

- (void)populateItems:(NSArray<GridItem*>*)items
       selectedItemID:(NSString*)selectedItemID {
  self.selectedItemID = selectedItemID;
  self.items = [NSMutableArray array];
  for (GridItem* item in items) {
    [self.items addObject:item.identifier];
  }
}

- (void)insertItem:(GridItem*)item
           atIndex:(NSUInteger)index
    selectedItemID:(NSString*)selectedItemID {
  [self.items insertObject:item.identifier atIndex:index];
  self.selectedItemID = selectedItemID;
}

- (void)removeItemWithID:(NSString*)removedItemID
          selectedItemID:(NSString*)selectedItemID {
  [self.items removeObject:removedItemID];
  self.selectedItemID = selectedItemID;
}

- (void)selectItemWithID:(NSString*)selectedItemID {
  self.selectedItemID = selectedItemID;
}

- (void)replaceItemID:(NSString*)itemID withItem:(GridItem*)item {
  NSUInteger index = [self.items indexOfObject:itemID];
  self.items[index] = item.identifier;
}

- (void)moveItemWithID:(NSString*)itemID toIndex:(NSUInteger)toIndex {
  [self.items removeObject:itemID];
  [self.items insertObject:itemID atIndex:toIndex];
}

@end

// Fake WebStateList delegate that attaches the tab ID tab helper.
class TabIdFakeWebStateListDelegate : public FakeWebStateListDelegate {
 public:
  TabIdFakeWebStateListDelegate() {}
  ~TabIdFakeWebStateListDelegate() override {}

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override {
    TabIdTabHelper::CreateForWebState(web_state);
    // Create NTPTabHelper to ensure VisibleURL is set to kChromeUINewTabURL.
    id delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
    NewTabPageTabHelper::CreateForWebState(web_state, delegate);
  }
};

class TabGridMediatorTest : public PlatformTest {
 public:
  TabGridMediatorTest() {}
  ~TabGridMediatorTest() override {}

  void SetUp() override {
    PlatformTest::SetUp();
    browser_state_ = TestChromeBrowserState::Builder().Build();
    web_state_list_delegate_ =
        std::make_unique<TabIdFakeWebStateListDelegate>();
    web_state_list_ =
        std::make_unique<WebStateList>(web_state_list_delegate_.get());
    tab_model_ = OCMClassMock([TabModel class]);
    OCMStub([tab_model_ webStateList]).andReturn(web_state_list_.get());
    OCMStub([tab_model_ browserState]).andReturn(browser_state_.get());
    NSMutableSet<NSString*>* identifiers = [[NSMutableSet alloc] init];

    // Insert some web states.
    for (int i = 0; i < 3; i++) {
      auto web_state = std::make_unique<web::TestWebState>();
      TabIdTabHelper::CreateForWebState(web_state.get());
      NSString* identifier =
          TabIdTabHelper::FromWebState(web_state.get())->tab_id();
      // Tab IDs should be unique.
      ASSERT_FALSE([identifiers containsObject:identifier]);
      [identifiers addObject:identifier];
      web_state_list_->InsertWebState(i, std::move(web_state),
                                      WebStateList::INSERT_FORCE_INDEX,
                                      WebStateOpener());
    }
    original_identifiers_ = [identifiers copy];
    web_state_list_->ActivateWebStateAt(1);
    original_selected_identifier_ =
        TabIdTabHelper::FromWebState(web_state_list_->GetWebStateAt(1))
            ->tab_id();
    consumer_ = [[FakeConsumer alloc] init];
    mediator_ = [[TabGridMediator alloc] initWithConsumer:consumer_];
    mediator_.tabModel = tab_model_;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
  std::unique_ptr<TabIdFakeWebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  id tab_model_;
  FakeConsumer* consumer_;
  TabGridMediator* mediator_;
  NSSet<NSString*>* original_identifiers_;
  NSString* original_selected_identifier_;
};

#pragma mark - Consumer tests

// Tests that the consumer is populated after the tab model is set on the
// mediator.
TEST_F(TabGridMediatorTest, ConsumerPopulateItems) {
  EXPECT_EQ(3UL, consumer_.items.count);
  EXPECT_NSEQ(original_selected_identifier_, consumer_.selectedItemID);
}

// Tests that the consumer is notified when a web state is inserted.
TEST_F(TabGridMediatorTest, ConsumerInsertItem) {
  ASSERT_EQ(3UL, consumer_.items.count);
  auto web_state = std::make_unique<web::TestWebState>();
  TabIdTabHelper::CreateForWebState(web_state.get());
  NSString* item_identifier =
      TabIdTabHelper::FromWebState(web_state.get())->tab_id();
  web_state_list_->InsertWebState(1, std::move(web_state),
                                  WebStateList::INSERT_FORCE_INDEX,
                                  WebStateOpener());
  EXPECT_EQ(4UL, consumer_.items.count);
  // The same ID should be selected after the insertion, since the new web state
  // wasn't selected.
  EXPECT_NSEQ(original_selected_identifier_, consumer_.selectedItemID);
  EXPECT_NSEQ(item_identifier, consumer_.items[1]);
  EXPECT_FALSE([original_identifiers_ containsObject:item_identifier]);
}

// Tests that the consumer is notified when a web state is removed.
// The selected web state at index 1 is removed. The web state originally
// at index 2 should be the new selected item.
TEST_F(TabGridMediatorTest, ConsumerRemoveItem) {
  web_state_list_->CloseWebStateAt(1, WebStateList::CLOSE_NO_FLAGS);
  EXPECT_EQ(2UL, consumer_.items.count);
  // Expect that a different web state is selected now.
  EXPECT_NSNE(original_selected_identifier_, consumer_.selectedItemID);
}

// Tests that the consumer is notified when the active web state is changed.
TEST_F(TabGridMediatorTest, ConsumerUpdateSelectedItem) {
  EXPECT_NSEQ(original_selected_identifier_, consumer_.selectedItemID);
  web_state_list_->ActivateWebStateAt(2);
  EXPECT_NSEQ(
      TabIdTabHelper::FromWebState(web_state_list_->GetWebStateAt(2))->tab_id(),
      consumer_.selectedItemID);
}

// Tests that the consumer is notified when a web state is replaced.
// The selected item is replaced, so the new selected item id should be the
// id of the new item.
TEST_F(TabGridMediatorTest, ConsumerReplaceItem) {
  auto new_web_state = std::make_unique<web::TestWebState>();
  TabIdTabHelper::CreateForWebState(new_web_state.get());
  NSString* new_item_identifier =
      TabIdTabHelper::FromWebState(new_web_state.get())->tab_id();
  web_state_list_->ReplaceWebStateAt(1, std::move(new_web_state));
  EXPECT_EQ(3UL, consumer_.items.count);
  EXPECT_NSEQ(new_item_identifier, consumer_.selectedItemID);
  EXPECT_NSEQ(new_item_identifier, consumer_.items[1]);
  EXPECT_FALSE([original_identifiers_ containsObject:new_item_identifier]);
}

// Tests that the consumer is notified when a web state is moved.
TEST_F(TabGridMediatorTest, ConsumerMoveItem) {
  NSString* item1 = consumer_.items[1];
  NSString* item2 = consumer_.items[2];
  web_state_list_->MoveWebStateAt(1, 2);
  EXPECT_NSEQ(item1, consumer_.items[2]);
  EXPECT_NSEQ(item2, consumer_.items[1]);
}

#pragma mark - Command tests

// Tests that the active index is updated when |-selectItemWithID:| is called.
// Tests that the consumer's selected index is updated.
TEST_F(TabGridMediatorTest, SelectItemCommand) {
  // Previous selected index is 1.
  NSString* identifier =
      TabIdTabHelper::FromWebState(web_state_list_->GetWebStateAt(2))->tab_id();
  [mediator_ selectItemWithID:identifier];
  EXPECT_EQ(2, web_state_list_->active_index());
  EXPECT_NSEQ(identifier, consumer_.selectedItemID);
}

// Tests that the |web_state_list_| count is decremented when
// |-closeItemWithID:| is called.
// Tests that the consumer's item count is also decremented.
TEST_F(TabGridMediatorTest, CloseItemCommand) {
  // Previously there were 3 items.
  NSString* identifier =
      TabIdTabHelper::FromWebState(web_state_list_->GetWebStateAt(0))->tab_id();
  [mediator_ closeItemWithID:identifier];
  EXPECT_EQ(2, web_state_list_->count());
  EXPECT_EQ(2UL, consumer_.items.count);
}

// Tests that the |web_state_list_| and consumer's list are empty when
// |-closeAllItems| is called. Tests that |-undoCloseAllItems| does not restore
// the |web_state_list_|.
TEST_F(TabGridMediatorTest, CloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ closeAllItems];
  EXPECT_EQ(0, web_state_list_->count());
  EXPECT_EQ(0UL, consumer_.items.count);
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(0, web_state_list_->count());
}

// Tests that the |web_state_list_| and consumer's list are empty when
// |-saveAndCloseAllItems| is called.
TEST_F(TabGridMediatorTest, SaveAndCloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  EXPECT_EQ(0, web_state_list_->count());
  EXPECT_EQ(0UL, consumer_.items.count);
}

// Tests that the |web_state_list_| is not restored to 3 items when
// |-undoCloseAllItems| is called after |-discardSavedClosedItems| is called.
TEST_F(TabGridMediatorTest, DiscardSavedClosedItemsCommand) {
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  [mediator_ discardSavedClosedItems];
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(0, web_state_list_->count());
  EXPECT_EQ(0UL, consumer_.items.count);
}

// Tests that the |web_state_list_| is restored to 3 items when
// |-undoCloseAllItems| is called.
TEST_F(TabGridMediatorTest, UndoCloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(3, web_state_list_->count());
  EXPECT_EQ(3UL, consumer_.items.count);
  EXPECT_TRUE([original_identifiers_ containsObject:consumer_.items[0]]);
  EXPECT_TRUE([original_identifiers_ containsObject:consumer_.items[1]]);
  EXPECT_TRUE([original_identifiers_ containsObject:consumer_.items[2]]);
}

// Tests that when |-addNewItem| is called, the |web_state_list_| count is
// incremented, the |active_index| is at the end of |web_state_list_|, the new
// web state has no opener, and the URL is the New Tab Page.
// Tests that the consumer has added an item with the correct identifier.
TEST_F(TabGridMediatorTest, AddNewItemAtEndCommand) {
  // Previously there were 3 items and the selected index was 1.
  [mediator_ addNewItem];
  EXPECT_EQ(4, web_state_list_->count());
  EXPECT_EQ(3, web_state_list_->active_index());
  web::WebState* web_state = web_state_list_->GetWebStateAt(3);
  ASSERT_TRUE(web_state);
  EXPECT_EQ(web_state->GetBrowserState(), browser_state_.get());
  EXPECT_FALSE(web_state->HasOpener());
  if (web::features::UseWKWebViewLoading()) {
    // The URL of pending item (i.e. kChromeUINewTabURL) will not be returned
    // here because WebState doesn't load the URL until it's visible and
    // NavigationManager::GetVisibleURL requires WebState::IsLoading to be true
    // to return pending item's URL.
    EXPECT_EQ("", web_state->GetVisibleURL().spec());
  } else {
    EXPECT_EQ(kChromeUINewTabURL, web_state->GetVisibleURL().spec());
  }
  NSString* identifier = TabIdTabHelper::FromWebState(web_state)->tab_id();
  EXPECT_FALSE([original_identifiers_ containsObject:identifier]);
  // Consumer checks.
  EXPECT_EQ(4UL, consumer_.items.count);
  EXPECT_NSEQ(identifier, consumer_.selectedItemID);
  EXPECT_NSEQ(identifier, consumer_.items[3]);
}

// Tests that when |-insertNewItemAtIndex:| is called, the |web_state_list_|
// count is incremented, the |active_index| is the newly added index, the new
// web state has no opener, and the URL is the new tab page.
// Checks that the consumer has added an item with the correct identifier.
TEST_F(TabGridMediatorTest, InsertNewItemCommand) {
  // Previously there were 3 items and the selected index was 1.
  [mediator_ insertNewItemAtIndex:0];
  EXPECT_EQ(4, web_state_list_->count());
  EXPECT_EQ(0, web_state_list_->active_index());
  web::WebState* web_state = web_state_list_->GetWebStateAt(0);
  ASSERT_TRUE(web_state);
  EXPECT_EQ(web_state->GetBrowserState(), browser_state_.get());
  EXPECT_FALSE(web_state->HasOpener());
  if (web::features::UseWKWebViewLoading()) {
    // The URL of pending item (i.e. kChromeUINewTabURL) will not be returned
    // here because WebState doesn't load the URL until it's visible and
    // NavigationManager::GetVisibleURL requires WebState::IsLoading to be true
    // to return pending item's URL.
    EXPECT_EQ("", web_state->GetVisibleURL().spec());
  } else {
    EXPECT_EQ(kChromeUINewTabURL, web_state->GetVisibleURL().spec());
  }
  NSString* identifier = TabIdTabHelper::FromWebState(web_state)->tab_id();
  EXPECT_FALSE([original_identifiers_ containsObject:identifier]);
  // Consumer checks.
  EXPECT_EQ(4UL, consumer_.items.count);
  EXPECT_NSEQ(identifier, consumer_.selectedItemID);
  EXPECT_NSEQ(identifier, consumer_.items[0]);
}

// Tests that |-insertNewItemAtIndex:| is a no-op if the mediator's TabModel
// is nil.
TEST_F(TabGridMediatorTest, InsertNewItemWithNoTabModelCommand) {
  mediator_.tabModel = nil;
  ASSERT_EQ(3, web_state_list_->count());
  ASSERT_EQ(1, web_state_list_->active_index());
  [mediator_ insertNewItemAtIndex:0];
  EXPECT_EQ(3, web_state_list_->count());
  EXPECT_EQ(1, web_state_list_->active_index());
}

// Tests that when |-moveItemFromIndex:toIndex:| is called, there is no change
// in the item count in |web_state_list_|, but that the constituent web states
// have been reordered.
TEST_F(TabGridMediatorTest, MoveItemCommand) {
  // Capture ordered original IDs.
  NSMutableArray<NSString*>* pre_move_ids = [[NSMutableArray alloc] init];
  for (int i = 0; i < 3; i++) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(i);
    [pre_move_ids addObject:TabIdTabHelper::FromWebState(web_state)->tab_id()];
  }
  NSString* pre_move_selected_id =
      pre_move_ids[web_state_list_->active_index()];
  // Items start ordered [A, B, C].
  [mediator_ moveItemWithID:consumer_.items[0] toIndex:2];
  // Items should now be ordered [B, C, A] -- the pre-move identifiers should
  // still be in this order.
  // Item count hasn't changed.
  EXPECT_EQ(3, web_state_list_->count());
  // Active index has moved -- it was 1, now 0.
  EXPECT_EQ(0, web_state_list_->active_index());
  // Identifier at 0, 1, 2 should match the original_identifier_ at 1, 2, 0.
  for (int index = 0; index < 2; index++) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(index);
    ASSERT_TRUE(web_state);
    NSString* identifier = TabIdTabHelper::FromWebState(web_state)->tab_id();
    EXPECT_NSEQ(identifier, pre_move_ids[(index + 1) % 3]);
    EXPECT_NSEQ(identifier, consumer_.items[index]);
  }
  EXPECT_EQ(pre_move_selected_id, consumer_.selectedItemID);
}
