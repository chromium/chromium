// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller+private.h"

#import <memory>

#import "base/feature_list.h"
#import "base/ios/block_types.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/side_swipe/card_side_swipe_view.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_gesture_recognizer.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_navigation_view.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_util.h"
#import "ios/chrome/browser/ui/tabs/requirements/tab_strip_highlighting.h"
#import "ios/chrome/browser/ui/toolbar/public/side_swipe_toolbar_interacting.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/web_navigation_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kSideSwipeWillStartNotification =
    @"kSideSwipeWillStartNotification";
NSString* const kSideSwipeDidStopNotification =
    @"kSideSwipeDidStopNotification";

class SideSwipeControllerBrowserRemover;

namespace {

enum class SwipeType { NONE, CHANGE_TAB, CHANGE_PAGE };

// Swipe starting distance from edge.
const CGFloat kSwipeEdge = 20;

// Distance between sections of iPad side swipe.
const CGFloat kIpadTabSwipeDistance = 100;

// Number of tabs to keep in the grey image cache.
const NSUInteger kIpadGreySwipeTabCount = 8;
}

@interface SideSwipeController () <CRWWebStateObserver,
                                   UIGestureRecognizerDelegate,
                                   WebStateListObserving> {
 @private

  // Zeroes out `_browser` when it is destroyed.
  std::unique_ptr<SideSwipeControllerBrowserRemover> _browserRemover;

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

// Browser passed on the initializer.
@property(nonatomic, assign) Browser* browser;
// The current active WebState.
@property(nonatomic, readonly) web::WebState* activeWebState;
// The browser state owning the current browser.
@property(nonatomic, readonly) ChromeBrowserState* browserState;
// The webStateList owned by the current browser.
@property(nonatomic, readonly) WebStateList* webStateList;

// Load grey snapshots for the next `kIpadGreySwipeTabCount` tabs in
// `direction`.
- (void)createGreyCache:(UISwipeGestureRecognizerDirection)direction;
// Tell snapshot cache to clear grey cache.
- (void)deleteGreyCache;
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
// Cleans up Browser, WebStateList, and WebState references in the instance of a
// BrowserDestroyed BrowserObserver call.
- (void)browserDestroyed;
@end

// A browser observer that nullifies SideSwipeController's pointer to browser
// when the browser is destroyed.
class SideSwipeControllerBrowserRemover : public BrowserObserver {
 public:
  SideSwipeControllerBrowserRemover(SideSwipeController* controller)
      : side_swipe_controller_(controller) {}

  void BrowserDestroyed(Browser* browser) override {
    [side_swipe_controller_ browserDestroyed];
  }

 private:
  __weak SideSwipeController* side_swipe_controller_;
};

@implementation SideSwipeController

@synthesize inSwipe = _inSwipe;
@synthesize swipeDelegate = _swipeDelegate;
@synthesize toolbarInteractionHandler = _toolbarInteractionHandler;
@synthesize primaryToolbarSnapshotProvider = _primaryToolbarSnapshotProvider;
@synthesize secondaryToolbarSnapshotProvider =
    _secondaryToolbarSnapshotProvider;
@synthesize snapshotDelegate = _snapshotDelegate;
@synthesize tabStripDelegate = _tabStripDelegate;

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  self = [super init];
  if (self) {
    _browser = browser;
    _browserRemover = std::make_unique<SideSwipeControllerBrowserRemover>(self);
    _browser->AddObserver(_browserRemover.get());

    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _browser->GetWebStateList()->AddObserver(_webStateListObserver.get());
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _scopedWebStateObservation = std::make_unique<
        base::ScopedObservation<web::WebState, web::WebStateObserver>>(
        _webStateObserverBridge.get());
    _fullscreenController = FullscreenController::FromBrowser(self.browser);
    if (self.activeWebState)
      _scopedWebStateObservation->Observe(self.activeWebState);
  }
  return self;
}

- (void)dealloc {
  if (self.webStateList) {
    self.webStateList->RemoveObserver(_webStateListObserver.get());
  }

  if (self.browser) {
    self.browser->RemoveObserver(_browserRemover.get());
    self.browser = nullptr;
  }

  _scopedWebStateObservation.reset();
  _webStateObserverBridge.reset();
}

- (void)browserDestroyed {
  self.webStateList->RemoveObserver(_webStateListObserver.get());
  _scopedWebStateObservation.reset();
  _webStateObserverBridge.reset();
  self.browser->RemoveObserver(_browserRemover.get());
  self.browser = nullptr;
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
  [_panGestureRecognizer setSwipeThreshold:48];
  [_panGestureRecognizer setDelegate:self];
  [view addGestureRecognizer:_panGestureRecognizer];
}

- (web::WebState*)activeWebState {
  return self.webStateList ? self.webStateList->GetActiveWebState() : nullptr;
}

- (ChromeBrowserState*)browserState {
  if (!_browser) {
    return nullptr;
  }
  return _browser->GetBrowserState();
}

- (WebStateList*)webStateList {
  if (!_browser) {
    return nullptr;
  }
  return _browser->GetWebStateList();
}

- (void)setEnabled:(BOOL)enabled {
  [_swipeGestureRecognizer setEnabled:enabled];
}

- (BOOL)shouldAutorotate {
  return !([_tabSideSwipeView window] || _inSwipe);
}

