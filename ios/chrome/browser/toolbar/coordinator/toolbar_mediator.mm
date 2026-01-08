// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/toolbar_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_consumer.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "url/gurl.h"

@interface ToolbarMediator () <CRWWebStateObserver, WebStateListObserving>
@end

@implementation ToolbarMediator {
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<ActiveWebStateObservationForwarder>
      _activeWebStateObservationForwarder;
  std::unique_ptr<web::WebStateObserverBridge> _activeWebStateObserver;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());

    _activeWebStateObserver =
        std::make_unique<web::WebStateObserverBridge>(self);
    _activeWebStateObservationForwarder =
        std::make_unique<ActiveWebStateObservationForwarder>(
            webStateList, _activeWebStateObserver.get());
  }
  return self;
}

- (void)disconnect {
  _activeWebStateObservationForwarder = nullptr;
  _activeWebStateObserver = nullptr;
  _webStateList->RemoveObserver(_webStateListObserver.get());
  _webStateListObserver = nullptr;
  _webStateList = nullptr;
}

- (void)setConsumer:(id<ToolbarConsumer>)consumer {
  _consumer = consumer;
  if (_webStateList && _webStateList->GetActiveWebState()) {
    [self updateConsumerWithWebState:_webStateList->GetActiveWebState()];
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didLoadPageWithSuccess:(BOOL)loadSuccess {
  [self updateConsumerWithWebState:webState];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [self updateConsumerWithWebState:webState];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self updateConsumerWithWebState:webState];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  [self updateConsumerWithWebState:webState];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self updateConsumerWithWebState:webState];
}

- (void)webStateDidChangeBackForwardState:(web::WebState*)webState {
  [self updateConsumerWithWebState:webState];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change() && status.new_active_web_state) {
    [self updateConsumerWithWebState:status.new_active_web_state];
  }
}

#pragma mark - Private

// Updates the consumer with the current state of the web state.
- (void)updateConsumerWithWebState:(web::WebState*)webState {
  if (!webState) {
    return;
  }
  [_consumer setCanGoBack:self.navigationBrowserAgent->CanGoBack(webState)];
  [_consumer
      setCanGoForward:self.navigationBrowserAgent->CanGoForward(webState)];
  [_consumer setIsLoading:webState->IsLoading()];

  GURL visibleURL = webState->GetVisibleURL();
  [_consumer
      setLocationBarText:[NSString
                             stringWithUTF8String:visibleURL.spec().c_str()]];

  [_consumer setShareEnabled:!visibleURL.is_empty()];
}

@end
