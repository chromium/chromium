// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator.h"

#import <UIKit/UIKit.h>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_mock_clock_override.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#include "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator_delegate.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/test/block_cleanup_test.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest_mac.h"
#include "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface StubSceneState : SceneState
@end

@implementation StubSceneState {
  UIWindow* _window;
}
- (void)setWindow:(UIWindow*)window {
  _window = window;
}
- (UIWindow*)window {
  return _window;
}
@end

@interface TestTabGridCoordinatorDelegate
    : NSObject <TabGridCoordinatorDelegate>
@property(nonatomic) BOOL didEndCalled;
@end

@implementation TestTabGridCoordinatorDelegate
@synthesize didEndCalled = _didEndCalled;
- (void)tabGrid:(TabGridCoordinator*)tabGrid
    shouldActivateBrowser:(Browser*)browser
           dismissTabGrid:(BOOL)dismissTabGrid
             focusOmnibox:(BOOL)focusOmnibox {
  // No-op.
}

- (void)tabGridDismissTransitionDidEnd:(TabGridCoordinator*)tabGrid {
  self.didEndCalled = YES;
}

- (TabGridPage)activePageForTabGrid:(TabGridCoordinator*)tabGrid {
  return TabGridPageRegularTabs;
}
@end

namespace {

void AddAgentsToBrowser(Browser* browser, SceneState* scene_state) {
  SnapshotBrowserAgent::CreateForBrowser(browser);
  SnapshotBrowserAgent::FromBrowser(browser)->SetSessionID(
      [[NSUUID UUID] UUIDString]);
  SceneStateBrowserAgent::CreateForBrowser(browser, scene_state);
}

class TabGridCoordinatorTest : public BlockCleanupTest {
 public:
  void SetUp() override {
    BlockCleanupTest::SetUp();
    scene_state_ = [[StubSceneState alloc] initWithAppState:nil];
    scene_state_.window =
        [[UIApplication sharedApplication].windows firstObject];

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        IOSChromeTabRestoreServiceFactory::GetInstance(),
        IOSChromeTabRestoreServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    chrome_browser_state_ = test_cbs_builder.Build();

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());

    AddAgentsToBrowser(browser_.get(), scene_state_);

    incognito_browser_ = std::make_unique<TestBrowser>();
    AddAgentsToBrowser(incognito_browser_.get(), scene_state_);

    UIWindow* window = GetAnyKeyWindow();
    coordinator_ = [[TabGridCoordinator alloc]
                     initWithWindow:window
         applicationCommandEndpoint:OCMProtocolMock(
                                        @protocol(ApplicationCommands))
        browsingDataCommandEndpoint:OCMProtocolMock(
                                        @protocol(BrowsingDataCommands))
                     regularBrowser:browser_.get()
                   incognitoBrowser:incognito_browser_.get()];
    coordinator_.animationsDisabledForTesting = YES;
    // TabGirdCoordinator will make its view controller the root, so stash the
    // original root view controller before starting |coordinator_|.
    original_root_view_controller_ = [GetAnyKeyWindow() rootViewController];

    [coordinator_ start];

    delegate_ = [[TestTabGridCoordinatorDelegate alloc] init];
    coordinator_.delegate = delegate_;

    normal_tab_view_controller_ = [[UIViewController alloc] init];
    normal_tab_view_controller_.view.frame = CGRectMake(20, 20, 10, 10);

    incognito_tab_view_controller_ = [[UIViewController alloc] init];
    incognito_tab_view_controller_.view.frame = CGRectMake(40, 40, 10, 10);
  }

  void TearDown() override {
    if (original_root_view_controller_) {
      GetAnyKeyWindow().rootViewController = original_root_view_controller_;
      original_root_view_controller_ = nil;
    }
    [coordinator_ stop];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  // Browser for the coordinator.
  std::unique_ptr<Browser> browser_;

  // Browser for the coordinator.
  std::unique_ptr<Browser> incognito_browser_;

  // Scene state emulated in this test.
  SceneState* scene_state_;

  // The TabGridCoordinator that is under test.  The test fixture sets
  // this VC as the root VC for the window.
  TabGridCoordinator* coordinator_;

  // Delegate for the coordinator's TabSwitcher interface.
  TestTabGridCoordinatorDelegate* delegate_;

  // The key window's original root view controller, which must be restored at
  // the end of the test.
  UIViewController* original_root_view_controller_;

  // The following view controllers are created by the test fixture and are
  // available for use in tests.
  UIViewController* normal_tab_view_controller_;
  UIViewController* incognito_tab_view_controller_;

  // Used to test logging the time spent in tab grid.
  base::HistogramTester histogram_tester_;
  base::ScopedMockClockOverride scoped_clock_;
};

// Tests that the tab grid view controller is the initial active view
// controller.
TEST_F(TabGridCoordinatorTest, InitialActiveViewController) {
  EXPECT_EQ(coordinator_.baseViewController, coordinator_.activeViewController);
}

// Tests that it is possible to set a TabViewController without first setting a
// TabSwitcher.
TEST_F(TabGridCoordinatorTest, TabViewControllerBeforeTabSwitcher) {
  [coordinator_ showTabViewController:normal_tab_view_controller_
                   shouldCloseTabGrid:YES
                           completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);

  // Now setting a TabSwitcher will make the switcher active.
  [coordinator_ showTabGrid];
  bool tab_switcher_active = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return coordinator_.baseViewController ==
               coordinator_.activeViewController;
      });
  EXPECT_TRUE(tab_switcher_active);
}

