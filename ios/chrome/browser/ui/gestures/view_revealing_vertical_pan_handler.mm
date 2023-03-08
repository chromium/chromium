// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"

#import "base/check_op.h"
#import "base/cxx17_backports.h"
#import "base/ios/block_types.h"
#import "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/gestures/layout_switcher.h"
#import "ios/chrome/browser/ui/gestures/pan_handler_scroll_view.h"

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

// Duration of the animation to reveal/hide the vi`ew.
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

// Defines which scrolling view is being tracked.
enum class ScrollViewTracking {
  None,
  Gesture,
  WebView,
  NTP,
};
}  // namespace

@interface ViewRevealingPanGestureRecognizer ()

// Trigger for this custom PanGestureRecognizer.
@property(nonatomic, assign) ViewRevealTrigger trigger;

@end

@implementation ViewRevealingPanGestureRecognizer

- (instancetype)initWithTarget:(id)target
                        action:(SEL)action
                       trigger:(ViewRevealTrigger)trigger {
  if (self = [super initWithTarget:target action:action]) {
    _trigger = trigger;
  }
  return self;
}

@end

@interface ViewRevealingVerticalPanHandler ()

// Privately redeclare `currentState` as readwrite.
@property(nonatomic, readwrite, assign) ViewRevealState currentState;

// The latest trigger that brought the view to the currentState (or nextState if
// a transition is on).
@property(nonatomic, readwrite, assign) ViewRevealTrigger changeStateTrigger;
// The state that the currentState will be set to if the transition animation
// completes with its REVERSED property set to NO.
@property(nonatomic, assign) ViewRevealState nextState;
// The property animator for revealing the view.
@property(nonatomic, strong) UIViewPropertyAnimator* animator;
// Total distance between the Peeked state and Revealed state.
@property(nonatomic, assign, readonly) CGFloat remainingHeight;
// The progress of the animator.
@property(nonatomic, assign) CGFloat progressWhenInterrupted;
// Set of UI elements which are animated during view reveal transitions.
@property(nonatomic, strong) NSHashTable<id<ViewRevealingAnimatee>>* animatees;
// The current state tracking whether the revealed view is undergoing a
// transition of layout. This is `::Inactive` initially. It is set to `::Active`
// when the transition layout is created.  It is set to `::Finishing` when the
// layout transition should start to finish. (This takes time because of
// finishing animations/UIKit restrictions). Finally, in the transition's
// completion block, this is set back to `::Inactive`.
@property(nonatomic, assign) LayoutTransitionState layoutTransitionState;
// Whether new pan gestures should be handled. Set to NO when a pan gesture ends
// and set to YES when a pan gesture starts while layoutInTransition is NO.
@property(nonatomic, assign) BOOL gesturesEnabled;
// YES if a drag gesture began but isn't active yet (thumbstrip-wise). If YES,
// on each scroll, it checks if an overscroll occured, and if so makes the
// gesture active. NO if gesture is active or if no check is required.
@property(nonatomic, assign) BOOL deferredScrollEnabled;
// The contentOffset during the previous call to -scrollViewDidScroll:. Used to
// keep the contentOffset the same during successive calls to
// -scrollViewDidScroll:.
@property(nonatomic, assign) CGPoint lastScrollOffset;
// The gesture vertical translation offset when transition started.
@property(nonatomic, assign) CGFloat startTransitionY;

// Holds the gesture recognizer that is currently in progess. Any other
// gestures received while one is active will be ignored.
@property(nonatomic, weak) UIGestureRecognizer* currentRecognizer;

// Defines which scrolling view is being tracked to avoid crossing signals.
@property(nonatomic, assign) ScrollViewTracking scrollViewTracking;

@end

@implementation ViewRevealingVerticalPanHandler

