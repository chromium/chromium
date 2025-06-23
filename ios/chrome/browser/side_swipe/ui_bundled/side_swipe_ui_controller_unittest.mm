// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_ui_controller.h"

#import "base/i18n/rtl.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_ui_controller+Testing.h"
#import "testing/platform_test.h"

class SideSwipeUIControllerTest : public PlatformTest {
 public:
  SideSwipeUIControllerTest() : view_([[UIView alloc] init]) {
    side_swipe_ui_controller_ = [[SideSwipeUIController alloc] init];
  }

  UIView* view_;
  SideSwipeUIController* side_swipe_ui_controller_;
};

// Tests if edge navigation is enabled on an RTL layout for a given direction.
TEST_F(SideSwipeUIControllerTest, TestNativeSwipeIsEnabledOnRtlEnv) {
  [side_swipe_ui_controller_ setLeadingEdgeNavigationEnabled:YES];
  [side_swipe_ui_controller_ setTrailingEdgeNavigationEnabled:NO];

  // Set the env lang to Arabic.
  base::i18n::SetICUDefaultLocale("ar");

  BOOL edgeNavigationIsEnabledOnLeftDirection =
      [side_swipe_ui_controller_ edgeNavigationIsEnabledForDirection:
                                     UISwipeGestureRecognizerDirectionLeft];

  // On an RTL layout, edge navigation is enabled on left direction since
  // leading edge navigation is enabled.
  EXPECT_TRUE(edgeNavigationIsEnabledOnLeftDirection);

  BOOL edgeNavigationIsEnabledOnRightDirection =
      [side_swipe_ui_controller_ edgeNavigationIsEnabledForDirection:
                                     UISwipeGestureRecognizerDirectionRight];

  // On an RTL layout, edge navigation is disabled on right direction since
  // trailing edge navigation is disabled.
  EXPECT_FALSE(edgeNavigationIsEnabledOnRightDirection);

  // Reset the lang env to en-US.
  base::i18n::SetICUDefaultLocale("en-US");
}

// Tests if edge navigation is enabled on an LTR layout for a given direction.
TEST_F(SideSwipeUIControllerTest, TestNativeSwipeIsEnabledOnLtrEnv) {
  [side_swipe_ui_controller_ setLeadingEdgeNavigationEnabled:YES];
  [side_swipe_ui_controller_ setTrailingEdgeNavigationEnabled:NO];

  BOOL edgeNavigationIsEnabledOnLeftDirection =
      [side_swipe_ui_controller_ edgeNavigationIsEnabledForDirection:
                                     UISwipeGestureRecognizerDirectionLeft];

  // On an LTR layout, edge navigation is disabled on left direction since
  // trailing edge navigation is disabled.
  EXPECT_FALSE(edgeNavigationIsEnabledOnLeftDirection);

  BOOL edgeNavigationIsEnabledOnRightDirection =
      [side_swipe_ui_controller_ edgeNavigationIsEnabledForDirection:
                                     UISwipeGestureRecognizerDirectionRight];

  // On an LTR layout, edge navigation is enabled on right direction since
  // leading edge navigation is enabled.
  EXPECT_TRUE(edgeNavigationIsEnabledOnRightDirection);
}

// Tests that gesture recognizers are removed from the original view that
// requested these when the UIViewController is disconnected.
TEST_F(SideSwipeUIControllerTest, TestGestureRecognizerRemovedOnDisconnect) {
  [side_swipe_ui_controller_ addHorizontalGesturesToView:view_];
  EXPECT_EQ(2U, view_.gestureRecognizers.count);

  [side_swipe_ui_controller_ disconnect];
  EXPECT_EQ(0U, view_.gestureRecognizers.count);
}
