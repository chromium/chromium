// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::test::TestFuture;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Test delegate to check if the correct methods are called.
@interface ButtonStackActionTestDelegate : NSObject <ButtonStackActionDelegate>
@property(nonatomic, assign) BOOL primaryActionTapped;
@property(nonatomic, assign) BOOL secondaryActionTapped;
@property(nonatomic, assign) BOOL tertiaryActionTapped;
@property(nonatomic, assign) BOOL dismissed;
// Block run when the delegate is notified of the dismissal.
@property(nonatomic, copy) ProceduralBlock dismissalHandler;
@end

@implementation ButtonStackActionTestDelegate
- (void)didTapPrimaryActionButton {
  self.primaryActionTapped = YES;
}
- (void)didTapSecondaryActionButton {
  self.secondaryActionTapped = YES;
}
- (void)didTapTertiaryActionButton {
  self.tertiaryActionTapped = YES;
}
- (void)didDismissButtonStackViewController {
  self.dismissed = YES;
  if (self.dismissalHandler) {
    ProceduralBlock handler = self.dismissalHandler;
    self.dismissalHandler = nil;
    handler();
  }
}
@end

// Test fixture for ButtonStackViewController.
class ButtonStackViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    configuration_ = [[ButtonStackConfiguration alloc] init];
    configuration_.primaryActionString = @"Primary";
    configuration_.secondaryActionString = @"Secondary";
    configuration_.tertiaryActionString = @"Tertiary";

    view_controller_ = [[ButtonStackViewController alloc]
        initWithConfiguration:configuration_];
    delegate_ = [[ButtonStackActionTestDelegate alloc] init];
    delegate_.dismissalHandler =
        base::CallbackToBlock(dismissal_future_.GetCallback());
    view_controller_.actionDelegate = delegate_;

    // Load the view.
    [view_controller_ view];
  }

  void TearDown() override {
    window_.hidden = YES;
    window_ = nil;
    PlatformTest::TearDown();
  }

  // Makes `root_view_controller` the root of a new visible window, so that
  // UIKit runs real appearance transitions.
  void ShowInWindow(UIViewController* root_view_controller) {
    UIWindowScene* window_scene = nil;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
      if ([scene isKindOfClass:UIWindowScene.class]) {
        window_scene = static_cast<UIWindowScene*>(scene);
        break;
      }
    }
    window_ = window_scene ? [[UIWindow alloc] initWithWindowScene:window_scene]
                           : [[UIWindow alloc] init];
    window_.rootViewController = root_view_controller;
    [window_ makeKeyAndVisible];
  }

  // Presents `presented` from `presenting` and waits for the presentation to
  // complete.
  void PresentAndWait(UIViewController* presenting,
                      UIViewController* presented) {
    TestFuture<void> presentation_future;
    [presenting presentViewController:presented
                             animated:NO
                           completion:base::CallbackToBlock(
                                          presentation_future.GetCallback())];
    ASSERT_TRUE(presentation_future.Wait());
  }

  // Waits until the view of `view_controller` is removed from the window.
  void WaitForViewRemovedFromWindow(UIViewController* view_controller) {
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
      return view_controller.viewIfLoaded.window == nil;
    }));
  }

  // Waits until the delegate is notified of the dismissal.
  void WaitForDismissalNotification() { ASSERT_TRUE(dismissal_future_.Wait()); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  TestFuture<void> dismissal_future_;
  ButtonStackConfiguration* configuration_;
  ButtonStackViewController* view_controller_;
  ButtonStackActionTestDelegate* delegate_;
  UIWindow* window_;
};

// Tests that tapping the primary button calls the delegate.
TEST_F(ButtonStackViewControllerTest, TestPrimaryAction) {
  EXPECT_FALSE(delegate_.primaryActionTapped);
  [view_controller_.primaryActionButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_TRUE(delegate_.primaryActionTapped);
}

// Tests that tapping the secondary button calls the delegate.
TEST_F(ButtonStackViewControllerTest, TestSecondaryAction) {
  EXPECT_FALSE(delegate_.secondaryActionTapped);
  [view_controller_.secondaryActionButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_TRUE(delegate_.secondaryActionTapped);
}

// Tests that tapping the tertiary button calls the delegate.
TEST_F(ButtonStackViewControllerTest, TestTertiaryAction) {
  EXPECT_FALSE(delegate_.tertiaryActionTapped);
  [view_controller_.tertiaryActionButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_TRUE(delegate_.tertiaryActionTapped);
}

// Tests that the delegate is not notified when the view disappears without
// being dismissed or popped.
TEST_F(ButtonStackViewControllerTest, TestDisappearWithoutDismissal) {
  [view_controller_ viewDidDisappear:NO];
  EXPECT_FALSE(delegate_.dismissed);
}

// Tests that the delegate is notified when the view controller is dismissed.
TEST_F(ButtonStackViewControllerTest, TestDismissal) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ShowInWindow(base_view_controller);
  PresentAndWait(base_view_controller, view_controller_);
  EXPECT_FALSE(delegate_.dismissed);

  [base_view_controller dismissViewControllerAnimated:NO completion:nil];
  WaitForDismissalNotification();
}

// Tests that the delegate is notified when the navigation controller embedding
// the view controller is dismissed.
TEST_F(ButtonStackViewControllerTest, TestDismissalOfNavigationController) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ShowInWindow(base_view_controller);
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:view_controller_];
  PresentAndWait(base_view_controller, navigation_controller);
  EXPECT_FALSE(delegate_.dismissed);

  [base_view_controller dismissViewControllerAnimated:NO completion:nil];
  WaitForDismissalNotification();
}

