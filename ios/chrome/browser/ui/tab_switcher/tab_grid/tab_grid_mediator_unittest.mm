// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mediator.h"

#import <Foundation/Foundation.h>
#import <memory>

#import "base/mac/foundation_util.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/sessions/core/live_tab.h"
#import "components/sessions/core/session_id.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/sessions/core/tab_restore_service_helper.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/commerce/shopping_persisted_data_tab_helper.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/sessions/fake_tab_restore_service.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/sync/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/tabs/closing_web_state_observer_browser_agent.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/session_state/web_session_state_tab_helper.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_client.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;

namespace sessions {
class TabRestoreServiceObserver;
class LiveTabContext;
}

namespace {

const char kPriceTrackingWithOptimizationGuideParam[] =
    "price_tracking_with_optimization_guide";
const char kHasPriceDropUserAction[] = "Commerce.TabGridSwitched.HasPriceDrop";
const char kHasNoPriceDropUserAction[] = "Commerce.TabGridSwitched.NoPriceDrop";

// Timeout for waiting for the TabCollectionConsumer updates.
constexpr base::TimeDelta kWaitForTabCollectionConsumerUpdateTimeout =
    base::Seconds(1);

std::unique_ptr<KeyedService> BuildFakeTabRestoreService(
    web::BrowserState* browser_state) {
  return std::make_unique<FakeTabRestoreService>();
}
}  // namespace

// Test object that conforms to TabCollectionConsumer and exposes inner state
// for test verification.
@interface FakeConsumer : NSObject <TabCollectionConsumer>
// The fake consumer only keeps the identifiers of items for simplicity
@property(nonatomic, strong) NSMutableArray<NSString*>* items;
@property(nonatomic, assign) NSString* selectedItemID;
@end
@implementation FakeConsumer
@synthesize items = _items;
@synthesize selectedItemID = _selectedItemID;

- (void)setItemsRequireAuthentication:(BOOL)require {
  // No-op.
}

- (void)populateItems:(NSArray<TabSwitcherItem*>*)items
       selectedItemID:(NSString*)selectedItemID {
  self.selectedItemID = selectedItemID;
  self.items = [NSMutableArray array];
  for (TabSwitcherItem* item in items) {
    [self.items addObject:item.identifier];
  }
}

- (void)insertItem:(TabSwitcherItem*)item
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

- (void)replaceItemID:(NSString*)itemID withItem:(TabSwitcherItem*)item {
  NSUInteger index = [self.items indexOfObject:itemID];
  self.items[index] = item.identifier;
}

- (void)moveItemWithID:(NSString*)itemID toIndex:(NSUInteger)toIndex {
  [self.items removeObject:itemID];
  [self.items insertObject:itemID atIndex:toIndex];
}

- (void)dismissModals {
  // No-op.
}

@end

// Fake WebStateList delegate that attaches the required tab helper.
class TabHelperFakeWebStateListDelegate : public FakeWebStateListDelegate {
 public:
  TabHelperFakeWebStateListDelegate() {}
  ~TabHelperFakeWebStateListDelegate() override {}

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override {
    // Create NTPTabHelper to ensure VisibleURL is set to kChromeUINewTabURL.
    id delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
    NewTabPageTabHelper::CreateForWebState(web_state);
    NewTabPageTabHelper::FromWebState(web_state)->SetDelegate(delegate);
    PagePlaceholderTabHelper::CreateForWebState(web_state);
    SnapshotTabHelper::CreateForWebState(web_state);
    WebSessionStateTabHelper::CreateForWebState(web_state);
  }
};

