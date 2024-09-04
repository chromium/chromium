// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/zero_suggest_prefetch_helper.h"

#import "base/feature_list.h"
#import "components/omnibox/browser/omnibox_controller.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

using web::WebState;
using web::WebStateObserverBridge;

@interface ZeroSuggestPrefetchHelper () <CRWWebStateObserver,
                                         WebStateListObserving>
@end

@implementation ZeroSuggestPrefetchHelper {
  /// Bridge to receive active web state events
  std::unique_ptr<ActiveWebStateObservationForwarder>
      _activeWebStateObservationForwarder;
  /// Bridge to receive WS events for the active web state.
  std::unique_ptr<WebStateObserverBridge> _webStateObserverBridge;

  /// Bridge to receive WSL events.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                          controller:(OmniboxController*)controller {
  self = [super init];
  if (self) {
    DCHECK(webStateList);
    DCHECK(controller);

    _webStateList = webStateList;
    _controller = controller;
    _webStateObserverBridge = std::make_unique<WebStateObserverBridge>(self);
    _activeWebStateObservationForwarder =
        std::make_unique<ActiveWebStateObservationForwarder>(
            webStateList, _webStateObserverBridge.get());
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserverBridge.get());

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(updateForAppWillForeground)
               name:UIApplicationWillEnterForegroundNotification
             object:nil];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(updateForAppDidBackground)
               name:UIApplicationDidEnterBackgroundNotification
             object:nil];

    [self startPrefetchIfNecessary];
  }
  return self;
}

- (void)disconnect {
  _activeWebStateObservationForwarder.reset();
  _webStateObserverBridge.reset();
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserverBridge.get());
    _webStateListObserverBridge.reset();
    _webStateList = nullptr;
  }

  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  [self startPrefetchIfNecessary];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change() && status.new_active_web_state) {
    [self startPrefetchIfNecessary];
  }
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  [self disconnect];
}

#pragma mark - private

/// Tell edit model to start prefetching whenever there's an active web state.
/// The prefetching is expected on navigation, tab switch, and page reload.
- (void)startPrefetchIfNecessary {
  WebState* activeWebState = _webStateList->GetActiveWebState();
  if (activeWebState == nullptr) {
    return;
  }

  self.controller->StartZeroSuggestPrefetch();
}

/// Indicates to this tab helper that the app has entered a foreground state.
- (void)updateForAppWillForeground {
  self.controller->autocomplete_controller()
      ->autocomplete_provider_client()
      ->set_in_background_state(false);

  [self startPrefetchIfNecessary];
}

/// Indicates to this tab helper that the app has entered a background state.
- (void)updateForAppDidBackground {
  self.controller->autocomplete_controller()
      ->autocomplete_provider_client()
      ->set_in_background_state(true);
}

@end
