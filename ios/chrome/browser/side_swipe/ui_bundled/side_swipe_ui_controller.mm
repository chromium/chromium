// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_ui_controller.h"

#import "base/ios/block_types.h"
#import "base/notreached.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/card_side_swipe_view.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/card_swipe_view_delegate.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_gesture_recognizer.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mutator.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_navigation_delegate.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_navigation_view.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_snapshot_navigation_view.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_tab_delegate.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_ui_controller_delegate.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/side_swipe_toolbar_interacting.h"
#import "ios/web/public/web_state.h"
#import "ui/base/device_form_factor.h"

namespace {

// Swipe starting distance from edge.
const CGFloat kSwipeEdge = 20;

// The distance between touches for a swipe between tabs to begin.
const CGFloat kPanGestureRecognizerThreshold = 25;

// Distance between sections of iPad side swipe.
const CGFloat kIpadTabSwipeDistance = 100;

}  // namespace

@implementation SideSwipeUIController {
  // Side swipe view for page navigation.
  UIView<HorizontalPanGestureHandler>* _pageSideSwipeView;

  // Side swipe view for tab navigation.
  CardSideSwipeView* _tabSideSwipeView;

  // Curtain over web view while waiting for it to load.
  UIView* _curtain;

  // Swipe gesture recognizer.
  SideSwipeGestureRecognizer* _swipeGestureRecognizer;
  SideSwipeGestureRecognizer* _panGestureRecognizer;

  // If the swipe is for a page change or a tab change.
  SwipeType _swipeType;

  // Whether to allow navigating from the leading edge.
  BOOL _leadingEdgeNavigationEnabled;

  // Whether to allow navigating from the trailing edge.
  BOOL _trailingEdgeNavigationEnabled;

  // Used in iPad side swipe gesture, tracks the starting tab index.
  unsigned int _startingTabIndex;

  // The disabler that prevents the toolbar from being scrolled away when the
  // side swipe gesture is being recognized.
  std::unique_ptr<ScopedFullscreenDisabler> _fullscreenDisabler;

  // The animated disabler displays the toolbar when a side swipe navigation
  // gesture is being recognized.
  std::unique_ptr<AnimatedScopedFullscreenDisabler> _animatedFullscreenDisabler;

  // The webStateList owned by the current browser.
  raw_ptr<WebStateList> _webStateList;

  // Used to fetch snapshot for tabs.
  raw_ptr<SnapshotBrowserAgent> _snapshotBrowserAgent;
}

- (instancetype)
    initWithFullscreenController:(FullscreenController*)fullscreenController
                    webStateList:(WebStateList*)webStateList
            snapshotBrowserAgent:(SnapshotBrowserAgent*)snapshotBrowserAgent {
  self = [super init];
  if (self) {
    _fullscreenController = fullscreenController;
    _snapshotBrowserAgent = snapshotBrowserAgent;
    _webStateList = webStateList;
  }
  return self;
}

- (void)disconnect {
  [_tabSideSwipeView disconnect];
  [self removeHorizontalGestureRecognizers];
  _snapshotBrowserAgent = nullptr;
  _fullscreenController = nullptr;
  _webStateList = nullptr;
}

- (void)addHorizontalGesturesToView:(UIView*)view {
  _swipeGestureRecognizer = [[SideSwipeGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleSwipe:)];
  [_swipeGestureRecognizer setMaximumNumberOfTouches:1];
  [_swipeGestureRecognizer setDelegate:self];
  [_swipeGestureRecognizer setSwipeEdge:kSwipeEdge];
  [view addGestureRecognizer:_swipeGestureRecognizer];

  // Add a second gesture recognizer to handle swiping on the toolbar to change
  // tabs.
  _panGestureRecognizer =
      [[SideSwipeGestureRecognizer alloc] initWithTarget:self
                                                  action:@selector(handlePan:)];
  [_panGestureRecognizer setMaximumNumberOfTouches:1];
  [_panGestureRecognizer setSwipeThreshold:kPanGestureRecognizerThreshold];
  [_panGestureRecognizer setDelegate:self];
  [view addGestureRecognizer:_panGestureRecognizer];
}

- (void)removeHorizontalGestureRecognizers {
  if (_swipeGestureRecognizer) {
    [_swipeGestureRecognizer.view
        removeGestureRecognizer:_swipeGestureRecognizer];
    _swipeGestureRecognizer = nil;
  }
  if (_panGestureRecognizer) {
    [_panGestureRecognizer.view removeGestureRecognizer:_panGestureRecognizer];
    _panGestureRecognizer = nil;
  }
}