class TabGridMediatorTest : public PlatformTest {
 public:
  TabGridMediatorTest() {}
  ~TabGridMediatorTest() override {}

  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(IOSChromeTabRestoreServiceFactory::GetInstance(),
                              base::BindRepeating(BuildFakeTabRestoreService));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    // Price Drops are only available to signed in MSBB users.
    browser_state_->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
    id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
    ios::FakeChromeIdentityService* identity_service_ =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    identity_service_->AddIdentity(identity);
    auth_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
            browser_state_.get()));
    auth_service_->SignIn(identity);

    tab_restore_service_ =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(
            browser_state_.get());
    NSMutableSet<NSString*>* identifiers = [[NSMutableSet alloc] init];
    browser_ = std::make_unique<TestBrowser>(
        browser_state_.get(),
        std::make_unique<TabHelperFakeWebStateListDelegate>());
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
    ClosingWebStateObserverBrowserAgent::CreateForBrowser(browser_.get());
    SnapshotBrowserAgent::CreateForBrowser(browser_.get());
    SnapshotBrowserAgent::FromBrowser(browser_.get())
        ->SetSessionID([[NSUUID UUID] UUIDString]);
    browser_list_ =
        BrowserListFactory::GetForBrowserState(browser_state_.get());
    browser_list_->AddBrowser(browser_.get());

    // Insert some web states.
    std::vector<std::string> urls{"https://foo/bar", "https://car/tar",
                                  "https://hello/world"};
    for (int i = 0; i < 3; i++) {
      auto web_state = CreateFakeWebStateWithURL(GURL(urls[i]));
      NSString* identifier = web_state.get()->GetStableIdentifier();
      // Tab IDs should be unique.
      ASSERT_FALSE([identifiers containsObject:identifier]);
      [identifiers addObject:identifier];
      browser_->GetWebStateList()->InsertWebState(
          i, std::move(web_state), WebStateList::INSERT_FORCE_INDEX,
          WebStateOpener());
    }
    original_identifiers_ = [identifiers copy];
    browser_->GetWebStateList()->ActivateWebStateAt(1);
    original_selected_identifier_ =
        browser_->GetWebStateList()->GetWebStateAt(1)->GetStableIdentifier();
    consumer_ = [[FakeConsumer alloc] init];
    mediator_ = [[TabGridMediator alloc] initWithConsumer:consumer_];
    mediator_.browser = browser_.get();
    mediator_.tabRestoreService = tab_restore_service_;
  }

  // Creates a FakeWebState with a navigation history containing exactly only
  // the given `url`.
  std::unique_ptr<web::FakeWebState> CreateFakeWebStateWithURL(
      const GURL& url) {
    auto web_state = std::make_unique<web::FakeWebState>();
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(url, ui::PAGE_TRANSITION_LINK);
    navigation_manager->SetLastCommittedItem(
        navigation_manager->GetItemAtIndex(0));
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetBrowserState(browser_state_.get());
    web_state->SetNavigationItemCount(1);
    web_state->SetCurrentURL(url);
    SnapshotTabHelper::CreateForWebState(web_state.get());
    return web_state;
  }

  void TearDown() override {
    // Forces the TabGridMediator to removes its Observer from WebStateList
    // before the Browser is destroyed.
    mediator_.browser = nullptr;
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  // Prepare the mock method to restore the tabs.
  void PrepareForRestoration() {
    TestSessionService* test_session_service =
        [[TestSessionService alloc] init];
    SessionRestorationBrowserAgent::CreateForBrowser(browser_.get(),
                                                     test_session_service);
    SessionRestorationBrowserAgent::FromBrowser(browser_.get())
        ->SetSessionID([[NSUUID UUID] UUIDString]);
  }

  void SetFakePriceDrop(web::WebState* web_state) {
    auto price_drop =
        std::make_unique<ShoppingPersistedDataTabHelper::PriceDrop>();
    price_drop->current_price = @"$5";
    price_drop->previous_price = @"$10";
    price_drop->url = web_state->GetLastCommittedURL();
    price_drop->timestamp = base::Time::Now();
    ShoppingPersistedDataTabHelper::FromWebState(web_state)
        ->SetPriceDropForTesting(std::move(price_drop));
  }

  void SetPriceDropIndicatorsFlag() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{commerce::kCommercePriceTracking,
          {{kPriceTrackingWithOptimizationGuideParam, "true"}}}},
        {});
  }

  bool WaitForConsumerUpdates(size_t expected_count) {
    return WaitUntilConditionOrTimeout(
        kWaitForTabCollectionConsumerUpdateTimeout, ^{
          base::RunLoop().RunUntilIdle();
          return expected_count == consumer_.items.count;
        });
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  sessions::TabRestoreService* tab_restore_service_;
  id tab_model_;
  FakeConsumer* consumer_;
  TabGridMediator* mediator_;
  NSSet<NSString*>* original_identifiers_;
  NSString* original_selected_identifier_;
  std::unique_ptr<Browser> browser_;
  BrowserList* browser_list_;
  base::UserActionTester user_action_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
  AuthenticationService* auth_service_ = nullptr;
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
  auto web_state = std::make_unique<web::FakeWebState>();
  NSString* item_identifier = web_state.get()->GetStableIdentifier();
  browser_->GetWebStateList()->InsertWebState(1, std::move(web_state),
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
  browser_->GetWebStateList()->CloseWebStateAt(1, WebStateList::CLOSE_NO_FLAGS);
  EXPECT_EQ(2UL, consumer_.items.count);
  // Expect that a different web state is selected now.
  EXPECT_NSNE(original_selected_identifier_, consumer_.selectedItemID);
}

// Tests that the consumer is notified when the active web state is changed.
TEST_F(TabGridMediatorTest, ConsumerUpdateSelectedItem) {
  EXPECT_NSEQ(original_selected_identifier_, consumer_.selectedItemID);
  browser_->GetWebStateList()->ActivateWebStateAt(2);
  EXPECT_NSEQ(
      browser_->GetWebStateList()->GetWebStateAt(2)->GetStableIdentifier(),
      consumer_.selectedItemID);
}

// Tests that the consumer is notified when a web state is replaced.
// The selected item is replaced, so the new selected item id should be the
// id of the new item.
TEST_F(TabGridMediatorTest, ConsumerReplaceItem) {
  auto new_web_state = std::make_unique<web::FakeWebState>();
  NSString* new_item_identifier = new_web_state->GetStableIdentifier();
  @autoreleasepool {
    browser_->GetWebStateList()->ReplaceWebStateAt(1, std::move(new_web_state));
  }
  EXPECT_EQ(3UL, consumer_.items.count);
  EXPECT_NSEQ(new_item_identifier, consumer_.selectedItemID);
  EXPECT_NSEQ(new_item_identifier, consumer_.items[1]);
  EXPECT_FALSE([original_identifiers_ containsObject:new_item_identifier]);
}

// Tests that the consumer is notified when a web state is moved.
TEST_F(TabGridMediatorTest, ConsumerMoveItem) {
  NSString* item1 = consumer_.items[1];
  NSString* item2 = consumer_.items[2];
  browser_->GetWebStateList()->MoveWebStateAt(1, 2);
  EXPECT_NSEQ(item1, consumer_.items[2]);
  EXPECT_NSEQ(item2, consumer_.items[1]);
}

#pragma mark - Command tests

// Tests that the active index is updated when `-selectItemWithID:` is called.
// Tests that the consumer's selected index is updated.
TEST_F(TabGridMediatorTest, SelectItemCommand) {
  // Previous selected index is 1.
  NSString* identifier =
      browser_->GetWebStateList()->GetWebStateAt(2)->GetStableIdentifier();
  [mediator_ selectItemWithID:identifier];
  EXPECT_EQ(2, browser_->GetWebStateList()->active_index());
  EXPECT_NSEQ(identifier, consumer_.selectedItemID);
}

// Tests that the WebStateList count is decremented when
// `-closeItemWithID:` is called.
// Tests that the consumer's item count is also decremented.
TEST_F(TabGridMediatorTest, CloseItemCommand) {
  // Previously there were 3 items.
  NSString* identifier =
      browser_->GetWebStateList()->GetWebStateAt(0)->GetStableIdentifier();
  [mediator_ closeItemWithID:identifier];
  EXPECT_EQ(2, browser_->GetWebStateList()->count());
  EXPECT_EQ(2UL, consumer_.items.count);
}

// Tests that the WebStateList and consumer's list are empty when
// `-closeAllItems` is called. Tests that `-undoCloseAllItems` does not restore
// the WebStateList.
TEST_F(TabGridMediatorTest, CloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ closeAllItems];
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.count);
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
}

