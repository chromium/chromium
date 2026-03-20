// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface FullscreenMediator () <CRWWebStateObserver, WebStateListObserving>

// The active WebState.
@property(nonatomic, assign) web::WebState* webState;

@end

@implementation FullscreenMediator {
  raw_ptr<FullscreenBrowserAgent> _browserAgent;
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
}

#pragma mark - Public

- (instancetype)initWithBrowserAgent:(FullscreenBrowserAgent*)browserAgent
                        webStateList:(WebStateList*)webStateList {
  if ((self = [super init])) {
    CHECK(browserAgent);
    CHECK(webStateList);
    _browserAgent = browserAgent;
    _webStateList = webStateList;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    self.webState = _webStateList->GetActiveWebState();
  }
  return self;
}

- (void)disconnect {
  _browserAgent = nullptr;
  _webStateList->RemoveObserver(_webStateListObserver.get());
  _webStateListObserver = nullptr;
  _webStateList = nullptr;
  self.webState = nullptr;
  _webStateObserver = nullptr;
}

#pragma mark - Properties

- (void)setWebState:(web::WebState*)webState {
  if (_webState == webState) {
    return;
  }
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
  }
  _webState = webState;
  if (_webState) {
    _webState->AddObserver(_webStateObserver.get());
  }
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change()) {
    self.webState = status.new_active_web_state;
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(self.webState, webState);
  self.webState = nullptr;
}

@end
