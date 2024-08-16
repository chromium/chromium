// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"

#import <memory>

#import "base/feature_list.h"
#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/card_side_swipe_view.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_gesture_recognizer.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator+Testing.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_navigation_view.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_util.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/ui_bundled/requirements/tab_strip_highlighting.h"
#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/toolbar/public/side_swipe_toolbar_interacting.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/device_form_factor.h"

NSString* const kSideSwipeWillStartNotification =
    @"kSideSwipeWillStartNotification";
NSString* const kSideSwipeDidStopNotification =
    @"kSideSwipeDidStopNotification";

namespace {

// Swipe starting distance from edge.
const CGFloat kSwipeEdge = 20;

// The distance between touches for a swipe between tabs to begin.
const CGFloat kPanGestureRecognizerThreshold = 25;

// Distance between sections of iPad side swipe.
const CGFloat kIpadTabSwipeDistance = 100;

}  // namespace

@interface SideSwipeMediator () <CRWWebStateObserver,
                                 UIGestureRecognizerDelegate,
                                 WebStateListObserving> {
 @private

  // Side swipe view for tab navigation.
  CardSideSwipeView* _tabSideSwipeView;

  // Side swipe view for page navigation.
  SideSwipeNavigationView* _pageSideSwipeView;

  // YES if the user is currently swiping.
  BOOL _inSwipe;

  // Swipe gesture recognizer.
  SideSwipeGestureRecognizer* _swipeGestureRecognizer;

  SideSwipeGestureRecognizer* _panGestureRecognizer;

  // Used in iPad side swipe gesture, tracks the starting tab index.
  unsigned int _startingTabIndex;

  // If the swipe is for a page change or a tab change.
  SwipeType _swipeType;

  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe the WebStateList from Objective-C.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // Scoped observer used to track registration of the WebStateObserverBridge.
  std::unique_ptr<base::ScopedObservation<web::WebState, web::WebStateObserver>>
      _scopedWebStateObservation;

  // Curtain over web view while waiting for it to load.
  UIView* _curtain;

  // The disabler that prevents the toolbar from being scrolled away when the
  // side swipe gesture is being recognized.
  std::unique_ptr<ScopedFullscreenDisabler> _fullscreenDisabler;

  // The animated disabler displays the toolbar when a side swipe navigation
  // gesture is being recognized.
  std::unique_ptr<AnimatedScopedFullscreenDisabler> _animatedFullscreenDisabler;
}

// The current active WebState.
@property(nonatomic, readonly) web::WebState* activeWebState;

// The webStateList owned by the current browser.
@property(nonatomic, readonly) WebStateList* webStateList;

// Whether to allow navigating from the leading edge.
@property(nonatomic, assign) BOOL leadingEdgeNavigationEnabled;

// Whether to allow navigating from the trailing edge.
@property(nonatomic, assign) BOOL trailingEdgeNavigationEnabled;

// Handle tab side swipe for iPad.  Change tabs according to swipe distance.
- (void)handleiPadTabSwipe:(SideSwipeGestureRecognizer*)gesture;
// Handle tab side swipe for iPhone. Introduces a CardSideSwipeView to convey
// the tab change.
- (void)handleiPhoneTabSwipe:(SideSwipeGestureRecognizer*)gesture;
// Overlays `curtain_` as a white view to hide the web view while it updates.
// Calls `completionHandler` when the curtain is removed.
- (void)addCurtainWithCompletionHandler:(ProceduralBlock)completionHandler;
// Removes the `curtain_` and calls `completionHandler` when the curtain is
// removed.
- (void)dismissCurtainWithCompletionHandler:(ProceduralBlock)completionHandler;
// Removes the `curtain_` if there was an active swipe, and resets
// `inSwipe_` value.
- (void)dismissCurtain;
@end

@implementation SideSwipeMediator

@synthesize inSwipe = _inSwipe;
@synthesize swipeDelegate = _swipeDelegate;
@synthesize toolbarInteractionHandler = _toolbarInteractionHandler;
@synthesize tabStripDelegate = _tabStripDelegate;

