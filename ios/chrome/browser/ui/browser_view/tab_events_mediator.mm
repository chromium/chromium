// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/tab_events_mediator.h"

#import "ios/chrome/browser/feature_engagement/model/tracker_util.h"
#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/browser_view/tab_consumer.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"
#import "ios/chrome/browser/ui/tabs/switch_to_tab_animation_view.h"
#import "ios/chrome/browser/ui/toolbar/public/side_swipe_toolbar_snapshot_providing.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_type.h"
#import "ios/chrome/browser/url_loading/model/new_tab_animation_tab_helper.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_observer_bridge.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface TabEventsMediator () <CRWWebStateObserver,
                                 WebStateListObserving,
                                 URLLoadingObserver>

@end

@implementation TabEventsMediator {
  // Bridges C++ WebStateObserver methods to this mediator.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridges C++ WebStateListObserver methods to TabEventsMediator.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;

  // Forwards observer methods for all WebStates in the WebStateList to
  // this mediator.
  std::unique_ptr<AllWebStateObservationForwarder>
      _allWebStateObservationForwarder;

  // Bridges C++ UrlLoadingObserver methods to TabEventsMediator.
  std::unique_ptr<UrlLoadingObserverBridge> _loadingObserverBridge;

  WebStateList* _webStateList;
  __weak NewTabPageCoordinator* _ntpCoordinator;
  UrlLoadingNotifierBrowserAgent* _loadingNotifier;
  ChromeBrowserState* _browserState;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                      ntpCoordinator:(NewTabPageCoordinator*)ntpCoordinator
                        browserState:(ChromeBrowserState*)browserState
                     loadingNotifier:
                         (UrlLoadingNotifierBrowserAgent*)urlLoadingNotifier {
  if (self = [super init]) {
    _webStateList = webStateList;
    // TODO(crbug.com/1348459): Stop lazy loading in NTPCoordinator and remove
    // this dependency.
    _ntpCoordinator = ntpCoordinator;
    _browserState = browserState;

    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    webStateList->AddObserver(_webStateListObserverBridge.get());
    _allWebStateObservationForwarder =
        std::make_unique<AllWebStateObservationForwarder>(
            _webStateList, _webStateObserverBridge.get());
    _loadingObserverBridge = std::make_unique<UrlLoadingObserverBridge>(self);
    _loadingNotifier = urlLoadingNotifier;
    _loadingNotifier->AddObserver(_loadingObserverBridge.get());
  }
  return self;
}

- (void)disconnect {
  _allWebStateObservationForwarder = nullptr;
  _webStateObserverBridge = nullptr;

  _loadingNotifier->RemoveObserver(_loadingObserverBridge.get());
  _loadingObserverBridge.reset();

  _webStateList->RemoveObserver(_webStateListObserverBridge.get());
  _webStateListObserverBridge.reset();

  _webStateList = nullptr;
  _ntpCoordinator = nil;
  _browserState = nil;
  self.consumer = nil;
}

#pragma mark - CRWWebStateObserver methods.

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  web::WebState* currentWebState = _webStateList->GetActiveWebState();

  // If there is no first responder, try to make the webview or the NTP first
  // responder to have it answer keyboard commands (e.g. space bar to scroll).
  if (!GetFirstResponder() && currentWebState) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(webState);
    if (NTPHelper && NTPHelper->IsActive()) {
      // TODO(crbug.com/1348459): Stop lazy loading in NTPCoordinator and remove
      // this dependency.
      UIViewController* viewController = _ntpCoordinator.viewController;
      [viewController becomeFirstResponder];
    } else {
      [currentWebState->GetWebViewProxy() becomeFirstResponder];
    }
  }
}

#pragma mark - WebStateListObserving methods

