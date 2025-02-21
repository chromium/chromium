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
#import "ios/chrome/browser/fullscreen/ui_bundled/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/card_side_swipe_view.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_constants.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_consumer.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_gesture_recognizer.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator+Testing.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_navigation_view.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_snapshot_navigation_view.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_util.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/ui_bundled/requirements/tab_strip_highlighting.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/side_swipe_toolbar_interacting.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/device_form_factor.h"

namespace {

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

  // The webStateList owned by the current browser.
  raw_ptr<WebStateList> _webStateList;
}

// The current active WebState.
@property(nonatomic, readonly) web::WebState* activeWebState;

// Handle tab side swipe for iPad.  Change tabs according to swipe distance.
- (void)handleiPadTabSwipe:(SideSwipeGestureRecognizer*)gesture;
// Handle tab side swipe for iPhone. Introduces a CardSideSwipeView to convey
// the tab change.
- (void)handleiPhoneTabSwipe:(SideSwipeGestureRecognizer*)gesture;
@end

@implementation SideSwipeMediator

@synthesize inSwipe = _inSwipe;
@synthesize swipeDelegate = _swipeDelegate;
@synthesize toolbarInteractionHandler = _toolbarInteractionHandler;
@synthesize tabStripDelegate = _tabStripDelegate;

- (instancetype)initWithFullscreenController:
                    (FullscreenController*)fullscreenController
                                webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    CHECK(webStateList);
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
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateList = nullptr;
  }
  _scopedWebStateObservation.reset();
  _webStateObserverBridge.reset();
  _fullscreenController = nullptr;
  [_tabSideSwipeView disconnect];
}

- (void)addHorizontalGesturesToView:(UIView*)view {
  // Add a gesture recognizer to handle swiping on the toolbar to change
  // tabs.
  _panGestureRecognizer =
      [[SideSwipeGestureRecognizer alloc] initWithTarget:self
                                                  action:@selector(handlePan:)];
  [_panGestureRecognizer setMaximumNumberOfTouches:1];
  [_panGestureRecognizer setSwipeThreshold:kPanGestureRecognizerThreshold];
  [_panGestureRecognizer setDelegate:self];
  [view addGestureRecognizer:_panGestureRecognizer];
}

- (web::WebState*)activeWebState {
  return _webStateList ? _webStateList->GetActiveWebState() : nullptr;
}

- (void)setEnabled:(BOOL)enabled {
  [_swipeGestureRecognizer setEnabled:enabled];
}

