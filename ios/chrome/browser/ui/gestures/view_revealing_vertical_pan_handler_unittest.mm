// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"

#import <UIKit/UIKit.h>

#import "base/check_op.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/gestures/layout_switcher.h"
#import "ios/chrome/browser/ui/gestures/layout_switcher_provider.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_animatee.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A fake animatee with one observable animated property.
@interface FakeAnimatee : UIView <ViewRevealingAnimatee>
@property(nonatomic, assign) ViewRevealState state;
@end

@implementation FakeAnimatee
- (instancetype)init {
  self = [super init];
  if (self) {
    self.state = ViewRevealState::Hidden;
  }
  return self;
}
- (void)animateViewReveal:(ViewRevealState)viewRevealState {
  self.state = viewRevealState;
}
@end

// A fake layout switcher provider.
@interface FakeLayoutSwitcherProvider : NSObject <LayoutSwitcherProvider>
@end

@implementation FakeLayoutSwitcherProvider
@synthesize layoutSwitcher = _layoutSwitcher;

- (instancetype)initWithLayoutSwitcher:(id<LayoutSwitcher>)layoutSwitcher {
  self = [super init];
  if (self) {
    _layoutSwitcher = layoutSwitcher;
  }
  return self;
}
@end

// A fake layout switcher with observable layout properties.
@interface FakeLayoutSwitcher : NSObject <LayoutSwitcher>
@property(nonatomic, assign) LayoutSwitcherState state;
@property(nonatomic, assign) LayoutSwitcherState nextState;
@property(nonatomic, copy) void (^transitionCompletionBlock)
    (BOOL completed, BOOL finished);
@end

@implementation FakeLayoutSwitcher

- (LayoutSwitcherState)currentLayoutSwitcherState {
  return self.state;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    self.state = LayoutSwitcherState::Horizontal;
  }
  return self;
}

- (void)willTransitionToLayout:(LayoutSwitcherState)nextState
                    completion:
                        (void (^)(BOOL completed, BOOL finished))completion {
  self.nextState = nextState;
  self.transitionCompletionBlock = completion;
}

- (void)didUpdateTransitionLayoutProgress:(CGFloat)progress {
}

- (void)didTransitionToLayoutSuccessfully:(BOOL)success {
  if (success) {
    self.state = self.nextState;
  }
  self.transitionCompletionBlock(YES, success);
}
@end

// A fake gesture recognizer that allows the translation and velocity to be set.
@interface FakeGestureRecognizer : UIPanGestureRecognizer
@property(nonatomic, assign) CGPoint translation;
@property(nonatomic, assign) CGPoint velocity;
@end

@implementation FakeGestureRecognizer
- (CGPoint)translationInView:(UIView*)view {
  return self.translation;
}
- (CGPoint)velocityInView:(UIView*)view {
  return self.velocity;
}
@end

namespace {
// The distance between the Hidden and Peeked states.
const CGFloat kThumbStripHeight = 212.0f;
const CGFloat kBaseViewHeight = 800.0f;
// The percentage of the total distance traveled by a gesture required to
// trigger a transition.
const CGFloat kRevealThreshold = 1 / 3.0f;
// A small extra distance to guarantee that the minimum distance has been
// traveled during a pan gesture simulation.
const CGFloat kSmallOffset = 10.0f;
// The delay in ms after the gesture ends before starting a new one.
const int kAnimationDelay = 5;

// The test class, passed as argument to TEST_F().
using ViewRevealingVerticalPanHandlerTest = PlatformTest;

// Simulates a fake vertical pan gesture from beginning, to change, to end.
// `translation_y` is by how much the gesture translates vertically
void SimulatePanGesture(ViewRevealingVerticalPanHandler* pan_handler,
                        double translation_y) {
  // A small offset in the same direction as the translation to guarantee that
  // the gesture's translation is greater than the reveal threshold.
  double offset = translation_y > 0 ? kSmallOffset : -kSmallOffset;
  FakeGestureRecognizer* fake_gesture_recognizer =
      [[FakeGestureRecognizer alloc]
          initWithTarget:pan_handler
                  action:@selector(handlePanGesture:)];

  if (![pan_handler gestureRecognizerShouldBegin:fake_gesture_recognizer]) {
    return;
  }

  fake_gesture_recognizer.state = UIGestureRecognizerStateBegan;
  [pan_handler handlePanGesture:fake_gesture_recognizer];
  fake_gesture_recognizer.state = UIGestureRecognizerStateChanged;
  fake_gesture_recognizer.translation = CGPointMake(0, translation_y);
  [pan_handler handlePanGesture:fake_gesture_recognizer];
  fake_gesture_recognizer.translation = CGPointMake(0, translation_y + offset);
  fake_gesture_recognizer.state = UIGestureRecognizerStateEnded;
  [pan_handler handlePanGesture:fake_gesture_recognizer];
  // The runloop needs to be spun between the end of a gesture and the begining
  // of another one, because the current state of the pan_handler needs to be
  // updated to its next state before starting a new transition.
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(kAnimationDelay));
}