- (instancetype)initWithPeekedHeight:(CGFloat)peekedHeight
                      baseViewHeight:(CGFloat)baseViewHeight
                        initialState:(ViewRevealState)initialState {
  if (self = [super init]) {
    _peekedHeight = peekedHeight;
    _baseViewHeight = baseViewHeight;
    _remainingHeight = baseViewHeight - peekedHeight;
    _currentState = initialState;
    _animatees = [NSHashTable weakObjectsHashTable];
    _layoutTransitionState = LayoutTransitionState::Inactive;
    _scrollViewTracking = ScrollViewTracking::None;
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
  self.scrollViewTracking = ScrollViewTracking::Gesture;
  CGFloat translationY = [gesture translationInView:gesture.view.superview].y;

  if ([gesture isKindOfClass:ViewRevealingPanGestureRecognizer.class]) {
    self.changeStateTrigger =
        (base::mac::ObjCCastStrict<ViewRevealingPanGestureRecognizer>(gesture))
            .trigger;
  } else {
    self.changeStateTrigger = ViewRevealTrigger::Unknown;
  }

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
  [UIView performWithoutAnimation:^{
    if ([animatee respondsToSelector:@selector
                  (willAnimateViewRevealFromState:toState:)]) {
      [animatee willAnimateViewRevealFromState:self.currentState
                                       toState:self.currentState];
    }
    if ([animatee respondsToSelector:@selector(animateViewReveal:)]) {
      [animatee animateViewReveal:self.currentState];
    }
    if ([animatee respondsToSelector:@selector
                  (didAnimateViewRevealFromState:toState:trigger:)]) {
      [animatee didAnimateViewRevealFromState:self.currentState
                                      toState:self.currentState
                                      trigger:self.changeStateTrigger];
    }
  }];
}

- (void)setBaseViewHeight:(CGFloat)baseViewHeight {
  _baseViewHeight = baseViewHeight;
  _remainingHeight = baseViewHeight - _peekedHeight;
}

- (void)setNextState:(ViewRevealState)state
            animated:(BOOL)animated
             trigger:(ViewRevealTrigger)trigger {
  // Remember new trigger even if the next state is ignored.
  self.changeStateTrigger = trigger;

  // Don't change animation if state is already currentState, it creates
  // confusion.
  if (self.currentState == state) {
    return;
  }

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
    if ([animatee respondsToSelector:@selector
                  (willAnimateViewRevealFromState:toState:)]) {
      [animatee willAnimateViewRevealFromState:self.currentState
                                       toState:self.nextState];
    }
  }
}

// Called inside an animation block to animate all animatees to the next view
// reveal state.
- (void)animateToNextViewRevealState {
  for (id<ViewRevealingAnimatee> animatee in self.animatees) {
    if ([animatee respondsToSelector:@selector(animateViewReveal:)]) {
      [animatee animateViewReveal:self.nextState];
    }
  }
}

// Called inside the completion block of the current animation. Takes as
// argument the state to which the animatees did animate to.
- (void)didAnimateViewRevealFromState:(ViewRevealState)fromViewRevealState
                              toState:(ViewRevealState)toViewRevealState {
  for (id<ViewRevealingAnimatee> animatee in self.animatees) {
    if ([animatee respondsToSelector:@selector
                  (didAnimateViewRevealFromState:toState:trigger:)]) {
      [animatee didAnimateViewRevealFromState:fromViewRevealState
                                      toState:toViewRevealState
                                      trigger:self.changeStateTrigger];
    }
  }
}

// Calls animatees who want to know when a web view drag starts and when it
// ends (at the end of deceleration).
- (void)webViewIsDragging:(BOOL)dragging
          viewRevealState:(ViewRevealState)viewRevealState {
  for (id<ViewRevealingAnimatee> animatee in self.animatees) {
    if ([animatee respondsToSelector:@selector(webViewIsDragging:
                                                 viewRevealState:)]) {
      [animatee webViewIsDragging:dragging viewRevealState:viewRevealState];
    }
  }
}

