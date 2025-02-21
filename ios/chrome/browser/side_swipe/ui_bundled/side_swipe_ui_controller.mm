// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_ui_controller.h"

#import "base/ios/block_types.h"
#import "base/notreached.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_gesture_recognizer.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mutator.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_navigation_delegate.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_navigation_view.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_snapshot_navigation_view.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_ui_controller_delegate.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/side_swipe_toolbar_interacting.h"

namespace {

// Swipe starting distance from edge.
const CGFloat kSwipeEdge = 20;

}  // namespace

@implementation SideSwipeUIController {
  // Side swipe view for page navigation.
  UIView<HorizontalPanGestureHandler>* _pageSideSwipeView;

  // Curtain over web view while waiting for it to load.
  UIView* _curtain;

  // Swipe gesture recognizer.
  SideSwipeGestureRecognizer* _swipeGestureRecognizer;

  // If the swipe is for a page change or a tab change.
  SwipeType _swipeType;

  // YES if the user is currently swiping.
  BOOL _inSwipe;

  // The animated disabler displays the toolbar when a side swipe navigation
  // gesture is being recognized.
  std::unique_ptr<AnimatedScopedFullscreenDisabler> _animatedFullscreenDisabler;

  // Whether to allow navigating from the leading edge.
  BOOL _leadingEdgeNavigationEnabled;

  // Whether to allow navigating from the trailing edge.
  BOOL _trailingEdgeNavigationEnabled;
}

- (void)addHorizontalGesturesToView:(UIView*)view {
  _swipeGestureRecognizer = [[SideSwipeGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleSwipe:)];
  [_swipeGestureRecognizer setMaximumNumberOfTouches:1];
  [_swipeGestureRecognizer setDelegate:self];
  [_swipeGestureRecognizer setSwipeEdge:kSwipeEdge];
  [view addGestureRecognizer:_swipeGestureRecognizer];
}

- (void)animateSwipe:(SwipeType)swipeType
         inDirection:(UISwipeGestureRecognizerDirection)direction {
  if (self.mutator.inSwipe ||
      [_sideSwipeUIControllerDelegate preventSideSwipe]) {
    return;
  }
  switch (swipeType) {
    case SwipeType::NONE:
    case SwipeType::CHANGE_TAB:
      NOTREACHED();
    case SwipeType::CHANGE_PAGE:
      [self animatePageNavigationInDirection:direction];
      break;
  }
}

- (void)prepareForSlideInDirection:(UISwipeGestureRecognizerDirection)direction
                     snapshotImage:(UIImage*)snapshotImage {
  if (_inSwipe || [_sideSwipeUIControllerDelegate preventSideSwipe] ||
      !snapshotImage) {
    return;
  }

  _inSwipe = YES;
  [_sideSwipeUIControllerDelegate
      updateAccessoryViewsForSideSwipeWithVisibility:NO];

  _pageSideSwipeView = [self fullscreenSnapshotSideSwipeView:direction
                                               snapshotImage:snapshotImage];

  UIView* fullscreenView =
      [_sideSwipeUIControllerDelegate sideSwipeFullscreenView];

  [_pageSideSwipeView setTargetView:fullscreenView];
  [fullscreenView.superview insertSubview:_pageSideSwipeView
                             belowSubview:fullscreenView];

  __weak SideSwipeUIController* weakSelf = self;

  [self addCurtainWithCompletionHandler:^{
    [weakSelf handleCurtainCompletion];
  }];

  [_pageSideSwipeView moveTargetViewOffscreenInDirection:direction];
}

- (void)slideToCenterAnimated {
  [_pageSideSwipeView moveTargetViewOnScreenWithAnimation];
}

- (void)setEnabled:(BOOL)enabled {
  [_swipeGestureRecognizer setEnabled:enabled];
}

#pragma mark - SideSwipeConsumer

- (void)setTrailingEdgeNavigationEnabled:(BOOL)enabled {
  _trailingEdgeNavigationEnabled = enabled;
}

- (void)setLeadingEdgeNavigationEnabled:(BOOL)enabled {
  _leadingEdgeNavigationEnabled = enabled;
}

- (void)cancelOnGoingSwipe {
  [self dismissCurtain];
  // Toggling the gesture's enabled state off and on will effectively cancel
  // the gesture recognizer.
  [_swipeGestureRecognizer setEnabled:NO];
  [_swipeGestureRecognizer setEnabled:YES];
}

- (void)webPageLoaded {
  [self dismissCurtain];
}

