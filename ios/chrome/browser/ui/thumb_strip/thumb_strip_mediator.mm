// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_mediator.h"

#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ThumbStripMediator () <WebStateListObserving> {
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}
@end

@implementation ThumbStripMediator

- (instancetype)init {
  if (self = [super init]) {
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)dealloc {
  if (_regularWebStateList) {
    _regularWebStateList->RemoveObserver(_webStateListObserver.get());
  }
  if (_incognitoWebStateList) {
    _incognitoWebStateList->RemoveObserver(_webStateListObserver.get());
  }
}

- (void)setRegularWebStateList:(WebStateList*)regularWebStateList {
  if (_regularWebStateList) {
    _regularWebStateList->RemoveObserver(_webStateListObserver.get());
    [self removeObserverFromWebState:_regularWebStateList->GetActiveWebState()];
  }

  _regularWebStateList = regularWebStateList;

  if (_regularWebStateList) {
    _regularWebStateList->AddObserver(_webStateListObserver.get());
    [self addObserverToWebState:_regularWebStateList->GetActiveWebState()];
  }
}

- (void)setIncognitoWebStateList:(WebStateList*)incognitoWebStateList {
  if (_incognitoWebStateList) {
    _incognitoWebStateList->RemoveObserver(_webStateListObserver.get());
    [self
        removeObserverFromWebState:_incognitoWebStateList->GetActiveWebState()];
  }

  _incognitoWebStateList = incognitoWebStateList;

  if (_incognitoWebStateList) {
    _incognitoWebStateList->AddObserver(_webStateListObserver.get());
    [self addObserverToWebState:_incognitoWebStateList->GetActiveWebState()];
  }
}

- (void)setWebViewScrollViewObserver:
    (id<CRWWebViewScrollViewProxyObserver>)observer {
  if (self.incognitoWebStateList) {
    [self removeObserverFromWebState:self.incognitoWebStateList
                                         ->GetActiveWebState()];
  }
  if (self.regularWebStateList) {
    [self removeObserverFromWebState:self.regularWebStateList
                                         ->GetActiveWebState()];
  }

  _webViewScrollViewObserver = observer;
  if (self.incognitoWebStateList) {
    [self
        addObserverToWebState:self.incognitoWebStateList->GetActiveWebState()];
  }
  if (self.regularWebStateList) {
    [self addObserverToWebState:self.regularWebStateList->GetActiveWebState()];
  }
}

#pragma mark - Privates

// Remove |self.webViewScrollViewObserver| from the given |webState|. |webState|
// can be nullptr.
- (void)removeObserverFromWebState:(web::WebState*)webState {
  if (webState && self.webViewScrollViewObserver) {
    [webState->GetWebViewProxy().scrollViewProxy
        removeObserver:self.webViewScrollViewObserver];
  }
}

// Add |self.webViewScrollViewObserver| to the given |webState|. |webState| can
// be nullptr.
- (void)addObserverToWebState:(web::WebState*)webState {
  if (webState && self.webViewScrollViewObserver) {
    [webState->GetWebViewProxy().scrollViewProxy
        addObserver:self.webViewScrollViewObserver];
  }
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  [self removeObserverFromWebState:oldWebState];
  [self addObserverToWebState:newWebState];
}

@end
