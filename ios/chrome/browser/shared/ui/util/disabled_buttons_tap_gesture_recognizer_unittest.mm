// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/disabled_buttons_tap_gesture_recognizer.h"

#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test `UITouch` subclass returning configurable `view` and coordinates
// converted from `rootView`.
@interface FakeDisabledButtonTouch : UITouch
@property(nonatomic, weak) UIView* rootView;
@property(nonatomic, assign) CGPoint pointInRootView;
@property(nonatomic, weak) UIView* hitTestedView;
@end

@implementation FakeDisabledButtonTouch

- (UIView*)view {
  return self.hitTestedView;
}

- (CGPoint)locationInView:(UIView*)view {
  if (self.rootView && view) {
    return [self.rootView convertPoint:self.pointInRootView toView:view];
  }
  return self.pointInRootView;
}

@end

// Test subclass returning a configurable `-locationInView:` for testing
// `-gestureRecognizerShouldBegin:`.
@interface TestDisabledButtonsTapGestureRecognizer
    : DisabledButtonsTapGestureRecognizer
@property(nonatomic, assign) CGPoint fakeLocationInView;
@end

@implementation TestDisabledButtonsTapGestureRecognizer

- (CGPoint)locationInView:(UIView*)view {
  return self.fakeLocationInView;
}

@end

namespace {

using DisabledButtonsTapGestureRecognizerTest = PlatformTest;

// Tests that `DisabledButtonsTapGestureRecognizer` receives touches only over
// visible disabled tracked buttons and ignores enabled, hidden, clipped, or
// non-interactive ancestors and overlapping enabled controls.
TEST_F(DisabledButtonsTapGestureRecognizerTest,
       ReceivesTouchesOnlyOverVisibleDisabledTrackedButtons) {
  UIView* container = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 300, 44)];
  UIView* wrapper = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 200, 44)];
  UIButton* back_button =
      [[UIButton alloc] initWithFrame:CGRectMake(0, 0, 80, 44)];
  UIButton* forward_button =
      [[UIButton alloc] initWithFrame:CGRectMake(90, 0, 80, 44)];
  UIButton* untracked_button =
      [[UIButton alloc] initWithFrame:CGRectMake(210, 0, 80, 44)];
  [wrapper addSubview:back_button];
  [wrapper addSubview:forward_button];
  [container addSubview:wrapper];
  [container addSubview:untracked_button];

  DisabledButtonsTapGestureRecognizer* recognizer =
      [[DisabledButtonsTapGestureRecognizer alloc]
          initWithTarget:nil
                  action:nullptr
                 buttons:@[ back_button, forward_button ]];
  [container addGestureRecognizer:recognizer];
  EXPECT_FALSE(recognizer.cancelsTouchesInView);

  FakeDisabledButtonTouch* touch = [[FakeDisabledButtonTouch alloc] init];
  touch.rootView = container;
  touch.hitTestedView = wrapper;

  const CGPoint back_center = CGPointMake(40, 22);
  const CGPoint forward_center = CGPointMake(130, 22);
  const CGPoint untracked_center = CGPointMake(250, 22);

  // Disabled tracked buttons accept touches; untracked disabled button does
  // not.
  back_button.enabled = NO;
  forward_button.enabled = NO;
  untracked_button.enabled = NO;

  touch.pointInRootView = back_center;
  EXPECT_TRUE([recognizer.delegate gestureRecognizer:recognizer
                                  shouldReceiveTouch:touch]);
  // A disabled `UIButton` is typically itself the hit-test view (`touch.view`).
  touch.hitTestedView = back_button;
  EXPECT_TRUE([recognizer.delegate gestureRecognizer:recognizer
                                  shouldReceiveTouch:touch]);
  touch.hitTestedView = wrapper;
  touch.pointInRootView = forward_center;
  EXPECT_TRUE([recognizer.delegate gestureRecognizer:recognizer
                                  shouldReceiveTouch:touch]);
  touch.hitTestedView = forward_button;
  EXPECT_TRUE([recognizer.delegate gestureRecognizer:recognizer
                                  shouldReceiveTouch:touch]);
  touch.hitTestedView = wrapper;
  touch.pointInRootView = untracked_center;
  EXPECT_FALSE([recognizer.delegate gestureRecognizer:recognizer
                                   shouldReceiveTouch:touch]);
  touch.hitTestedView = untracked_button;
  EXPECT_FALSE([recognizer.delegate gestureRecognizer:recognizer
                                   shouldReceiveTouch:touch]);
  touch.hitTestedView = wrapper;

  // Enabled tracked button rejects touches while sibling disabled button still
  // accepts.
  back_button.enabled = YES;
  touch.pointInRootView = back_center;
  EXPECT_FALSE([recognizer.delegate gestureRecognizer:recognizer
                                   shouldReceiveTouch:touch]);
  touch.pointInRootView = forward_center;
  EXPECT_TRUE([recognizer.delegate gestureRecognizer:recognizer
                                  shouldReceiveTouch:touch]);

  // Hidden button, zero-alpha button, hidden wrapper, non-interactive wrapper,
  // and collapsed wrapper all reject touches.
  forward_button.hidden = YES;
  EXPECT_FALSE([recognizer.delegate gestureRecognizer:recognizer
                                   shouldReceiveTouch:touch]);
  forward_button.hidden = NO;

  forward_button.alpha = 0.0;
  EXPECT_FALSE([recognizer.delegate gestureRecognizer:recognizer
                                   shouldReceiveTouch:touch]);
  forward_button.alpha = 1.0;

  wrapper.hidden = YES;
  EXPECT_FALSE([recognizer.delegate gestureRecognizer:recognizer
                                   shouldReceiveTouch:touch]);
  wrapper.hidden = NO;

  wrapper.userInteractionEnabled = NO;
  EXPECT_FALSE([recognizer.delegate gestureRecognizer:recognizer
                                   shouldReceiveTouch:touch]);
  wrapper.userInteractionEnabled = YES;

  wrapper.frame = CGRectMake(0, 0, 80, 44);
  EXPECT_FALSE([recognizer.delegate gestureRecognizer:recognizer
                                   shouldReceiveTouch:touch]);
  wrapper.frame = CGRectMake(0, 0, 200, 44);

  // Overlapping enabled control or interactive non-control sibling view that
  // won `hitTest:` is never intercepted even if the point lies inside a
  // disabled tracked button's bounds.
  untracked_button.enabled = YES;
  touch.hitTestedView = untracked_button;
  EXPECT_FALSE([recognizer.delegate gestureRecognizer:recognizer
                                   shouldReceiveTouch:touch]);

  UIView* overlapping_sibling =
      [[UIView alloc] initWithFrame:forward_button.frame];
  [wrapper addSubview:overlapping_sibling];
  touch.hitTestedView = overlapping_sibling;
  EXPECT_FALSE([recognizer.delegate gestureRecognizer:recognizer
                                   shouldReceiveTouch:touch]);
  [overlapping_sibling removeFromSuperview];

  // Pan gestures (such as toolbar side-swipes) take precedence over
  // disabled-button taps; other tap recognizers do not.
  UIPanGestureRecognizer* pan_recognizer =
      [[UIPanGestureRecognizer alloc] init];
  UITapGestureRecognizer* other_tap_recognizer =
      [[UITapGestureRecognizer alloc] init];
  EXPECT_TRUE([recognizer.delegate gestureRecognizer:recognizer
             shouldRequireFailureOfGestureRecognizer:pan_recognizer]);
  EXPECT_FALSE([recognizer.delegate gestureRecognizer:recognizer
              shouldRequireFailureOfGestureRecognizer:other_tap_recognizer]);
}