// Tests that the WebStateList and consumer's list are empty when
// `-saveAndCloseAllItems` is called.
TEST_F(TabGridMediatorTest, SaveAndCloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.count);
}

// Tests that the WebStateList is not restored to 3 items when
// `-undoCloseAllItems` is called after `-discardSavedClosedItems` is called.
TEST_F(TabGridMediatorTest, DiscardSavedClosedItemsCommand) {
  PrepareForRestoration();
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  [mediator_ discardSavedClosedItems];
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.count);
}

// Tests that the WebStateList is restored to 3 items when
// `-undoCloseAllItems` is called.
TEST_F(TabGridMediatorTest, UndoCloseAllItemsCommand) {
  PrepareForRestoration();
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(3, browser_->GetWebStateList()->count());
  EXPECT_EQ(3UL, consumer_.items.count);
  EXPECT_TRUE([original_identifiers_ containsObject:consumer_.items[0]]);
  EXPECT_TRUE([original_identifiers_ containsObject:consumer_.items[1]]);
  EXPECT_TRUE([original_identifiers_ containsObject:consumer_.items[2]]);
}

// Tests that the WebStateList is restored to 3 items when
// `-undoCloseAllItems` is called.
TEST_F(TabGridMediatorTest, UndoCloseAllItemsCommandWithNTP) {
  PrepareForRestoration();
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  // The three tabs created in the SetUp should be passed to the restore
  // service.
  EXPECT_EQ(3UL, tab_restore_service_->entries().size());
  std::set<SessionID::id_type> ids;
  for (auto& entry : tab_restore_service_->entries()) {
    ids.insert(entry->id.id());
  }
  EXPECT_EQ(3UL, ids.size());
  // There should be no tabs in the WebStateList.
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.count);

  // Add three new tabs.
  auto web_state1 = CreateFakeWebStateWithURL(GURL("https://test/url1"));
  browser_->GetWebStateList()->InsertWebState(0, std::move(web_state1),
                                              WebStateList::INSERT_FORCE_INDEX,
                                              WebStateOpener());
  // Second tab is a NTP.
  auto web_state2 = CreateFakeWebStateWithURL(GURL(kChromeUINewTabURL));
  browser_->GetWebStateList()->InsertWebState(1, std::move(web_state2),
                                              WebStateList::INSERT_FORCE_INDEX,
                                              WebStateOpener());
  auto web_state3 = CreateFakeWebStateWithURL(GURL("https://test/url2"));
  browser_->GetWebStateList()->InsertWebState(2, std::move(web_state3),
                                              WebStateList::INSERT_FORCE_INDEX,
                                              WebStateOpener());
  browser_->GetWebStateList()->ActivateWebStateAt(0);

  [mediator_ saveAndCloseAllItems];
  // The NTP should not be saved.
  EXPECT_EQ(5UL, tab_restore_service_->entries().size());
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.count);
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(3UL, tab_restore_service_->entries().size());
  EXPECT_EQ(3UL, consumer_.items.count);
  // Check the session entries were not changed.
  for (auto& entry : tab_restore_service_->entries()) {
    EXPECT_EQ(1UL, ids.count(entry->id.id()));
  }
}