- (void)willChangeWebStateList:(WebStateList*)webStateList
                        change:(const WebStateListChangeDetach&)detachChange
                        status:(const WebStateListStatus&)status {
  // When the active webState is detached, the view should be reset.
  if (detachChange.detached_web_state() == _webStateList->GetActiveWebState()) {
    [self.consumer resetTab];
  }
}

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  BOOL isActivationHandled = NO;
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // The activation is handled after this switch statement.
      break;
    case WebStateListChange::Type::kDetach: {
      // When an NTP web state is closed, check if the coordinator should be
      // stopped.
      const WebStateListChangeDetach& detachChange =
          change.As<WebStateListChangeDetach>();
      if (detachChange.is_closing()) {
        NewTabPageTabHelper* NTPTabHelper = NewTabPageTabHelper::FromWebState(
            detachChange.detached_web_state());
        if (status.active_web_state_change()) {
          // The active WebState can be updated when multiple WebStates are
          // closed by `CloseAllWebStates()` or `CloseAllNonPinnedWebStates()`.
          // Call `-didNavigateAwayFromNTP:` to update NTP and record metrics
          // before stopping NTP.
          [self didChangeActiveWebState:status.new_active_web_state
                      oldActiveWebState:status.old_active_web_state
                             isInserted:NO];
          isActivationHandled = YES;
        }
        if (NTPTabHelper->IsActive()) {
          [self stopNTPIfNeeded];
        }
      }
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replaceChange =
          change.As<WebStateListChangeReplace>();
      NewTabPageTabHelper* NTPTabHelper =
          NewTabPageTabHelper::FromWebState(replaceChange.replaced_web_state());
      if (NTPTabHelper->IsActive()) {
        [self stopNTPIfNeeded];
      }

      web::WebState* currentWebState = _webStateList->GetActiveWebState();
      web::WebState* newWebState = replaceChange.inserted_web_state();
      // Add `newTab`'s view to the hierarchy if it's the current Tab.
      if (currentWebState == newWebState) {
        // Set this before triggering any of the possible page loads in
        // displayTabViewIfActive.
        newWebState->SetKeepRenderProcessAlive(true);
        [self.consumer displayTabViewIfActive];
      }
      break;
    }
    case WebStateListChange::Type::kInsert: {
      // If a tab is inserted in the background (not activating), trigger an
      // animation. (The animation for foreground tab insertion is handled in
      // `didChangeActiveWebState`).
      if (!status.active_web_state_change()) {
        [self.consumer initiateNewTabBackgroundAnimation];
      }
      break;
    }
  }

  if (!isActivationHandled && status.active_web_state_change()) {
    [self didChangeActiveWebState:status.new_active_web_state
                oldActiveWebState:status.old_active_web_state
                       isInserted:change.type() ==
                                  WebStateListChange::Type::kInsert];
  }
}

#pragma mark - WebStateListObserving helpers (Private)

- (void)didChangeActiveWebState:(web::WebState*)newActiveWebState
              oldActiveWebState:(web::WebState*)oldActiveWebState
                     isInserted:(bool)isInserted {
  // If the user is leaving an NTP web state, trigger a visibility change.
  if (oldActiveWebState && _ntpCoordinator.started) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(oldActiveWebState);
    if (NTPHelper->IsActive()) {
      [_ntpCoordinator didNavigateAwayFromNTP];
    }
  }

  if (oldActiveWebState) {
    [self.consumer prepareForNewTabAnimation];
  }
  if (newActiveWebState) {
    // Activating without inserting an NTP requires starting it in two
    // scenarios: 1) After doing a batch tab restore (i.e. undo tab removals,
    // initial startup). 2) After re-activating the Browser and a non-active
    // WebState is showing the NTP. BrowserCoordinator's -setActive: only starts
    // the NTP if it is the active view.
    [self startNTPIfNeededForActiveWebState:newActiveWebState];

    // If the user is entering an NTP web state, trigger a visibility change.
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(newActiveWebState);
    if (NTPHelper->IsActive()) {
      [_ntpCoordinator didNavigateToNTPInWebState:newActiveWebState];
    }

    if (isInserted) {
      // This starts the new tab animation. It is important for the
      // NTPCoordinator to know about the new web state
      // (via the call to `-didNavigateToNTPInWebState:` above) before this is
      // called.
      if (!_webStateList->IsBatchInProgress()) {
        [self didInsertActiveWebState:newActiveWebState];
      }
    }

    [self.consumer webStateSelected];
  }
}

- (void)startNTPIfNeededForActiveWebState:(web::WebState*)webState {
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  if (NTPHelper && NTPHelper->IsActive() && !_ntpCoordinator.started) {
    [_ntpCoordinator start];
  }
}

- (void)stopNTPIfNeeded {
  for (int i = 0; i < _webStateList->count(); i++) {
    NewTabPageTabHelper* iterNtpHelper =
        NewTabPageTabHelper::FromWebState(_webStateList->GetWebStateAt(i));
    if (iterNtpHelper->IsActive()) {
      return;
    }
  }
  [_ntpCoordinator stop];
}