// Tests that the delegate is notified when the view controller is a child of
// a view controller embedded in a navigation controller that is dismissed.
TEST_F(ButtonStackViewControllerTest, TestDismissalOfAncestorContainer) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ShowInWindow(base_view_controller);
  UIViewController* parent_view_controller = [[UIViewController alloc] init];
  [parent_view_controller addChildViewController:view_controller_];
  [parent_view_controller.view addSubview:view_controller_.view];
  [view_controller_ didMoveToParentViewController:parent_view_controller];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:parent_view_controller];
  PresentAndWait(base_view_controller, navigation_controller);
  EXPECT_FALSE(delegate_.dismissed);

  [base_view_controller dismissViewControllerAnimated:NO completion:nil];
  WaitForDismissalNotification();
}

// Tests that the delegate is notified when the view controller is popped from
// its navigation controller.
TEST_F(ButtonStackViewControllerTest, TestPopFromNavigationController) {
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:[[UIViewController alloc] init]];
  ShowInWindow(navigation_controller);
  [navigation_controller pushViewController:view_controller_ animated:NO];
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return view_controller_.view.window != nil;
  }));
  EXPECT_FALSE(delegate_.dismissed);

  [navigation_controller popViewControllerAnimated:NO];
  WaitForDismissalNotification();
}

// Tests that the delegate is not notified when another view controller is
// pushed on top of the view controller.
TEST_F(ButtonStackViewControllerTest, TestPushOnTopDoesNotNotify) {
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:view_controller_];
  ShowInWindow(navigation_controller);

  [navigation_controller pushViewController:[[UIViewController alloc] init]
                                   animated:NO];
  WaitForViewRemovedFromWindow(view_controller_);
  EXPECT_FALSE(delegate_.dismissed);
}

// Tests that the delegate is not notified when the view controller is a child
// of a view controller that is popped, as UIKit only reports
// `isMovingFromParentViewController` to the popped view controller itself.
TEST_F(ButtonStackViewControllerTest, TestPopOfParentDoesNotNotify) {
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:[[UIViewController alloc] init]];
  ShowInWindow(navigation_controller);
  UIViewController* parent_view_controller = [[UIViewController alloc] init];
  [parent_view_controller addChildViewController:view_controller_];
  [parent_view_controller.view addSubview:view_controller_.view];
  [view_controller_ didMoveToParentViewController:parent_view_controller];
  [navigation_controller pushViewController:parent_view_controller animated:NO];
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return view_controller_.view.window != nil;
  }));

  [navigation_controller popViewControllerAnimated:NO];
  WaitForViewRemovedFromWindow(view_controller_);
  EXPECT_FALSE(delegate_.dismissed);
}

// Tests the loading state.
TEST_F(ButtonStackViewControllerTest, TestLoadingState) {
  [view_controller_ setLoading:YES];
  EXPECT_FALSE(view_controller_.primaryActionButton.enabled);
  EXPECT_FALSE(view_controller_.secondaryActionButton.enabled);
  EXPECT_FALSE(view_controller_.tertiaryActionButton.enabled);
  EXPECT_EQ(PrimaryButtonImageSpinner,
            view_controller_.primaryActionButton.primaryButtonImage);
  EXPECT_NSEQ(@"", view_controller_.primaryActionButton.configuration.title);

  [view_controller_ setLoading:NO];
  EXPECT_TRUE(view_controller_.primaryActionButton.enabled);
  EXPECT_TRUE(view_controller_.secondaryActionButton.enabled);
  EXPECT_TRUE(view_controller_.tertiaryActionButton.enabled);
  EXPECT_EQ(PrimaryButtonImageNone,
            view_controller_.primaryActionButton.primaryButtonImage);
  EXPECT_NSEQ(configuration_.primaryActionString,
              view_controller_.primaryActionButton.configuration.title);
}

