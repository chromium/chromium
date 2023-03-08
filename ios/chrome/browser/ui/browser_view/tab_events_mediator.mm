// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/tab_events_mediator.h"

#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/browser_view/tab_consumer.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"
#import "ios/chrome/browser/url_loading/new_tab_animation_tab_helper.h"
#import "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabEventsMediator () <CRWWebStateObserver, WebStateListObserving>

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

  WebStateList* _webStateList;
  __weak NewTabPageCoordinator* _ntpCoordinator;
  SessionRestorationBrowserAgent* _sessionRestorationBrowserAgent;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                      ntpCoordinator:(NewTabPageCoordinator*)ntpCoordinator
                    restorationAgent:(SessionRestorationBrowserAgent*)
                                         sessionRestorationBrowserAgent {
  if (self = [super init]) {
    _webStateList = webStateList;
    // TODO(crbug.com/1348459): Stop lazy loading in NTPCoordinator and remove
    // this dependency.
    _ntpCoordinator = ntpCoordinator;
    _sessionRestorationBrowserAgent = sessionRestorationBrowserAgent;

    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    webStateList->AddObserver(_webStateListObserverBridge.get());
    _allWebStateObservationForwarder =
        std::make_unique<AllWebStateObservationForwarder>(
            _webStateList, _webStateObserverBridge.get());
  }
  return self;
}

- (void)disconnect {
  _allWebStateObservationForwarder = nullptr;
  _webStateObserverBridge = nullptr;

  _webStateList->RemoveObserver(_webStateListObserverBridge.get());
  _webStateListObserverBridge.reset();

  _webStateList = nullptr;
  _ntpCoordinator = nil;
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
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  if (reason == ActiveWebStateChangeReason::Inserted) {
    [self didInsertActiveWebState:newWebState];
  }
  if (oldWebState) {
    [self.consumer prepareForNewTabAnimation];
  }
  // NOTE: webStateSelected expects to always be called with a
  // non-null WebState.
  if (newWebState) {
    [self.consumer webStateSelected:newWebState];
  }
}

// Observer method, WebState replaced in `webStateList`.
- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  web::WebState* currentWebState = _webStateList->GetActiveWebState();
  // Add `newTab`'s view to the hierarchy if it's the current Tab.
  if (currentWebState == newWebState) {
    // Set this before triggering any of the possible page loads in
    // displayWebStateIfActive.
    newWebState->SetKeepRenderProcessAlive(true);
    [self.consumer displayWebStateIfActive:newWebState];
  }
}

#pragma mark - WebStateListObserving helpers (Private)

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
  // Since we share the NTP coordinator across web states, the feed type could
  // be different from default, so we reset it.
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(newWebState);
  if (NTPHelper && NTPHelper->IsActive()) {
    [_ntpCoordinator start];
    FeedType defaultFeedType = NTPHelper->DefaultFeedType();
    if (_ntpCoordinator.selectedFeed != defaultFeedType) {
      [_ntpCoordinator selectFeedType:defaultFeedType];
    }
  }
  BOOL inBackground =
      (NTPHelper && NTPHelper->ShouldShowStartSurface()) || !animated;
  if (inBackground) {
    [self.consumer initiateNewTabBackgroundAnimation];
  } else {
    [self.consumer initiateNewTabForegroundAnimationForWebState:newWebState];
  }
}

@end