- (instancetype)
    initWithFullscreenController:(FullscreenController*)fullscreenController
                    webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _scopedWebStateObservation = std::make_unique<
        base::ScopedObservation<web::WebState, web::WebStateObserver>>(
        _webStateObserverBridge.get());
    _fullscreenController = fullscreenController;
    if (self.activeWebState) {
      _scopedWebStateObservation->Observe(self.activeWebState);
    }
  }
  return self;
}

- (void)dealloc {
  // TODO(crbug.com/40276402);
  DUMP_WILL_BE_CHECK(!_fullscreenController);
}

- (void)disconnect {
  if (self.webStateList) {
    self.webStateList->RemoveObserver(_webStateListObserver.get());
  }
  _scopedWebStateObservation.reset();
  _webStateObserverBridge.reset();
  _fullscreenController = nullptr;
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

- (void)animateSwipe:(SwipeType)swipeType
         inDirection:(UISwipeGestureRecognizerDirection)direction {
  if (_inSwipe || [_swipeDelegate preventSideSwipe]) {
    return;
  }
  switch (swipeType) {
    case SwipeType::NONE:
    case SwipeType::CHANGE_TAB:
      NOTREACHED_IN_MIGRATION();
      break;
    case SwipeType::CHANGE_PAGE:
      [self animatePageNavigationInDirection:direction];
      break;
  }
}

- (web::WebState*)activeWebState {
  return self.webStateList ? self.webStateList->GetActiveWebState() : nullptr;
}

- (void)setEnabled:(BOOL)enabled {
  [_swipeGestureRecognizer setEnabled:enabled];
}

- (BOOL)shouldAutorotate {
  return !([_tabSideSwipeView window] || _inSwipe);
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
  NOTREACHED_IN_MIGRATION();
}

- (void)handleiPadTabSwipe:(SideSwipeGestureRecognizer*)gesture {
  // Don't handle swipe when there are no tabs.
  int count = self.webStateList->count();
  if (count == 0) {
    return;
  }

  if (gesture.state == UIGestureRecognizerStateBegan) {
    // Disable fullscreen while the side swipe gesture is occurring.
    _fullscreenDisabler =
        std::make_unique<ScopedFullscreenDisabler>(self.fullscreenController);
    SnapshotTabHelper::FromWebState(self.activeWebState)
        ->UpdateSnapshotWithCallback(nil);
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kSideSwipeWillStartNotification
                      object:nil];
    [self.tabStripDelegate setHighlightsSelectedTab:YES];
    _startingTabIndex = self.webStateList->active_index();
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

      web::WebState* currentWebState = self.activeWebState;
      int currentIndex = self.webStateList->GetIndexOfWebState(currentWebState);
      DCHECK_GE(currentIndex, 0);
      // Wrap around edges.
      int newIndex = (int)(_startingTabIndex + indexDelta) % count;

      // C99 defines the modulo result as negative if our offset is negative.
      if (newIndex < 0) {
        newIndex += count;
      }

      if (newIndex != currentIndex) {
        [self willActivateWebStateAtIndex:newIndex];
        web::WebState* webState = self.webStateList->GetWebStateAt(newIndex);
        // Toggle overlay preview mode for selected tab.
        PagePlaceholderTabHelper::FromWebState(webState)
            ->AddPlaceholderForNextNavigation();
        self.webStateList->ActivateWebStateAt(newIndex);

        // And disable overlay preview mode for last selected tab.
        PagePlaceholderTabHelper::FromWebState(currentWebState)
            ->CancelPlaceholderForNextNavigation();
      }
    }
  } else {
    if (gesture.state == UIGestureRecognizerStateCancelled) {
      web::WebState* webState =
          self.webStateList->GetWebStateAt(_startingTabIndex);
      PagePlaceholderTabHelper::FromWebState(webState)
          ->CancelPlaceholderForNextNavigation();
      self.webStateList->ActivateWebStateAt(_startingTabIndex);
    }
    PagePlaceholderTabHelper::FromWebState(self.activeWebState)
        ->CancelPlaceholderForNextNavigation();

    // Redisplay the view if it was in overlay preview mode.
    [_swipeDelegate sideSwipeRedisplayTabView];
    [self.tabStripDelegate setHighlightsSelectedTab:NO];
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kSideSwipeDidStopNotification
                      object:nil];

    // Stop disabling fullscreen.
    _fullscreenDisabler = nullptr;
  }
}

