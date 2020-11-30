// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"

#import "base/notreached.h"
#include "base/numerics/ranges.h"
#import "ios/chrome/browser/ui/gestures/layout_switcher.h"

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

// Represents one of the three possible "states" of view reveal, which are:
// No view revealed (Hidden), view partially revealed (Peeked), and view
// completely revealed (Revealed).
@property(nonatomic, assign) ViewRevealState currentState;
// The state that the currentState will be set to if the transition animation
// completes with its REVERSED property set to NO.
@property(nonatomic, assign) ViewRevealState nextState;
// The property animator for revealing the view.
@property(nonatomic, strong) UIViewPropertyAnimator* animator;
// Total distance between the Peeked state and Revealed state. Equal to
// |revealedHeight| - |peekedHeight|.
@property(nonatomic, assign, readonly) CGFloat remainingHeight;
// Height of the cover view (the view in front of the view that will be
// revealed) that will still be visible after the remaining reveal transition.
@property(nonatomic, assign, readonly) CGFloat revealedCoverHeight;
// The progress of the animator.
@property(nonatomic, assign) CGFloat progressWhenInterrupted;
// Set of UI elements which are animated during view reveal transitions.
@property(nonatomic, strong) NSHashTable<id<ViewRevealingAnimatee>>* animatees;
// Whether the revealed view is undergoing a transition of layout. Set to YES
// when the transition layout is created and set to NO inside the transition's
// completion block.
@property(nonatomic, assign) BOOL layoutInTransition;
// Whether the layout transition is being interactively scrubbed by the user.
// Set to YES when the transition layout is created and set to NO when the pan
// gesture ends.
@property(nonatomic, assign) BOOL layoutBeingInteractedWith;
// Whether new pan gestures should be handled. Set to NO when a pan gesture ends
// and set to YES when a pan gesture starts while layoutInTransition is NO.
@property(nonatomic, assign) BOOL gesturesEnabled;

// The contentOffset during the previous call to -scrollViewDidScroll:. Used to
// keep the contentOffset the same during successive calls to
// -scrollViewDidScroll:.
@property(nonatomic, assign) CGPoint lastScrollOffset;

@end

@implementation ViewRevealingVerticalPanHandler

- (instancetype)initWithPeekedHeight:(CGFloat)peekedHeight
                 revealedCoverHeight:(CGFloat)revealedCoverHeight
                      baseViewHeight:(CGFloat)baseViewHeight {
  if (self = [super init]) {
    _peekedHeight = peekedHeight;
    _revealedCoverHeight = revealedCoverHeight;
    _baseViewHeight = baseViewHeight;
    _revealedHeight = baseViewHeight - revealedCoverHeight;
    _remainingHeight = _revealedHeight - peekedHeight;
    _currentState = ViewRevealState::Hidden;
    _animatees = [NSHashTable weakObjectsHashTable];
    _layoutInTransition = NO;
  }
  return self;
}

- (void)handlePanGesture:(UIPanGestureRecognizer*)gesture {
  // Avoid handling a pan gesture if it started before the layout transition was
  // finished.
  if (!self.layoutInTransition &&
      gesture.state == UIGestureRecognizerStateBegan) {
    self.gesturesEnabled = YES;
  }
  if (!self.gesturesEnabled)
    return;
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

- (void)setBaseViewHeight:(CGFloat)baseViewHeight {
  _baseViewHeight = baseViewHeight;
  _revealedHeight = baseViewHeight - _revealedCoverHeight;
  _remainingHeight = _revealedHeight - _peekedHeight;
}

- (void)setState:(ViewRevealState)state animated:(BOOL)animated {
  self.nextState = state;
  [self createAnimatorIfNeeded];
  if (animated) {
    [self.animator startAnimation];
  } else {
    [self.animator setFractionComplete:1];
    [self.animator stopAnimation:NO];
    [self.animator finishAnimationAtPosition:UIViewAnimatingPositionEnd];
  }
  [self completeLayoutTransitionSuccessfully:YES];
}

#pragma mark - Private Methods: Animating

// Called right before an animation block to warn all animatees of a transition
// from the current view reveal state.
- (void)willAnimateViewReveal {
  for (id<ViewRevealingAnimatee> animatee in self.animatees) {
    [animatee willAnimateViewReveal:self.currentState];
  }
}

// Called inside an animation block to animate all animatees to the next view
// reveal state.
- (void)animateToNextViewRevealState {
  for (id<ViewRevealingAnimatee> animatee in self.animatees) {
    [animatee animateViewReveal:self.nextState];
  }
}

// Called inside the completion block of the current animation. Takes as
// argument the state to which the animatees did animate to.
- (void)didAnimateViewReveal:(ViewRevealState)viewRevealState {
  for (id<ViewRevealingAnimatee> animatee in self.animatees) {
    [animatee didAnimateViewReveal:viewRevealState];
  }
}

// Creates the animation for the transition to the next view reveal state, if
// different from the current state.
- (void)createAnimatorIfNeeded {
  if (self.currentState == self.nextState) {
    return;
  }
  [self willAnimateViewReveal];
  [self.animator stopAnimation:YES];
  self.animator = [[UIViewPropertyAnimator alloc]
      initWithDuration:kAnimationDuration
          dampingRatio:1
            animations:^() {
              [self animateToNextViewRevealState];
            }];
  __weak ViewRevealingVerticalPanHandler* weakSelf = self;
  [self.animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    if (!weakSelf.animator.reversed) {
      weakSelf.currentState = weakSelf.nextState;
    }
    [weakSelf didAnimateViewReveal:weakSelf.currentState];
  }];
  [self.animator pauseAnimation];
  [self createLayoutTransitionIfNeeded];
}