// Tests that it is possible to set a TabViewController after setting a
// TabSwitcher.
TEST_F(TabGridCoordinatorTest, TabViewControllerAfterTabSwitcher) {
  [coordinator_ showTabGrid];
  EXPECT_EQ(coordinator_.baseViewController, coordinator_.activeViewController);

  [coordinator_ showTabViewController:normal_tab_view_controller_
                   shouldCloseTabGrid:YES
                           completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);

  // Showing the TabSwitcher again will make it active.
  [coordinator_ showTabGrid];
  bool tab_switcher_active = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return coordinator_.baseViewController ==
               coordinator_.activeViewController;
      });
  EXPECT_TRUE(tab_switcher_active);
}

// Tests swapping between two TabViewControllers.
TEST_F(TabGridCoordinatorTest, SwapTabViewControllers) {
  [coordinator_ showTabViewController:normal_tab_view_controller_
                   shouldCloseTabGrid:YES
                           completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);

  [coordinator_ showTabViewController:incognito_tab_view_controller_
                   shouldCloseTabGrid:YES
                           completion:nil];
  EXPECT_EQ(incognito_tab_view_controller_, coordinator_.activeViewController);
}

// Tests calling showTabSwitcher twice in a row with the same VC.
TEST_F(TabGridCoordinatorTest, ShowTabSwitcherTwice) {
  [coordinator_ showTabGrid];
  EXPECT_EQ(coordinator_.baseViewController, coordinator_.activeViewController);

  [coordinator_ showTabGrid];
  EXPECT_EQ(coordinator_.baseViewController, coordinator_.activeViewController);
}

// Tests calling showTabViewController twice in a row with the same VC.
TEST_F(TabGridCoordinatorTest, ShowTabViewControllerTwice) {
  [coordinator_ showTabViewController:normal_tab_view_controller_
                   shouldCloseTabGrid:YES
                           completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);

  [coordinator_ showTabViewController:normal_tab_view_controller_
                   shouldCloseTabGrid:YES
                           completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);
}

// Tests that setting the active view controller work and that completion
// handlers are called properly after the new view controller is made active.
TEST_F(TabGridCoordinatorTest, CompletionHandlers) {
  // Setup: show the switcher.
  [coordinator_ showTabGrid];

  // Tests that the completion handler is called when showing a tab view
  // controller. Tests that the delegate 'didEnd' method is also called.
  delegate_.didEndCalled = NO;
  __block BOOL completion_handler_was_called = NO;
  [coordinator_ showTabViewController:normal_tab_view_controller_
                   shouldCloseTabGrid:YES
                           completion:^{
                             completion_handler_was_called = YES;
                           }];
  base::test::ios::WaitUntilCondition(^bool() {
    return completion_handler_was_called;
  });
  ASSERT_TRUE(completion_handler_was_called);
  EXPECT_TRUE(delegate_.didEndCalled);

  // Tests that the completion handler is called when replacing an existing tab
  // view controller. Tests that the delegate 'didEnd' method is *not* called.
  delegate_.didEndCalled = NO;
  [coordinator_ showTabViewController:incognito_tab_view_controller_
                   shouldCloseTabGrid:YES
                           completion:^{
                             completion_handler_was_called = YES;
                           }];
  base::test::ios::WaitUntilCondition(^bool() {
    return completion_handler_was_called;
  });
  ASSERT_TRUE(completion_handler_was_called);
  EXPECT_FALSE(delegate_.didEndCalled);
}

// Tests that the tab grid coordinator sizes its view controller to the window.
TEST_F(TabGridCoordinatorTest, SizeTabGridCoordinatorViewController) {
  CGRect rect = [UIScreen mainScreen].bounds;
  EXPECT_TRUE(
      CGRectEqualToRect(rect, coordinator_.baseViewController.view.frame));
}

// Tests that the time spent in the tab grid is correctly logged.
TEST_F(TabGridCoordinatorTest, TimeSpentInTabGrid) {
  histogram_tester_.ExpectTotalCount("IOS.TabSwitcher.TimeSpent", 0);
  scoped_clock_.Advance(base::TimeDelta::FromMinutes(1));
  [coordinator_ showTabGrid];
  histogram_tester_.ExpectTotalCount("IOS.TabSwitcher.TimeSpent", 0);
  scoped_clock_.Advance(base::TimeDelta::FromSeconds(20));
  [coordinator_ showTabViewController:normal_tab_view_controller_
                   shouldCloseTabGrid:YES
                           completion:nil];
  histogram_tester_.ExpectUniqueTimeSample("IOS.TabSwitcher.TimeSpent",
                                           base::TimeDelta::FromSeconds(20), 1);
  histogram_tester_.ExpectTotalCount("IOS.TabSwitcher.TimeSpent", 1);
}

// Test that the tab grid coordinator reports the tab grid as the main interface
// correctly.
TEST_F(TabGridCoordinatorTest, tabGridActive) {
  // tabGridActive is false until the first appearance.
  EXPECT_FALSE(coordinator_.tabGridActive);

  [coordinator_ showTabViewController:normal_tab_view_controller_
                   shouldCloseTabGrid:YES
                           completion:nil];
  EXPECT_FALSE(coordinator_.tabGridActive);

  [coordinator_ showTabGrid];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return coordinator_.tabGridActive;
      }));
}

}  // namespace
