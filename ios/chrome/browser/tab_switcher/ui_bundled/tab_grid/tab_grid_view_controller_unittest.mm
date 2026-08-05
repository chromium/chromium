// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_view_controller.h"

#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_container_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/pinned_tabs/pinned_tabs_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_mutator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_new_tab_button.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_top_toolbar.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Fake WebStateList delegate that attaches the required tab helper.
class TabGridFakeWebStateListDelegate : public FakeWebStateListDelegate {
 public:
  TabGridFakeWebStateListDelegate() {}
  ~TabGridFakeWebStateListDelegate() override {}

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override {
    SnapshotTabHelper::CreateForWebState(web_state);
    SnapshotSourceTabHelper::CreateForWebState(web_state);
  }
};

class TabGridViewControllerTest : public PlatformTest,
                                  public ::testing::WithParamInterface<bool> {
 protected:
  TabGridViewControllerTest() {
    InitializeViewController(TabGridPageConfiguration::kAllPagesEnabled);

    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(
        profile_.get(), std::make_unique<TabGridFakeWebStateListDelegate>());
    SnapshotBrowserAgent::CreateForBrowser(browser_.get());
  }
  ~TabGridViewControllerTest() override {}

  // Checks that `view_controller_` can perform the `action` with the given
  // `sender`.
  bool CanPerform(NSString* action, id sender) {
    return [view_controller_ canPerformAction:NSSelectorFromString(action)
                                   withSender:sender];
  }

  void InitializeViewController(TabGridPageConfiguration configuration) {
    view_controller_ =
        [[TabGridViewController alloc] initWithPageConfiguration:configuration];
    view_controller_.topToolbar =
        [[TabGridTopToolbar alloc] initWithLayoutGuideCenter:nil];
    view_controller_.bottomToolbar =
        [[TabGridBottomToolbar alloc] initWithFrame:CGRectZero];

    regular_grid_ = [[GridContainerViewController alloc] init];
    incognito_grid_ = [[GridContainerViewController alloc] init];
    third_panel_grid_ = [[GridContainerViewController alloc] init];
    view_controller_.incognitoGridContainerViewController = incognito_grid_;
    view_controller_.regularGridContainerViewController = regular_grid_;
    view_controller_.tabGroupsGridContainerViewController = third_panel_grid_;
    view_controller_.pinnedTabsViewController =
        [[PinnedTabsViewController alloc] init];

    tab_grid_state_ = [[TabGridState alloc] init];
    tab_grid_state_.tabGridVisible = YES;
    view_controller_.tabGridState = tab_grid_state_;

    view_controller_.mutator = mock_mutator_;
  }

  // Checks that `view_controller_` can perform the `action`. The sender is set
  // to nil when performing this check.
  bool CanPerform(NSString* action) { return CanPerform(action, nil); }

  void ExpectUMA(NSString* action, const std::string& user_action) {
    ASSERT_EQ(user_action_tester_.GetActionCount(user_action), 0);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    [view_controller_ performSelector:NSSelectorFromString(action)];
#pragma clang diagnostic pop
    EXPECT_EQ(user_action_tester_.GetActionCount(user_action), 1);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::UserActionTester user_action_tester_;
  TabGridViewController* view_controller_;
  TabGridState* tab_grid_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  GridContainerViewController* regular_grid_;
  GridContainerViewController* incognito_grid_;
  GridContainerViewController* third_panel_grid_;
  id mock_mutator_ = OCMProtocolMock(@protocol(TabGridMutator));
};

// Checks that TabGridViewController returns key commands.
TEST_F(TabGridViewControllerTest, ReturnsKeyCommands) {
  EXPECT_GT(view_controller_.keyCommands.count, 0u);
}

// Tests targetForAction resolves targets correctly for toolbar actions, VC
// actions, and unhandled actions.
TEST_F(TabGridViewControllerTest, TestTargetForAction) {
  id findTarget = [view_controller_ targetForAction:@selector(keyCommand_find)
                                         withSender:nil];
  EXPECT_EQ(findTarget, view_controller_.topToolbar);

  [view_controller_.topToolbar setCloseAllActionEnabled:YES];
  id closeAllTarget =
      [view_controller_ targetForAction:@selector(keyCommand_closeAll)
                             withSender:nil];
  EXPECT_EQ(closeAllTarget, view_controller_.topToolbar);

  id openTabTarget =
      [view_controller_ targetForAction:@selector(keyCommand_openNewRegularTab)
                             withSender:nil];
  EXPECT_EQ(openTabTarget, view_controller_);

  id unhandledTarget = [view_controller_ targetForAction:@selector(copy:)
                                              withSender:nil];
  EXPECT_EQ(unhandledTarget, nil);
}

// Checks whether TabGridViewController can perform the actions to open tabs.
TEST_F(TabGridViewControllerTest, CanPerform_OpenTabsActions) {
  NSArray<NSString*>* actions = @[
    @"keyCommand_openNewTab",
    @"keyCommand_openNewRegularTab",
    @"keyCommand_openNewIncognitoTab",
  ];

  [view_controller_ setCurrentPageAndPageControl:TabGridPageIncognitoTabs
                                        animated:NO];
  for (NSString* action in actions) {
    EXPECT_TRUE(CanPerform(action));
  }

  [view_controller_ setCurrentPageAndPageControl:TabGridPageRegularTabs
                                        animated:NO];
  for (NSString* action in actions) {
    EXPECT_TRUE(CanPerform(action));
  }

  [view_controller_ setCurrentPageAndPageControl:TabGridPageTabGroups
                                        animated:NO];
  for (NSString* action in actions) {
    EXPECT_FALSE(CanPerform(action));
  }

  [view_controller_ setCurrentPageAndPageControl:TabGridPageRegularTabs
                                        animated:NO];
  for (NSString* action in actions) {
    EXPECT_TRUE(CanPerform(action));
  }
}

// Checks that actions can't be performed when a modal is presented.
TEST_F(TabGridViewControllerTest, CantPerform_Actions_WhenModalPresented) {
  id mock_view_controller = OCMPartialMock(view_controller_);
  UIViewController* dummy_vc = [[UIViewController alloc] init];
  OCMStub([mock_view_controller presentedViewController]).andReturn(dummy_vc);

  EXPECT_FALSE(CanPerform(@"keyCommand_openNewTab"));
  EXPECT_FALSE(CanPerform(@"keyCommand_openNewRegularTab"));
  EXPECT_FALSE(CanPerform(@"keyCommand_openNewIncognitoTab"));
}

// Checks that opening regular tabs can't be performed when disabled.
TEST_F(TabGridViewControllerTest, CantPerform_OpenRegularTab_WhenDisabled) {
  InitializeViewController(TabGridPageConfiguration::kIncognitoPageOnly);

  EXPECT_FALSE(CanPerform(@"keyCommand_openNewRegularTab"));

  // Verify that incognito tabs can still be opened as a sanity check.
  EXPECT_TRUE(CanPerform(@"keyCommand_openNewIncognitoTab"));
}

// Checks that opening incognito tabs can't be performed when disabled.
TEST_F(TabGridViewControllerTest, CantPerform_OpenIncognitoTab_WhenDisabled) {
  InitializeViewController(TabGridPageConfiguration::kIncognitoPageDisabled);

  EXPECT_FALSE(CanPerform(@"keyCommand_openNewIncognitoTab"));

  // Verify that regular tabs can still be opened as a sanity check.
  EXPECT_TRUE(CanPerform(@"keyCommand_openNewRegularTab"));
}

// Checks that opening a tab on the current page can't be performed if the page
// is disabled.
TEST_F(TabGridViewControllerTest,
       CantPerform_OpenTab_OnCurrentPage_WhenDisabled) {
  InitializeViewController(TabGridPageConfiguration::kIncognitoPageDisabled);

  [view_controller_ setCurrentPageAndPageControl:TabGridPageIncognitoTabs
                                        animated:NO];

  EXPECT_FALSE(CanPerform(@"keyCommand_openNewTab"));
}

// Checks that TabGridViewController implements the following actions.
TEST_F(TabGridViewControllerTest, ImplementsActions) {
  // Load the view.
  std::ignore = view_controller_.view;

  [view_controller_ keyCommand_openNewTab];
  [view_controller_ keyCommand_openNewRegularTab];
  [view_controller_ keyCommand_openNewIncognitoTab];

  OCMStub([mock_mutator_ pageChanged:TabGridPageIncognitoTabs
                         interaction:TabSwitcherPageChangeInteraction::kNone]);
  [view_controller_ keyCommand_select1];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
  EXPECT_EQ(TabGridPageIncognitoTabs, view_controller_.currentPage);

  OCMStub([mock_mutator_ pageChanged:TabGridPageRegularTabs
                         interaction:TabSwitcherPageChangeInteraction::kNone]);
  [view_controller_ keyCommand_select2];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
  EXPECT_EQ(TabGridPageRegularTabs, view_controller_.currentPage);

  OCMStub([mock_mutator_ pageChanged:TabGridPageTabGroups
                         interaction:TabSwitcherPageChangeInteraction::kNone]);
  [view_controller_ keyCommand_select3];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
  EXPECT_EQ(TabGridPageTabGroups, view_controller_.currentPage);
}

// Checks that metrics are correctly reported.
TEST_F(TabGridViewControllerTest, Metrics) {
  // Load the view.
  std::ignore = view_controller_.view;
  ExpectUMA(@"keyCommand_openNewTab", "MobileKeyCommandOpenNewTab");
  ExpectUMA(@"keyCommand_openNewRegularTab",
            "MobileKeyCommandOpenNewRegularTab");
  ExpectUMA(@"keyCommand_openNewIncognitoTab",
            "MobileKeyCommandOpenNewIncognitoTab");
}

// Checks that `topToolbar` search bar is unfocused on `contentWillDisappear`.
TEST_F(TabGridViewControllerTest, UnfocusesSearchBarOnDisappear) {
  // Load the view.
  std::ignore = view_controller_.view;
  id mock_top_toolbar = OCMPartialMock(view_controller_.topToolbar);

  OCMExpect([mock_top_toolbar unfocusSearchBar]);

  [view_controller_ contentWillDisappearAnimated:NO];

  EXPECT_OCMOCK_VERIFY(mock_top_toolbar);
  [mock_top_toolbar stopMocking];
}

// Checks that `topToolbar` search bar is unfocused on page change.
TEST_F(TabGridViewControllerTest, UnfocusesSearchBarOnTransitionToTabGroups) {
  // Load the view.
  std::ignore = view_controller_.view;
  id mock_top_toolbar = OCMPartialMock(view_controller_.topToolbar);

  OCMExpect([mock_top_toolbar unfocusSearchBar]);

  [view_controller_ setCurrentPageAndPageControl:TabGridPageTabGroups
                                        animated:NO];

  EXPECT_OCMOCK_VERIFY(mock_top_toolbar);
  [mock_top_toolbar stopMocking];
}

}  // namespace
