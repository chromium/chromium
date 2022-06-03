// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/chrome_activity_overlay_coordinator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ChromeActivityOverlayCoordinatorTest = PlatformTest;

// Tests that invoking start and stop on the coordinator presents and dismisses
// the activity overlay view, respectively.
TEST_F(ChromeActivityOverlayCoordinatorTest, StartAndStop) {
  __weak UIView* overlay_view;
  @autoreleasepool {
    base::test::TaskEnvironment task_environment_;
    UIViewController* base_view_controller = [[UIViewController alloc] init];
    std::unique_ptr<Browser> browser_ = std::make_unique<TestBrowser>();
    ChromeActivityOverlayCoordinator* coordinator =
        [[ChromeActivityOverlayCoordinator alloc]
            initWithBaseViewController:base_view_controller
                               browser:browser_.get()];

    EXPECT_EQ(0u, [base_view_controller.childViewControllers count]);

    [coordinator start];
    EXPECT_EQ(1u, [base_view_controller.childViewControllers count]);
    overlay_view = [base_view_controller.childViewControllers firstObject].view;
    EXPECT_TRUE(
        [[base_view_controller.view subviews] containsObject:overlay_view]);

    [coordinator stop];
    EXPECT_EQ(0u, [base_view_controller.childViewControllers count]);
  }
  EXPECT_FALSE(overlay_view);
}
