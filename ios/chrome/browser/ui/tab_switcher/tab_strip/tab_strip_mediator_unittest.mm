// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_mediator.h"

#import "components/favicon/core/favicon_service.h"
#import "components/favicon/core/favicon_url.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_swift.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Fake consumer to get the passed value in tests.
@interface FakeTabStripConsumer : NSObject <TabStripConsumer>

@property(nonatomic, copy) NSArray<TabSwitcherItem*>* items;
@property(nonatomic, strong) TabSwitcherItem* selectedItem;
@property(nonatomic, strong) TabSwitcherItem* reloadedItem;

@end

@implementation FakeTabStripConsumer

- (void)populateWithItems:(NSArray<TabSwitcherItem*>*)items
             selectedItem:(TabSwitcherItem*)selectedItem {
  self.items = items;
  self.selectedItem = selectedItem;
}

- (void)selectItem:(TabSwitcherItem*)item {
  self.selectedItem = item;
}

- (void)reloadItem:(TabSwitcherItem*)item {
  self.reloadedItem = item;
}

- (void)replaceItem:(TabSwitcherItem*)oldItem
           withItem:(TabSwitcherItem*)newItem {
  NSMutableArray<TabSwitcherItem*>* replacedItems = [NSMutableArray array];
  for (NSUInteger index = 0; index < self.items.count; index++) {
    if ([self.items[index] isEqual:oldItem]) {
      [replacedItems addObject:newItem];
    } else {
      [replacedItems addObject:self.items[index]];
    }
  }
  self.items = replacedItems;
}

@end

// Test fixture for the TabStripMediator.
class TabStripMediatorTest : public PlatformTest {
 public:
  TabStripMediatorTest() {
    TestChromeBrowserState::Builder browser_state_builder;
    browser_state_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    browser_state_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());

    browser_state_ = browser_state_builder.Build();
    browser_ = std::make_unique<TestBrowser>(
        browser_state_.get(), std::make_unique<FakeWebStateListDelegate>());
    web_state_list_ = browser_->GetWebStateList();

    consumer_ = [[FakeTabStripConsumer alloc] init];
  }

  ~TabStripMediatorTest() override { [mediator_ disconnect]; }

  void InitializeMediator() {
    mediator_ = [[TabStripMediator alloc] initWithConsumer:consumer_];
    mediator_.browserState = browser_state_.get();
    mediator_.webStateList = web_state_list_;
  }

  void AddWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(browser_state_.get());
    favicon::WebFaviconDriver::CreateForWebState(
        web_state.get(),
        ios::FaviconServiceFactory::GetForBrowserState(
            browser_state_.get(), ServiceAccessType::IMPLICIT_ACCESS));

    web_state_list_->InsertWebState(0, std::move(web_state),
                                    WebStateList::INSERT_ACTIVATE,
                                    WebStateOpener());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  WebStateList* web_state_list_;
  TabStripMediator* mediator_;
  FakeTabStripConsumer* consumer_;
};

// Tests that the mediator correctly populates the consumer at startup and after
// an update of the WebStateList.
TEST_F(TabStripMediatorTest, ConsumerPopulated) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  ASSERT_NE(nil, consumer_.selectedItem);
  EXPECT_EQ(web_state_list_->GetActiveWebState()->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
  ASSERT_EQ(2ul, consumer_.items.count);
  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.items[0].identifier);
  EXPECT_EQ(web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier(),
            consumer_.items[1].identifier);

  // Check that the webstate is correctly added to the consumer.
  AddWebState();

  ASSERT_NE(nil, consumer_.selectedItem);
  EXPECT_EQ(web_state_list_->GetActiveWebState()->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
  ASSERT_EQ(3ul, consumer_.items.count);
  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.items[0].identifier);
  EXPECT_EQ(web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier(),
            consumer_.items[1].identifier);
  EXPECT_EQ(web_state_list_->GetWebStateAt(2)->GetUniqueIdentifier(),
            consumer_.items[2].identifier);

  // Check that the webstate is correctly removed from the consumer.
  web_state_list_->CloseWebStateAt(web_state_list_->active_index(),
                                   WebStateList::CLOSE_USER_ACTION);

  ASSERT_NE(nil, consumer_.selectedItem);
  EXPECT_EQ(web_state_list_->GetActiveWebState()->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
  ASSERT_EQ(2ul, consumer_.items.count);
  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.items[0].identifier);
  EXPECT_EQ(web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier(),
            consumer_.items[1].identifier);
}