// Creates a transition layout in the revealed view if going from Peeked to
// Revealed state or vice-versa.
- (void)createLayoutTransitionIfNeeded {
  if (self.layoutInTransition) {
    // Cancel the current layout transition.
    [self.layoutSwitcherProvider.layoutSwitcher
        didUpdateTransitionLayoutProgress:0];
    [self.layoutSwitcherProvider.layoutSwitcher
        didTransitionToLayoutSuccessfully:NO];
    self.layoutBeingInteractedWith = NO;
    return;
  }

  if (self.nextState == ViewRevealState::Revealed) {
    [self willTransitionToLayout:LayoutSwitcherState::Grid];
  } else if (self.currentState == ViewRevealState::Revealed &&
             (self.nextState == ViewRevealState::Peeked ||
              self.nextState == ViewRevealState::Hidden)) {
    [self willTransitionToLayout:LayoutSwitcherState::Horizontal];
  }
}

// Notifies the layout switcher that a layout transition should happen.
- (void)willTransitionToLayout:(LayoutSwitcherState)nextState {
  auto completion = ^(BOOL completed, BOOL finished) {
    self.layoutInTransition = NO;
  };
  [self.layoutSwitcherProvider.layoutSwitcher
      willTransitionToLayout:nextState
                  completion:completion];
  self.layoutInTransition = YES;
  self.layoutBeingInteractedWith = YES;
}

// Initiates a transition if there isn't already one running
- (void)animateTransitionIfNeeded {
  if (self.animator.isRunning) {
    self.animator.reversed = NO;
    return;
  }

  self.nextState = ViewRevealState::Peeked;
  // If the current state is Peeked, the animator is not created just yet
  // because the gesture might go in one of two directions. It will only be
  // created after the gesture changes and its translation direction is
  // determined.
  [self createAnimatorIfNeeded];
}

#pragma mark - Private Methods: Pan handling

// Returns whether the gesture's translation and velocity were enough to trigger
// the revealing of a view with the specified height (partial reveal height or
// remaining reveal height).
- (BOOL)shouldRevealWithTranslation:(CGFloat)translation
                           velocity:(CGFloat)velocity
                             height:(CGFloat)height {
  CGFloat progress = self.progressWhenInterrupted +
                     (translation + velocity * kVelocityWeight) / height;
  return progress > kRevealThreshold;
}

// Returns whether the gesture's translation and velocity were enough to trigger
// the hiding of a view with the specified height (partial reveal height or
// remaining reveal height).
- (BOOL)shouldHideWithTranslation:(CGFloat)translation
                         velocity:(CGFloat)velocity
                           height:(CGFloat)height {
  return [self shouldRevealWithTranslation:translation
                                  velocity:velocity
                                    height:-height];
}

// Returns what the next state should be, given the translation, velocity of
// the gesture, and the current state.
- (ViewRevealState)nextStateWithTranslation:(CGFloat)translation
                                   Velocity:(CGFloat)velocity {
  switch (self.currentState) {
    case ViewRevealState::Hidden:
      return [self shouldRevealWithTranslation:translation
                                      velocity:velocity
                                        height:self.peekedHeight]
                 ? ViewRevealState::Peeked
                 : ViewRevealState::Hidden;
    case ViewRevealState::Revealed:
      return [self shouldHideWithTranslation:translation
                                    velocity:velocity
                                      height:self.remainingHeight]
                 ? ViewRevealState::Peeked
                 : ViewRevealState::Revealed;
    case ViewRevealState::Peeked:
      if ([self shouldHideWithTranslation:translation
                                 velocity:velocity
                                   height:self.peekedHeight]) {
        return ViewRevealState::Hidden;
      }
      if ([self shouldRevealWithTranslation:translation
                                   velocity:velocity
                                     height:self.remainingHeight]) {
        return ViewRevealState::Revealed;
      }
      return self.currentState;
  }
}