- (void)didInsertActiveWebState:(web::WebState*)newWebState {
  DCHECK(newWebState);
  auto* animationTabHelper =
      NewTabAnimationTabHelper::FromWebState(newWebState);
  BOOL animated =
      !animationTabHelper || animationTabHelper->ShouldAnimateNewTab();
  if (animationTabHelper) {
    // Remove the helper because it isn't needed anymore.
    NewTabAnimationTabHelper::RemoveFromWebState(newWebState);
  }
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(newWebState);
  BOOL inBackground =
      (NTPHelper && NTPHelper->ShouldShowStartSurface()) || !animated;
  if (inBackground) {
    [self.consumer initiateNewTabBackgroundAnimation];
  } else {
    [self.consumer initiateNewTabForegroundAnimationForWebState:newWebState];
  }
}

#pragma mark - URLLoadingObserver

- (void)newTabWillLoadURL:(GURL)URL isUserInitiated:(BOOL)isUserInitiated {
  if (isUserInitiated) {
    // Send either the "New Tab Opened" or "New Incognito Tab" opened to the
    // feature_engagement::Tracker based on `inIncognito`.
    feature_engagement::NotifyNewTabEvent(_browserState,
                                          _browserState->IsOffTheRecord());
  }
}

- (void)tabWillLoadURL:(GURL)URL
        transitionType:(ui::PageTransition)transitionType {
  [self.consumer dismissBookmarkModalController];

  web::WebState* currentWebState = _webStateList->GetActiveWebState();
  if (currentWebState &&
      (transitionType & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {
    new_tab_page_uma::RecordActionFromOmnibox(
        _browserState->IsOffTheRecord(), currentWebState, URL, transitionType);
  }
}
- (void)willSwitchToTabWithURL:(GURL)URL
              newWebStateIndex:(NSInteger)newWebStateIndex {
  base::WeakPtr<web::WebState> weakWebStateBeingActivated =
      _webStateList->GetWebStateAt(newWebStateIndex)->GetWeakPtr();
  web::WebState* webStateBeingActivated = weakWebStateBeingActivated.get();
  if (!webStateBeingActivated) {
    return;
  }
  SnapshotTabHelper* snapshotTabHelper =
      SnapshotTabHelper::FromWebState(webStateBeingActivated);
  BOOL willAddPlaceholder =
      PagePlaceholderTabHelper::FromWebState(webStateBeingActivated)
          ->will_add_placeholder_for_next_navigation();
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(webStateBeingActivated);
  UIImage* topToolbarImage = [self.toolbarSnapshotProvider
      toolbarSideSwipeSnapshotForWebState:webStateBeingActivated
                          withToolbarType:ToolbarType::kPrimary];
  UIImage* bottomToolbarImage = [self.toolbarSnapshotProvider
      toolbarSideSwipeSnapshotForWebState:webStateBeingActivated
                          withToolbarType:ToolbarType::kSecondary];
  SwitchToTabAnimationPosition position =
      newWebStateIndex > _webStateList->active_index()
          ? SwitchToTabAnimationPositionAfter
          : SwitchToTabAnimationPositionBefore;

  [self.consumer switchToTabAnimationPosition:position
                            snapshotTabHelper:snapshotTabHelper
                           willAddPlaceholder:willAddPlaceholder
                          newTabPageTabHelper:NTPHelper
                              topToolbarImage:topToolbarImage
                           bottomToolbarImage:bottomToolbarImage];
}

#pragma mark - NewTabPageTabHelperDelegate

- (void)newTabPageHelperDidChangeVisibility:(NewTabPageTabHelper*)NTPHelper
                                forWebState:(web::WebState*)webState {
  if (webState != _webStateList->GetActiveWebState()) {
    // In the instance that a pageload starts while the WebState is not the
    // active WebState anymore, do nothing.
    return;
  }
  // Handle NTP visibility changes within a web state.
  if (NTPHelper->IsActive()) {
    if (!_ntpCoordinator.started) {
      [_ntpCoordinator start];
    }
    [_ntpCoordinator didNavigateToNTPInWebState:webState];
  } else {
    [_ntpCoordinator didNavigateAwayFromNTP];
    [_ntpCoordinator stopIfNeeded];
  }
  [self.consumer displayTabViewIfActive];
}

@end