// Invoked when the active tab is about to be changed.
- (void)willActivateWebStateAtIndex:(int)index {
  if (!self.activeWebState || index == WebStateList::kInvalidIndex) {
    return;
  }
  int currentIndex = self.webStateList->GetIndexOfWebState(self.activeWebState);
  if (currentIndex != index && currentIndex != WebStateList::kInvalidIndex) {
    _engagementTracker->NotifyEvent(
        feature_engagement::events::kIOSSwipeToolbarToChangeTabUsed);
    [self.helpHandler handleToolbarSwipeGesture];
  }
}

- (BOOL)canNavigate:(BOOL)goBack {
  if (!self.activeWebState) {
    return NO;
  }
  if (goBack && self.activeWebState->GetNavigationManager()->CanGoBack()) {
    return YES;
  }
  if (!goBack && self.activeWebState->GetNavigationManager()->CanGoForward()) {
    return YES;
  }
  return NO;
}

// Show swipe to navigate.
- (void)handleSwipeToNavigate:(SideSwipeGestureRecognizer*)gesture {
  if (gesture.state == UIGestureRecognizerStateBegan) {
    // Make sure the Toolbar is visible by disabling Fullscreen.
    _animatedFullscreenDisabler =
        std::make_unique<AnimatedScopedFullscreenDisabler>(
            self.fullscreenController);
    _animatedFullscreenDisabler->StartAnimation();

    _inSwipe = YES;
    [_swipeDelegate updateAccessoryViewsForSideSwipeWithVisibility:NO];
    BOOL goBack = IsSwipingBack(gesture.direction);

    CGRect gestureBounds = gesture.view.bounds;
    CGFloat headerHeight = [_swipeDelegate headerHeightForSideSwipe];
    CGRect navigationFrame =
        CGRectMake(CGRectGetMinX(gestureBounds),
                   CGRectGetMinY(gestureBounds) + headerHeight,
                   CGRectGetWidth(gestureBounds),
                   CGRectGetHeight(gestureBounds) - headerHeight);

    _pageSideSwipeView = [[SideSwipeNavigationView alloc]
        initWithFrame:navigationFrame
        withDirection:gesture.direction
          canNavigate:[self canNavigate:goBack]
                image:[UIImage imageNamed:@"side_swipe_navigation_back"]];
    [_pageSideSwipeView setTargetView:[_swipeDelegate sideSwipeContentView]];

    [gesture.view insertSubview:_pageSideSwipeView
                   belowSubview:[_swipeDelegate topToolbarView]];
  } else if (gesture.state == UIGestureRecognizerStateCancelled ||
             gesture.state == UIGestureRecognizerStateEnded ||
             gesture.state == UIGestureRecognizerStateFailed) {
    // Enable fullscreen functionality after the Toolbar has been shown, and
    // the gesture is over.
    _animatedFullscreenDisabler = nullptr;
  }

  __weak SideSwipeMediator* weakSelf = self;
  [_pageSideSwipeView handleHorizontalPan:gesture
      onOverThresholdCompletion:^{
        [weakSelf handleOverThresholdCompletion:gesture.direction];
      }
      onUnderThresholdCompletion:^{
        [weakSelf handleUnderThresholdCompletion];
      }];
}