// Tests that when `-addNewItem` is called, the WebStateList count is
// incremented, the `active_index` is at the end of WebStateList, the new
// web state has no opener, and the URL is the New Tab Page.
// Tests that the consumer has added an item with the correct identifier.
TEST_F(TabGridMediatorTest, AddNewItemAtEndCommand) {
  // Previously there were 3 items and the selected index was 1.
  [mediator_ addNewItem];
  EXPECT_EQ(4, browser_->GetWebStateList()->count());
  EXPECT_EQ(3, browser_->GetWebStateList()->active_index());
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(3);
  ASSERT_TRUE(web_state);
  EXPECT_EQ(web_state->GetBrowserState(), browser_state_.get());
  EXPECT_FALSE(web_state->HasOpener());
  // The URL of pending item (i.e. kChromeUINewTabURL) will not be returned
  // here because WebState doesn't load the URL until it's visible and
  // NavigationManager::GetVisibleURL requires WebState::IsLoading to be true
  // to return pending item's URL.
  EXPECT_EQ("", web_state->GetVisibleURL().spec());
  NSString* identifier = web_state->GetStableIdentifier();
  EXPECT_FALSE([original_identifiers_ containsObject:identifier]);
  // Consumer checks.
  EXPECT_EQ(4UL, consumer_.items.count);
  EXPECT_NSEQ(identifier, consumer_.selectedItemID);
  EXPECT_NSEQ(identifier, consumer_.items[3]);
}