#pragma mark - UIGestureRecognizerDelegate Methods

// Gestures should only be recognized within `contentArea_` or the toolbar.
- (BOOL)gestureRecognizerShouldBegin:(SideSwipeGestureRecognizer*)gesture {
  if (_inSwipe) {
    return NO;
  }

  if ([_sideSwipeUIControllerDelegate preventSideSwipe]) {
    return NO;
  }

  if (IsContextualPanelEnabled()) {
    // Don't handle gesture if it's meant for the Contextual Panel Entrypoint
    // (gesture began in its frame) and that entrypoint is currently large.
    // `contextualPanelEntrypointView` is nil if the entrypoint is not currently
    // large, which means the gesture won't be blocked here.
    UIView* contextualPanelEntrypointView = [self.layoutGuideCenter
        referencedViewUnderName:kContextualPanelLargeEntrypointGuide];
    CGPoint touchLocationInEntrypointViewCoordinates =
        [contextualPanelEntrypointView convertPoint:[gesture locationInView:nil]
                                           fromView:nil];
    BOOL tapInsideContextualPanelEntrypointContainer =
        [contextualPanelEntrypointView
            pointInside:touchLocationInEntrypointViewCoordinates
              withEvent:nil];

    if (tapInsideContextualPanelEntrypointContainer) {
      return NO;
    }
  }

  CGPoint location = [gesture locationInView:gesture.view];

  // Since the toolbar and the contentView can overlap, check the toolbar frame
  // first, and confirm the right gesture recognizer is firing.
  if ([self.toolbarInteractionHandler
          isInsideToolbar:[gesture.view convertPoint:location toView:nil]]) {
    return NO;
  }

  // Otherwise, only allow contentView touches with `swipeGestureRecognizer_`.
  // The content view frame is inset by -1 because CGRectContainsPoint does
  // include points on the max X and Y edges, which will happen frequently with
  // edge swipes from the right side.
  CGRect contentViewFrame = CGRectInset(
      [[_sideSwipeUIControllerDelegate sideSwipeContentView] frame], -1, -1);

  if (!CGRectContainsPoint(contentViewFrame, location)) {
    return NO;
  }

  if (![gesture isEqual:_swipeGestureRecognizer]) {
    return NO;
  }

  if (![self edgeNavigationIsEnabledForDirection:gesture.direction]) {
    return NO;
  }

  _swipeType = SwipeType::CHANGE_PAGE;
  return YES;
}

// Always return yes, as this swipe should work with various recognizers,
// including UITextTapRecognizer, UILongPressGestureRecognizer,
// UIScrollViewPanGestureRecognizer and others.
- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldBeRequiredToFailByGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  // Take precedence over a pan gesture recognizer so that moving up and
  // down while swiping doesn't trigger overscroll actions.
  if ([otherGestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]]) {
    return YES;
  }
  // Take precedence over a WKWebView side swipe gesture.
  if ([otherGestureRecognizer
          isKindOfClass:[UIScreenEdgePanGestureRecognizer class]]) {
    return YES;
  }
  return NO;
}

#pragma mark - private

// Animate page navigation.
- (void)animatePageNavigationInDirection:
    (UISwipeGestureRecognizerDirection)direction {
  _inSwipe = YES;
  [_mutator setInSwipe:YES];

  [_sideSwipeUIControllerDelegate
      updateAccessoryViewsForSideSwipeWithVisibility:NO];

  UIView* navigatingView =
      [_sideSwipeUIControllerDelegate sideSwipeContentView].superview;
  CGRect navigatingBounds = navigatingView.bounds;
  CGFloat headerHeight =
      [_sideSwipeUIControllerDelegate headerHeightForSideSwipe];
  CGRect navigationFrame =
      CGRectMake(CGRectGetMinX(navigatingBounds),
                 CGRectGetMinY(navigatingBounds) + headerHeight,
                 CGRectGetWidth(navigatingBounds),
                 CGRectGetHeight(navigatingBounds) - headerHeight);

  BOOL canNavigate = [self.navigationDelegate
      canNavigateInDirection:IsSwipingBack(direction)
                                 ? NavigationDirectionBack
                                 : NavigationDirectionForward];

  if ([self swipingFullScreenContent:direction]) {
    _pageSideSwipeView = [self
        fullscreenSnapshotSideSwipeView:direction
                          snapshotImage:[self.navigationDelegate
                                            swipeNavigationSnapshotForDirection:
                                                direction]];
    UIView* fullscreenView =
        [_sideSwipeUIControllerDelegate sideSwipeFullscreenView];
    [_pageSideSwipeView setTargetView:fullscreenView];
    if (_pageSideSwipeView) {
      [fullscreenView.superview insertSubview:_pageSideSwipeView
                                 belowSubview:fullscreenView];
    }
  } else {
    _pageSideSwipeView = [self webContentSideSwipeView:navigationFrame
                                             direction:direction
                                           canNavigate:canNavigate];
    [_pageSideSwipeView
        setTargetView:[_sideSwipeUIControllerDelegate sideSwipeContentView]];
    [navigatingView
        insertSubview:_pageSideSwipeView
         belowSubview:[_sideSwipeUIControllerDelegate topToolbarView]];
  }

  __weak SideSwipeUIController* weakSelf = self;
  [_pageSideSwipeView
      animateHorizontalPanWithDirection:direction
                      completionHandler:^{
                        if (canNavigate) {
                          [weakSelf handleOverThresholdCompletion:direction];
                        } else {
                          [weakSelf handleUnderThresholdCompletion];
                        }
                      }];
}

