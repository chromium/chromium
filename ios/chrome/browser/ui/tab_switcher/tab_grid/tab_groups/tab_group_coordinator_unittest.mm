// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_coordinator.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/data_sharing/public/features.h"
#import "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_view_controller.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using tab_groups::TabGroupId;

namespace {

// Fake WebStateList delegate that attaches the required tab helper.
class TabGroupCoordinatorFakeWebStateListDelegate
    : public FakeWebStateListDelegate {
 public:
  TabGroupCoordinatorFakeWebStateListDelegate() {}
  ~TabGroupCoordinatorFakeWebStateListDelegate() override {}

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override {
    SnapshotTabHelper::CreateForWebState(web_state);
  }
};

// Creates a FakeTabGroupSyncService.
std::unique_ptr<KeyedService> CreateFakeTabGroupSyncService(
    web::BrowserState* context) {
  return std::make_unique<tab_groups::FakeTabGroupSyncService>();
}

class TabGroupCoordinatorTest : public PlatformTest {
 protected:
  TabGroupCoordinatorTest() {
    feature_list_.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});
  }

  void SetUp() override {
    PlatformTest::SetUp();
    // Create a TestProfileIOS with required services.
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        base::BindRepeating(&CreateFakeTabGroupSyncService));
    profile_ = std::move(builder).Build();

    tab_group_sync_service_ = static_cast<tab_groups::FakeTabGroupSyncService*>(
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile_.get()));

    browser_ = std::make_unique<TestBrowser>(
        profile_.get(),
        std::make_unique<TabGroupCoordinatorFakeWebStateListDelegate>());
    SnapshotBrowserAgent::CreateForBrowser(browser_.get());

    tab_groups_commands_handler_ =
        OCMProtocolMock(@protocol(TabGroupsCommands));
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:tab_groups_commands_handler_
                             forProtocol:@protocol(TabGroupsCommands)];

    base_view_controller_ = [[UIViewController alloc] init];

    // Create a group with one tab.
    tab_groups::TabGroupVisualData temporaryVisualData(
        u"Test group", tab_groups::TabGroupColorId::kCyan);
    WebStateList* web_state_list = browser_->GetWebStateList();
    web_state_list->InsertWebState(
        std::make_unique<web::FakeWebState>(web::WebStateID::NewUnique()),
        WebStateList::InsertionParams::Automatic().Activate());
    const TabGroup* group = web_state_list->CreateGroup(
        {0}, temporaryVisualData, TabGroupId::GenerateNew());

    // Make the group a SavedTabGroup in the TabGroupSyncService, if available.
    if (tab_group_sync_service_) {
      // Make a SavedTabGroupTab matching the first tab.
      base::Uuid saved_group_id = base::Uuid::GenerateRandomV4();
      web::WebStateID tab_id =
          web_state_list->GetWebStateAt(0)->GetUniqueIdentifier();
      tab_groups::SavedTabGroupTab first_tab(
          GURL("http://example.com"), u"Some tab", saved_group_id,
          std::make_optional(0), base::Uuid::GenerateRandomV4(),
          tab_id.identifier());
      // Make a SavedTabGroup matching the first group.
      tab_groups::LocalTabGroupID local_group_id =
          web_state_list->GetGroupOfWebStateAt(0)->tab_group_id();
      tab_groups::SavedTabGroup saved_group(
          u"first title", tab_groups::TabGroupColorId::kBlue, {first_tab},
          std::make_optional(0), saved_group_id, local_group_id);
      // Add the SavedTabGroup to the TabGroupSyncService.
      tab_group_sync_service_->AddGroup(saved_group);
    }

    coordinator_ = [[TabGroupCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                          tabGroup:group];
    mode_holder_ = [[TabGridModeHolder alloc] init];
    coordinator_.modeHolder = mode_holder_;
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

  // Needed for test profile created by TestBrowser().
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<tab_groups::FakeTabGroupSyncService> tab_group_sync_service_;
  std::unique_ptr<TestBrowser> browser_;
  id<TabGroupsCommands> tab_groups_commands_handler_;
  UIViewController* base_view_controller_;
  TabGridModeHolder* mode_holder_;
  TabGroupCoordinator* coordinator_;
};

// Same as TabGroupCoordinatorTest, but with the Join-only Shared Tab Groups
// feature enabled.
class TabGroupCoordinatorWithSharedTabGroupsJoinOnlyTest
    : public TabGroupCoordinatorTest {
 protected:
  TabGroupCoordinatorWithSharedTabGroupsJoinOnlyTest() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        {kTabGroupsIPad, kModernTabStrip, kTabGroupSync,
         data_sharing::features::kDataSharingJoinOnly},
        {});
  }
};

// Same as TabGroupCoordinatorTest, but with the Shared Tab Groups feature
// fully enabled.
class TabGroupCoordinatorWithSharedTabGroupsTest
    : public TabGroupCoordinatorTest {
 protected:
  TabGroupCoordinatorWithSharedTabGroupsTest() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        {kTabGroupsIPad, kModernTabStrip, kTabGroupSync,
         data_sharing::features::kDataSharingFeature},
        {});
  }
};

// Checks that the coordinator and its view controller are created upon start.
// The face pile is nil because Shared Tab Groups is not enabled.
TEST_F(TabGroupCoordinatorTest, Started) {
  [coordinator_ start];

  EXPECT_NE(coordinator_, nil);
  EXPECT_NE(coordinator_.viewController, nil);
  EXPECT_EQ(coordinator_.viewController.facePile, nil);
}

// Checks that the coordinator and its view controller are created upon start.
// The face pile is nil because the user can't share, just join.
TEST_F(TabGroupCoordinatorWithSharedTabGroupsJoinOnlyTest, Started) {
  [coordinator_ start];

  EXPECT_NE(coordinator_, nil);
  EXPECT_NE(coordinator_.viewController, nil);
  EXPECT_EQ(coordinator_.viewController.facePile, nil);
}

// Checks that the coordinator and its view controller are created upon start.
// The face pile is not nil because the user can start sharing.
TEST_F(TabGroupCoordinatorWithSharedTabGroupsTest, Started) {
  [coordinator_ start];

  EXPECT_NE(coordinator_, nil);
  EXPECT_NE(coordinator_.viewController, nil);
  EXPECT_NE(coordinator_.viewController.facePile, nil);
}

// Checks that the face pile is not nil because the group is shared.
TEST_F(TabGroupCoordinatorWithSharedTabGroupsJoinOnlyTest,
       Started_GroupIsShared) {
  // Share the group.
  WebStateList* web_state_list = browser_->GetWebStateList();
  tab_groups::LocalTabGroupID tab_group_id =
      web_state_list->GetGroupOfWebStateAt(0)->tab_group_id();
  tab_group_sync_service_->MakeTabGroupShared(tab_group_id, "foo");

  [coordinator_ start];

  EXPECT_NE(coordinator_.viewController.facePile, nil);
}

// Checks that the face pile is not nil because the group is shared.
TEST_F(TabGroupCoordinatorWithSharedTabGroupsTest, Started_GroupIsShared) {
  // Share the group.
  WebStateList* web_state_list = browser_->GetWebStateList();
  tab_groups::LocalTabGroupID tab_group_id =
      web_state_list->GetGroupOfWebStateAt(0)->tab_group_id();
  tab_group_sync_service_->MakeTabGroupShared(tab_group_id, "foo");

  [coordinator_ start];

  EXPECT_NE(coordinator_.viewController.facePile, nil);
}

}  // namespace
