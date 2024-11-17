// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_mediator.h"

#import "base/test/metrics/user_action_tester.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/types.h"
#import "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#import "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#import "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_sync_service_observer_bridge.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/test/fake_tab_grid_toolbars_mediator.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

const char* kSelectTabGroupsUMA = "MobileTabGridSelectTabGroups";

}  // namespace

@interface FakeTabGroupsPanelConsumer : NSObject <TabGroupsPanelConsumer>
@property(nonatomic, readonly, copy) NSArray<TabGroupsPanelItem*>* items;
@property(nonatomic, readonly) NSUInteger populateItemsCallCount;
@property(nonatomic, readonly) NSUInteger reconfigureItemCallCount;
@end

@implementation FakeTabGroupsPanelConsumer

#pragma mark TabGroupsPanelConsumer

- (void)populateItems:(NSArray<TabGroupsPanelItem*>*)items {
  _items = [items copy];
  _populateItemsCallCount++;
}

- (void)reconfigureItem:(TabGroupsPanelItem*)item {
  _reconfigureItemCallCount++;
}

- (void)dismissModals {
}

@end

class TabGroupsPanelMediatorTest : public PlatformTest {
 protected:
  TabGroupsPanelMediatorTest() : web_state_list_(&web_state_list_delegate_) {
    profile_ = TestProfileIOS::Builder().Build();
    // Create a regular browser.
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
    browser_list_->AddBrowser(browser_.get());
    mode_holder_ = [[TabGridModeHolder alloc] init];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<BrowserList> browser_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  ::testing::NiceMock<tab_groups::MockTabGroupSyncService>
      tab_group_sync_service_;
  TabGridModeHolder* mode_holder_;
};

// Tests that the service observation starts and stops when the mediator is
// released.
TEST_F(TabGroupsPanelMediatorTest, StartStopObserving_Released) {
  // Use a strict mock.
  tab_groups::MockTabGroupSyncService strict_tab_group_sync_service;
  // Expect the observation start.
  EXPECT_CALL(strict_tab_group_sync_service, AddObserver(_)).Times(1);
  __unused TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&strict_tab_group_sync_service
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];

  // Expect the observation end when the mediator is released.
  EXPECT_CALL(strict_tab_group_sync_service, RemoveObserver(_)).Times(1);
  mediator = nil;
}

// Tests that the service observation starts and stops when the mediator is
// released.
TEST_F(TabGroupsPanelMediatorTest, StartStopObserving_Disconnect) {
  // Use a strict mock.
  tab_groups::MockTabGroupSyncService strict_tab_group_sync_service;
  // Expect the observation start.
  EXPECT_CALL(strict_tab_group_sync_service, AddObserver(_)).Times(1);
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&strict_tab_group_sync_service
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];

  {
    EXPECT_CALL(strict_tab_group_sync_service, RemoveObserver(_)).Times(1);
    [mediator disconnect];
  }
}

// Tests that the UMA for selecting the Tab Groups panel is correctly recorded.
TEST_F(TabGroupsPanelMediatorTest, RecordUMAWhenSelected) {
  base::UserActionTester user_action_tester;
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];

  EXPECT_EQ(0, user_action_tester.GetActionCount(kSelectTabGroupsUMA));

  // Select a different page.
  [mediator currentlySelectedGrid:NO];
  EXPECT_EQ(0, user_action_tester.GetActionCount(kSelectTabGroupsUMA));

  // Select Tab Groups.
  [mediator currentlySelectedGrid:YES];
  EXPECT_EQ(1, user_action_tester.GetActionCount(kSelectTabGroupsUMA));

  // Unselect Tab Groups.
  [mediator currentlySelectedGrid:NO];
  EXPECT_EQ(1, user_action_tester.GetActionCount(kSelectTabGroupsUMA));
}

// Tests that when the panel not the selected one, no toolbar delegate is set,
// no toolbar config is returned.
TEST_F(TabGroupsPanelMediatorTest, NotSelected_NoToolbarsDelegateOrConfig) {
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  FakeTabGridToolbarsMediator* toolbars_mutator =
      [[FakeTabGridToolbarsMediator alloc] init];
  mediator.toolbarsMutator = toolbars_mutator;

  [mediator currentlySelectedGrid:NO];

  EXPECT_EQ(toolbars_mutator.delegate, nil);
  EXPECT_EQ(toolbars_mutator.configuration, nil);
}

