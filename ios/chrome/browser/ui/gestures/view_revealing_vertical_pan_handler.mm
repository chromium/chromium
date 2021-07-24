// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"

#import "base/check_op.h"
#include "base/cxx17_backports.h"
#include "base/logging.h"
#import "base/notreached.h"
#import "ios/chrome/browser/ui/gestures/layout_switcher.h"
#import "ios/chrome/browser/ui/gestures/pan_handler_scroll_view.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

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

// The 3 stages or steps of the transitions handled by the view revealing
// vertical pan handler class.
enum class LayoutTransitionState {
  // The layout is not transitioning.
  Inactive,
  // The layout is actively transitioning.
  Active,
  // The layout transition is in the process of finishing. The UIKit collection
  // view transition API breaks if a transition is finished again in the time
  // between when the finish is requested and the UIKit animations and cleanup
  // are completed.
  Finishing,
};

}  // namespace

@interface ViewRevealingVerticalPanHandler ()

// Privately redeclare |currentState| as readwrite.
@property(nonatomic, readwrite, assign) ViewRevealState currentState;

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
// The current state tracking whether the revealed view is undergoing a
// transition of layout. This is |::Inactive| initially. It is set to |::Active|
// when the transition layout is created.  It is set to |::Finishing| when the
// layout transition should start to finish. (This takes time because of
// finishing animations/UIKit restrictions). Finally, in the transition's
// completion block, this is set back to |::Inactive|.
@property(nonatomic, assign) LayoutTransitionState layoutTransitionState;
// Whether new pan gestures should be handled. Set to NO when a pan gesture ends
// and set to YES when a pan gesture starts while layoutInTransition is NO.
@property(nonatomic, assign) BOOL gesturesEnabled;

// The contentOffset during the previous call to -scrollViewDidScroll:. Used to
// keep the contentOffset the same during successive calls to
// -scrollViewDidScroll:.
@property(nonatomic, assign) CGPoint lastScrollOffset;

// Holds the gesture recognizer that is currently in progess. Any other
// gestures received while one is active will be ignored.
@property(nonatomic, weak) UIGestureRecognizer* currentRecognizer;

@end

@implementation ViewRevealingVerticalPanHandler

- (instancetype)initWithPeekedHeight:(CGFloat)peekedHeight
                 revealedCoverHeight:(CGFloat)revealedCoverHeight
                      baseViewHeight:(CGFloat)baseViewHeight
                        initialState:(ViewRevealState)initialState {
  if (self = [super init]) {
    _peekedHeight = peekedHeight;
    _revealedCoverHeight = revealedCoverHeight;
    _baseViewHeight = baseViewHeight;
    _revealedHeight = baseViewHeight - revealedCoverHeight;
    _remainingHeight = _revealedHeight - peekedHeight;
    _currentState = initialState;
    _animatees = [NSHashTable weakObjectsHashTable];
    _layoutTransitionState = LayoutTransitionState::Inactive;
  }
  return self;
}

- (void)handlePanGesture:(UIPanGestureRecognizer*)gesture {
  // Start handling gestures again once the layout is no longer transitioning.
  if (self.layoutTransitionState == LayoutTransitionState::Inactive &&
      gesture.state == UIGestureRecognizerStateBegan) {
    self.gesturesEnabled = YES;
  }
  if (!self.gesturesEnabled)
    return;
  self.currentRecognizer = gesture;
  CGFloat translationY = [gesture translationInView:gesture.view.superview].y;

  if (gesture.state == UIGestureRecognizerStateBegan) {
    [self panGestureBegan];
  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    [self panGestureChangedWithTranslation:translationY];
  } else if (gesture.state == UIGestureRecognizerStateEnded) {
    CGFloat velocityY = [gesture velocityInView:gesture.view.superview].y;
    [self panGestureEndedWithTranslation:translationY velocity:velocityY];
    self.currentRecognizer = nil;
  } else if (gesture.state == UIGestureRecognizerStateCancelled) {
    self.currentRecognizer = nil;
  }
}

- (void)addAnimatee:(id<ViewRevealingAnimatee>)animatee {
  [self.animatees addObject:animatee];
  // Make sure the newly added animatee is in the correct state.
  [animatee willAnimateViewRevealFromState:self.currentState
                                   toState:self.currentState];
  [animatee animateViewReveal:self.currentState];
  [animatee didAnimateViewReveal:self.currentState];
}

- (void)setBaseViewHeight:(CGFloat)baseViewHeight {
  _baseViewHeight = baseViewHeight;
  _revealedHeight = baseViewHeight - _revealedCoverHeight;
  _remainingHeight = _revealedHeight - _peekedHeight;
}

