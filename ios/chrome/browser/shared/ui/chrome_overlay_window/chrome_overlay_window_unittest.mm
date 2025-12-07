// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/chrome_overlay_window/chrome_overlay_window.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/chrome_overlay_window/chrome_overlay_container_view.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Expose the private readonly property for testing.
@interface ChromeOverlayWindow (Testing)
@property(nonatomic, readonly) ChromeOverlayContainerView* overlayContainerView;
@end

class ChromeOverlayWindowTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    window_ =
        [[ChromeOverlayWindow alloc] initWithFrame:CGRectMake(0, 0, 400, 500)];
  }

  ChromeOverlayWindow* window_;
};

// Tests that the overlay container is always the front-most subview.
TEST_F(ChromeOverlayWindowTest, OverlayContainerIsAlwaysOnTop) {
  UIView* overlayContainer = [window_ overlayContainerView];
  ASSERT_NE(overlayContainer, nil);

  // The overlay container should be on top initially.
  EXPECT_EQ(window_.subviews.lastObject, overlayContainer);

  // Add a regular view.
  UIView* regularView = [[UIView alloc] init];
  [window_ addSubview:regularView];

  // The overlay container should still be on top.
  EXPECT_EQ(window_.subviews.lastObject, overlayContainer);

  // Add another view.
  UIView* anotherRegularView = [[UIView alloc] init];
  [window_ addSubview:anotherRegularView];

  // The overlay container should still be on top.
  EXPECT_EQ(window_.subviews.lastObject, overlayContainer);
}

// Tests that overlays are inserted in the correct order based on their level.
TEST_F(ChromeOverlayWindowTest, OverlaysAreSortedByLevel) {
  UIView* overlayNormal = [[UIView alloc] init];
  UIView* overlayStatusBar = [[UIView alloc] init];
  UIView* overlayAlert = [[UIView alloc] init];

  [window_ activateOverlay:overlayNormal withLevel:UIWindowLevelNormal];
  [window_ activateOverlay:overlayAlert withLevel:UIWindowLevelAlert];
  [window_ activateOverlay:overlayStatusBar withLevel:UIWindowLevelStatusBar];

  UIView* container = [window_ overlayContainerView];
  ASSERT_EQ(container.subviews.count, 3u);
  EXPECT_EQ(container.subviews[0], overlayNormal);
  EXPECT_EQ(container.subviews[1], overlayStatusBar);
  EXPECT_EQ(container.subviews[2], overlayAlert);
}

// Tests that deactivating an overlay removes it.
TEST_F(ChromeOverlayWindowTest, DeactivateOverlay) {
  UIView* overlayView = [[UIView alloc] init];
  [window_ activateOverlay:overlayView withLevel:UIWindowLevelNormal];

  UIView* container = [window_ overlayContainerView];
  ASSERT_EQ(container.subviews.count, 1u);

  [window_ deactivateOverlay:overlayView];
  EXPECT_EQ(container.subviews.count, 0u);
}

// Tests the visibility of the overlay container.
TEST_F(ChromeOverlayWindowTest, ContainerVisibility) {
  UIView* container = [window_ overlayContainerView];
  EXPECT_TRUE(container.hidden);

  UIView* overlayView = [[UIView alloc] init];
  [window_ activateOverlay:overlayView withLevel:UIWindowLevelNormal];
  EXPECT_FALSE(container.hidden);

  [window_ deactivateOverlay:overlayView];
  EXPECT_TRUE(container.hidden);
}