- (void)animateSwipe:(SwipeType)swipeType
         inDirection:(UISwipeGestureRecognizerDirection)direction {
  if (_inSwipe || [_sideSwipeUIControllerDelegate preventSideSwipe]) {
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
  [_panGestureRecognizer setEnabled:enabled];
  [_swipeGestureRecognizer setEnabled:enabled];
}

- (BOOL)isSideSwipeInProgress {
  return ([_tabSideSwipeView window] || _inSwipe);
}

- (void)stopSideSwipeAnimation {
  if (!_inSwipe) {
    return;
  }
  CGRect frame = [_sideSwipeUIControllerDelegate sideSwipeContentView].frame;
  frame.origin.x = 0;
  [_sideSwipeUIControllerDelegate sideSwipeContentView].frame = frame;
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
    if (![gesture isEqual:_panGestureRecognizer]) {
      return NO;
    }

    return [_sideSwipeUIControllerDelegate canBeginToolbarSwipe];
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

  if (!_pageSideSwipeView) {
    [self completeSideSwipeAnimationWithNavigation:canNavigate
                                         direction:direction];
    return;
  }

  __weak SideSwipeUIController* weakSelf = self;
  [_pageSideSwipeView
      animateHorizontalPanWithDirection:direction
                      completionHandler:^{
                        [weakSelf
                            completeSideSwipeAnimationWithNavigation:canNavigate
                                                           direction:direction];
                      }];
}

// Handles the completion of a side swipe gesture that did not meet the
// navigation threshold.
- (void)handleUnderThresholdCompletion {
  [_sideSwipeUIControllerDelegate
      updateAccessoryViewsForSideSwipeWithVisibility:YES];
  _inSwipe = NO;
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

// Handles the completion of a side swipe animation.
- (void)completeSideSwipeAnimationWithNavigation:(BOOL)canNavigate
                                       direction:
                                           (UISwipeGestureRecognizerDirection)
                                               direction {
  if (canNavigate) {
    [self handleOverThresholdCompletion:direction];
  } else {
    [self handleUnderThresholdCompletion];
  }
}

- (void)handleCurtainCompletion {
  _inSwipe = NO;
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
    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
      return [self handleiPhoneTabSwipe:gesture];
    } else {
      return [self handleiPadTabSwipe:gesture];
    }
  }
  if (_swipeType == SwipeType::CHANGE_PAGE) {
    return [self handleSwipeToNavigate:gesture];
  }
  NOTREACHED();
}

// Handles tab swipe completion following an update to the iPhone snapshot.
- (void)handleiPhoneSnapshotOnTabSwipe:(SideSwipeGestureRecognizer*)gesture {
  // Layout tabs with new snapshots in the current orientation.
  [_tabSideSwipeView updateViewsForDirection:gesture.direction];

  // Insert above the toolbar.
  [gesture.view addSubview:_tabSideSwipeView];

  __weak SideSwipeUIController* weakSelf = self;
  [_tabSideSwipeView handleHorizontalPan:gesture
                   actionBeforeTabSwitch:^(int destinationTabIndex) {
                     [weakSelf.tabsDelegate
                         willTabSwitchWithSwipeToTabIndex:destinationTabIndex];
                   }];
}

// Handles tab swipe completion following an update to the iPad snapshot.
- (void)handleiPadSnapshotOnTabSwipe {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kSideSwipeWillStartNotification
                    object:nil];
  _startingTabIndex = [self.tabsDelegate activeTabIndex];
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

- (void)handlePan:(SideSwipeGestureRecognizer*)gesture {
  // Do not trigger a CheckForOverRealization here, as it's expected
  // that many WebStates may realize from multiple swipes.
  web::IgnoreOverRealizationCheck();
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    return [self handleiPhoneTabSwipe:gesture];
  } else {
    return [self handleiPadTabSwipe:gesture];
  }
}