// Tests that when the panel is disabled by policy, the toolbars config is the
// disabled one.
TEST_F(TabGroupsPanelMediatorTest, DisabledByPolicy_DisabledToolbarsConfig) {
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:YES
                      browserList:browser_list_];
  FakeTabGridToolbarsMediator* toolbars_mutator =
      [[FakeTabGridToolbarsMediator alloc] init];
  mediator.toolbarsMutator = toolbars_mutator;

  [mediator currentlySelectedGrid:YES];

  EXPECT_EQ(toolbars_mutator.delegate,
            static_cast<id<TabGridToolbarsGridDelegate>>(mediator));
  EXPECT_NE(toolbars_mutator.configuration, nil);
  EXPECT_EQ(TabGridPageTabGroups, toolbars_mutator.configuration.page);

  // All buttons are disabled.
  EXPECT_FALSE(toolbars_mutator.configuration.doneButton);

  EXPECT_FALSE(toolbars_mutator.configuration.selectAllButton);
  EXPECT_EQ(0u, toolbars_mutator.configuration.selectedItemsCount);
  EXPECT_FALSE(toolbars_mutator.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.shareButton);
  EXPECT_FALSE(toolbars_mutator.configuration.addToButton);

  EXPECT_FALSE(toolbars_mutator.configuration.closeAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.newTabButton);
  EXPECT_FALSE(toolbars_mutator.configuration.searchButton);
  EXPECT_FALSE(toolbars_mutator.configuration.selectTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.undoButton);
  EXPECT_FALSE(toolbars_mutator.configuration.deselectAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.cancelSearchButton);
}

// Tests that when the panel is selected and not disabled by policy, the
// toolbars delegate and config are set accordingly. Since there are no tab in
// the Regular Tabs page, the Done button is still disabled.
TEST_F(TabGroupsPanelMediatorTest,
       EnabledByPolicyAndSelectedButNoRegularTab_DoneButtonDisabled) {
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  FakeTabGridToolbarsMediator* toolbars_mutator =
      [[FakeTabGridToolbarsMediator alloc] init];
  mediator.toolbarsMutator = toolbars_mutator;

  [mediator currentlySelectedGrid:YES];

  EXPECT_EQ(toolbars_mutator.delegate,
            static_cast<id<TabGridToolbarsGridDelegate>>(mediator));
  EXPECT_NE(toolbars_mutator.configuration, nil);
  EXPECT_EQ(TabGridPageTabGroups, toolbars_mutator.configuration.page);

  // Done button is disabled.
  EXPECT_FALSE(toolbars_mutator.configuration.doneButton);

  // All other buttons are disabled.
  EXPECT_FALSE(toolbars_mutator.configuration.selectAllButton);
  EXPECT_EQ(0u, toolbars_mutator.configuration.selectedItemsCount);
  EXPECT_FALSE(toolbars_mutator.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.shareButton);
  EXPECT_FALSE(toolbars_mutator.configuration.addToButton);

  EXPECT_FALSE(toolbars_mutator.configuration.closeAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.newTabButton);
  EXPECT_FALSE(toolbars_mutator.configuration.searchButton);
  EXPECT_FALSE(toolbars_mutator.configuration.selectTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.undoButton);
  EXPECT_FALSE(toolbars_mutator.configuration.deselectAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.cancelSearchButton);
}