- (void)setNextState:(ViewRevealState)state animated:(BOOL)animated {
  self.nextState = state;

  // If the layout is currently finishing its transition, a new transition
  // cannot be started. Instead, re-call this method once the transition has
  // finished.
  if (self.layoutTransitionState == LayoutTransitionState::Finishing) {
    return;
  }

  [self createAnimatorIfNeeded];
  if (animated) {
    [self.animator startAnimation];
  } else {
    [self.animator setFractionComplete:1];
    [self.animator stopAnimation:NO];
    [self.animator finishAnimationAtPosition:UIViewAnimatingPositionEnd];
  }
  // If the layout is currently changing, finish the transition.
  if (self.layoutTransitionState == LayoutTransitionState::Active) {
    [self didTransitionToLayoutSuccessfully:YES];
    self.gesturesEnabled = NO;
  }
}

#pragma mark - Private Methods: Animating

// Called right before an animation block to warn all animatees of a transition
// from the current view reveal state.
- (void)willAnimateViewReveal {
  for (id<ViewRevealingAnimatee> animatee in self.animatees) {
    [animatee willAnimateViewRevealFromState:self.currentState
                                     toState:self.nextState];
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

  __weak ViewRevealingVerticalPanHandler* weakSelf = self;
  self.animator = [[UIViewPropertyAnimator alloc]
      initWithDuration:kAnimationDuration
          dampingRatio:1
            animations:^() {
              [weakSelf animateToNextViewRevealState];
            }];
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
  if (self.layoutTransitionState == LayoutTransitionState::Finishing) {
    return;
  }

  if (self.layoutTransitionState == LayoutTransitionState::Active) {
    // Cancel the current layout transition.
    [self.layoutSwitcherProvider.layoutSwitcher
        didUpdateTransitionLayoutProgress:0];
    [self didTransitionToLayoutSuccessfully:NO];
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
  // Don't do anything if there isn't a layout switcher available. Especially
  // don't change the |layoutTransitionState|.
  if (!self.layoutSwitcherProvider.layoutSwitcher) {
    return;
  }
  DCHECK_EQ(self.layoutTransitionState, LayoutTransitionState::Inactive);
  auto completion = ^(BOOL completed, BOOL finished) {
    if (self.nextState == self.currentState ||
        self.animator.state == UIViewAnimatingStateActive) {
      self.layoutTransitionState = LayoutTransitionState::Inactive;
      return;
    }
    // If current state doesn't match the next state and the animator is not
    // active, then next state has been changed while the transition is
    // finishing. Start a new programmatic transition to the correct final
    // state. Triggering a transiton from inside the completion block of a
    // transition seems to cause the new transition's completion block to never
    // fire, so do that on the next run loop.
    dispatch_async(dispatch_get_main_queue(), ^{
      self.layoutTransitionState = LayoutTransitionState::Inactive;
      // Make sure the next state hasn't changed.
      if (self.nextState == self.currentState ||
          self.animator.state == UIViewAnimatingStateActive) {
        return;
      }
      [self setNextState:self.nextState animated:YES];
    });
  };
  [self.layoutSwitcherProvider.layoutSwitcher
      willTransitionToLayout:nextState
                  completion:completion];
  self.layoutTransitionState = LayoutTransitionState::Active;
}

// Notifies the layout switcher that a layout transition finished with
// |success|.
- (void)didTransitionToLayoutSuccessfully:(BOOL)success {
  // Don't do anything if there isn't a layout switcher available. Especially
  // don't change the |layoutTransitionState|.
  if (!self.layoutSwitcherProvider.layoutSwitcher) {
    return;
  }
  DCHECK_EQ(self.layoutTransitionState, LayoutTransitionState::Active);
  self.layoutTransitionState = LayoutTransitionState::Finishing;
  [self.layoutSwitcherProvider.layoutSwitcher
      didTransitionToLayoutSuccessfully:success];
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
  progress = base::clamp<CGFloat>(progress, 0, 1);
  self.animator.fractionComplete = progress;
  if (self.layoutTransitionState == LayoutTransitionState::Active) {
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
      // The transition state may be inactive even while panning when going
      // betwteen Hidden and Peeked states, as those two states don't involve a
      // layout transition.
      if (self.nextState != ViewRevealState::Revealed &&
          self.layoutTransitionState == LayoutTransitionState::Inactive) {
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

  if (self.animator.reversed) {
    self.nextState = self.currentState;
  }

  [self.animator continueAnimationWithTimingParameters:nil durationFactor:1];

  // If the layout is currently changing, finish the transition.
  if (self.layoutTransitionState == LayoutTransitionState::Active) {
    [self didTransitionToLayoutSuccessfully:!self.animator.reversed];
    self.gesturesEnabled = NO;
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  PanHandlerScrollView* view =
      [[PanHandlerScrollView alloc] initWithScrollView:scrollView];
  [self panHandlerScrollViewWillBeginDragging:view];
}

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  PanHandlerScrollView* view =
      [[PanHandlerScrollView alloc] initWithScrollView:scrollView];
  [self panHandlerScrollViewDidScroll:view];
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  PanHandlerScrollView* view =
      [[PanHandlerScrollView alloc] initWithScrollView:scrollView];
  [self panHandlerScrollViewWillEndDragging:view
                               withVelocity:velocity
                        targetContentOffset:targetContentOffset];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  // No-op.
}

#pragma mark - CRWWebViewScrollViewProxyObserver

- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  PanHandlerScrollView* view = [[PanHandlerScrollView alloc]
      initWithWebViewScrollViewProxy:webViewScrollViewProxy];
  [self panHandlerScrollViewWillBeginDragging:view];
}

- (void)webViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  PanHandlerScrollView* view = [[PanHandlerScrollView alloc]
      initWithWebViewScrollViewProxy:webViewScrollViewProxy];
  [self panHandlerScrollViewDidScroll:view];
}

- (void)webViewScrollViewWillEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                            withVelocity:(CGPoint)velocity
                     targetContentOffset:(inout CGPoint*)targetContentOffset {
  PanHandlerScrollView* view = [[PanHandlerScrollView alloc]
      initWithWebViewScrollViewProxy:webViewScrollViewProxy];
  [self panHandlerScrollViewWillEndDragging:view
                               withVelocity:velocity
                        targetContentOffset:targetContentOffset];
}

#pragma mark - UIScrollViewDelegate + CRWWebViewScrollViewProxyObserver

- (void)panHandlerScrollViewWillBeginDragging:
    (PanHandlerScrollView*)scrollView {
  if (self.currentRecognizer &&
      self.currentRecognizer != scrollView.panGestureRecognizer) {
    return;
  }
  switch (self.currentState) {
    case ViewRevealState::Hidden: {
      // The transition out of hidden state can only start if the scroll view
      // starts dragging from the top.
      CGFloat contentOffsetY =
          scrollView.contentOffset.y + scrollView.contentInset.top;
      if (!AreCGFloatsEqual(contentOffsetY, 0.0)) {
        return;
      }
      break;
    }
    case ViewRevealState::Peeked:
      break;
    case ViewRevealState::Revealed:
      // The scroll views should be covered in Revealed state, so should not
      // be able to be scrolled.
      NOTREACHED();
  }
  self.currentRecognizer = scrollView.panGestureRecognizer;
  [self panGestureBegan];
  self.lastScrollOffset = scrollView.contentOffset;
}

- (void)panHandlerScrollViewDidScroll:(PanHandlerScrollView*)scrollView {
  // Early return if there is no current recognizer or one that does not match
  // this scroll view's recognizer. The first can happen when the scroll view
  // scrolls after the user lifts their finger. This should not be handled as
  // these methods are only approximating the actual pan gesture handling from
  // above. The second can happen if the user scrolls and uses one of the pan
  // gestures simultaneously.
  if (self.currentRecognizer != scrollView.panGestureRecognizer) {
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
    currentScrollOffset.y = self.lastScrollOffset.y;
    scrollView.contentOffset = currentScrollOffset;
  }
  self.lastScrollOffset = scrollView.contentOffset;
}

- (void)panHandlerScrollViewWillEndDragging:(PanHandlerScrollView*)scrollView
                               withVelocity:(CGPoint)velocity
                        targetContentOffset:
                            (inout CGPoint*)targetContentOffset {
  // Early return if there is no current recognizer or one that does not match
  // this scroll view's recognizer. The first can happen when the scroll view
  // scrolls after the user lifts their finger. This should not be handled as
  // these methods are only approximating the actual pan gesture handling from
  // above. The second can happen if the user scrolls and uses one of the pan
  // gestures simultaneously.
  if (self.currentRecognizer != scrollView.panGestureRecognizer) {
    return;
  }
  self.currentRecognizer = nil;
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

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  if (self.currentRecognizer) {
    return NO;
  }
  return YES;
}

@end