// Show horizontal swipe stack view for iPhone.
- (void)handleiPhoneTabSwipe:(SideSwipeGestureRecognizer*)gesture {
  if (gesture.state == UIGestureRecognizerStateBegan) {
    _inSwipe = YES;

    CGRect frame =
        [[_sideSwipeUIControllerDelegate sideSwipeContentView] frame];

    // Add horizontal stack view controller.
    CGFloat headerHeight =
        self.fullscreenController->GetMaxViewportInsets().top;

    if (_tabSideSwipeView) {
      [_tabSideSwipeView setFrame:frame];
      [_tabSideSwipeView setTopMargin:headerHeight];
    } else {
      _tabSideSwipeView =
          [[CardSideSwipeView alloc] initWithFrame:frame
                                         topMargin:headerHeight
                                      webStateList:_webStateList
                              snapshotBrowserAgent:_snapshotBrowserAgent];
      _tabSideSwipeView.toolbarSnapshotProvider = self.toolbarSnapshotProvider;

      [_tabSideSwipeView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                             UIViewAutoresizingFlexibleHeight];
      [_tabSideSwipeView setDelegate:_cardSwipeViewDelegate];
      [_tabSideSwipeView setBackgroundColor:[UIColor blackColor]];
    }

    // Ensure that there's an up-to-date snapshot of the current tab.
    __weak SideSwipeUIController* weakSelf = self;
    [self.tabsDelegate updateActiveTabSnapshot:^() {
      [weakSelf handleiPhoneSnapshotOnTabSwipe:gesture];
    }];
    return;
  }

  CHECK_NE(gesture.state, UIGestureRecognizerStateBegan)
      << "UI gesture must go through snapshot completion callback to complete "
         "processing.";
  __weak SideSwipeUIController* weakSelf = self;
  [_tabSideSwipeView handleHorizontalPan:gesture
                   actionBeforeTabSwitch:^(int destinationTabIndex) {
                     [weakSelf.tabsDelegate
                         willTabSwitchWithSwipeToTabIndex:destinationTabIndex];
                   }];
}

// Handles iPad tab swipe gestures.
- (void)handleiPadTabSwipe:(SideSwipeGestureRecognizer*)gesture {
  // Don't handle swipe when there are no tabs.
  int count = [self.tabsDelegate tabCount];
  if (count == 0) {
    return;
  }

  if (gesture.state == UIGestureRecognizerStateBegan) {
    // Disable fullscreen while the side swipe gesture is occurring.
    _fullscreenDisabler =
        std::make_unique<ScopedFullscreenDisabler>(self.fullscreenController);
    __weak SideSwipeUIController* weakSelf = self;
    [self.tabsDelegate updateActiveTabSnapshot:^() {
      [weakSelf handleiPadSnapshotOnTabSwipe];
    }];
    return;
  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    // Side swipe for iPad involves changing the selected tab as the swipe moves
    // across the width of the view.  The screen is broken up into
    // `kIpadTabSwipeDistance` / `width` segments, with the current tab in the
    // first section.  The swipe does not wrap edges.
    CGFloat distance = [gesture locationInView:gesture.view].x;
    if (gesture.direction == UISwipeGestureRecognizerDirectionLeft) {
      distance = gesture.startPoint.x - distance;
    } else {
      distance -= gesture.startPoint.x;
    }

    int indexDelta = std::floor(distance / kIpadTabSwipeDistance);
    // Don't wrap past the first tab.
    if (indexDelta < count) {
      // Flip delta when swiping forward.
      if (IsSwipingForward(gesture.direction)) {
        indexDelta = 0 - indexDelta;
      }

      int currentIndex = [self.tabsDelegate activeTabIndex];
      DCHECK_GE(currentIndex, 0);
      // Wrap around edges.
      int newIndex = (int)(_startingTabIndex + indexDelta) % count;

      // C99 defines the modulo result as negative if our offset is negative.
      if (newIndex < 0) {
        newIndex += count;
      }

      if (newIndex != currentIndex) {
        [self.tabsDelegate willTabSwitchWithSwipeToTabIndex:newIndex];
        [self.tabsDelegate tabSwitchWithSwipeToTabIndex:newIndex];
      }
    }
  } else {
    [self.tabsDelegate didCompleteTabSwitchWithSwipe];

    // Redisplay the view if it was in overlay preview mode.
    [_sideSwipeUIControllerDelegate sideSwipeRedisplayTabView];
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kSideSwipeDidStopNotification
                      object:nil];

    // Stop disabling fullscreen.
    _fullscreenDisabler = nullptr;
  }
  CHECK_NE(gesture.state, UIGestureRecognizerStateBegan)
      << "UI gesture must go through snapshot completion callback to complete "
         "processing.";
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
  /// Check if the swipe is intended to reveal an overlay and if a snapshot for
  /// that overlay exists.
  return
      [self.navigationDelegate isSwipingToAnOverlay:direction] &&
      [self.navigationDelegate swipeNavigationSnapshotForDirection:direction];
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