- (BOOL)isSideSwipeInProgress {
  return ([_tabSideSwipeView window] || _inSwipe);
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

- (void)handleiPadTabSwipe:(SideSwipeGestureRecognizer*)gesture {
  // Don't handle swipe when there are no tabs.
  int count = _webStateList->count();
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
    _startingTabIndex = _webStateList->active_index();
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
      int currentIndex = _webStateList->GetIndexOfWebState(currentWebState);
      DCHECK_GE(currentIndex, 0);
      // Wrap around edges.
      int newIndex = (int)(_startingTabIndex + indexDelta) % count;

      // C99 defines the modulo result as negative if our offset is negative.
      if (newIndex < 0) {
        newIndex += count;
      }

      if (newIndex != currentIndex) {
        [self willActivateWebStateAtIndex:newIndex];
        web::WebState* webState = _webStateList->GetWebStateAt(newIndex);
        // Toggle overlay preview mode for selected tab.
        PagePlaceholderTabHelper::FromWebState(webState)
            ->AddPlaceholderForNextNavigation();
        _webStateList->ActivateWebStateAt(newIndex);

        // And disable overlay preview mode for last selected tab.
        PagePlaceholderTabHelper::FromWebState(currentWebState)
            ->CancelPlaceholderForNextNavigation();
      }
    }
  } else {
    if (gesture.state == UIGestureRecognizerStateCancelled) {
      web::WebState* webState = _webStateList->GetWebStateAt(_startingTabIndex);
      PagePlaceholderTabHelper::FromWebState(webState)
          ->CancelPlaceholderForNextNavigation();
      _webStateList->ActivateWebStateAt(_startingTabIndex);
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
  int currentIndex = _webStateList->GetIndexOfWebState(self.activeWebState);
  if (currentIndex != index && currentIndex != WebStateList::kInvalidIndex) {
    _engagementTracker->NotifyEvent(
        feature_engagement::events::kIOSSwipeToolbarToChangeTabUsed);
    [self.helpHandler handleToolbarSwipeGesture];
  }
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
                                      webStateList:_webStateList];
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

- (void)resetContentView {
  if (!_inSwipe) {
    return;
  }
  CGRect frame = [_swipeDelegate sideSwipeContentView].frame;
  frame.origin.x = 0;
  [_swipeDelegate sideSwipeContentView].frame = frame;
}

- (void)updateNavigationEdgeSwipeForWebState:(web::WebState*)webState {
  if (!webState) {
    return;
  }

  [self.consumer setLeadingEdgeNavigationEnabled:NO];
  [self.consumer setTrailingEdgeNavigationEnabled:NO];

  web::NavigationItem* item =
      webState->GetNavigationManager()->GetVisibleItem();
  if (UseNativeSwipe(item)) {
    [self.consumer setLeadingEdgeNavigationEnabled:YES];
    [self.consumer setTrailingEdgeNavigationEnabled:YES];
  }

  // If the previous page has lens overlay invoked, enable leading edge swipe.
  if (SwipingBackLeadsToLensOverlay(self.activeWebState)) {
    [self.consumer setLeadingEdgeNavigationEnabled:YES];
  }

  // If the previous page is an NTP, enable leading edge swipe.
  std::vector<web::NavigationItem*> backItems =
      webState->GetNavigationManager()->GetBackwardItems();

  if (backItems.size() > 0 && UseNativeSwipe(backItems[0])) {
    [self.consumer setLeadingEdgeNavigationEnabled:YES];
  }

  // If the next page is an NTP, enable trailing edge swipe.
  std::vector<web::NavigationItem*> forwardItems =
      webState->GetNavigationManager()->GetForwardItems();
  if (forwardItems.size() > 0 && UseNativeSwipe(forwardItems[0])) {
    [self.consumer setTrailingEdgeNavigationEnabled:YES];
  }
}

#pragma mark - SideSwipeNavigationDelegate

- (BOOL)canNavigateInDirection:(NavigationDirection)direction {
  if (!self.activeWebState) {
    return NO;
  }

  BOOL goingBack = direction == NavigationDirectionBack;

  if (goingBack && self.activeWebState->GetNavigationManager()->CanGoBack()) {
    return YES;
  }

  if (!goingBack &&
      self.activeWebState->GetNavigationManager()->CanGoForward()) {
    return YES;
  }
  return NO;
}

- (UIImage*)swipeNavigationSnapshotForDirection:
    (UISwipeGestureRecognizerDirection)direction {
  return SwipeNavigationSnapshot(direction, self.activeWebState);
}

- (BOOL)isSwipingToAnOverlay:(UISwipeGestureRecognizerDirection)direction {
  return IsSwipingToAnOverlay(direction, self.activeWebState);
}

#pragma mark - SideSwipeMutator

- (void)navigateInDirection:(NavigationDirection)direction {
  if (!self.activeWebState) {
    return;
  }

  if (direction == NavigationDirectionBack) {
    web_navigation_util::GoBack(self.activeWebState);
  } else {
    web_navigation_util::GoForward(self.activeWebState);
  }

  CHECK(self.engagementTracker);
  self.engagementTracker->NotifyEvent(
      feature_engagement::events::kIOSSwipeBackForwardUsed);
}

#pragma mark - CRWWebStateObserver Methods

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [self.consumer webPageLoaded];
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

  // Cancel any an ongoing swipe for the old webState.
  [self.consumer cancelOnGoingSwipe];

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