// Animate page navigation.
- (void)animatePageNavigationInDirection:
    (UISwipeGestureRecognizerDirection)direction {
  _inSwipe = YES;
  [_swipeDelegate updateAccessoryViewsForSideSwipeWithVisibility:NO];

  UIView* navigatingView = [_swipeDelegate sideSwipeContentView].superview;
  CGRect navigatingBounds = navigatingView.bounds;
  CGFloat headerHeight = [_swipeDelegate headerHeightForSideSwipe];
  CGRect navigationFrame =
      CGRectMake(CGRectGetMinX(navigatingBounds),
                 CGRectGetMinY(navigatingBounds) + headerHeight,
                 CGRectGetWidth(navigatingBounds),
                 CGRectGetHeight(navigatingBounds) - headerHeight);

  BOOL canNavigate = [self canNavigate:IsSwipingBack(direction)];
  _pageSideSwipeView = [[SideSwipeNavigationView alloc]
      initWithFrame:navigationFrame
      withDirection:direction
        canNavigate:canNavigate
              image:[UIImage imageNamed:@"side_swipe_navigation_back"]];
  [_pageSideSwipeView setTargetView:[_swipeDelegate sideSwipeContentView]];

  [navigatingView insertSubview:_pageSideSwipeView
                   belowSubview:[_swipeDelegate topToolbarView]];

  __weak SideSwipeMediator* weakSelf = self;
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

- (void)handleOverThresholdCompletion:
    (UISwipeGestureRecognizerDirection)direction {
  web::WebState* webState = self.activeWebState;
  BOOL wantsBack = IsSwipingBack(direction);
  if (webState) {
    if (wantsBack) {
      web_navigation_util::GoBack(webState);
    } else {
      web_navigation_util::GoForward(webState);
    }
    CHECK(self.engagementTracker);
    self.engagementTracker->NotifyEvent(
        feature_engagement::events::kIOSSwipeBackForwardUsed);
  }
  __weak SideSwipeMediator* weakSelf = self;
  // Checking -IsLoading() is likely incorrect, but to narrow the scope of
  // fixes for slim navigation manager, only ignore this state when
  // slim is disabled.  With slim navigation enabled, this false when
  // pages can be served from WKWebView's page cache.
  if (webState) {
    [self addCurtainWithCompletionHandler:^{
      [weakSelf handleCurtainCompletion];
    }];
  } else {
    _inSwipe = NO;
  }
  [_swipeDelegate updateAccessoryViewsForSideSwipeWithVisibility:YES];
}

- (void)handleCurtainCompletion {
  _inSwipe = NO;
}

- (void)handleUnderThresholdCompletion {
  [_swipeDelegate updateAccessoryViewsForSideSwipeWithVisibility:YES];
  _inSwipe = NO;
}

// Show horizontal swipe stack view for iPhone.
- (void)handleiPhoneTabSwipe:(SideSwipeGestureRecognizer*)gesture {
  if (gesture.state == UIGestureRecognizerStateBegan) {
    _inSwipe = YES;

    CGRect frame = [[_swipeDelegate sideSwipeContentView] frame];

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
                                      webStateList:self.webStateList];
      _tabSideSwipeView.toolbarSnapshotProvider = self.toolbarSnapshotProvider;

      [_tabSideSwipeView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                             UIViewAutoresizingFlexibleHeight];
      [_tabSideSwipeView setDelegate:_swipeDelegate];
      [_tabSideSwipeView setBackgroundColor:[UIColor blackColor]];
    }

    // Ensure that there's an up-to-date snapshot of the current tab.
    if (self.activeWebState) {
      SnapshotTabHelper::FromWebState(self.activeWebState)
          ->UpdateSnapshotWithCallback(nil);
    }

    // Layout tabs with new snapshots in the current orientation.
    [_tabSideSwipeView updateViewsForDirection:gesture.direction];

    // Insert above the toolbar.
    [gesture.view addSubview:_tabSideSwipeView];
  }

  __weak SideSwipeMediator* weakSelf = self;
  [_tabSideSwipeView
        handleHorizontalPan:gesture
      actionBeforeTabSwitch:^(int destinationWebStateIndex) {
        [weakSelf willActivateWebStateAtIndex:destinationWebStateIndex];
      }];
}

- (void)addCurtainWithCompletionHandler:(ProceduralBlock)completionHandler {
  if (!_curtain) {
    _curtain = [[UIView alloc]
        initWithFrame:[_swipeDelegate sideSwipeContentView].bounds];
    [_curtain setBackgroundColor:[UIColor whiteColor]];
  }
  [[_swipeDelegate sideSwipeContentView] addSubview:_curtain];

  // Fallback in case load takes a while. 3 seconds is a balance between how
  // long it can take a web view to clear the previous page image, and what
  // feels like to 'too long' to see the curtain.
  [self performSelector:@selector(dismissCurtainWithCompletionHandler:)
             withObject:[completionHandler copy]
             afterDelay:3];
}