// Tests the confirmed state.
TEST_F(ButtonStackViewControllerTest, TestConfirmedState) {
  [view_controller_ setConfirmed:YES];
  EXPECT_FALSE(view_controller_.primaryActionButton.enabled);
  EXPECT_FALSE(view_controller_.secondaryActionButton.enabled);
  EXPECT_FALSE(view_controller_.tertiaryActionButton.enabled);
  EXPECT_EQ(PrimaryButtonImageCheckmark,
            view_controller_.primaryActionButton.primaryButtonImage);
  EXPECT_NSEQ(@"", view_controller_.primaryActionButton.configuration.title);

  [view_controller_ setConfirmed:NO];
  EXPECT_TRUE(view_controller_.primaryActionButton.enabled);
  EXPECT_TRUE(view_controller_.secondaryActionButton.enabled);
  EXPECT_TRUE(view_controller_.tertiaryActionButton.enabled);
  EXPECT_EQ(PrimaryButtonImageNone,
            view_controller_.primaryActionButton.primaryButtonImage);
  EXPECT_NSEQ(configuration_.primaryActionString,
              view_controller_.primaryActionButton.configuration.title);
}

// Tests that setting loading to YES when confirmed is YES sets confirmed to NO.
TEST_F(ButtonStackViewControllerTest, TestLoadingWhenConfirmed) {
  [view_controller_ setConfirmed:YES];
  EXPECT_EQ(PrimaryButtonImageCheckmark,
            view_controller_.primaryActionButton.primaryButtonImage);

  [view_controller_ setLoading:YES];
  EXPECT_EQ(PrimaryButtonImageSpinner,
            view_controller_.primaryActionButton.primaryButtonImage);
}

// Tests that setting confirmed to YES when loading is YES sets loading to NO.
TEST_F(ButtonStackViewControllerTest, TestConfirmedWhenLoading) {
  [view_controller_ setLoading:YES];
  EXPECT_EQ(PrimaryButtonImageSpinner,
            view_controller_.primaryActionButton.primaryButtonImage);

  [view_controller_ setConfirmed:YES];
  EXPECT_EQ(PrimaryButtonImageCheckmark,
            view_controller_.primaryActionButton.primaryButtonImage);
}

// Tests that buttons are only created if they have a title.
TEST_F(ButtonStackViewControllerTest, TestPartialConfiguration) {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.primaryActionString = @"Primary Only";
  ButtonStackViewController* view_controller =
      [[ButtonStackViewController alloc] initWithConfiguration:configuration];
  [view_controller view];
  EXPECT_FALSE(view_controller.primaryActionButton.hidden);
  EXPECT_TRUE(view_controller.secondaryActionButton.hidden);
  EXPECT_TRUE(view_controller.tertiaryActionButton.hidden);
}

// Tests that updating the configuration correctly updates the buttons.
TEST_F(ButtonStackViewControllerTest, TestUpdateConfiguration) {
  // All buttons should be visible initially.
  EXPECT_FALSE(view_controller_.primaryActionButton.hidden);
  EXPECT_FALSE(view_controller_.secondaryActionButton.hidden);
  EXPECT_FALSE(view_controller_.tertiaryActionButton.hidden);

  // Update to a configuration with only a secondary action.
  ButtonStackConfiguration* new_configuration =
      [[ButtonStackConfiguration alloc] init];
  new_configuration.secondaryActionString = @"New Secondary";
  [view_controller_ updateConfiguration:new_configuration];

  // Verify the button states.
  EXPECT_TRUE(view_controller_.primaryActionButton.hidden);
  EXPECT_FALSE(view_controller_.secondaryActionButton.hidden);
  EXPECT_TRUE(view_controller_.tertiaryActionButton.hidden);
  EXPECT_NSEQ(@"New Secondary",
              view_controller_.secondaryActionButton.configuration.title);
}

// Tests that updating the configuration correctly updates the button styles.
TEST_F(ButtonStackViewControllerTest, TestUpdateConfigurationStyle) {
  // Update to a destructive primary action.
  ButtonStackConfiguration* new_configuration =
      [[ButtonStackConfiguration alloc] init];
  new_configuration.primaryActionString = @"Destructive";
  new_configuration.primaryButtonStyle = ChromeButtonStylePrimaryDestructive;
  [view_controller_ updateConfiguration:new_configuration];

  // Verify the button style was updated.
  EXPECT_EQ(ChromeButtonStylePrimaryDestructive,
            view_controller_.primaryActionButton.style);
}

// Tests that reloading the configuration correctly updates the buttons.
TEST_F(ButtonStackViewControllerTest, TestReloadConfiguration) {
  // All buttons should be visible initially.
  EXPECT_NSEQ(@"Primary",
              view_controller_.primaryActionButton.configuration.title);

  // Update the configuration directly.
  view_controller_.configuration.primaryActionString = @"New Primary";
  [view_controller_ reloadConfiguration];

  // Verify the button title has been updated.
  EXPECT_NSEQ(@"New Primary",
              view_controller_.primaryActionButton.configuration.title);
}