// Tests that changing the selected tab is correctly reflected in the consumer.
TEST_F(TabStripMediatorTest, SelectTab) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());

  web_state_list_->ActivateWebStateAt(0);

  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
}

// Check that replacing a tab in the WebStateList is reflected in the TabStrip.
TEST_F(TabStripMediatorTest, ReplacedTab) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());

  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  web_state_list_->ReplaceWebStateAt(1, std::move(web_state));

  ASSERT_EQ(2, web_state_list_->count());

  EXPECT_EQ(web_state_id, consumer_.selectedItem.identifier);
  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.items[0].identifier);
  EXPECT_EQ(web_state_id, consumer_.items[1].identifier);
}

// Tests that closing a tab works.
TEST_F(TabStripMediatorTest, WebStateChange) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());
  ASSERT_EQ(2, web_state_list_->count());

  // Check title update.
  static_cast<web::FakeWebState*>(web_state_list_->GetWebStateAt(0))
      ->SetTitle(u"test test");
  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.reloadedItem.identifier);

  consumer_.reloadedItem = nil;

  // Check loading state update.
  static_cast<web::FakeWebState*>(web_state_list_->GetWebStateAt(1))
      ->SetLoading(true);
  EXPECT_EQ(web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier(),
            consumer_.reloadedItem.identifier);

  consumer_.reloadedItem = nil;

  // Check loading state update.
  static_cast<web::FakeWebState*>(web_state_list_->GetWebStateAt(1))
      ->SetLoading(false);
  EXPECT_EQ(web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier(),
            consumer_.reloadedItem.identifier);

  consumer_.reloadedItem = nil;

  // Check favicon update.
  favicon::WebFaviconDriver* driver = favicon::WebFaviconDriver::FromWebState(
      web_state_list_->GetWebStateAt(1));
  driver->OnFaviconUpdated(GURL(),
                           favicon::FaviconDriverObserver::TOUCH_LARGEST,
                           GURL(), false, gfx::Image());

  EXPECT_EQ(web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier(),
            consumer_.reloadedItem.identifier);
}

// Tests that adding a new tab works.
TEST_F(TabStripMediatorTest, AddTab) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());
  ASSERT_EQ(2, web_state_list_->count());

  [mediator_ addNewItem];

  EXPECT_EQ(2, web_state_list_->active_index());
  EXPECT_EQ(3, web_state_list_->count());

  EXPECT_EQ(web_state_list_->GetWebStateAt(2)->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
}

// Tests that activating a tab works.
TEST_F(TabStripMediatorTest, ActivateTab) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());
  ASSERT_EQ(2, web_state_list_->count());

  TabSwitcherItem* item = [[TabSwitcherItem alloc]
      initWithIdentifier:web_state_list_->GetWebStateAt(0)
                             ->GetUniqueIdentifier()];

  [mediator_ activateItem:item];

  EXPECT_EQ(0, web_state_list_->active_index());

  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
}

// Tests that closing a tab works.
TEST_F(TabStripMediatorTest, CloseTab) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());
  ASSERT_EQ(2, web_state_list_->count());

  TabSwitcherItem* item = [[TabSwitcherItem alloc]
      initWithIdentifier:web_state_list_->GetWebStateAt(1)
                             ->GetUniqueIdentifier()];
  [mediator_ closeItem:item];

  EXPECT_EQ(0, web_state_list_->active_index());
  EXPECT_EQ(1, web_state_list_->count());

  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
}
