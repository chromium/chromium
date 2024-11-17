// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_coordinator.h"

#import "base/test/task_environment.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/testing_pref_service.h"
#import "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/disabled_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_container_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_main_tab_grid_delegate.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@class TabGridToolbarsConfiguration;

namespace {

// Creates a nice mock of TabGroupSyncService. It's "nice" since these tests
// don't really care what is called on the service, they just need to pass it
// down to the coordinator's mediator.
std::unique_ptr<KeyedService> CreateMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<
      ::testing::NiceMock<tab_groups::MockTabGroupSyncService>>();
}

}  // namespace

@interface TestDisabledGridViewControllerDelegate
    : NSObject <DisabledGridViewControllerDelegate>
@end

@implementation TestDisabledGridViewControllerDelegate

#pragma mark - DisabledGridViewControllerDelegate

- (void)didTapLinkWithURL:(const GURL&)URL {
  // No-op.
}

- (bool)isViewControllerSubjectToParentalControls {
  return false;
}

@end

@interface TestToolbarsMutator : NSObject <GridToolbarsMutator>
@end

@implementation TestToolbarsMutator

#pragma mark - GridToolbarsMutator

- (void)setToolbarConfiguration:(TabGridToolbarsConfiguration*)configuration {
  // No-op.
}

- (void)setToolbarsButtonsDelegate:(id<TabGridToolbarsGridDelegate>)delegate {
  // No-op.
}

- (void)setButtonsEnabled:(BOOL)enabled {
  // No-op.
}

@end

class TabGroupsPanelCoordinatorTest : public PlatformTest {
 protected:
  TabGroupsPanelCoordinatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        base::BindRepeating(&CreateMockSyncService));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    tab_grid_handler_mock_ = OCMProtocolMock(@protocol(TabGridCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:tab_grid_handler_mock_
                     forProtocol:@protocol(TabGridCommands)];

    base_view_controller_ = [[UIViewController alloc] init];
    toolbars_mutator_ = [[TestToolbarsMutator alloc] init];
    disabled_grid_view_controller_delegate_ =
        [[TestDisabledGridViewControllerDelegate alloc] init];
    coordinator_ = [[TabGroupsPanelCoordinator alloc]
            initWithBaseViewController:base_view_controller_
                        regularBrowser:browser_.get()
                       toolbarsMutator:toolbars_mutator_
        disabledViewControllerDelegate:disabled_grid_view_controller_delegate_];
  }

  // Needed for test profile created by TestBrowser().
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  TestToolbarsMutator* toolbars_mutator_;
  TabGroupsPanelCoordinator* coordinator_;
  TestDisabledGridViewControllerDelegate*
      disabled_grid_view_controller_delegate_;
  id tab_grid_handler_mock_;
};

// Tests that the mediator and view controllers are nil before `start`.
TEST_F(TabGroupsPanelCoordinatorTest, NilPropertiesBeforeStart) {
  EXPECT_EQ(nil, coordinator_.mediator);
  EXPECT_EQ(nil, coordinator_.gridViewController);
  EXPECT_EQ(nil, coordinator_.disabledViewController);
  EXPECT_EQ(nil, coordinator_.gridContainerViewController);
}

// Tests that with no Incognito mode policy, the third panel is Tab Groups.
TEST_F(TabGroupsPanelCoordinatorTest, NoIncognitoPolicy_TabGroupsShown) {
  [coordinator_ start];

  EXPECT_NE(nil, coordinator_.mediator);
  EXPECT_EQ(toolbars_mutator_, coordinator_.mediator.toolbarsMutator);
  EXPECT_NE(nil, coordinator_.gridViewController);
  EXPECT_EQ(nil, coordinator_.disabledViewController);
  EXPECT_NE(nil, coordinator_.gridContainerViewController);
  EXPECT_EQ(coordinator_.gridViewController,
            coordinator_.gridContainerViewController.containedViewController);
}

// Tests that with Incognito mode disabled by policy, the third panel is Tab
// Groups.
TEST_F(TabGroupsPanelCoordinatorTest, IncognitoDisabled_TabGroupsShown) {
  // Disable Incognito with policy.
  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kDisabled)));

  [coordinator_ start];

  EXPECT_NE(nil, coordinator_.mediator);
  EXPECT_EQ(toolbars_mutator_, coordinator_.mediator.toolbarsMutator);
  EXPECT_NE(nil, coordinator_.gridViewController);
  EXPECT_EQ(nil, coordinator_.disabledViewController);
  EXPECT_NE(nil, coordinator_.gridContainerViewController);
  EXPECT_EQ(coordinator_.gridViewController,
            coordinator_.gridContainerViewController.containedViewController);
}

// Tests that with Incognito mode forced by policy, the third panel is the
// disabled Tab Groups view.
TEST_F(TabGroupsPanelCoordinatorTest, IncognitoForced_TabGroupsDisabled) {
  // Force Incognito with policy.
  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kForced)));

  [coordinator_ start];

  EXPECT_NE(nil, coordinator_.mediator);
  EXPECT_EQ(toolbars_mutator_, coordinator_.mediator.toolbarsMutator);
  EXPECT_EQ(nil, coordinator_.gridViewController);
  EXPECT_NE(nil, coordinator_.disabledViewController);
  EXPECT_EQ(disabled_grid_view_controller_delegate_,
            coordinator_.disabledViewController.delegate);
  EXPECT_NE(nil, coordinator_.gridContainerViewController);
  EXPECT_EQ(coordinator_.disabledViewController,
            coordinator_.gridContainerViewController.containedViewController);
}