- (void)createGreyCache:(UISwipeGestureRecognizerDirection)direction {
  NSInteger dx = (direction == UISwipeGestureRecognizerDirectionLeft) ? -1 : 1;
  NSInteger index = _startingTabIndex + dx;
  NSMutableArray* sessionIDs =
      [NSMutableArray arrayWithCapacity:kIpadGreySwipeTabCount];
  for (NSUInteger count = 0; count < kIpadGreySwipeTabCount; count++) {
    // Wrap around edges.
    if (index >= self.webStateList->count())
      index = 0;
    else if (index < 0)
      index = self.webStateList->count() - 1;

    // Don't wrap past the starting index.
    if (index == (NSInteger)_startingTabIndex)
      break;

    web::WebState* webState = self.webStateList->GetWebStateAt(index);
    if (webState && PagePlaceholderTabHelper::FromWebState(webState)
                        ->will_add_placeholder_for_next_navigation()) {
      [sessionIDs addObject:webState->GetStableIdentifier()];
    }
    index = index + dx;
  }
  [SnapshotBrowserAgent::FromBrowser(self.browser)->snapshot_cache()
      createGreyCache:sessionIDs];
}

- (void)deleteGreyCache {
  [SnapshotBrowserAgent::FromBrowser(self.browser)->snapshot_cache()
      removeGreyCache];
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
  NOTREACHED();
}

- (void)handleiPadTabSwipe:(SideSwipeGestureRecognizer*)gesture {
  // Don't handle swipe when tabs are sorted by recency.
  if (IsTabGridSortedByRecency()) {
    return;
  }

  // Don't handle swipe when there are no tabs.
  int count = self.webStateList->count();
  if (count == 0)
    return;

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
    [self createGreyCache:gesture.direction];
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
      if (IsSwipingForward(gesture.direction))
        indexDelta = 0 - indexDelta;

      web::WebState* currentWebState = self.activeWebState;
      int currentIndex = self.webStateList->GetIndexOfWebState(currentWebState);
      DCHECK_GE(currentIndex, 0);
      // Wrap around edges.
      int newIndex = (int)(_startingTabIndex + indexDelta) % count;

      // C99 defines the modulo result as negative if our offset is negative.
      if (newIndex < 0)
        newIndex += count;

      if (newIndex != currentIndex) {
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
    [self deleteGreyCache];
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kSideSwipeDidStopNotification
                      object:nil];

    // Stop disabling fullscreen.
    _fullscreenDisabler = nullptr;
  }
}

- (BOOL)canNavigate:(BOOL)goBack {
  if (!self.activeWebState)
    return NO;
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

  __weak SideSwipeController* weakSelf = self;
  [_pageSideSwipeView handleHorizontalPan:gesture
      onOverThresholdCompletion:^{
        [weakSelf handleOverThresholdCompletion:gesture];
      }
      onUnderThresholdCompletion:^{
        [weakSelf handleUnderThresholdCompletion];
      }];
}

- (void)handleOverThresholdCompletion:(SideSwipeGestureRecognizer*)gesture {
  web::WebState* webState = self.activeWebState;
  BOOL wantsBack = IsSwipingBack(gesture.direction);
  if (webState) {
    if (wantsBack) {
      web_navigation_util::GoBack(webState);
    } else {
      web_navigation_util::GoForward(webState);
    }
  }
  __weak SideSwipeController* weakSelf = self;
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
  // Don't handle swipe when tabs are sorted by recency.
  if (IsTabGridSortedByRecency()) {
    return;
  }

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
      _tabSideSwipeView.topToolbarSnapshotProvider =
          self.primaryToolbarSnapshotProvider;
      _tabSideSwipeView.bottomToolbarSnapshotProvider =
          self.secondaryToolbarSnapshotProvider;

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

  [_tabSideSwipeView handleHorizontalPan:gesture];
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
  if (!_inSwipe)
    return;
  __weak SideSwipeController* weakSelf = self;
  [self dismissCurtainWithCompletionHandler:^{
    [weakSelf handleCurtainCompletion];
  }];
}

- (void)updateNavigationEdgeSwipeForWebState:(web::WebState*)webState {
  if (!webState)
    return;

  // With slim nav enabled, disable SideSwipeController's edge swipe for a
  // typical navigation.  Continue to use SideSwipeController when on, before,
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
  if (backItems.size() > 0 && UseNativeSwipe(backItems[0]))
    self.leadingEdgeNavigationEnabled = YES;

  // If the next page is an NTP, enable trailing edge swipe.
  std::vector<web::NavigationItem*> forwardItems =
      webState->GetNavigationManager()->GetForwardItems();
  if (forwardItems.size() > 0 && UseNativeSwipe(forwardItems[0]))
    self.trailingEdgeNavigationEnabled = YES;
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

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  // If there is any an ongoing swipe for the old webState, cancel it and
  // dismiss the curtain.
  [self dismissCurtain];
  // Toggling the gesture's enabled state off and on will effectively cancel
  // the gesture recognizer.
  [_swipeGestureRecognizer setEnabled:NO];
  [_swipeGestureRecognizer setEnabled:YES];
  // Track the new active WebState for navigation events. Also remove the old if
  // there was one.
  if (oldWebState)
    _scopedWebStateObservation->Reset();
  if (newWebState)
    _scopedWebStateObservation->Observe(newWebState);

  [self updateNavigationEdgeSwipeForWebState:newWebState];
}

@end
