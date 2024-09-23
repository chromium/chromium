// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sad_tab/ui_bundled/sad_tab_coordinator.h"

#import "ios/chrome/browser/lens/model/lens_browser_agent.h"
#import "ios/chrome/browser/sad_tab/ui_bundled/sad_tab_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for testing SadTabCoordinator class.
class SadTabCoordinatorTest : public PlatformTest {
 protected:
  SadTabCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_ = [[UIViewController alloc] init];
    UILayoutGuide* guide = [[NamedGuide alloc] initWithName:kContentAreaGuide];
    [base_view_controller_.view addLayoutGuide:guide];
    AddSameConstraints(guide, base_view_controller_.view);
    LensBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
  }
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
};

// Tests starting coordinator.
TEST_F(SadTabCoordinatorTest, Start) {
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()];

  [coordinator start];

  // Verify that presented view controller is SadTabViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  SadTabViewController* view_controller =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([SadTabViewController class], [view_controller class]);

  // Verify SadTabViewController state.
  EXPECT_FALSE(view_controller.offTheRecord);
  EXPECT_FALSE(view_controller.repeatedFailure);
  [coordinator stop];
  // TODO(crbug.com/40823248): To remove after cleaning as it should be handle
  // in the stop function.
  [coordinator disconnect];
}

// Tests stopping coordinator.
TEST_F(SadTabCoordinatorTest, Stop) {
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()];

  [coordinator start];
  ASSERT_EQ(1U, base_view_controller_.childViewControllers.count);

  [coordinator stop];
  // TODO(crbug.com/40823248): To remove after cleaning as it should be handle
  // in the stop function.
  [coordinator disconnect];
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
}

// Tests dismissing Sad Tab.
TEST_F(SadTabCoordinatorTest, Dismiss) {
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()];

  [coordinator start];
  ASSERT_EQ(1U, base_view_controller_.childViewControllers.count);

  [coordinator sadTabTabHelperDismissSadTab:nullptr];
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
  [coordinator stop];
  // TODO(crbug.com/40823248): To remove after cleaning as it should be handle
  // in the stop function.
  [coordinator disconnect];
}

// Tests hiding Sad Tab.
TEST_F(SadTabCoordinatorTest, Hide) {
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()];

  [coordinator start];
  ASSERT_EQ(1U, base_view_controller_.childViewControllers.count);

  [coordinator sadTabTabHelperDidHide:nullptr];
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
  [coordinator stop];
  // TODO(crbug.com/40823248): To remove after cleaning as it should be handle
  // in the stop function.
  [coordinator disconnect];
}

// Tests SadTabViewController state for the first failure in non-incognito mode.
TEST_F(SadTabCoordinatorTest, FirstFailureInNonIncognito) {
  web::FakeWebState web_state;
  web_state.WasShown();
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()];

  [coordinator sadTabTabHelper:nullptr
      presentSadTabForWebState:&web_state
               repeatedFailure:NO];

  // Verify that presented view controller is SadTabViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  SadTabViewController* view_controller =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([SadTabViewController class], [view_controller class]);

  // Verify SadTabViewController state.
  EXPECT_FALSE(view_controller.offTheRecord);
  EXPECT_FALSE(view_controller.repeatedFailure);
  [coordinator stop];
  // TODO(crbug.com/40823248): To remove after cleaning as it should be handle
  // in the stop function.
  [coordinator disconnect];
}

// Tests SadTabViewController state for the repeated failure in incognito mode.
TEST_F(SadTabCoordinatorTest, FirstFailureInIncognito) {
  web::FakeWebState web_state;
  web_state.WasShown();
  std::unique_ptr<Browser> otr_browser = std::make_unique<TestBrowser>(
      browser_->GetProfile()->GetOffTheRecordProfile());
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:otr_browser.get()];

  [coordinator sadTabTabHelper:nullptr
      presentSadTabForWebState:&web_state
               repeatedFailure:YES];

  // Verify that presented view controller is SadTabViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  SadTabViewController* view_controller =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([SadTabViewController class], [view_controller class]);

  // Verify SadTabViewController state.
  EXPECT_TRUE(view_controller.offTheRecord);
  EXPECT_TRUE(view_controller.repeatedFailure);
  [coordinator stop];
  // TODO(crbug.com/40823248): To remove after cleaning as it should be handle
  // in the stop function.
  [coordinator disconnect];
}

// Tests SadTabViewController state for the repeated failure in incognito mode.
TEST_F(SadTabCoordinatorTest, ShowFirstFailureInIncognito) {
  std::unique_ptr<Browser> otr_browser = std::make_unique<TestBrowser>(
      browser_->GetProfile()->GetOffTheRecordProfile());
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:otr_browser.get()];

  [coordinator sadTabTabHelper:nullptr didShowForRepeatedFailure:YES];

  // Verify that presented view controller is SadTabViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  SadTabViewController* view_controller =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([SadTabViewController class], [view_controller class]);

  // Verify SadTabViewController state.
  EXPECT_TRUE(view_controller.offTheRecord);
  EXPECT_TRUE(view_controller.repeatedFailure);
  [coordinator stop];
  // TODO(crbug.com/40823248): To remove after cleaning as it should be handle
  // in the stop function.
  [coordinator disconnect];
}

// Tests action button tap for the first failure.
TEST_F(SadTabCoordinatorTest, FirstFailureAction) {
  web::FakeWebState web_state;
  web_state.WasShown();
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()];

  [coordinator sadTabTabHelper:nullptr
      presentSadTabForWebState:&web_state
               repeatedFailure:NO];

  // Verify that presented view controller is SadTabViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  SadTabViewController* view_controller =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([SadTabViewController class], [view_controller class]);

  // Ensure that the action button can be pressed.
  [view_controller.actionButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  [coordinator stop];
  // TODO(crbug.com/40823248): To remove after cleaning as it should be handle
  // in the stop function.
  [coordinator disconnect];
}

// Tests action button tap for the repeated failure.
TEST_F(SadTabCoordinatorTest, RepeatedFailureAction) {
  web::FakeWebState web_state;
  web_state.WasShown();
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()];

  id mock_application_commands_handler_ =
      OCMStrictProtocolMock(@protocol(ApplicationCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_application_commands_handler_
                   forProtocol:@protocol(ApplicationCommands)];
  OCMExpect([mock_application_commands_handler_
      showReportAnIssueFromViewController:base_view_controller_
                                   sender:UserFeedbackSender::SadTab]);

  [coordinator sadTabTabHelper:nullptr
      presentSadTabForWebState:&web_state
               repeatedFailure:YES];

  // Verify that presented view controller is SadTabViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  SadTabViewController* view_controller =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([SadTabViewController class], [view_controller class]);

  // Verify dispatcher's message.
  [view_controller.actionButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mock_application_commands_handler_);
  [coordinator stop];
  // TODO(crbug.com/40823248): To remove after cleaning as it should be handle
  // in the stop function.
  [coordinator disconnect];
}

// Tests that view controller is not presented for the hidden web state.
TEST_F(SadTabCoordinatorTest, IgnoreSadTabFromHiddenWebState) {
  web::FakeWebState web_state;
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()];

  [coordinator sadTabTabHelper:nullptr
      presentSadTabForWebState:&web_state
               repeatedFailure:NO];

  // Verify that view controller was not presented for the hidden web state.
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
  [coordinator stop];
  // TODO(crbug.com/40823248): To remove after cleaning as it should be handle
  // in the stop function.
  [coordinator disconnect];
}