// Simulates 4 transitions of state in a ViewRevealingVerticalPanHandler (Hidden
// -> Peeked -> Revealed -> Peeked -> Hidden), and observes the resulting change
// of state in a fake animatee.
TEST_F(ViewRevealingVerticalPanHandlerTest, DetectPan) {
  // Create a view revealing vertical pan handler.
  ViewRevealingVerticalPanHandler* pan_handler =
      [[ViewRevealingVerticalPanHandler alloc]
          initWithPeekedHeight:kThumbStripHeight
                baseViewHeight:kBaseViewHeight
                  initialState:ViewRevealState::Hidden];

  // Create a fake layout switcher and a provider.
  FakeLayoutSwitcher* fake_layout_switcher = [[FakeLayoutSwitcher alloc] init];
  FakeLayoutSwitcherProvider* fake_layout_switcher_provider =
      [[FakeLayoutSwitcherProvider alloc]
          initWithLayoutSwitcher:fake_layout_switcher];
  pan_handler.layoutSwitcherProvider = fake_layout_switcher_provider;
  EXPECT_EQ(LayoutSwitcherState::Horizontal, fake_layout_switcher.state);

  // Create a fake animatee.
  FakeAnimatee* fake_animatee = [[FakeAnimatee alloc] init];
  [pan_handler addAnimatee:fake_animatee];
  EXPECT_EQ(ViewRevealState::Hidden, fake_animatee.state);

  // Simulate a pan gesture from Hidden state to Peeked state.
  SimulatePanGesture(pan_handler, kThumbStripHeight * kRevealThreshold);
  EXPECT_EQ(ViewRevealState::Peeked, fake_animatee.state);

  // Simulate a pan gesture from Peeked state to Revealed state. The layout
  // should transition to full state.
  SimulatePanGesture(pan_handler, kBaseViewHeight * kRevealThreshold);
  EXPECT_EQ(ViewRevealState::Revealed, fake_animatee.state);
  EXPECT_EQ(LayoutSwitcherState::Grid, fake_layout_switcher.state);

  // Simulate a pan gesture from Revealed state to Peeked state. The layout
  // should transition back to horizontal state.
  SimulatePanGesture(pan_handler, -(kBaseViewHeight * kRevealThreshold));
  EXPECT_EQ(ViewRevealState::Peeked, fake_animatee.state);
  EXPECT_EQ(LayoutSwitcherState::Horizontal, fake_layout_switcher.state);

  // Simulate a pan gesture from Peeked state to Hidden state.
  SimulatePanGesture(pan_handler, -(kThumbStripHeight * kRevealThreshold));
  EXPECT_EQ(ViewRevealState::Hidden, fake_animatee.state);
}

// Tests that manually moving the pan handler between the two outer-most states
// updates everything correctly.
TEST_F(ViewRevealingVerticalPanHandlerTest, ManualStateChange) {
  // Create a view revealing vertical pan handler.
  ViewRevealingVerticalPanHandler* pan_handler =
      [[ViewRevealingVerticalPanHandler alloc]
          initWithPeekedHeight:kThumbStripHeight
                baseViewHeight:kBaseViewHeight
                  initialState:ViewRevealState::Hidden];

  // Create a fake layout switcher and a provider.
  FakeLayoutSwitcher* fake_layout_switcher = [[FakeLayoutSwitcher alloc] init];
  FakeLayoutSwitcherProvider* fake_layout_switcher_provider =
      [[FakeLayoutSwitcherProvider alloc]
          initWithLayoutSwitcher:fake_layout_switcher];
  pan_handler.layoutSwitcherProvider = fake_layout_switcher_provider;
  EXPECT_EQ(LayoutSwitcherState::Horizontal, fake_layout_switcher.state);

  // Create a fake animatee. Try direct to tab grid and back.
  FakeAnimatee* fake_animatee = [[FakeAnimatee alloc] init];
  [pan_handler addAnimatee:fake_animatee];
  EXPECT_EQ(ViewRevealState::Hidden, fake_animatee.state);

  [pan_handler setNextState:ViewRevealState::Revealed
                   animated:NO
                    trigger:ViewRevealTrigger::Unknown];
  EXPECT_EQ(ViewRevealState::Revealed, fake_animatee.state);
  EXPECT_EQ(LayoutSwitcherState::Grid, fake_layout_switcher.state);

  [pan_handler setNextState:ViewRevealState::Hidden
                   animated:NO
                    trigger:ViewRevealTrigger::Unknown];
  EXPECT_EQ(ViewRevealState::Hidden, fake_animatee.state);
  EXPECT_EQ(LayoutSwitcherState::Grid, fake_layout_switcher.state);

  // Try from hidden to peek to hidden
  [pan_handler setNextState:ViewRevealState::Peeked
                   animated:NO
                    trigger:ViewRevealTrigger::Unknown];
  EXPECT_EQ(ViewRevealState::Peeked, fake_animatee.state);
  EXPECT_EQ(LayoutSwitcherState::Horizontal, fake_layout_switcher.state);

  [pan_handler setNextState:ViewRevealState::Hidden
                   animated:NO
                    trigger:ViewRevealTrigger::Unknown];
  EXPECT_EQ(ViewRevealState::Hidden, fake_animatee.state);
  EXPECT_EQ(LayoutSwitcherState::Horizontal, fake_layout_switcher.state);
}