// Updates the progress of the animator, depending on the current and next
// states.
- (void)updateAnimatorProgress:(CGFloat)translation {
  CGFloat progress;
  switch (self.currentState) {
    case ViewRevealState::Peeked: {
      CGFloat height =
          (self.nextState == ViewRevealState::Hidden ? -self.peekedHeight
                                                     : self.remainingHeight);
      progress = translation / height;
      break;
    }
    case ViewRevealState::Hidden:
      progress = translation / self.peekedHeight;
      break;
    case ViewRevealState::Revealed:
      progress = translation / (-self.remainingHeight);
      break;
  }

  progress += self.progressWhenInterrupted;
  progress = base::ClampToRange<CGFloat>(progress, 0, 1);
  self.animator.fractionComplete = progress;
  if (self.layoutBeingInteractedWith) {
    [self.layoutSwitcherProvider.layoutSwitcher
        didUpdateTransitionLayoutProgress:progress];
  }
}

// Handles the start of the pan gesture.
- (void)panGestureBegan {
  [self animateTransitionIfNeeded];
  [self.animator pauseAnimation];
  self.progressWhenInterrupted = self.animator.fractionComplete;
}

// Handles the movement after the start of the gesture.
- (void)panGestureChangedWithTranslation:(CGFloat)translation {
  if (self.currentState == ViewRevealState::Peeked) {
    // If the gesture translation passes through the midpoint (the point where
    // the state is Peeked), the current animation should be stopped and a new
    // one created.
    if (translation > 0) {
      if (self.nextState != ViewRevealState::Revealed &&
          !self.layoutInTransition) {
        self.nextState = ViewRevealState::Revealed;
        [self createAnimatorIfNeeded];
      }
    } else {
      if (self.nextState != ViewRevealState::Hidden) {
        self.nextState = ViewRevealState::Hidden;
        [self createAnimatorIfNeeded];
      }
    }
  }
  [self updateAnimatorProgress:translation];
}

// Handles the end of the gesture.
- (void)panGestureEndedWithTranslation:(CGFloat)translation
                              velocity:(CGFloat)velocity {
  self.animator.reversed =
      (self.currentState == [self nextStateWithTranslation:translation
                                                  Velocity:velocity]);

  [self.animator continueAnimationWithTimingParameters:nil durationFactor:1];

  [self completeLayoutTransitionSuccessfully:!self.animator.reversed];
}

// If the layout is currently changing, tells the layout provider to
// finish the transition.
- (void)completeLayoutTransitionSuccessfully:(BOOL)success {
  if (self.layoutBeingInteractedWith) {
    [self.layoutSwitcherProvider.layoutSwitcher
        didTransitionToLayoutSuccessfully:success];
    self.gesturesEnabled = NO;
    self.layoutBeingInteractedWith = NO;
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  switch (self.currentState) {
    case ViewRevealState::Hidden:
      // The transition out of hidden state can only start if the scroll view
      // starts dragging from the top.
      if (!self.animator.isRunning && scrollView.contentOffset.y != 0) {
        return;
      }
      break;
    case ViewRevealState::Peeked:
      break;
    case ViewRevealState::Revealed:
      // The scroll views should be covered in Revealed state, so should not
      // be able to be scrolled.
      NOTREACHED();
      break;
  }
  [self panGestureBegan];
  self.lastScrollOffset = scrollView.contentOffset;
}

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // These delegate methods are approximating the pan gesture handling from
  // above, so only change things if the user is actively scrolling.
  if (!scrollView.isDragging) {
    return;
  }
  UIPanGestureRecognizer* gesture = scrollView.panGestureRecognizer;
  CGFloat translationY = [gesture translationInView:gesture.view.superview].y;
  // When in Peeked state, scrolling can only transition to Hidden state.
  if (self.currentState == ViewRevealState::Peeked && translationY > 0) {
    translationY = 0;
  }
  [self panGestureChangedWithTranslation:translationY];
  // During the transition, the ViewRevealingAnimatees should be moving, not the
  // scroll view.
  if (self.animator.fractionComplete > 0 &&
      self.animator.fractionComplete < 1) {
    CGPoint currentScrollOffset = scrollView.contentOffset;
    currentScrollOffset.y = std::max(self.lastScrollOffset.y, 0.0);
    scrollView.contentOffset = currentScrollOffset;
  }
  self.lastScrollOffset = scrollView.contentOffset;
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  if (self.currentState == ViewRevealState::Hidden &&
      self.animator.state != UIViewAnimatingStateActive) {
    return;
  }
  UIPanGestureRecognizer* gesture = scrollView.panGestureRecognizer;
  CGFloat translationY = [gesture translationInView:gesture.view.superview].y;
  CGFloat velocityY = [gesture velocityInView:gesture.view.superview].y;
  // When in Peeked state, scrolling can only transition to Hidden state.
  if (self.currentState == ViewRevealState::Peeked && translationY > 0) {
    translationY = 0;
    velocityY = 0;
  }

  [self panGestureEndedWithTranslation:translationY velocity:velocityY];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  // No-op.
}

@end
