// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/tab_events_mediator.h"

#import "ios/chrome/browser/feature_engagement/tracker_util.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/browser_view/tab_consumer.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"
#import "ios/chrome/browser/ui/tabs/switch_to_tab_animation_view.h"
#import "ios/chrome/browser/url_loading/new_tab_animation_tab_helper.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_observer_bridge.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  SessionRestorationBrowserAgent* _sessionRestorationBrowserAgent;
  UrlLoadingNotifierBrowserAgent* _loadingNotifier;
  ChromeBrowserState* _browserState;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                      ntpCoordinator:(NewTabPageCoordinator*)ntpCoordinator
                    restorationAgent:(SessionRestorationBrowserAgent*)
                                         sessionRestorationBrowserAgent
                        browserState:(ChromeBrowserState*)browserState
                     loadingNotifier:
                         (UrlLoadingNotifierBrowserAgent*)urlLoadingNotifier {
  if (self = [super init]) {
    _webStateList = webStateList;
    // TODO(crbug.com/1348459): Stop lazy loading in NTPCoordinator and remove
    // this dependency.
    _ntpCoordinator = ntpCoordinator;
    _sessionRestorationBrowserAgent = sessionRestorationBrowserAgent;
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

- (void)webStateList:(WebStateList*)webStateList
    willDetachWebState:(web::WebState*)webState
               atIndex:(int)atIndex {
  // When the active webState is detached, the view should be reset.
  web::WebState* currentWebState = _webStateList->GetActiveWebState();
  if (webState == currentWebState) {
    [self.consumer resetTab];
  }
}

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  // If a tab is inserted in the background (not activating), trigger an
  // animation. (The animation for foreground tab insertion is handled in
  // `didChangeActiveWebState`).
  if (!activating) {
    [self.consumer initiateNewTabBackgroundAnimation];
  }
}

- (void)webStateList:(WebStateList*)webStateList
    willCloseWebState:(web::WebState*)webState
              atIndex:(int)atIndex
           userAction:(BOOL)userAction {
  // When an NTP web state is closed, check if the coordinator should be
  // stopped.
  NewTabPageTabHelper* NTPTabHelper =
      NewTabPageTabHelper::FromWebState(webState);
  if (NTPTabHelper->IsActive()) {
    [self stopNTPIfNeeded];
  }
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  // If the user is leaving an NTP web state, trigger a visibility change.
  if (oldWebState && _ntpCoordinator.started) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(oldWebState);
    if (NTPHelper->IsActive()) {
      [_ntpCoordinator didNavigateAwayFromNTP];
    }
  }

  if (oldWebState) {
    [self.consumer prepareForNewTabAnimation];
  }
  if (newWebState) {
    // Activating without inserting an NTP requires starting it in two
    // scenarios: 1) After doing a batch tab restore (i.e. undo tab removals,
    // initial startup). 2) After re-activating the Browser and a non-active
    // WebState is showing the NTP. BrowserCoordinator's -setActive: only starts
    // the NTP if it is the active view.
    [self startNTPIfNeededForActiveWebState:newWebState];

    // If the user is entering an NTP web state, trigger a visibility change.
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(newWebState);
    if (NTPHelper->IsActive()) {
      [_ntpCoordinator didNavigateToNTPInWebState:newWebState];
    }

    if (reason == ActiveWebStateChangeReason::Inserted) {
      // This starts the new tab animation. It is important for the
      // NTPCoordinator to know about the new web state
      // (via the call to `-didNavigateToNTPInWebState:` above) before this is
      // called.
      [self didInsertActiveWebState:newWebState];
    }

    [self.consumer webStateSelected];
  }
}

// Observer method, WebState replaced in `webStateList`.
- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  NewTabPageTabHelper* NTPTabHelper =
      NewTabPageTabHelper::FromWebState(oldWebState);
  if (NTPTabHelper->IsActive()) {
    [self stopNTPIfNeeded];
  }

  web::WebState* currentWebState = _webStateList->GetActiveWebState();
  // Add `newTab`'s view to the hierarchy if it's the current Tab.
  if (currentWebState == newWebState) {
    // Set this before triggering any of the possible page loads in
    // displayTabViewIfActive.
    newWebState->SetKeepRenderProcessAlive(true);
    [self.consumer displayTabViewIfActive];
  }
}

#pragma mark - WebStateListObserving helpers (Private)

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
  if (_sessionRestorationBrowserAgent->IsRestoringSession()) {
    return;
  }
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
  [self.consumer switchtoTabWithNewWebStateIndex:newWebStateIndex];
}

@end
