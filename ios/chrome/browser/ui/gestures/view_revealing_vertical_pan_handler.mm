// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"

#include "base/numerics/ranges.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The weight multiplier of the gesture velocity used in the equation that
// determines whether the translation and velocity of the gesture are enough
// to trigger revealing the view. The threshold is the percentage of the height
// of the view that must be "traveled" by the gesture to trigger the transition.
const CGFloat kVelocityWeight = 0.5f;
const CGFloat kRevealThreshold = 1 / 3.0f;

// Duration of the animation to reveal/hide the view.
const CGFloat kAnimationDuration = 0.25f;
}  // namespace

@interface ViewRevealingVerticalPanHandler ()

// The property animator for revealing the view.
@property(nonatomic, strong) UIViewPropertyAnimator* animator;
// Whether the view is currently revealed or not.
@property(nonatomic, assign, getter=isViewRevealed, setter=setIsViewRevealed:)
    BOOL viewRevealed;
// The progress of the animator before being interrupted.
@property(nonatomic, assign) CGFloat progressWhenInterrupted;
// Set of UI elements which are animated during thumb strip transitions.
@property(nonatomic, strong) NSMutableSet<id<ViewRevealingAnimatee>>* animatees;

@end

@implementation ViewRevealingVerticalPanHandler

- (instancetype)initWithHeight:(CGFloat)height {
  if (self = [super init]) {
    _viewRevealed = NO;
    _viewHeight = height;
    _animatees = [[NSMutableSet alloc] init];
  }
  return self;
}

- (void)handlePanGesture:(UIPanGestureRecognizer*)gesture {
  CGFloat translationY = [gesture translationInView:gesture.view.superview].y;

  if (gesture.state == UIGestureRecognizerStateBegan) {
    [self panGestureBegan];
  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    [self panGestureChangedWithTranslation:translationY];
  } else if (gesture.state == UIGestureRecognizerStateEnded) {
    CGFloat velocityY = [gesture velocityInView:gesture.view.superview].y;
    [self panGestureEndedWithTranslation:translationY velocity:velocityY];
  }
}

- (void)addAnimatee:(id<ViewRevealingAnimatee>)animatee {
  [self.animatees addObject:animatee];
}

#pragma mark - Private Methods: Animating

// Called right before an animation block to warn all animatees.
- (void)willAnimateViewReveal:(BOOL)viewRevealed {
  for (id<ViewRevealingAnimatee> animatee in self.animatees) {
    [animatee willAnimateViewReveal:viewRevealed];
  }
}

// Called inside an animation block to animate all animatees.
- (void)animateViewReveal:(BOOL)viewRevealed {
  for (id<ViewRevealingAnimatee> animatee in self.animatees) {
    [animatee animateViewReveal:viewRevealed];
  }
}

// Called after an animation block.
- (void)didAnimateViewReveal:(BOOL)viewRevealed {
  for (id<ViewRevealingAnimatee> animatee in self.animatees) {
    [animatee didAnimateViewReveal:viewRevealed];
  }
}

#pragma mark - Private Methods: Pan handling

// Returns whether the gesture's translation and velocity were enough to trigger
// revealing the view.
- (BOOL)attainedRevealThresholdWithTranslation:(CGFloat)translation
                                      Velocity:(CGFloat)velocity {
  return self.progressWhenInterrupted +
             (translation + velocity * kVelocityWeight) / self.viewHeight >
         kRevealThreshold;
}

// Returns whether the gesture's translation and velocity were enough to trigger
// hiding the view.
- (BOOL)attainedHideThresholdWithTranslation:(CGFloat)translation
                                    Velocity:(CGFloat)velocity {
  return self.progressWhenInterrupted +
             (translation + velocity * kVelocityWeight) / (-self.viewHeight) >
         kRevealThreshold;
}

// Initiate a transition if it isn't already running
- (void)animateTransitionIfNeeded {
  if (self.animator.isRunning) {
    self.animator.reversed = NO;
    return;
  }

  [self willAnimateViewReveal:self.viewRevealed];
  // Create the transition to reveal the view.
  auto animationBlock = ^() {
    [self animateViewReveal:self.isViewRevealed];
  };
  auto completionBlock = ^(UIViewAnimatingPosition finalPosition) {
    if (!self.animator.reversed) {
      self.isViewRevealed = !self.isViewRevealed;
    }
    [self didAnimateViewReveal:self.isViewRevealed];
  };

  self.animator =
      [[UIViewPropertyAnimator alloc] initWithDuration:kAnimationDuration
                                          dampingRatio:1
                                            animations:animationBlock];
  [self.animator addCompletion:completionBlock];
}

// Handles the start of the pan gesture.
- (void)panGestureBegan {
  [self animateTransitionIfNeeded];
  [self.animator pauseAnimation];
  self.progressWhenInterrupted = self.animator.fractionComplete;
}

// Handles the movement after the start of the gesture.
- (void)panGestureChangedWithTranslation:(CGFloat)translation {
  CGFloat progress =
      (self.isViewRevealed ? -1 : 1) * translation / self.viewHeight +
      self.progressWhenInterrupted;
  progress = base::ClampToRange<CGFloat>(progress, 0.0, 1.0);
  self.animator.fractionComplete = progress;
}

// Handles the end of the gesture.
- (void)panGestureEndedWithTranslation:(CGFloat)translation
                              velocity:(CGFloat)velocity {
  self.animator.reversed = YES;

  if ((!self.isViewRevealed &&
       [self attainedRevealThresholdWithTranslation:translation
                                           Velocity:velocity]) ||
      (self.isViewRevealed &&
       [self attainedHideThresholdWithTranslation:translation
                                         Velocity:velocity])) {
    self.animator.reversed = NO;
  }

  [self.animator continueAnimationWithTimingParameters:nil durationFactor:1];
}

@end
