// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_mock_clock_override.h"
#import "base/test/test_timeouts.h"
#import "base/time/time.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_view/ui_bundled/fake_browser_view_controller.h"
#import "ios/chrome/browser/browser_view/ui_bundled/safe_area_provider.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/main/ui/browser_layout_view_controller.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_coordinator_delegate.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@interface StubSceneState : SceneState

// Window for the associated scene, if any.
// This is redeclared relative to FakeScene.window, except this is now readwrite
// and backed by an instance variable.
@property(nonatomic, strong, readwrite) UIWindow* window;

@end

@implementation StubSceneState {
}

@synthesize window = _window;

@end

@interface TestTabGridCoordinatorDelegate
    : NSObject <TabGridCoordinatorDelegate>
@property(nonatomic) BOOL didEndCalled;
@end

@implementation TestTabGridCoordinatorDelegate

@synthesize didEndCalled = _didEndCalled;
- (void)tabGrid:(TabGridCoordinator*)tabGrid
    shouldActivateBrowser:(Browser*)browser
             focusOmnibox:(BOOL)focusOmnibox {
  // No-op.
}

- (void)tabGridDismissTransitionDidEnd:(TabGridCoordinator*)tabGrid {
  self.didEndCalled = YES;
}

@end

// Category to allow access for testing.
@interface TabGridCoordinator (Testing)
@property(nonatomic, readonly) UIViewController* viewController;
@end

namespace {

// Name of the directory where snapshots are saved.
const char kIdentifier[] = "Identifier";

void AddAgentsToBrowser(Browser* browser) {
  SnapshotBrowserAgent::CreateForBrowser(browser);
  SnapshotBrowserAgent::FromBrowser(browser)->SetSessionID(kIdentifier);
}

class TabGridCoordinatorTest : public BlockCleanupTest {
 public:
  void SetUp() override {
    BlockCleanupTest::SetUp();
    scene_state_ = [[StubSceneState alloc] initWithAppState:nil];

    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
      UIWindowScene* windowScene =
          base::apple::ObjCCastStrict<UIWindowScene>(scene);
      UIWindow* window = [windowScene.windows firstObject];
      if (window) {
        scene_state_.window = window;
        break;
      }
    }

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSChromeTabRestoreServiceFactory::GetInstance(),
        IOSChromeTabRestoreServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(ios::BookmarkModelFactory::GetInstance(),
                              ios::BookmarkModelFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        tab_groups::TabGroupSyncServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    bookmarks::test::WaitForBookmarkModelToLoad(
        ios::BookmarkModelFactory::GetForProfile(profile_.get()));

    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    // Set up SceneCommands mock.
    id mock_application_handler = OCMProtocolMock(@protocol(SceneCommands));
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mock_application_handler
                             forProtocol:@protocol(SceneCommands)];

    // Set up GeminiCommands mock.
    id mock_gemini_handler = OCMProtocolMock(@protocol(BWGCommands));
    [dispatcher startDispatchingToTarget:mock_gemini_handler
                             forProtocol:@protocol(BWGCommands)];

    // Set up QuickDeleteCommands mock.
    id mock_quick_delete_handler_ =
        OCMStrictProtocolMock(@protocol(QuickDeleteCommands));
    [dispatcher startDispatchingToTarget:mock_quick_delete_handler_
                             forProtocol:@protocol(QuickDeleteCommands)];

    AddAgentsToBrowser(browser_.get());

    incognito_browser_ = std::make_unique<TestBrowser>(
        profile_->GetOffTheRecordProfile(), scene_state_);

    AddAgentsToBrowser(incognito_browser_.get());

    id mockSceneHandler = OCMProtocolMock(@protocol(SceneCommands));
    IncognitoReauthSceneAgent* reauth_agent = [[IncognitoReauthSceneAgent alloc]
        initWithReauthModule:[[ReauthenticationModule alloc] init]
                sceneHandler:mockSceneHandler];
    [scene_state_ addAgent:reauth_agent];

    coordinator_ = [[TabGridCoordinator alloc]
        initWithSceneCommandsEndpoint:mockSceneHandler
                       regularBrowser:browser_.get()
                      inactiveBrowser:browser_->CreateInactiveBrowser()
                     incognitoBrowser:incognito_browser_.get()];
    coordinator_.animationsDisabledForTesting = YES;

    // TabGridCoordinator will make its view controller the root, so stash the
    // original root view controller before starting `coordinator_`.
    original_root_view_controller_ = [GetAnyKeyWindow() rootViewController];

