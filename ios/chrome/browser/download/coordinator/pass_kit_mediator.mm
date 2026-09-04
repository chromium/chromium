// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/pass_kit_mediator.h"

#import "base/check.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface PassKitMediator () <CRWWebStateObserver, WebStateListObserving>
@end

@implementation PassKitMediator {
  __weak id<WebContentCommands> _webContentHandler;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _webStateListObservation;
  std::unique_ptr<web::WebStateObserverBridge> _activeWebStateObserver;
  std::unique_ptr<
      base::ScopedObservation<web::WebState, web::WebStateObserverBridge>>
      _activeWebStateObservation;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                   webContentHandler:(id<WebContentCommands>)webContentHandler {
  self = [super init];
  if (self) {
    CHECK(webStateList);
    _webContentHandler = webContentHandler;

    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateListObservation = std::make_unique<
        base::ScopedObservation<WebStateList, WebStateListObserverBridge>>(
        _webStateListObserver.get());
    _webStateListObservation->Observe(webStateList);

    _activeWebStateObserver =
        std::make_unique<web::WebStateObserverBridge>(self);
    _activeWebStateObservation = std::make_unique<
        base::ScopedObservation<web::WebState, web::WebStateObserverBridge>>(
        _activeWebStateObserver.get());
    [self updateActiveWebState:webStateList->GetActiveWebState()];
  }
  return self;
}

- (void)disconnect {
  _webContentHandler = nil;
  _activeWebStateObservation.reset();
  _activeWebStateObserver.reset();
  _webStateListObservation.reset();
  _webStateListObserver.reset();
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change()) {
    [self updateActiveWebState:status.new_active_web_state];
    if (status.old_active_web_state) {
      [_webContentHandler dismissPassKitDialog];
    }
  }
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  _webStateListObservation.reset();
  _webStateListObserver.reset();
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext {
  if (!navigationContext->IsSameDocument()) {
    [_webContentHandler dismissPassKitDialog];
  }
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  if (navigationContext->HasCommitted() &&
      !navigationContext->IsSameDocument()) {
    [_webContentHandler dismissPassKitDialog];
  }
}

- (void)webStateDestroyed:(web::WebState*)webState {
  _activeWebStateObservation->Reset();
  [_webContentHandler dismissPassKitDialog];
}

#pragma mark - Private

- (void)updateActiveWebState:(web::WebState*)newActiveWebState {
  _activeWebStateObservation->Reset();
  if (newActiveWebState) {
    _activeWebStateObservation->Observe(newActiveWebState);
  }
}

@end
