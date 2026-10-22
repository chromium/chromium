// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_ui_controller.h"

#import "base/i18n/language_tag.h"
#import "base/i18n/rtl.h"
#import "base/i18n/test/scoped_icu_locale.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_gesture_recognizer.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_ui_controller+Testing.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Gesture recognizer subclass that exposes a writable state for testing.
@interface TestSideSwipeGestureRecognizer : SideSwipeGestureRecognizer
@property(nonatomic, assign) UIGestureRecognizerState testState;
@end

@implementation TestSideSwipeGestureRecognizer
- (UIGestureRecognizerState)state {
  return _testState;
}
@end

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
  base::i18n::ScopedDefaultIcuLocale scoped_locale(
      base::i18n::GetKnownLanguageTag("ar"));

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

// Tests that tab swipe pan gestures route to the card swipe handler in compact
// size classes and to the iPad tab strip swipe handler in Regular x Regular
// size classes, even after a previous compact swipe has occurred.
TEST_F(SideSwipeUIControllerTest, TestTabSwipeRoutingBySizeClass) {
  UIWindow* window = [[UIWindow alloc] init];
  [window addSubview:view_];

  id controller_mock = OCMPartialMock(side_swipe_ui_controller_);
  __block int iphone_swipe_calls = 0;
  __block int ipad_swipe_calls = 0;
  OCMStub([controller_mock handleiPhoneTabSwipe:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        ++iphone_swipe_calls;
      });
  OCMStub([controller_mock handleiPadTabSwipe:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        ++ipad_swipe_calls;
      });

  TestSideSwipeGestureRecognizer* gesture =
      [[TestSideSwipeGestureRecognizer alloc] init];
  [view_ addGestureRecognizer:gesture];

  // Compact width: routes to handleiPhoneTabSwipe:.
  view_.traitOverrides.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
  view_.traitOverrides.verticalSizeClass = UIUserInterfaceSizeClassRegular;
  [view_ updateTraitsIfNeeded];

  gesture.testState = UIGestureRecognizerStateBegan;
  [side_swipe_ui_controller_ handlePan:gesture];
  gesture.testState = UIGestureRecognizerStateEnded;
  [side_swipe_ui_controller_ handlePan:gesture];
  EXPECT_EQ(2, iphone_swipe_calls);
  EXPECT_EQ(0, ipad_swipe_calls);

  // Regular x Regular: routes to handleiPadTabSwipe:.
  view_.traitOverrides.horizontalSizeClass = UIUserInterfaceSizeClassRegular;
  view_.traitOverrides.verticalSizeClass = UIUserInterfaceSizeClassRegular;
  [view_ updateTraitsIfNeeded];

  gesture.testState = UIGestureRecognizerStateBegan;
  [side_swipe_ui_controller_ handlePan:gesture];
  gesture.testState = UIGestureRecognizerStateEnded;
  [side_swipe_ui_controller_ handlePan:gesture];
  EXPECT_EQ(2, iphone_swipe_calls);
  EXPECT_EQ(2, ipad_swipe_calls);
}

// Tests that a tab swipe gesture latches its handler on Began and continues
// routing subsequent events to the same handler if size classes change
// mid-gesture.
TEST_F(SideSwipeUIControllerTest,
       TestTabSwipeLatchesHandlerAcrossMidGestureSizeClassChange) {
  UIWindow* window = [[UIWindow alloc] init];
  [window addSubview:view_];

  id controller_mock = OCMPartialMock(side_swipe_ui_controller_);
  __block int iphone_swipe_calls = 0;
  __block int ipad_swipe_calls = 0;
  OCMStub([controller_mock handleiPhoneTabSwipe:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        ++iphone_swipe_calls;
      });
  OCMStub([controller_mock handleiPadTabSwipe:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        ++ipad_swipe_calls;
      });

  TestSideSwipeGestureRecognizer* gesture =
      [[TestSideSwipeGestureRecognizer alloc] init];
  [view_ addGestureRecognizer:gesture];

  // Start in Compact width, then transition to Regular x Regular mid-gesture.
  view_.traitOverrides.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
  view_.traitOverrides.verticalSizeClass = UIUserInterfaceSizeClassRegular;
  [view_ updateTraitsIfNeeded];

  gesture.testState = UIGestureRecognizerStateBegan;
  [side_swipe_ui_controller_ handlePan:gesture];

  view_.traitOverrides.horizontalSizeClass = UIUserInterfaceSizeClassRegular;
  [view_ updateTraitsIfNeeded];

  gesture.testState = UIGestureRecognizerStateChanged;
  [side_swipe_ui_controller_ handlePan:gesture];
  gesture.testState = UIGestureRecognizerStateEnded;
  [side_swipe_ui_controller_ handlePan:gesture];
  EXPECT_EQ(3, iphone_swipe_calls);
  EXPECT_EQ(0, ipad_swipe_calls);

  // Start in Regular x Regular, then transition to Compact width mid-gesture.
  gesture.testState = UIGestureRecognizerStateBegan;
  [side_swipe_ui_controller_ handlePan:gesture];

  view_.traitOverrides.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
  [view_ updateTraitsIfNeeded];

  gesture.testState = UIGestureRecognizerStateChanged;
  [side_swipe_ui_controller_ handlePan:gesture];
  gesture.testState = UIGestureRecognizerStateEnded;
  [side_swipe_ui_controller_ handlePan:gesture];
  EXPECT_EQ(3, iphone_swipe_calls);
  EXPECT_EQ(3, ipad_swipe_calls);
}
