// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"

#import <memory>

#import "base/feature_list.h"
#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/timer/timer.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_constants.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_consumer.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator+Testing.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_util.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/web_state_observer_bridge.h"

namespace {

// The timeout used for updating snapshots following a side-swipe gesture.
constexpr base::TimeDelta kUpdateSnapshotTimeout = base::Milliseconds(100);

}  // namespace

@interface SideSwipeMediator () <CRWWebStateObserver,
                                 UIGestureRecognizerDelegate,
                                 WebStateListObserving> {
  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe the WebStateList from Objective-C.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // Scoped observer used to track registration of the WebStateObserverBridge.
  std::unique_ptr<base::ScopedObservation<web::WebState, web::WebStateObserver>>
      _scopedWebStateObservation;

  // The webStateList owned by the current browser.
  raw_ptr<WebStateList> _webStateList;

  // Timer to ensure that snapshot updates that take longer than a maximum
  // delay will release their callback even if the update is incomplete.
  base::OneShotTimer _snapshotTimer;
}

// The current active WebState.
@property(nonatomic, readonly) web::WebState* activeWebState;

@end

@implementation SideSwipeMediator

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
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
    if (self.activeWebState) {
      _scopedWebStateObservation->Observe(self.activeWebState);
    }
  }
  return self;
}

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateList = nullptr;
  }
  _scopedWebStateObservation.reset();
  _webStateObserverBridge.reset();
}

- (web::WebState*)activeWebState {
  return _webStateList ? _webStateList->GetActiveWebState() : nullptr;
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

#pragma mark - SideSwipeTabDelegate

- (void)willTabSwitchWithSwipeToTabIndex:(int)newTabIndex {
  if (!self.activeWebState || newTabIndex == WebStateList::kInvalidIndex) {
    return;
  }
  int currentIndex = _webStateList->GetIndexOfWebState(self.activeWebState);
  if (currentIndex != newTabIndex &&
      currentIndex != WebStateList::kInvalidIndex) {
    _engagementTracker->NotifyEvent(
        feature_engagement::events::kIOSSwipeToolbarToChangeTabUsed);
    [self.helpHandler handleToolbarSwipeGesture];
  }
}

- (void)tabSwitchWithSwipeToTabIndex:(int)newTabIndex {
  if (!self.activeWebState || newTabIndex == WebStateList::kInvalidIndex) {
    return;
  }
  // Disable overlay preview mode for last selected tab.
  PagePlaceholderTabHelper::FromWebState(self.activeWebState)
      ->CancelPlaceholderForNextNavigation();

  web::WebState* webState = _webStateList->GetWebStateAt(newTabIndex);
  // Enable overlay preview mode for selected tab.
  PagePlaceholderTabHelper::FromWebState(webState)
      ->AddPlaceholderForNextNavigation();

  _webStateList->ActivateWebStateAt(newTabIndex);
}

- (void)didCompleteTabSwitchWithSwipe {
  PagePlaceholderTabHelper::FromWebState(self.activeWebState)
      ->CancelPlaceholderForNextNavigation();
}

- (int)activeTabIndex {
  return _webStateList->active_index();
}

- (void)updateActiveTabSnapshot:(ProceduralBlock)callback {
  if (!self.activeWebState) {
    [self runSnapshotCallback:callback];
    return;
  }
  SnapshotTabHelper* snapshotTabHelper =
      SnapshotTabHelper::FromWebState(self.activeWebState);
  if (!snapshotTabHelper) {
    [self runSnapshotCallback:callback];
    return;
  }
  __weak __typeof(self) weakSelf = self;
  _snapshotTimer.Start(FROM_HERE, kUpdateSnapshotTimeout, base::BindOnce(^{
                         [weakSelf runSnapshotCallback:callback];
                       }));
  snapshotTabHelper->UpdateSnapshotWithCallback(^(UIImage* image) {
    [weakSelf onSnapshotUpdated:callback];
  });
}

- (void)runSnapshotCallback:(ProceduralBlock)callback {
  if (callback) {
    callback();
  }
}

- (void)onSnapshotUpdated:(ProceduralBlock)callback {
  if (!_snapshotTimer.IsRunning()) {
    // If the timer is not running, then the callback was already called.
    return;
  }
  // Otherwise, timer should be stopped and callback should be called.
  _snapshotTimer.Stop();
  [self runSnapshotCallback:callback];
}

- (int)tabCount {
  return _webStateList->count();
}

#pragma mark - CRWWebStateObserver Methods

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [self.consumer webPageLoaded];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self updateNavigationEdgeSwipeForWebState:webState];
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