// Tests that when the panel is selected and not disabled by policy, the
// toolbars delegate and config are set accordingly. Since there is a Regular
// tab, the Done button is finally enabled.
TEST_F(TabGroupsPanelMediatorTest,
       EnabledByPolicyAndSelectedWithRegularTab_DoneButtonEnabled) {
  // Add a web state in a local scope. Otherwise, the NiceMock for the sync
  // service reports an uninteresting call to RemoveObserver (??).
  {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_list_.InsertWebState(std::move(web_state));
  }
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  FakeTabGridToolbarsMediator* toolbars_mutator =
      [[FakeTabGridToolbarsMediator alloc] init];
  mediator.toolbarsMutator = toolbars_mutator;

  [mediator currentlySelectedGrid:YES];

  EXPECT_EQ(toolbars_mutator.delegate,
            static_cast<id<TabGridToolbarsGridDelegate>>(mediator));
  EXPECT_NE(toolbars_mutator.configuration, nil);
  EXPECT_EQ(TabGridPageTabGroups, toolbars_mutator.configuration.page);

  // Done button is enabled.
  EXPECT_TRUE(toolbars_mutator.configuration.doneButton);

  // All other buttons are disabled.
  EXPECT_FALSE(toolbars_mutator.configuration.selectAllButton);
  EXPECT_EQ(0u, toolbars_mutator.configuration.selectedItemsCount);
  EXPECT_FALSE(toolbars_mutator.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.shareButton);
  EXPECT_FALSE(toolbars_mutator.configuration.addToButton);

  EXPECT_FALSE(toolbars_mutator.configuration.closeAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.newTabButton);
  EXPECT_FALSE(toolbars_mutator.configuration.searchButton);
  EXPECT_FALSE(toolbars_mutator.configuration.selectTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.undoButton);
  EXPECT_FALSE(toolbars_mutator.configuration.deselectAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.cancelSearchButton);
}

// Tests that setting a consumer before the service initialization doesn't
// populate the consumer
TEST_F(TabGroupsPanelMediatorTest,
       SetConsumerDoesntPopulatesFromUninitializedService) {
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  // Prepare a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  EXPECT_NSEQ(consumer.items, nil);
  EXPECT_EQ(consumer.populateItemsCallCount, 0u);
  // Set a saved tab group.
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  std::vector<tab_groups::SavedTabGroup> groups = {group};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(groups));

  mediator.consumer = consumer;

  EXPECT_NSEQ(consumer.items, nil);
  EXPECT_EQ(consumer.populateItemsCallCount, 0u);
}

// Tests that setting a consumer after the service initialization populates it
// with data from the service's GetAllGroups API.
TEST_F(TabGroupsPanelMediatorTest,
       SetConsumerPopulatesFromInitializedAndNonEmptyService) {
  tab_groups::TabGroupSyncService::Observer* observer = nullptr;
  EXPECT_CALL(tab_group_sync_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Prepare a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  EXPECT_NSEQ(consumer.items, nil);
  EXPECT_EQ(consumer.populateItemsCallCount, 0u);
  // Set a saved tab group.
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  std::vector<tab_groups::SavedTabGroup> groups = {group};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(groups));
  // Initialize the service.
  observer->OnInitialized();

  // Set the consumer.
  mediator.consumer = consumer;

  EXPECT_EQ(consumer.items.count, 1u);
  EXPECT_EQ(consumer.populateItemsCallCount, 1u);
}

// Tests that setting a consumer populates it with data from the service's
// GetAllGroups API.
TEST_F(TabGroupsPanelMediatorTest,
       SetConsumerPopulatesFromInitializedEmptyService) {
  tab_groups::TabGroupSyncService::Observer* observer = nullptr;
  EXPECT_CALL(tab_group_sync_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Prepare a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  EXPECT_NSEQ(consumer.items, nil);
  EXPECT_EQ(consumer.populateItemsCallCount, 0u);
  // Set no saved tab group.
  std::vector<tab_groups::SavedTabGroup> groups = {};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(groups));
  // Initialize the service.
  observer->OnInitialized();

  // Set the consumer.
  mediator.consumer = consumer;

  EXPECT_EQ(consumer.items.count, 0u);
  EXPECT_EQ(consumer.populateItemsCallCount, 1u);
}

// Tests that when the sync service is initialized, the consumer is repopulated
// with data from the service's GetAllGroups API.
TEST_F(TabGroupsPanelMediatorTest,
       ServiceInitialized_RequeriesGroupsAndPopulates) {
  tab_groups::TabGroupSyncService::Observer* observer = nullptr;
  EXPECT_CALL(tab_group_sync_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Set no saved tab group.
  std::vector<tab_groups::SavedTabGroup> groups = {};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(groups));
  // Set a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  mediator.consumer = consumer;
  EXPECT_NSEQ(consumer.items, nil);
  EXPECT_EQ(consumer.populateItemsCallCount, 0u);
  // Set a saved tab group.
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  groups = {group};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(groups));

  observer->OnInitialized();

  EXPECT_EQ(consumer.items.count, 1u);
  EXPECT_EQ(consumer.populateItemsCallCount, 1u);
}