// Tests that when `-insertNewItemAtIndex:` is called, the WebStateList
// count is incremented, the `active_index` is the newly added index, the new
// web state has no opener, and the URL is the new tab page.
// Checks that the consumer has added an item with the correct identifier.
TEST_F(TabGridMediatorTest, InsertNewItemCommand) {
  // Previously there were 3 items and the selected index was 1.
  [mediator_ insertNewItemAtIndex:0];
  EXPECT_EQ(4, browser_->GetWebStateList()->count());
  EXPECT_EQ(0, browser_->GetWebStateList()->active_index());
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(0);
  ASSERT_TRUE(web_state);
  EXPECT_EQ(web_state->GetBrowserState(), browser_state_.get());
  EXPECT_FALSE(web_state->HasOpener());
  // The URL of pending item (i.e. kChromeUINewTabURL) will not be returned
  // here because WebState doesn't load the URL until it's visible and
  // NavigationManager::GetVisibleURL requires WebState::IsLoading to be true
  // to return pending item's URL.
  EXPECT_EQ("", web_state->GetVisibleURL().spec());
  NSString* identifier = web_state->GetStableIdentifier();
  EXPECT_FALSE([original_identifiers_ containsObject:identifier]);
  // Consumer checks.
  EXPECT_EQ(4UL, consumer_.items.count);
  EXPECT_NSEQ(identifier, consumer_.selectedItemID);
  EXPECT_NSEQ(identifier, consumer_.items[0]);
}

// Tests that `-insertNewItemAtIndex:` is a no-op if the mediator's browser
// is bullptr.
TEST_F(TabGridMediatorTest, InsertNewItemWithNoBrowserCommand) {
  mediator_.browser = nullptr;
  ASSERT_EQ(3, browser_->GetWebStateList()->count());
  ASSERT_EQ(1, browser_->GetWebStateList()->active_index());
  [mediator_ insertNewItemAtIndex:0];
  EXPECT_EQ(3, browser_->GetWebStateList()->count());
  EXPECT_EQ(1, browser_->GetWebStateList()->active_index());
}

// Tests that when `-moveItemFromIndex:toIndex:` is called, there is no change
// in the item count in WebStateList, but that the constituent web states
// have been reordered.
TEST_F(TabGridMediatorTest, MoveItemCommand) {
  // Capture ordered original IDs.
  NSMutableArray<NSString*>* pre_move_ids = [[NSMutableArray alloc] init];
  for (int i = 0; i < 3; i++) {
    web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(i);
    [pre_move_ids addObject:web_state->GetStableIdentifier()];
  }
  NSString* pre_move_selected_id =
      pre_move_ids[browser_->GetWebStateList()->active_index()];
  // Items start ordered [A, B, C].
  [mediator_ moveItemWithID:consumer_.items[0] toIndex:2];
  // Items should now be ordered [B, C, A] -- the pre-move identifiers should
  // still be in this order.
  // Item count hasn't changed.
  EXPECT_EQ(3, browser_->GetWebStateList()->count());
  // Active index has moved -- it was 1, now 0.
  EXPECT_EQ(0, browser_->GetWebStateList()->active_index());
  // Identifier at 0, 1, 2 should match the original_identifier_ at 1, 2, 0.
  for (int index = 0; index < 2; index++) {
    web::WebState* web_state =
        browser_->GetWebStateList()->GetWebStateAt(index);
    ASSERT_TRUE(web_state);
    NSString* identifier = web_state->GetStableIdentifier();
    EXPECT_NSEQ(identifier, pre_move_ids[(index + 1) % 3]);
    EXPECT_NSEQ(identifier, consumer_.items[index]);
  }
  EXPECT_EQ(pre_move_selected_id, consumer_.selectedItemID);
}