// Tests that a second gesture does not interrupt the first gesture.
TEST_F(ViewRevealingVerticalPanHandlerTest, ConflictingGestures) {
  // Create a view revealing vertical pan handler.
  ViewRevealingVerticalPanHandler* pan_handler =
      [[ViewRevealingVerticalPanHandler alloc]
          initWithPeekedHeight:kThumbStripHeight
                baseViewHeight:kBaseViewHeight
                  initialState:ViewRevealState::Hidden];

  // Create a fake animatee.
  FakeAnimatee* fake_animatee = [[FakeAnimatee alloc] init];
  [pan_handler addAnimatee:fake_animatee];
  EXPECT_EQ(ViewRevealState::Hidden, fake_animatee.state);

  // Scroll down to transition to Peeked state.
  SimulatePanGesture(pan_handler, kThumbStripHeight * kRevealThreshold);
  EXPECT_EQ(ViewRevealState::Peeked, fake_animatee.state);

  // Start a gesture moving further down.
  FakeGestureRecognizer* fake_gesture_recognizer =
      [[FakeGestureRecognizer alloc]
          initWithTarget:pan_handler
                  action:@selector(handlePanGesture:)];

  EXPECT_TRUE(
      [pan_handler gestureRecognizerShouldBegin:fake_gesture_recognizer]);
  fake_gesture_recognizer.state = UIGestureRecognizerStateBegan;
  [pan_handler handlePanGesture:fake_gesture_recognizer];
  fake_gesture_recognizer.state = UIGestureRecognizerStateChanged;
  fake_gesture_recognizer.translation = CGPointMake(0, kSmallOffset);

  // Start and finish a second gesture moving back up to Hidden.
  SimulatePanGesture(pan_handler, -(kThumbStripHeight * kRevealThreshold));
  // This second gesture should NOT change the state back to Hidden.
  EXPECT_NE(ViewRevealState::Hidden, fake_animatee.state);

  // Start and finish a second gesture moving down to Revealed.
  SimulatePanGesture(pan_handler, kBaseViewHeight * kRevealThreshold);
  // This second gesture should NOT change the state to Revealed.
  EXPECT_NE(ViewRevealState::Revealed, fake_animatee.state);

  // Finish the ongoing gesture down to Revealed. This should change the state
  // to revealed.
  fake_gesture_recognizer.state = UIGestureRecognizerStateChanged;
  fake_gesture_recognizer.translation = CGPointMake(0, kBaseViewHeight);
  fake_gesture_recognizer.state = UIGestureRecognizerStateEnded;
  [pan_handler handlePanGesture:fake_gesture_recognizer];
  // The runloop needs to be spun between the end of a gesture and the begining
  // of another one, because the current state of the pan_handler needs to be
  // updated to its next state before starting a new transition.
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(kAnimationDelay));

  EXPECT_NE(ViewRevealState::Revealed, fake_animatee.state);
}

// Tests that a current state reports the right value.
TEST_F(ViewRevealingVerticalPanHandlerTest, CurrentState) {
  // Create a view revealing vertical pan handler.
  ViewRevealingVerticalPanHandler* pan_handler =
      [[ViewRevealingVerticalPanHandler alloc]
          initWithPeekedHeight:kThumbStripHeight
                baseViewHeight:kBaseViewHeight
                  initialState:ViewRevealState::Hidden];
  EXPECT_EQ(ViewRevealState::Hidden, pan_handler.currentState);

  [pan_handler setNextState:ViewRevealState::Revealed
                   animated:NO
                    trigger:ViewRevealTrigger::Unknown];
  EXPECT_EQ(ViewRevealState::Revealed, pan_handler.currentState);

  [pan_handler setNextState:ViewRevealState::Peeked
                   animated:NO
                    trigger:ViewRevealTrigger::Unknown];
  EXPECT_EQ(ViewRevealState::Peeked, pan_handler.currentState);

  [pan_handler setNextState:ViewRevealState::Hidden
                   animated:NO
                    trigger:ViewRevealTrigger::Unknown];
  EXPECT_EQ(ViewRevealState::Hidden, pan_handler.currentState);
}

}  // namespace