    delegate_ = [[TestTabGridCoordinatorDelegate alloc] init];
    coordinator_.delegate = delegate_;

    [coordinator_ start];

    normal_tab_view_controller_ = [[FakeBrowserViewController alloc] init];
    incognito_tab_view_controller_ = [[FakeBrowserViewController alloc] init];
    incognito_tab_view_controller_.view.frame = CGRectMake(40, 40, 10, 10);

    safe_area_provider_ =
        [[SafeAreaProvider alloc] initWithBrowser:browser_.get()];
    incognito_safe_area_provider_ =
        [[SafeAreaProvider alloc] initWithBrowser:incognito_browser_.get()];

    layout_view_controller_ = [[BrowserLayoutViewController alloc] init];
    layout_view_controller_.safeAreaProvider = safe_area_provider_;
    incognito_layout_view_controller_ =
        [[BrowserLayoutViewController alloc] init];
    incognito_layout_view_controller_.incognito = YES;
    incognito_layout_view_controller_.safeAreaProvider =
        incognito_safe_area_provider_;
  }

  void TearDown() override {
    if (original_root_view_controller_) {
      GetAnyKeyWindow().rootViewController = original_root_view_controller_;
      original_root_view_controller_ = nil;
    }
    [coordinator_ stop];
  }

  UIViewController* GetViewController() { return coordinator_.viewController; }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;

  // Browser for the coordinator.
  std::unique_ptr<Browser> browser_;

  // Incognito browser for the coordinator.
  std::unique_ptr<Browser> incognito_browser_;

  // Scene state emulated in this test.
  StubSceneState* scene_state_;

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
  FakeBrowserViewController* normal_tab_view_controller_;
  FakeBrowserViewController* incognito_tab_view_controller_;
  BrowserLayoutViewController* layout_view_controller_;
  BrowserLayoutViewController* incognito_layout_view_controller_;
  SafeAreaProvider* safe_area_provider_;
  SafeAreaProvider* incognito_safe_area_provider_;

  // Used to test logging the time spent in tab grid.
  base::HistogramTester histogram_tester_;
  base::ScopedMockClockOverride scoped_clock_;
};

// Tests that the tab grid view controller is the initial active view
// controller.
TEST_F(TabGridCoordinatorTest, InitialActiveViewController) {
  EXPECT_EQ(GetViewController(), coordinator_.activeViewController);
}

// Tests that it is possible to set a BrowserLayoutViewController without first
// setting a TabSwitcher.
TEST_F(TabGridCoordinatorTest, BrowserLayoutViewControllerBeforeTabSwitcher) {
  layout_view_controller_.browserViewController = normal_tab_view_controller_;
  [coordinator_ showBrowserLayoutViewController:layout_view_controller_
                                      incognito:NO
                                     completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);

  // Now setting a TabSwitcher will make the switcher active.
  [coordinator_ showTabGridPage:TabGridPageIncognitoTabs];
  bool tab_switcher_active = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return GetViewController() == coordinator_.activeViewController;
      });
  EXPECT_TRUE(tab_switcher_active);
}

// Tests that it is possible to set a BrowserLayoutViewController after setting
// a TabSwitcher.
TEST_F(TabGridCoordinatorTest, BrowserLayoutViewControllerAfterTabSwitcher) {
  [coordinator_ showTabGridPage:TabGridPageIncognitoTabs];
  EXPECT_EQ(GetViewController(), coordinator_.activeViewController);

  layout_view_controller_.browserViewController = normal_tab_view_controller_;
  [coordinator_ showBrowserLayoutViewController:layout_view_controller_
                                      incognito:NO
                                     completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);

  [coordinator_ showTabGridPage:TabGridPageIncognitoTabs];
  bool tab_switcher_active = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return GetViewController() == coordinator_.activeViewController;
      });
  EXPECT_TRUE(tab_switcher_active);
}

// Tests swapping between two BrowserLayoutViewControllers.
TEST_F(TabGridCoordinatorTest, SwapBrowserLayoutViewControllers) {
  layout_view_controller_.browserViewController = normal_tab_view_controller_;
  [coordinator_ showBrowserLayoutViewController:layout_view_controller_
                                      incognito:NO
                                     completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);

  incognito_layout_view_controller_.browserViewController =
      incognito_tab_view_controller_;
  [coordinator_
      showBrowserLayoutViewController:incognito_layout_view_controller_
                            incognito:YES
                           completion:nil];
  EXPECT_EQ(incognito_tab_view_controller_, coordinator_.activeViewController);
}

