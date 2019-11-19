// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/context_menu_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Fixture to test ContextMenuCoordinator.
class ContextMenuCoordinatorTest : public PlatformTest {
 public:
  ContextMenuCoordinatorTest() {
    // Save the current key window and restore it after the test.
    previous_key_window_ = [[UIApplication sharedApplication] keyWindow];
    window_ = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    [window_ makeKeyAndVisible];
    view_controller_ = [[UIViewController alloc] init];
    [window_ setRootViewController:view_controller_];
  }

  ~ContextMenuCoordinatorTest() override {
    [previous_key_window_ makeKeyAndVisible];
  }

 protected:
  UIWindow* previous_key_window_;
  ContextMenuCoordinator* menu_coordinator_;
  UIWindow* window_;
  UIViewController* view_controller_;
};

// Tests the context menu reports as visible after presenting.
TEST_F(ContextMenuCoordinatorTest, ValidateIsVisible) {
  menu_coordinator_ = [[ContextMenuCoordinator alloc]
      initWithBaseViewController:view_controller_
                           title:@"Context Menu"
                          inView:view_controller_.view
                      atLocation:CGPointZero];
  [menu_coordinator_ start];
  EXPECT_TRUE([menu_coordinator_ isVisible]);
}

// Tests the context menu dismissal.
TEST_F(ContextMenuCoordinatorTest, ValidateDismissalOnStop) {
  menu_coordinator_ = [[ContextMenuCoordinator alloc]
      initWithBaseViewController:view_controller_
                           title:nil
                          inView:view_controller_.view
                      atLocation:CGPointZero];
  [menu_coordinator_ start];
  [menu_coordinator_ stop];
  EXPECT_FALSE([menu_coordinator_ isVisible]);
}

// Tests that only the expected actions are present on the context menu.
TEST_F(ContextMenuCoordinatorTest, ValidateActions) {
  menu_coordinator_ = [[ContextMenuCoordinator alloc]
      initWithBaseViewController:view_controller_
                           title:nil
                          inView:view_controller_.view
                      atLocation:CGPointZero];

  NSArray* menu_titles = @[ @"foo", @"bar" ];
  for (NSString* title in menu_titles) {
    [menu_coordinator_ addItemWithTitle:title
                                 action:^{
                                 }];
  }

  [menu_coordinator_ start];

  EXPECT_TRUE([[view_controller_ presentedViewController]
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alert_controller =
      base::mac::ObjCCastStrict<UIAlertController>(
          [view_controller_ presentedViewController]);

  NSMutableArray* remaining_titles = [menu_titles mutableCopy];
  for (UIAlertAction* action in alert_controller.actions) {
    if (action.style != UIAlertActionStyleCancel) {
      EXPECT_TRUE([remaining_titles containsObject:action.title]);
      [remaining_titles removeObject:action.title];
    }
  }

  EXPECT_EQ(0LU, [remaining_titles count]);
}

// Validates that the cancel action is present on the context menu.
TEST_F(ContextMenuCoordinatorTest, CancelButtonExists) {
  menu_coordinator_ = [[ContextMenuCoordinator alloc]
      initWithBaseViewController:view_controller_
                           title:nil
                          inView:view_controller_.view
                      atLocation:CGPointZero];

  [menu_coordinator_ start];

  EXPECT_TRUE([[view_controller_ presentedViewController]
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alert_controller =
      base::mac::ObjCCastStrict<UIAlertController>(
          [view_controller_ presentedViewController]);

  EXPECT_EQ(1LU, alert_controller.actions.count);
  EXPECT_EQ(UIAlertActionStyleCancel,
            [alert_controller.actions.firstObject style]);
}

// Tests that the ContextMenuParams are used to display context menu.
TEST_F(ContextMenuCoordinatorTest, ValidateContextMenuParams) {
  CGPoint location = CGPointMake(100.0, 125.0);
  NSString* title = @"Context Menu Title";
  menu_coordinator_ = [[ContextMenuCoordinator alloc]
      initWithBaseViewController:view_controller_
                           title:title
                          inView:view_controller_.view
                      atLocation:location];
  [menu_coordinator_ start];

  EXPECT_TRUE([[view_controller_ presentedViewController]
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alert_controller =
      base::mac::ObjCCastStrict<UIAlertController>(
          [view_controller_ presentedViewController]);

  EXPECT_EQ(title, alert_controller.title);

  // Only validate the popover location if it is displayed in a popover.
  if (alert_controller.popoverPresentationController) {
    CGPoint presentedLocation =
        alert_controller.popoverPresentationController.sourceRect.origin;
    EXPECT_EQ(location.x, presentedLocation.x);
    EXPECT_EQ(location.y, presentedLocation.y);
  }
}
