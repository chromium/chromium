// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_scene_observer.h"

#import "base/scoped_observation.h"
#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface DefaultBrowserBannerPromoSceneObserver () <CRWWebStateObserver,
                                                      WebStateListObserving>
@end

@implementation DefaultBrowserBannerPromoSceneObserver {
  // The scene state this observer is observing.
  __weak SceneState* _sceneState;
  // The app agent that owns this observer and that this observer should alert
  // of changes.
  __weak DefaultBrowserBannerPromoAppAgent* _appAgent;

  // Whether the scene is currently in the foreground.
  BOOL _sceneIsForeground;

  // Bridge to allow this object to observe web states.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  // Forwarder that forwards web state events in the active web state here.
  std::unique_ptr<ActiveWebStateObservationForwarder>
      _activeWebStateObservationForwarder;

  // Bridge to allow this object to observe web state lists.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  // Scoped observation for web state lists.
  std::unique_ptr<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _webStateListObservation;
}

- (instancetype)initWithSceneState:(SceneState*)sceneState
                          appAgent:
                              (DefaultBrowserBannerPromoAppAgent*)appAgent {
  self = [super init];
  if (self) {
    _sceneState = sceneState;
    _appAgent = appAgent;

    [_sceneState addObserver:self];

    _sceneIsForeground =
        sceneState.activationLevel >= SceneActivationLevelForegroundInactive;

    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);

    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateListObservation = std::make_unique<
        base::ScopedObservation<WebStateList, WebStateListObserverBridge>>(
        _webStateListObserver.get());
  }
  return self;
}

#pragma mark - Private

// Returns whether the current navigation context changes should count as a
// navigation for this feature.
- (BOOL)navigationOccuredInNavigationContext:
    (web::NavigationContext*)navigationContext {
  // New tabs are added with a blank GURL. Those should automatically count as
  // a navigation.
  if (!self.lastNavigatedURL || self.lastNavigatedURL == GURL()) {
    return YES;
  }

  // New url counts as a navigation.
  return self.lastNavigatedURL != navigationContext->GetUrl().GetWithoutRef() ||
         !navigationContext->IsSameDocument();
}

// Updates all necessary observation and state when the scene state's status
// changes.
- (void)sceneStateChangedData {
  if (!_sceneState.UIEnabled) {
    return;
  }

  Browser* mainBrowser =
      _sceneState.browserProviderInterface.mainBrowserProvider.browser;
  if (!mainBrowser) {
    return;
  }

  WebStateList* webStateList = mainBrowser->GetWebStateList();
  if (!webStateList) {
    return;
  }

  web::WebState* activeMainWebState = webStateList->GetActiveWebState();

  if (_sceneIsForeground && !_sceneState.incognitoContentVisible) {
    _webStateListObservation->Observe(webStateList);
    _activeWebStateObservationForwarder =
        std::make_unique<ActiveWebStateObservationForwarder>(
            webStateList, _webStateObserver.get());
    // Sometimes, like when opening a link in a new window, the scene state
    // doesn't start with an active web state. In this case, the first active
    // web state will be observed via the WebStateList observer methods.
    if (activeMainWebState) {
      [self webStateActivated:activeMainWebState];
      [_appAgent updatePromoState];
    }
  } else {
    _webStateListObservation->Reset();
    _activeWebStateObservationForwarder.reset();
    if (activeMainWebState) {
      [self webStateDeactivated:activeMainWebState];
    }

    [_appAgent updatePromoState];
  }
}

- (void)webStateActivated:(web::WebState*)webState {
  const GURL& currentURL = webState->GetLastCommittedURL().GetWithoutRef();
  self.lastNavigatedURL = currentURL;
}

- (void)webStateDeactivated:(web::WebState*)webState {
  self.lastNavigatedURL = std::nullopt;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (!IsDefaultBrowserBannerPromoEnabled()) {
    return;
  }

  // Alert the owning app agent so it can clean up this observer.
  if (level == SceneActivationLevelDisconnected) {
    [_appAgent onSceneDisconnected:_sceneState];
    return;
  }

  BOOL sceneIsForeground = level >= SceneActivationLevelForegroundInactive;

  // If no change in foreground state has happened, stop here.
  if (_sceneIsForeground == sceneIsForeground) {
    return;
  }

  _sceneIsForeground = sceneIsForeground;
  [self sceneStateChangedData];
}

- (void)sceneState:(SceneState*)sceneState
    isDisplayingIncognitoContent:(BOOL)incognitoContentVisible {
  if (!IsDefaultBrowserBannerPromoEnabled()) {
    return;
  }

  [self sceneStateChangedData];
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  if (!IsDefaultBrowserBannerPromoEnabled()) {
    return;
  }

  // If the scene is not in the foreground yet, skip this change.
  if (!_sceneIsForeground) {
    return;
  }

  [self sceneStateChangedData];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (!status.active_web_state_change()) {
    return;
  }

  if (status.old_active_web_state) {
    [self webStateDeactivated:status.old_active_web_state];
  }
  if (status.new_active_web_state) {
    [self webStateActivated:status.new_active_web_state];
  }

  // Make sure the promo state is correct now that a new web state is active.
  [_appAgent updatePromoState];
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  _webStateListObservation->Reset();
  _activeWebStateObservationForwarder.reset();

  web::WebState* activeWebState = webStateList->GetActiveWebState();
  if (activeWebState) {
    [self webStateDeactivated:activeWebState];
  }

  [_appAgent updatePromoState];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  if (![self navigationOccuredInNavigationContext:navigationContext]) {
    return;
  }

  self.lastNavigatedURL = navigationContext->GetUrl().GetWithoutRef();

  [_appAgent updatePromoState];
}

@end