// Creates the animation for the transition to the next view reveal state, if
// different from the current state.
- (void)createAnimatorIfNeeded {
  if (self.currentState == self.nextState) {
    return;
  }
  ViewRevealState startState = self.currentState;
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
    // Animator crashes if stopAnimation is called during this completer. It can
    // happened if a listener to `didAnimateViewRevealFromState:toState`
    // triggers a new state change. Make it nil to avoid calling that.
    weakSelf.animator = nil;
    [weakSelf didAnimateViewRevealFromState:startState
                                    toState:weakSelf.currentState];
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

  // Table of required layout (h = Horizontal, g = Grid) change and animation
  // (n = NO, y = YES) based on from and to state:
  // From:              To: Hidden  Peeked  Revealed/Fullscreen
  // Hidden                 x       h/n     g/n
  // Peeked                 x       x       g/y
  // Revealed/Fullscreen    x       h/y     x
  if (self.currentState == self.nextState) {
    return;
  }

  LayoutSwitcherState nextLayoutState =
      self.layoutSwitcherProvider.layoutSwitcher.currentLayoutSwitcherState;
  BOOL animated = NO;

  switch (self.currentState) {
    case ViewRevealState::Hidden: {
      nextLayoutState = (self.nextState == ViewRevealState::Revealed)
                            ? LayoutSwitcherState::Grid
                            : LayoutSwitcherState::Horizontal;
      break;
    }
    case ViewRevealState::Peeked:
      if (self.nextState == ViewRevealState::Revealed) {
        nextLayoutState = LayoutSwitcherState::Grid;
        animated = YES;
      }
      break;
    case ViewRevealState::Revealed:
      if (self.nextState == ViewRevealState::Peeked) {
        nextLayoutState = LayoutSwitcherState::Horizontal;
        animated = YES;
      }
      break;
  }

  if (self.layoutSwitcherProvider.layoutSwitcher.currentLayoutSwitcherState !=
      nextLayoutState) {
    [self willTransitionToLayout:nextLayoutState];
    if (!animated) {
      [self.layoutSwitcherProvider.layoutSwitcher
          didUpdateTransitionLayoutProgress:1];
      [self didTransitionToLayoutSuccessfully:YES];
    }
  }
}

// Notifies the layout switcher that a layout transition should happen.
- (void)willTransitionToLayout:(LayoutSwitcherState)nextState {
  // Don't do anything if there isn't a layout switcher available. Especially
  // don't change the `layoutTransitionState`.
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
      [self setNextState:self.nextState
                animated:YES
                 trigger:self.changeStateTrigger];
    });
  };
  [self.layoutSwitcherProvider.layoutSwitcher
      willTransitionToLayout:nextState
                  completion:completion];
  self.layoutTransitionState = LayoutTransitionState::Active;
}

// Notifies the layout switcher that a layout transition finished with
// `success`.
- (void)didTransitionToLayoutSuccessfully:(BOOL)success {
  // Don't do anything if there isn't a layout switcher available. Especially
  // don't change the `layoutTransitionState`.
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

#pragma mark - UIScrollViewDelegate (NTP)

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  PanHandlerScrollView* view =
      [[PanHandlerScrollView alloc] initWithScrollView:scrollView];
  self.scrollViewTracking = ScrollViewTracking::NTP;
  [self webViewIsDragging:YES viewRevealState:self.currentState];
  [self panHandlerScrollViewWillBeginDragging:view];
}

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  if (self.scrollViewTracking != ScrollViewTracking::NTP) {
    return;
  }

  PanHandlerScrollView* view =
      [[PanHandlerScrollView alloc] initWithScrollView:scrollView];
  [self panHandlerScrollViewDidScroll:view];
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  if (self.scrollViewTracking != ScrollViewTracking::NTP) {
    return;
  }

  PanHandlerScrollView* view =
      [[PanHandlerScrollView alloc] initWithScrollView:scrollView];
  [self panHandlerScrollViewWillEndDragging:view
                               withVelocity:velocity
                        targetContentOffset:targetContentOffset];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  if (self.scrollViewTracking != ScrollViewTracking::NTP) {
    return;
  }
  [self webViewIsDragging:NO viewRevealState:self.currentState];
}

#pragma mark - CRWWebViewScrollViewProxyObserver

- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  PanHandlerScrollView* view = [[PanHandlerScrollView alloc]
      initWithWebViewScrollViewProxy:webViewScrollViewProxy];
  self.scrollViewTracking = ScrollViewTracking::WebView;
  [self webViewIsDragging:YES viewRevealState:self.currentState];
  [self panHandlerScrollViewWillBeginDragging:view];
}

- (void)webViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  if (self.scrollViewTracking != ScrollViewTracking::WebView) {
    return;
  }
  PanHandlerScrollView* view = [[PanHandlerScrollView alloc]
      initWithWebViewScrollViewProxy:webViewScrollViewProxy];
  [self panHandlerScrollViewDidScroll:view];
}

