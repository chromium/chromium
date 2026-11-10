// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/test/metrics/user_action_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_mediator.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_help_improve_table_view_controller.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/test/fakes/fake_ui_navigation_controller.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

@interface SuggestionsFromGeminiCoordinator (Testing) <
    SuggestionsFromGeminiMediatorDelegate>
@end

@interface SuggestionsFromGeminiTableViewController (Testing)
- (void)didTapLinkURL:(NSURL*)URL;
@end

@interface SuggestionsFromGeminiHelpImproveTableViewController (Testing)
- (void)didTapLinkURL:(NSURL*)URL;
@end

namespace {

class SuggestionsFromGeminiCoordinatorTest : public PlatformTest {
 protected:
  SuggestionsFromGeminiCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    mock_scene_commands_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_scene_commands_handler_
                     forProtocol:@protocol(SceneCommands)];
    navigation_controller_ = [[FakeUINavigationController alloc] init];
    coordinator_ = [[SuggestionsFromGeminiCoordinator alloc]
        initWithBaseNavigationController:navigation_controller_
                                 browser:browser_.get()];
  }

  ~SuggestionsFromGeminiCoordinatorTest() override { [coordinator_ stop]; }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  id mock_scene_commands_handler_;
  FakeUINavigationController* navigation_controller_;
  SuggestionsFromGeminiCoordinator* coordinator_;
};

// Tests that the coordinator starts and pushes its view controller.
TEST_F(SuggestionsFromGeminiCoordinatorTest, TestStartStop) {
  EXPECT_EQ(0u, navigation_controller_.viewControllers.count);
  [coordinator_ start];
  EXPECT_EQ(1u, navigation_controller_.viewControllers.count);
  [coordinator_ stop];
}

// Tests that the mediator delegate call triggers the SceneCommands
// closePresentedViewsAndOpenURL: method.
TEST_F(SuggestionsFromGeminiCoordinatorTest, TestManageConnectedAppsOpensUrl) {
  [coordinator_ start];

  OCMExpect([mock_scene_commands_handler_
      closePresentedViewsAndOpenURL:[OCMArg checkWithBlock:^BOOL(
                                                OpenNewTabCommand* cmd) {
        return cmd.URL == GURL(kGeminiExtensionsURL) && cmd.fromChrome;
      }]]);

  [coordinator_ suggestionsFromGeminiMediatorDidSelectConnectedApps:nil];

  EXPECT_OCMOCK_VERIFY(mock_scene_commands_handler_);
}

// Tests that opening the help improve page logs the correct user action.
TEST_F(SuggestionsFromGeminiCoordinatorTest,
       TestOpenHelpImproveReportsUserAction) {
  [coordinator_ start];

  base::UserActionTester user_action_tester;

  [coordinator_ suggestionsFromGeminiMediatorDidSelectHelpImprove:nil];

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.SuggestionsFromGeminiHelpImprove"));
}

// Tests that both the main SuggestionsFromGemini view controller and the Help
// Improve subpage view controller have their sceneHandler configured so
// tapping the "Learn more" link in the enterprise info popover opens the URL.
TEST_F(SuggestionsFromGeminiCoordinatorTest,
       TestEnterprisePopoverLinkOpensUrl) {
  EXPECT_EQ(0u, navigation_controller_.viewControllers.count);
  [coordinator_ start];
  ASSERT_EQ(1u, navigation_controller_.viewControllers.count);

  SuggestionsFromGeminiTableViewController* view_controller =
      static_cast<SuggestionsFromGeminiTableViewController*>(
          navigation_controller_.viewControllers.firstObject);
  ASSERT_NE(nil, view_controller);
  EXPECT_NE(nil, view_controller.sceneHandler);

  OCMExpect([mock_scene_commands_handler_
      closePresentedViewsAndOpenURL:[OCMArg checkWithBlock:^BOOL(
                                                OpenNewTabCommand* cmd) {
        return cmd.URL == GURL(kChromeUIManagementURL) && cmd.fromChrome;
      }]]);
  [view_controller
      didTapLinkURL:[NSURL URLWithString:@(kChromeUIManagementURL)]];
  EXPECT_OCMOCK_VERIFY(mock_scene_commands_handler_);

  [coordinator_ suggestionsFromGeminiMediatorDidSelectHelpImprove:nil];
  ASSERT_EQ(2u, navigation_controller_.viewControllers.count);

  SuggestionsFromGeminiHelpImproveTableViewController* help_improve_controller =
      static_cast<SuggestionsFromGeminiHelpImproveTableViewController*>(
          navigation_controller_.viewControllers.lastObject);
  ASSERT_NE(nil, help_improve_controller);
  EXPECT_NE(nil, help_improve_controller.sceneHandler);

  OCMExpect([mock_scene_commands_handler_
      closePresentedViewsAndOpenURL:[OCMArg checkWithBlock:^BOOL(
                                                OpenNewTabCommand* cmd) {
        return cmd.URL == GURL(kChromeUIManagementURL) && cmd.fromChrome;
      }]]);
  [help_improve_controller
      didTapLinkURL:[NSURL URLWithString:@(kChromeUIManagementURL)]];
  EXPECT_OCMOCK_VERIFY(mock_scene_commands_handler_);
}

}  // namespace