// Tests that `-gestureRecognizerShouldBegin:` re-checks button bounds, enabled
// state, and visibility at lift time so drag-offs or mid-press state changes do
// not misfire.
TEST_F(DisabledButtonsTapGestureRecognizerTest,
       GestureRecognizerShouldBeginReverifiesButtonAtLiftTime) {
  UIView* container = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 300, 44)];
  UIButton* back_button =
      [[UIButton alloc] initWithFrame:CGRectMake(0, 0, 80, 44)];
  back_button.enabled = NO;
  [container addSubview:back_button];

  TestDisabledButtonsTapGestureRecognizer* recognizer =
      [[TestDisabledButtonsTapGestureRecognizer alloc]
          initWithTarget:nil
                  action:nullptr
                 buttons:@[ back_button ]];
  [container addGestureRecognizer:recognizer];

  // Lifted inside the disabled button: begins.
  recognizer.fakeLocationInView = CGPointMake(40, 22);
  EXPECT_TRUE([recognizer.delegate gestureRecognizerShouldBegin:recognizer]);

  // Dragged off the button before lifting: rejected.
  recognizer.fakeLocationInView = CGPointMake(110, 22);
  EXPECT_FALSE([recognizer.delegate gestureRecognizerShouldBegin:recognizer]);

  // Back inside bounds, but button became enabled or hidden before lifting:
  // rejected.
  recognizer.fakeLocationInView = CGPointMake(40, 22);
  back_button.enabled = YES;
  EXPECT_FALSE([recognizer.delegate gestureRecognizerShouldBegin:recognizer]);
  back_button.enabled = NO;

  back_button.hidden = YES;
  EXPECT_FALSE([recognizer.delegate gestureRecognizerShouldBegin:recognizer]);
}

}  // namespace