// Tests that the groups pushed to the consumer are sorted with the most recent
// first.
TEST_F(TabGroupsPanelMediatorTest, PopulatesSortedGroups) {
  tab_groups::TabGroupSyncService::Observer* observer = nullptr;
  EXPECT_CALL(tab_group_sync_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Set a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  mediator.consumer = consumer;
  // Set 2 saved tab group with the oldest first.
  tab_groups::SavedTabGroup group_1 = tab_groups::test::CreateTestSavedTabGroup(
      base::Time::Now() - base::Days(2));
  tab_groups::SavedTabGroup group_2 =
      tab_groups::test::CreateTestSavedTabGroup(base::Time::Now());
  std::vector<tab_groups::SavedTabGroup> groups = {group_1, group_2};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(groups));
  observer->OnInitialized();

  EXPECT_EQ(consumer.items.count, 2u);
  EXPECT_EQ(consumer.items[0].savedTabGroupID, group_2.saved_guid());
  EXPECT_EQ(consumer.items[1].savedTabGroupID, group_1.saved_guid());
}

// Tests that the consumer is asked to reconfigure an item when the observer
// notifies a group update.
TEST_F(TabGroupsPanelMediatorTest, UpdateGroup) {
  tab_groups::TabGroupSyncService::Observer* observer = nullptr;
  EXPECT_CALL(tab_group_sync_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Set a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  mediator.consumer = consumer;
  // Set a saved tab group.
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  std::vector<tab_groups::SavedTabGroup> groups = {group};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(groups));
  observer->OnInitialized();
  EXPECT_EQ(consumer.items.count, 1u);
  EXPECT_EQ(consumer.reconfigureItemCallCount, 0u);

  observer->OnTabGroupUpdated(group, tab_groups::TriggerSource::REMOTE);

  EXPECT_EQ(consumer.items.count, 1u);
  EXPECT_EQ(consumer.reconfigureItemCallCount, 1u);
}

// Tests that a group which doesn't exist locally is deleted from the tab group
// sync service.
TEST_F(TabGroupsPanelMediatorTest, DeleteRemoteGroup) {
  auto sync_service = std::make_unique<tab_groups::FakeTabGroupSyncService>();
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:sync_service.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];

  // Set a saved tab group.
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  sync_service->AddGroup(group);
  EXPECT_TRUE(sync_service->GetGroup(group.saved_guid()).has_value());

  TabGroupsPanelItem* item = [[TabGroupsPanelItem alloc] init];
  item.savedTabGroupID = group.saved_guid();
  [mediator deleteSyncedTabGroup:item.savedTabGroupID];

  EXPECT_FALSE(sync_service->GetGroup(group.saved_guid()).has_value());
}

// Tests that a group which exists locally is deleted from the tab group sync
// service.
TEST_F(TabGroupsPanelMediatorTest, DeleteLocalGroup) {
  auto sync_service = std::make_unique<tab_groups::FakeTabGroupSyncService>();
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:sync_service.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];

  // Create a web state.
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  browser_->GetWebStateList()->InsertWebState(
      std::move(fake_web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  // Create a tab group.
  tab_groups::TabGroupId tab_group_id = tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data = tab_groups::TabGroupVisualData(
      u"Test Group", tab_groups::TabGroupColorId::kGrey);
  const TabGroup* tab_group =
      browser_->GetWebStateList()->CreateGroup({0}, visual_data, tab_group_id);

  // Set a saved tab group with a local ID of the tab group created above.
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  group.SetLocalGroupId(tab_group->tab_group_id());
  sync_service->AddGroup(group);
  EXPECT_EQ(1u, browser_->GetWebStateList()->GetGroups().size());
  EXPECT_EQ(1, browser_->GetWebStateList()->count());

  TabGroupsPanelItem* item = [[TabGroupsPanelItem alloc] init];
  item.savedTabGroupID = group.saved_guid();
  [mediator deleteSyncedTabGroup:item.savedTabGroupID];

  // Check if the number of groups and tabs is 0.
  EXPECT_EQ(0u, browser_->GetWebStateList()->GetGroups().size());
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
}