// Handles the completion of a side swipe gesture that did not meet the
// navigation threshold.
- (void)handleUnderThresholdCompletion {
  [_sideSwipeUIControllerDelegate
      updateAccessoryViewsForSideSwipeWithVisibility:YES];
  _inSwipe = NO;
  [self.mutator setInSwipe:NO];
}

// Handles the completion of a side swipe gesture that met the navigation
// threshold.
- (void)handleOverThresholdCompletion:
    (UISwipeGestureRecognizerDirection)direction {
  BOOL wantsBack = IsSwipingBack(direction);
  [self.mutator navigateInDirection:wantsBack ? NavigationDirectionBack
                                              : NavigationDirectionForward];
  __weak SideSwipeUIController* weakSelf = self;

  [self addCurtainWithCompletionHandler:^{
    [weakSelf handleCurtainCompletion];
  }];

  [_sideSwipeUIControllerDelegate
      updateAccessoryViewsForSideSwipeWithVisibility:YES];
}

- (void)handleCurtainCompletion {
  _inSwipe = NO;
  [self.mutator setInSwipe:NO];
}

// Adds a visual "curtain" overlay to the view, used to obscure content during
// page loading.
- (void)addCurtainWithCompletionHandler:(ProceduralBlock)completionHandler {
  if (!_curtain) {
    _curtain = [[UIView alloc]
        initWithFrame:[_sideSwipeUIControllerDelegate sideSwipeContentView]
                          .bounds];
    [_curtain setBackgroundColor:[UIColor whiteColor]];
  }
  [[_sideSwipeUIControllerDelegate sideSwipeContentView] addSubview:_curtain];

  // Fallback in case load takes a while. 3 seconds is a balance between how
  // long it can take a web view to clear the previous page image, and what
  // feels like to 'too long' to see the curtain.
  [self performSelector:@selector(dismissCurtainWithCompletionHandler:)
             withObject:[completionHandler copy]
             afterDelay:3];
}

// Removes the visual "curtain" overlay from the view, revealing the underlying
// content.
- (void)dismissCurtainWithCompletionHandler:(ProceduralBlock)completionHandler {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  [_curtain removeFromSuperview];
  _curtain = nil;
  completionHandler();
}

- (void)dismissCurtain {
  if (!_inSwipe) {
    return;
  }
  __weak SideSwipeUIController* weakSelf = self;
  [self dismissCurtainWithCompletionHandler:^{
    [weakSelf handleCurtainCompletion];
  }];
}

// Handles swipe gesture.
- (void)handleSwipe:(SideSwipeGestureRecognizer*)gesture {
  DCHECK(_swipeType != SwipeType::NONE);
  if (_swipeType == SwipeType::CHANGE_TAB) {
    return;
  }
  if (_swipeType == SwipeType::CHANGE_PAGE) {
    return [self handleSwipeToNavigate:gesture];
  }
  NOTREACHED();
}