- (void)webViewScrollViewWillEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                            withVelocity:(CGPoint)velocity
                     targetContentOffset:(inout CGPoint*)targetContentOffset {
  if (self.scrollViewTracking != ScrollViewTracking::WebView) {
    return;
  }

  PanHandlerScrollView* view = [[PanHandlerScrollView alloc]
      initWithWebViewScrollViewProxy:webViewScrollViewProxy];
  [self panHandlerScrollViewWillEndDragging:view
                               withVelocity:velocity
                        targetContentOffset:targetContentOffset];

  if (!self.deferredScrollEnabled) {
    self.scrollViewTracking = ScrollViewTracking::None;
  }
}

- (void)webViewScrollViewDidEndDecelerating:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  if (self.scrollViewTracking != ScrollViewTracking::WebView) {
    return;
  }
  [self webViewIsDragging:NO viewRevealState:self.currentState];
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
      self.deferredScrollEnabled = YES;
      break;
    }
    case ViewRevealState::Peeked: {
      [self startDraggingForPanHandlerScrollView:scrollView];
      break;
    }
    case ViewRevealState::Revealed:
      // The scroll views should be covered in Revealed state, so should not
      // be able to be scrolled. But this can still happen with NTP, due to
      // some async calls, so ignore.
      break;
  }
}

- (void)panHandlerScrollViewDidScroll:(PanHandlerScrollView*)scrollView {
  // Early return if there is no current recognizer or one that does not match
  // this scroll view's recognizer. The first can happen when the scroll view
  // scrolls after the user lifts their finger. This should not be handled as
  // these methods are only approximating the actual pan gesture handling from
  // above. The second can happen if the user scrolls and uses one of the pan
  // gestures simultaneously.
  if (self.currentRecognizer != scrollView.panGestureRecognizer &&
      ![self checkDeferredDraggingForPanHandlerScrollView:scrollView]) {
    return;
  }
  UIPanGestureRecognizer* gesture = scrollView.panGestureRecognizer;
  CGFloat translationY = [gesture translationInView:gesture.view.superview].y -
                         self.startTransitionY;
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
    // Stop monitoring for top overscroll
    self.deferredScrollEnabled = NO;
    return;
  }
  self.currentRecognizer = nil;
  if (self.currentState == ViewRevealState::Hidden &&
      self.animator.state != UIViewAnimatingStateActive) {
    return;
  }
  UIPanGestureRecognizer* gesture = scrollView.panGestureRecognizer;
  CGFloat translationY = [gesture translationInView:gesture.view.superview].y -
                         self.startTransitionY;
  CGFloat velocityY = [gesture velocityInView:gesture.view.superview].y;
  // When in Peeked state, scrolling can only transition to Hidden state.
  if (self.currentState == ViewRevealState::Peeked && translationY > 0) {
    translationY = 0;
    velocityY = 0;
  }
  // Sometimes when user changes direction, the translation and velocity ends
  // up going in different direction, 'locking' the transition in mid step
  // instead of completing it. Setting translation to 0 forces both to point
  // in the same velocity direction, and lets the animator finish the
  // transition.
  if ((translationY < 0 && velocityY > 0) ||
      (translationY > 0 && velocityY < 0)) {
    translationY = 0;
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

#pragma mark - Private

// If self.deferredScrollEnabled is YES, returns YES if the pan gesture should
// be active for the given `scrollView` due to a top overscroll and sets up the
// initial pan state.
- (BOOL)checkDeferredDraggingForPanHandlerScrollView:
    (PanHandlerScrollView*)scrollView {
  if (!self.deferredScrollEnabled ||
      self.currentState != ViewRevealState::Hidden) {
    return NO;
  }
  // Check for overscroll at the top.
  CGFloat contentOffsetY =
      scrollView.contentOffset.y + scrollView.contentInset.top;
  if (contentOffsetY <= 0.0) {
    [self startDraggingForPanHandlerScrollView:scrollView];
    return YES;
  }
  return NO;
}

- (void)startDraggingForPanHandlerScrollView:(PanHandlerScrollView*)scrollView {
  UIPanGestureRecognizer* gesture = scrollView.panGestureRecognizer;
  self.startTransitionY = [gesture translationInView:gesture.view.superview].y;
  self.currentRecognizer = scrollView.panGestureRecognizer;
  self.changeStateTrigger = ViewRevealTrigger::WebScroll;
  [self panGestureBegan];
  self.lastScrollOffset = scrollView.contentOffset;
  self.deferredScrollEnabled = NO;
}

@end
