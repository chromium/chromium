// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"

#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface DefaultBrowserBannerPromoAppAgent () <BrowserListObserver,
                                                 CRWWebStateObserver,
                                                 ProfileStateObserver,
                                                 WebStateListObserving>

@end

@implementation DefaultBrowserBannerPromoAppAgent {
  // Observer bridge for observing browser lists.
  std::unique_ptr<BrowserListObserverBridge> _browserListObserverBridge;

  // Observer bridge for observing web state lists.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;

  // Observer bridge for observing web states.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Forwarder for
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _browserListObserverBridge =
        std::make_unique<BrowserListObserverBridge>(self);
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
  }
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    profileStateConnected:(ProfileState*)profileState {
  if (!IsDefaultBrowserBannerPromoEnabled()) {
    return;
  }
  [profileState addObserver:self];
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage != ProfileInitStage::kFinal) {
    return;
  }

  BrowserList* browserList =
      BrowserListFactory::GetForProfile(profileState.profile);

  browserList->AddObserver(_browserListObserverBridge.get());

  // Make sure that already-existing browsers get handled.
  for (Browser* browser :
       browserList->BrowsersOfType(BrowserList::BrowserType::kRegular)) {
    [self browserList:browserList browserAdded:browser];
  }
}

#pragma mark - BrowserListObserver

- (void)browserList:(const BrowserList*)browserList
       browserAdded:(Browser*)browser {
  // Only observe web states in regular browsers.
  if (browser->type() != Browser::Type::kRegular) {
    return;
  }

  WebStateList* webStateList = browser->GetWebStateList();
  webStateList->AddObserver(_webStateListObserverBridge.get());

  web::WebState* webState = webStateList->GetActiveWebState();
  if (webState) {
    webState->AddObserver(_webStateObserverBridge.get());
  }
}

- (void)browserList:(const BrowserList*)browserList
     browserRemoved:(Browser*)browser {
  WebStateList* webStateList = browser->GetWebStateList();
  webStateList->RemoveObserver(_webStateListObserverBridge.get());

  web::WebState* webState = webStateList->GetActiveWebState();
  if (webState) {
    webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

- (void)browserListWillShutdown:(BrowserList*)browserList {
  // Make sure that already-existing browsers are cleaned up as well.
  for (Browser* browser :
       browserList->BrowsersOfType(BrowserList::BrowserType::kRegular)) {
    [self browserList:browserList browserRemoved:browser];
  }

  browserList->RemoveObserver(_browserListObserverBridge.get());
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (!status.active_web_state_change()) {
    return;
  }

  if (status.old_active_web_state) {
    status.old_active_web_state->RemoveObserver(_webStateObserverBridge.get());
  }
  if (status.new_active_web_state) {
    status.new_active_web_state->AddObserver(_webStateObserverBridge.get());
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext {
  // TODO(crbug.com/374119252): Handle navigation.
}

@end
