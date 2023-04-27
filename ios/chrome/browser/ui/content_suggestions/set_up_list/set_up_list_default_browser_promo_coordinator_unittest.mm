// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_coordinator_delegate.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests the SetUpListView and subviews.
class SetUpListDefaultBrowserPromoCoordinatorTest : public PlatformTest {
 public:
  SetUpListDefaultBrowserPromoCoordinatorTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    window_ = [[UIWindow alloc] init];
    window_.rootViewController = [[UIViewController alloc] init];
    [window_ addSubview:window_.rootViewController.view];
    UIView.animationsEnabled = NO;
    mock_application_ = OCMStrictClassMock([UIApplication class]);
    coordinator_ = [[SetUpListDefaultBrowserPromoCoordinator alloc]
        initWithBaseViewController:window_.rootViewController
                           browser:browser_.get()
                       application:mock_application_];
    delegate_ = OCMProtocolMock(
        @protocol(SetUpListDefaultBrowserPromoCoordinatorDelegate));
    coordinator_.delegate = delegate_;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  UIWindow* window_;
  SetUpListDefaultBrowserPromoCoordinator* coordinator_;
  id delegate_;
  id mock_application_;
};

#pragma mark - Tests

// Test that touching the primary button calls the correct delegate method
// and opens the settings.
TEST_F(SetUpListDefaultBrowserPromoCoordinatorTest, PrimaryButton) {
  [coordinator_ start];

  OCMExpect([delegate_ setUpListDefaultBrowserPromoDidFinish:YES]);
  OCMExpect([mock_application_
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil]);
  [coordinator_ didTapPrimaryActionButton];
  [delegate_ verify];

  [coordinator_ stop];
}

// Test that touching the secondary button calls the correct delegate method.
TEST_F(SetUpListDefaultBrowserPromoCoordinatorTest, SecondaryButton) {
  [coordinator_ start];

  OCMExpect([delegate_ setUpListDefaultBrowserPromoDidFinish:NO]);
  [coordinator_ didTapSecondaryActionButton];
  [delegate_ verify];

  [coordinator_ stop];
}

// Test that touching the secondary button calls the correct delegate method.
TEST_F(SetUpListDefaultBrowserPromoCoordinatorTest, SwipeToDismiss) {
  [coordinator_ start];

  OCMExpect([delegate_ setUpListDefaultBrowserPromoDidFinish:NO]);
  UIPresentationController* presentation_controller =
      window_.rootViewController.presentedViewController.presentationController;
  [coordinator_ presentationControllerDidDismiss:presentation_controller];
  [delegate_ verify];

  [coordinator_ stop];
}
