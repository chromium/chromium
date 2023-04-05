// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_mediator.h"

#import "ios/chrome/browser/overlays/public/overlay_presentation_context.h"
#import "ios/chrome/browser/url/url_util.h"
#import "ios/chrome/browser/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ThumbStripMediator () <WebStateListObserving, CRWWebStateObserver> {
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<ActiveWebStateObservationForwarder> _regularForwarder;
  std::unique_ptr<ActiveWebStateObservationForwarder> _incognitoForwarder;
}
@end

@implementation ThumbStripMediator

- (instancetype)init {
  if (self = [super init]) {
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);

    // Set up the active web state observer.
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
  }
  return self;
}

- (void)dealloc {
  _regularForwarder = nullptr;
  _incognitoForwarder = nullptr;
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
    _regularForwarder = nullptr;
  }

  _regularWebStateList = regularWebStateList;

  if (_regularWebStateList) {
    _regularWebStateList->AddObserver(_webStateListObserver.get());
    [self addObserverToWebState:_regularWebStateList->GetActiveWebState()];
    _regularForwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        _regularWebStateList, _webStateObserver.get());
  }
}

- (void)setIncognitoWebStateList:(WebStateList*)incognitoWebStateList {
  if (_incognitoWebStateList) {
    _incognitoWebStateList->RemoveObserver(_webStateListObserver.get());
    [self
        removeObserverFromWebState:_incognitoWebStateList->GetActiveWebState()];
    _incognitoForwarder = nullptr;
  }

  _incognitoWebStateList = incognitoWebStateList;

  if (_incognitoWebStateList) {
    _incognitoWebStateList->AddObserver(_webStateListObserver.get());
    [self addObserverToWebState:_incognitoWebStateList->GetActiveWebState()];
    _incognitoForwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        _incognitoWebStateList, _webStateObserver.get());
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

// Remove `self.webViewScrollViewObserver` from the given `webState`. `webState`
// can be nullptr.
- (void)removeObserverFromWebState:(web::WebState*)webState {
  if (webState && self.webViewScrollViewObserver) {
    [webState->GetWebViewProxy().scrollViewProxy
        removeObserver:self.webViewScrollViewObserver];
  }
}

// Add `self.webViewScrollViewObserver` to the given `webState`. `webState` can
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

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  // Don't alert the consumer if this navigation is the first navigation in
  // a newly opened tab. That doesn't count.
  if (IsUrlNtp(webState->GetVisibleURL()) &&
      webState->GetLastCommittedURL().is_empty()) {
    return;
  }
  [self.consumer navigationDidStart];
}

#pragma mark - ViewRevealingAnimatee

- (void)willAnimateViewRevealFromState:(ViewRevealState)currentViewRevealState
                               toState:(ViewRevealState)nextViewRevealState {
  if (nextViewRevealState == ViewRevealState::Revealed) {
    self.regularOverlayPresentationContext->SetUIDisabled(true);
    if (self.incognitoOverlayPresentationContext) {
      self.incognitoOverlayPresentationContext->SetUIDisabled(true);
    }
  }
}

- (void)animateViewReveal:(ViewRevealState)nextViewRevealState {
  // No-op.
}

- (void)didAnimateViewRevealFromState:(ViewRevealState)startViewRevealState
                              toState:(ViewRevealState)currentViewRevealState
                              trigger:(ViewRevealTrigger)trigger {
  if (currentViewRevealState == ViewRevealState::Peeked ||
      currentViewRevealState == ViewRevealState::Hidden) {
    self.regularOverlayPresentationContext->SetUIDisabled(false);
    if (self.incognitoOverlayPresentationContext) {
      self.incognitoOverlayPresentationContext->SetUIDisabled(false);
    }
  }
}

@end