// Handles page swipes.
- (void)handleSwipeToNavigate:(SideSwipeGestureRecognizer*)gesture {
  if (gesture.state == UIGestureRecognizerStateBegan) {
    // Make sure the Toolbar is visible by disabling Fullscreen.
    _animatedFullscreenDisabler =
        std::make_unique<AnimatedScopedFullscreenDisabler>(
            self.fullscreenController);
    _animatedFullscreenDisabler->StartAnimation();

    _inSwipe = YES;
    [self.mutator setInSwipe:YES];
    [_sideSwipeUIControllerDelegate
        updateAccessoryViewsForSideSwipeWithVisibility:NO];
    BOOL goBack = IsSwipingBack(gesture.direction);

    CGRect gestureBounds = gesture.view.bounds;
    CGFloat headerHeight =
        [_sideSwipeUIControllerDelegate headerHeightForSideSwipe];
    CGRect navigationFrame =
        CGRectMake(CGRectGetMinX(gestureBounds),
                   CGRectGetMinY(gestureBounds) + headerHeight,
                   CGRectGetWidth(gestureBounds),
                   CGRectGetHeight(gestureBounds) - headerHeight);

    if ([self swipingFullScreenContent:gesture.direction]) {
      _pageSideSwipeView =
          [self fullscreenSnapshotSideSwipeView:gesture.direction
                                  snapshotImage:
                                      [self.navigationDelegate
                                          swipeNavigationSnapshotForDirection:
                                              gesture.direction]];
      UIView* fullscreenView =
          [_sideSwipeUIControllerDelegate sideSwipeFullscreenView];
      [_pageSideSwipeView setTargetView:fullscreenView];
      if (_pageSideSwipeView) {
        [fullscreenView.superview insertSubview:_pageSideSwipeView
                                   belowSubview:fullscreenView];
      }
    } else {
      BOOL canNavigate = [self.navigationDelegate
          canNavigateInDirection:goBack ? NavigationDirectionBack
                                        : NavigationDirectionForward];
      _pageSideSwipeView = [self webContentSideSwipeView:navigationFrame
                                               direction:gesture.direction
                                             canNavigate:canNavigate];
      [_pageSideSwipeView
          setTargetView:[_sideSwipeUIControllerDelegate sideSwipeContentView]];
      [gesture.view
          insertSubview:_pageSideSwipeView
           belowSubview:[_sideSwipeUIControllerDelegate topToolbarView]];
    }
  } else if (gesture.state == UIGestureRecognizerStateCancelled ||
             gesture.state == UIGestureRecognizerStateEnded ||
             gesture.state == UIGestureRecognizerStateFailed) {
    // Enable fullscreen functionality after the Toolbar has been shown, and
    // the gesture is over.
    _animatedFullscreenDisabler = nullptr;
  }

  __weak SideSwipeUIController* weakSelf = self;
  [_pageSideSwipeView handleHorizontalPan:gesture
      onOverThresholdCompletion:^{
        [weakSelf handleOverThresholdCompletion:gesture.direction];
      }
      onUnderThresholdCompletion:^{
        [weakSelf handleUnderThresholdCompletion];
      }];
}

// Determines whether edge navigation is enabled for the specified swipe
// direction.
- (BOOL)edgeNavigationIsEnabledForDirection:
    (UISwipeGestureRecognizerDirection)direction {
  if (IsSwipingBack(direction) && !_leadingEdgeNavigationEnabled) {
    return NO;
  }

  if (IsSwipingForward(direction) && !_trailingEdgeNavigationEnabled) {
    return NO;
  }

  return YES;
}

// Creates and returns a view, showing a navigation arrow covering the web
// content area.
- (SideSwipeNavigationView*)
    webContentSideSwipeView:(CGRect)frame
                  direction:(UISwipeGestureRecognizerDirection)direction
                canNavigate:(BOOL)canNavigate {
  return [[SideSwipeNavigationView alloc]
      initWithFrame:frame
      withDirection:direction
        canNavigate:canNavigate
              image:[UIImage imageNamed:@"side_swipe_navigation_back"]];
}

// Returns YES, if the the whole page should be swiped.
- (BOOL)swipingFullScreenContent:(UISwipeGestureRecognizerDirection)direction {
  return [self.navigationDelegate isSwipingToAnOverlay:direction];
}

// Creates and returns a view, showing a `snapshotImage` on fullscreen.
- (SideSwipeSnapshotNavigationView*)
    fullscreenSnapshotSideSwipeView:(UISwipeGestureRecognizerDirection)direction
                      snapshotImage:(UIImage*)snapshotImage {
  if (!snapshotImage) {
    return nil;
  }

  return [[SideSwipeSnapshotNavigationView alloc]
      initWithFrame:[[_sideSwipeUIControllerDelegate sideSwipeFullscreenView]
                        frame]
           snapshot:snapshotImage];
}

@end