// Tests calling showTabSwitcher twice in a row with the same VC.
TEST_F(TabGridCoordinatorTest, ShowTabSwitcherTwice) {
  [coordinator_ showTabGridPage:TabGridPageIncognitoTabs];
  EXPECT_EQ(GetViewController(), coordinator_.activeViewController);

  [coordinator_ showTabGridPage:TabGridPageIncognitoTabs];
  EXPECT_EQ(GetViewController(), coordinator_.activeViewController);
}

// Tests calling showBrowserLayoutViewController twice in a row with the same
// VC.
TEST_F(TabGridCoordinatorTest, ShowBrowserLayoutViewControllerTwice) {
  layout_view_controller_.browserViewController = normal_tab_view_controller_;
  [coordinator_ showBrowserLayoutViewController:layout_view_controller_
                                      incognito:NO
                                     completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);

  [coordinator_ showBrowserLayoutViewController:layout_view_controller_
                                      incognito:NO
                                     completion:nil];
  EXPECT_EQ(normal_tab_view_controller_, coordinator_.activeViewController);
}

// Tests that setting the active view controller work and that completion
// handlers are called properly after the new view controller is made active.
TEST_F(TabGridCoordinatorTest, CompletionHandlers) {
  // Setup: show the switcher.
  [coordinator_ showTabGridPage:TabGridPageIncognitoTabs];

  // Tests that the completion handler is called when showing a browser layout
  // view controller. Tests that the delegate 'didEnd' method is also called.
  delegate_.didEndCalled = NO;
  __block BOOL completion_handler_was_called = NO;
  layout_view_controller_.browserViewController = normal_tab_view_controller_;
  [coordinator_ showBrowserLayoutViewController:layout_view_controller_
                                      incognito:NO
                                     completion:^{
                                       completion_handler_was_called = YES;
                                     }];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^bool() {
        return completion_handler_was_called;
      }));
  ASSERT_TRUE(completion_handler_was_called);
  EXPECT_TRUE(delegate_.didEndCalled);

  // Tests that the completion handler is called when replacing an existing
  // browser layout view controller. Tests that the delegate 'didEnd' method is
  // *not* called.
  delegate_.didEndCalled = NO;
  incognito_layout_view_controller_.browserViewController =
      incognito_tab_view_controller_;
  [coordinator_
      showBrowserLayoutViewController:incognito_layout_view_controller_
                            incognito:YES
                           completion:^{
                             completion_handler_was_called = YES;
                           }];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^bool() {
        return completion_handler_was_called;
      }));
  ASSERT_TRUE(completion_handler_was_called);
  EXPECT_FALSE(delegate_.didEndCalled);
}

// Tests that the tab grid coordinator sizes its view controller to the window.
TEST_F(TabGridCoordinatorTest, SizeTabGridCoordinatorViewController) {
  CGRect rect = [UIScreen mainScreen].bounds;
  EXPECT_TRUE(CGRectEqualToRect(rect, GetViewController().view.frame));
}

// Tests that the time spent in the tab grid is correctly logged.
TEST_F(TabGridCoordinatorTest, TimeSpentInTabGrid) {
  histogram_tester_.ExpectTotalCount("IOS.TabSwitcher.TimeSpent", 0);
  scoped_clock_.Advance(base::Minutes(1));
  [coordinator_ showTabGridPage:TabGridPageIncognitoTabs];
  histogram_tester_.ExpectTotalCount("IOS.TabSwitcher.TimeSpent", 0);
  scoped_clock_.Advance(base::Seconds(20));
  layout_view_controller_.browserViewController = normal_tab_view_controller_;
  [coordinator_ showBrowserLayoutViewController:layout_view_controller_
                                      incognito:NO
                                     completion:nil];
  histogram_tester_.ExpectUniqueTimeSample("IOS.TabSwitcher.TimeSpent",
                                           base::Seconds(20), 1);
  histogram_tester_.ExpectTotalCount("IOS.TabSwitcher.TimeSpent", 1);
}

// Test that the tab grid coordinator reports the tab grid as the main interface
// correctly.
TEST_F(TabGridCoordinatorTest, tabGridActive) {
  // tabGridActive is false until the first appearance.
  EXPECT_FALSE(coordinator_.tabGridActive);

  layout_view_controller_.browserViewController = normal_tab_view_controller_;
  [coordinator_ showBrowserLayoutViewController:layout_view_controller_
                                      incognito:NO
                                     completion:nil];
  EXPECT_FALSE(coordinator_.tabGridActive);

  [coordinator_ showTabGridPage:TabGridPageIncognitoTabs];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return coordinator_.tabGridActive;
      }));
}

}  // namespace
