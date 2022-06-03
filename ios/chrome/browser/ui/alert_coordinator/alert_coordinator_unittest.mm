// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#include "base/test/task_environment.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/test/scoped_key_window.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - Fixture.

// Fixture to test AlertCoordinator.
class AlertCoordinatorTest : public PlatformTest {
 protected:
  AlertCoordinatorTest() {
    view_controller_ = [[UIViewController alloc] init];
    browser_ = std::make_unique<TestBrowser>();
    [scoped_key_window_.Get() setRootViewController:view_controller_];
  }

  void StartAlertCoordinator() { [alert_coordinator_ start]; }

  UIViewController* GetViewController() { return view_controller_; }

  AlertCoordinator* GetAlertCoordinator(UIViewController* view_controller) {
    return GetAlertCoordinator(view_controller, @"Test title", nil);
  }

  AlertCoordinator* GetAlertCoordinator(UIViewController* view_controller,
                                        NSString* title,
                                        NSString* message) {
    alert_coordinator_ =
        [[AlertCoordinator alloc] initWithBaseViewController:view_controller
                                                     browser:browser_.get()
                                                       title:title
                                                     message:message];
    return alert_coordinator_;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  AlertCoordinator* alert_coordinator_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* view_controller_;
  std::unique_ptr<Browser> browser_;
};

#pragma mark - Tests.

// Tests the alert coordinator reports as visible after presenting and at least
// on button is created.
TEST_F(AlertCoordinatorTest, ValidateIsVisible) {
  // Setup.
  UIViewController* view_controller = GetViewController();
  AlertCoordinator* alert_coordinator = GetAlertCoordinator(view_controller);

  ASSERT_FALSE(alert_coordinator.visible);
  ASSERT_EQ(nil, view_controller.presentedViewController);

  // Action.
  StartAlertCoordinator();

  // Test.
  EXPECT_TRUE(alert_coordinator.visible);
  EXPECT_TRUE([view_controller.presentedViewController
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alert_controller =
      base::mac::ObjCCastStrict<UIAlertController>(
          view_controller.presentedViewController);
  EXPECT_EQ(1LU, alert_controller.actions.count);
}

// Tests the alert coordinator reports as not visible after presenting on a non
// visible view.
TEST_F(AlertCoordinatorTest, ValidateIsNotVisible) {
  // Setup.
  UIWindow* window =
      [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  UIViewController* view_controller = [[UIViewController alloc] init];
  [window setRootViewController:view_controller];

  AlertCoordinator* alert_coordinator = GetAlertCoordinator(view_controller);

  // Action.
  StartAlertCoordinator();

  // Test.
  EXPECT_FALSE(alert_coordinator.visible);
  EXPECT_EQ(nil, [view_controller presentedViewController]);
}

// Tests the alert coordinator has a correct title and message.
TEST_F(AlertCoordinatorTest, TitleAndMessage) {
  // Setup.
  UIViewController* view_controller = GetViewController();
  NSString* title = @"Foo test title!";
  NSString* message = @"Foo bar message.";

  GetAlertCoordinator(view_controller, title, message);

  // Action.
  StartAlertCoordinator();

  // Test.
  // Get the alert.
  EXPECT_TRUE([view_controller.presentedViewController
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alert_controller =
      base::mac::ObjCCastStrict<UIAlertController>(
          view_controller.presentedViewController);

  // Test the results.
  EXPECT_EQ(title, alert_controller.title);
  EXPECT_EQ(message, alert_controller.message);
}

// Tests the alert coordinator dismissal.
TEST_F(AlertCoordinatorTest, ValidateDismissalOnStop) {
  // Setup.
  UIViewController* view_controller = GetViewController();
  AlertCoordinator* alert_coordinator = GetAlertCoordinator(view_controller);

  StartAlertCoordinator();

  ASSERT_TRUE(alert_coordinator.visible);
  ASSERT_NE(nil, view_controller.presentedViewController);
  ASSERT_TRUE([view_controller.presentedViewController
      isKindOfClass:[UIAlertController class]]);

  id viewControllerMock = [OCMockObject partialMockForObject:view_controller];
  [[[viewControllerMock expect] andForwardToRealObject]
      dismissViewControllerAnimated:NO
                         completion:nil];

  // Action.
  [alert_coordinator stop];

  // Test.
  EXPECT_FALSE(alert_coordinator.visible);
  EXPECT_OCMOCK_VERIFY(viewControllerMock);
}

// Tests that only the expected actions are present on the alert.
TEST_F(AlertCoordinatorTest, ValidateActions) {
  // Setup.
  UIViewController* view_controller = GetViewController();
  AlertCoordinator* alert_coordinator = GetAlertCoordinator(view_controller);

  NSDictionary* actions = @{
    @"foo" : @(UIAlertActionStyleDestructive),
    @"foo2" : @(UIAlertActionStyleDestructive),
    @"bar" : @(UIAlertActionStyleDefault),
    @"bar2" : @(UIAlertActionStyleDefault),
    @"testCancel" : @(UIAlertActionStyleCancel),
  };

  NSMutableDictionary* remainingActions = [actions mutableCopy];

  // Action.
  for (id key in actions) {
    UIAlertActionStyle style =
        static_cast<UIAlertActionStyle>([actions[key] integerValue]);
    [alert_coordinator addItemWithTitle:key action:nil style:style];
  }

  // Test.
  // Present the alert.
  StartAlertCoordinator();

  // Get the alert.
  EXPECT_TRUE([view_controller.presentedViewController
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alert_controller =
      base::mac::ObjCCastStrict<UIAlertController>(
          view_controller.presentedViewController);

  // Test the results.
  for (UIAlertAction* action in alert_controller.actions) {
    EXPECT_TRUE([remainingActions objectForKey:action.title]);
    UIAlertActionStyle style =
        static_cast<UIAlertActionStyle>([actions[action.title] integerValue]);
    EXPECT_EQ(style, action.style);
    [remainingActions removeObjectForKey:action.title];
  }

  EXPECT_EQ(0LU, [remainingActions count]);
}

// Tests that only the first cancel action is added.
TEST_F(AlertCoordinatorTest, OnlyOneCancelAction) {
  // Setup.
  UIViewController* view_controller = GetViewController();
  AlertCoordinator* alert_coordinator = GetAlertCoordinator(view_controller);

  NSString* firstButtonTitle = @"Cancel1";

  // Action.
  [alert_coordinator addItemWithTitle:firstButtonTitle
                               action:nil
                                style:UIAlertActionStyleCancel];
  [alert_coordinator addItemWithTitle:@"Cancel2"
                               action:nil
                                style:UIAlertActionStyleCancel];

  // Test.
  // Present the alert.
  StartAlertCoordinator();

  // Get the alert.
  EXPECT_TRUE([view_controller.presentedViewController
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alert_controller =
      base::mac::ObjCCastStrict<UIAlertController>(
          view_controller.presentedViewController);

  // Test the results.
  EXPECT_EQ(1LU, alert_controller.actions.count);

  UIAlertAction* action = [alert_controller.actions objectAtIndex:0];
  EXPECT_EQ(firstButtonTitle, action.title);
  EXPECT_EQ(UIAlertActionStyleCancel, action.style);
}

// Tests that the |noInteractionAction| block is called for an alert coordinator
// which is stopped before the user has interacted with it.
TEST_F(AlertCoordinatorTest, NoInteractionActionTest) {
  // Setup.
  UIViewController* view_controller = GetViewController();
  AlertCoordinator* alert_coordinator = GetAlertCoordinator(view_controller);

  __block BOOL block_called = NO;

  alert_coordinator.noInteractionAction = ^{
    block_called = YES;
  };

  StartAlertCoordinator();

  // Action.
  [alert_coordinator stop];

  // Test.
  EXPECT_TRUE(block_called);
}

// Tests that the |noInteractionAction| block is not called for an alert
// coordinator which is dismissed with the cancel button.
TEST_F(AlertCoordinatorTest, NoInteractionActionWithCancelTest) {
  // Setup.
  UIViewController* view_controller = GetViewController();
  AlertCoordinator* alert_coordinator = GetAlertCoordinator(view_controller);

  __block BOOL block_called = NO;

  alert_coordinator.noInteractionAction = ^{
    block_called = YES;
  };

  StartAlertCoordinator();

  // Action.
  [alert_coordinator executeCancelHandler];
  [alert_coordinator stop];

  // Test.
  EXPECT_FALSE(block_called);
}

// Tests that the alert coordinator is dismissed if destroyed without being
// stopped.
TEST_F(AlertCoordinatorTest, AlertDismissedOnDestroy) {
  // Setup.
  UIViewController* view_controller = GetViewController();
  AlertCoordinator* alert_coordinator = GetAlertCoordinator(view_controller);

  ASSERT_FALSE(alert_coordinator.visible);
  ASSERT_EQ(nil, view_controller.presentedViewController);

  __block BOOL block_called = NO;

  alert_coordinator.noInteractionAction = ^{
    block_called = YES;
  };

  StartAlertCoordinator();

  alert_coordinator = nil;

  EXPECT_FALSE(block_called);
}
