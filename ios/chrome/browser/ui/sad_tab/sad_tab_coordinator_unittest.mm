// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sad_tab/sad_tab_coordinator.h"

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/sad_tab/sad_tab_view_controller.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing SadTabCoordinator class.
class SadTabCoordinatorTest : public PlatformTest {
 protected:
  SadTabCoordinatorTest()
      : base_view_controller_([[UIViewController alloc] init]),
        browser_state_(TestChromeBrowserState::Builder().Build()) {
    UILayoutGuide* guide = [[NamedGuide alloc] initWithName:kContentAreaGuide];
    [base_view_controller_.view addLayoutGuide:guide];
    AddSameConstraints(guide, base_view_controller_.view);
  }
  web::WebTaskEnvironment task_environment_;
  UIViewController* base_view_controller_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests starting coordinator.
TEST_F(SadTabCoordinatorTest, Start) {
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                    browserState:browser_state_.get()];

  [coordinator start];

  // Verify that presented view controller is SadTabViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  SadTabViewController* view_controller =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([SadTabViewController class], [view_controller class]);

  // Verify SadTabViewController state.
  EXPECT_FALSE(view_controller.offTheRecord);
  EXPECT_FALSE(view_controller.repeatedFailure);
}

// Tests stopping coordinator.
TEST_F(SadTabCoordinatorTest, Stop) {
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                    browserState:browser_state_.get()];

  [coordinator start];
  ASSERT_EQ(1U, base_view_controller_.childViewControllers.count);

  [coordinator stop];
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
}

// Tests dismissing Sad Tab.
TEST_F(SadTabCoordinatorTest, Dismiss) {
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                    browserState:browser_state_.get()];

  [coordinator start];
  ASSERT_EQ(1U, base_view_controller_.childViewControllers.count);

  [coordinator sadTabTabHelperDismissSadTab:nullptr];
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
}

// Tests hiding Sad Tab.
TEST_F(SadTabCoordinatorTest, Hide) {
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                    browserState:browser_state_.get()];

  [coordinator start];
  ASSERT_EQ(1U, base_view_controller_.childViewControllers.count);

  [coordinator sadTabTabHelperDidHide:nullptr];
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
}

// Tests SadTabViewController state for the first failure in non-incognito mode.
TEST_F(SadTabCoordinatorTest, FirstFailureInNonIncognito) {
  web::TestWebState web_state;
  web_state.WasShown();
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                    browserState:browser_state_.get()];

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
}

// Tests SadTabViewController state for the repeated failure in incognito mode.
TEST_F(SadTabCoordinatorTest, FirstFailureInIncognito) {
  web::TestWebState web_state;
  web_state.WasShown();
  ios::ChromeBrowserState* otr_browser_state =
      browser_state_->GetOffTheRecordChromeBrowserState();
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                    browserState:otr_browser_state];

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
}

// Tests SadTabViewController state for the repeated failure in incognito mode.
TEST_F(SadTabCoordinatorTest, ShowFirstFailureInIncognito) {
  ios::ChromeBrowserState* otr_browser_state =
      browser_state_->GetOffTheRecordChromeBrowserState();
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                    browserState:otr_browser_state];

  [coordinator sadTabTabHelper:nullptr didShowForRepeatedFailure:YES];

  // Verify that presented view controller is SadTabViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  SadTabViewController* view_controller =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([SadTabViewController class], [view_controller class]);

  // Verify SadTabViewController state.
  EXPECT_TRUE(view_controller.offTheRecord);
  EXPECT_TRUE(view_controller.repeatedFailure);
}

// Tests action button tap for the first failure.
TEST_F(SadTabCoordinatorTest, FirstFailureAction) {
  web::TestWebState web_state;
  web_state.WasShown();
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                    browserState:browser_state_.get()];
  coordinator.dispatcher = OCMStrictProtocolMock(@protocol(BrowserCommands));
  OCMExpect([coordinator.dispatcher reload]);

  [coordinator sadTabTabHelper:nullptr
      presentSadTabForWebState:&web_state
               repeatedFailure:NO];

  // Verify that presented view controller is SadTabViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  SadTabViewController* view_controller =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([SadTabViewController class], [view_controller class]);

  // Verify dispatcher's message.
  [view_controller.actionButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(coordinator.dispatcher);
}

// Tests action button tap for the repeated failure.
TEST_F(SadTabCoordinatorTest, RepeatedFailureAction) {
  web::TestWebState web_state;
  web_state.WasShown();
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                    browserState:browser_state_.get()];
  coordinator.dispatcher =
      OCMStrictProtocolMock(@protocol(ApplicationCommands));
  OCMExpect([coordinator.dispatcher
      showReportAnIssueFromViewController:base_view_controller_]);

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
  EXPECT_OCMOCK_VERIFY(coordinator.dispatcher);
}

// Tests that view controller is not presented for the hidden web state.
TEST_F(SadTabCoordinatorTest, IgnoreSadTabFromHiddenWebState) {
  web::TestWebState web_state;
  SadTabCoordinator* coordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                    browserState:browser_state_.get()];

  [coordinator sadTabTabHelper:nullptr
      presentSadTabForWebState:&web_state
               repeatedFailure:NO];

  // Verify that view controller was not presented for the hidden web state.
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
}