- (void)resetContentView {
  if (!_inSwipe) {
    return;
  }
  CGRect frame = [_swipeDelegate sideSwipeContentView].frame;
  frame.origin.x = 0;
  [_swipeDelegate sideSwipeContentView].frame = frame;
}

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
  __weak SideSwipeMediator* weakSelf = self;
  [self dismissCurtainWithCompletionHandler:^{
    [weakSelf handleCurtainCompletion];
  }];
}

- (void)updateNavigationEdgeSwipeForWebState:(web::WebState*)webState {
  if (!webState) {
    return;
  }

  // With slim nav enabled, disable SideSwipeMediator's edge swipe for a
  // typical navigation.  Continue to use SideSwipeMediator when on, before,
  // or after a native page.
  self.leadingEdgeNavigationEnabled = NO;
  self.trailingEdgeNavigationEnabled = NO;

  web::NavigationItem* item =
      webState->GetNavigationManager()->GetVisibleItem();
  if (UseNativeSwipe(item)) {
    self.leadingEdgeNavigationEnabled = YES;
    self.trailingEdgeNavigationEnabled = YES;
  }

  // If the previous page is an NTP, enable leading edge swipe.
  std::vector<web::NavigationItem*> backItems =
      webState->GetNavigationManager()->GetBackwardItems();
  if (backItems.size() > 0 && UseNativeSwipe(backItems[0])) {
    self.leadingEdgeNavigationEnabled = YES;
  }

  // If the next page is an NTP, enable trailing edge swipe.
  std::vector<web::NavigationItem*> forwardItems =
      webState->GetNavigationManager()->GetForwardItems();
  if (forwardItems.size() > 0 && UseNativeSwipe(forwardItems[0])) {
    self.trailingEdgeNavigationEnabled = YES;
  }
}

#pragma mark - CRWWebStateObserver Methods

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [self dismissCurtain];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self updateNavigationEdgeSwipeForWebState:webState];
}

#pragma mark - UIGestureRecognizerDelegate Methods

// Gestures should only be recognized within `contentArea_` or the toolbar.
- (BOOL)gestureRecognizerShouldBegin:(SideSwipeGestureRecognizer*)gesture {
  if (_inSwipe) {
    return NO;
  }

  if ([_swipeDelegate preventSideSwipe]) {
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

    return [_swipeDelegate canBeginToolbarSwipe];
  }

  // Otherwise, only allow contentView touches with `swipeGestureRecognizer_`.
  // The content view frame is inset by -1 because CGRectContainsPoint does
  // include points on the max X and Y edges, which will happen frequently with
  // edge swipes from the right side.
  CGRect contentViewFrame =
      CGRectInset([[_swipeDelegate sideSwipeContentView] frame], -1, -1);
  if (CGRectContainsPoint(contentViewFrame, location)) {
    if (![gesture isEqual:_swipeGestureRecognizer]) {
      return NO;
    }

    if (gesture.direction == UISwipeGestureRecognizerDirectionRight &&
        !self.leadingEdgeNavigationEnabled) {
      return NO;
    }

    if (gesture.direction == UISwipeGestureRecognizerDirectionLeft &&
        !self.trailingEdgeNavigationEnabled) {
      return NO;
    }
    _swipeType = SwipeType::CHANGE_PAGE;
    return YES;
  }
  return NO;
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

#pragma mark - WebStateListObserving Methods

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (!status.active_web_state_change()) {
    return;
  }

  // If there is any an ongoing swipe for the old webState, cancel it and
  // dismiss the curtain.
  [self dismissCurtain];
  // Toggling the gesture's enabled state off and on will effectively cancel
  // the gesture recognizer.
  [_swipeGestureRecognizer setEnabled:NO];
  [_swipeGestureRecognizer setEnabled:YES];
  // Track the new active WebState for navigation events. Also remove the old if
  // there was one.
  if (status.old_active_web_state) {
    _scopedWebStateObservation->Reset();
  }
  if (status.new_active_web_state) {
    _scopedWebStateObservation->Observe(status.new_active_web_state);
  }

  [self updateNavigationEdgeSwipeForWebState:status.new_active_web_state];
}

@end