// Tests that when `-searchItemsWithText:` is called, there is no change in the
// items in WebStateList and the correct items are populated by the consumer.
TEST_F(TabGridMediatorTest, SearchItemsWithTextCommand) {
  // Capture ordered original IDs.
  NSMutableArray<NSString*>* pre_search_ids = [[NSMutableArray alloc] init];
  for (int i = 0; i < 3; i++) {
    web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(i);
    [pre_search_ids addObject:web_state->GetStableIdentifier()];
  }
  NSString* expected_result_identifier =
      browser_->GetWebStateList()->GetWebStateAt(2)->GetStableIdentifier();

  [mediator_ searchItemsWithText:@"hello"];

  // Only one result should be found.
  EXPECT_TRUE(WaitForConsumerUpdates(1UL));
  EXPECT_NSEQ(expected_result_identifier, consumer_.items[0]);

  // Web states count should not change.
  EXPECT_EQ(3, browser_->GetWebStateList()->count());
  // Active index should not change.
  EXPECT_EQ(1, browser_->GetWebStateList()->active_index());
  // The order of the items should be the same.
  for (int i = 0; i < 3; i++) {
    web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(i);
    ASSERT_TRUE(web_state);
    NSString* identifier = web_state->GetStableIdentifier();
    EXPECT_NSEQ(identifier, pre_search_ids[i]);
  }
}

// Tests that when `-resetToAllItems:` is called, the consumer gets all the
// items from items in WebStateList and correct item selected.
TEST_F(TabGridMediatorTest, resetToAllItems) {
  ASSERT_EQ(3, browser_->GetWebStateList()->count());
  ASSERT_EQ(3UL, consumer_.items.count);

  [mediator_ searchItemsWithText:@"hello"];
  // Only 1 result is in the consumer after the search is done.
  ASSERT_TRUE(WaitForConsumerUpdates(1UL));

  [mediator_ resetToAllItems];
  // consumer should revert back to have the items from the webstate list.
  ASSERT_TRUE(WaitForConsumerUpdates(3UL));
  // Active index should not change.
  EXPECT_NSEQ(original_selected_identifier_, consumer_.selectedItemID);

  // The order of the items on the consumer be the exact same order as the in
  // WebStateList.
  for (int i = 0; i < 3; i++) {
    web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(i);
    ASSERT_TRUE(web_state);
    NSString* identifier = web_state->GetStableIdentifier();
    EXPECT_NSEQ(identifier, consumer_.items[i]);
  }
}

TEST_F(TabGridMediatorTest, TestSelectItemWithNoPriceDrop) {
  SetPriceDropIndicatorsFlag();
  web::WebState* web_state_to_select =
      browser_->GetWebStateList()->GetWebStateAt(2);
  // No need to set a null price drop - it will be null by default. Simply
  // need to create the helper.
  ShoppingPersistedDataTabHelper::CreateForWebState(web_state_to_select);
  [mediator_ selectItemWithID:web_state_to_select->GetStableIdentifier()];
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kHasNoPriceDropUserAction));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kHasPriceDropUserAction));
}

TEST_F(TabGridMediatorTest, TestSelectItemWithPriceDrop) {
  SetPriceDropIndicatorsFlag();
  web::WebState* web_state_to_select =
      browser_->GetWebStateList()->GetWebStateAt(2);
  ShoppingPersistedDataTabHelper::CreateForWebState(web_state_to_select);
  SetFakePriceDrop(web_state_to_select);
  [mediator_ selectItemWithID:web_state_to_select->GetStableIdentifier()];
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kHasPriceDropUserAction));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kHasNoPriceDropUserAction));
}

TEST_F(TabGridMediatorTest, TestSelectItemWithPriceDropExperimentOff) {
  web::WebState* web_state_to_select =
      browser_->GetWebStateList()->GetWebStateAt(2);
  [mediator_ selectItemWithID:web_state_to_select->GetStableIdentifier()];
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kHasNoPriceDropUserAction));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kHasPriceDropUserAction));
}
