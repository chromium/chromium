// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/safari_download_mediator.h"

#import <memory>

#import "base/check.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/download/coordinator/safari_download_mediator_delegate.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface SafariDownloadMediator () <CRWWebStateObserver>
@end

@implementation SafariDownloadMediator {
  // Bridge forwarding active `WebState` events to this mediator. Cleared in
  // `-disconnect` after `_activeWebStateObservation`.
  std::unique_ptr<web::WebStateObserverBridge> _activeWebStateObserver;
  // Scoped observation of the active `WebState`. Declared after and reset
  // before `_activeWebStateObserver` in `-disconnect`.
  std::unique_ptr<base::ScopedObservation<web::WebState, web::WebStateObserver>>
      _activeWebStateObservation;
}

#pragma mark - Initialization

- (instancetype)initWithActiveWebState:(web::WebState*)activeWebState {
  self = [super init];
  if (self) {
    _activeWebStateObserver =
        std::make_unique<web::WebStateObserverBridge>(self);
    _activeWebStateObservation = std::make_unique<
        base::ScopedObservation<web::WebState, web::WebStateObserver>>(
        _activeWebStateObserver.get());
    if (activeWebState) {
      _activeWebStateObservation->Observe(activeWebState);
    }
  }
  return self;
}

#pragma mark - Public

- (void)disconnect {
  self.delegate = nil;
  self.tabHelperDelegate = nil;
  _activeWebStateObservation.reset();
  _activeWebStateObserver.reset();
}

#pragma mark - TabsDependencyInstalling

- (void)webStateInserted:(web::WebState*)webState {
  if (!_activeWebStateObservation) {
    return;
  }
  CHECK(self.tabHelperDelegate);
  SafariDownloadTabHelper* tabHelper =
      SafariDownloadTabHelper::FromWebState(webState);
  if (tabHelper) {
    tabHelper->set_delegate(self.tabHelperDelegate);
  }
}

- (void)webStateRemoved:(web::WebState*)webState {
  SafariDownloadTabHelper* tabHelper =
      SafariDownloadTabHelper::FromWebState(webState);
  if (tabHelper) {
    tabHelper->set_delegate(nil);
  }
}

- (void)webStateDeleted:(web::WebState*)webState {
  // Nothing to do.
}

- (void)newWebStateActivated:(web::WebState*)newActive
           oldActiveWebState:(web::WebState*)oldActive {
  if (!_activeWebStateObservation) {
    return;
  }
  _activeWebStateObservation->Reset();
  if (newActive) {
    _activeWebStateObservation->Observe(newActive);
  }
  if (oldActive) {
    [self.delegate safariDownloadMediator:self
            shouldDismissAlertForWebState:oldActive];
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateWasHidden:(web::WebState*)webState {
  [self.delegate safariDownloadMediator:self
          shouldDismissAlertForWebState:webState];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext {
  if (!navigationContext->IsSameDocument()) {
    [self.delegate safariDownloadMediator:self
            shouldDismissAlertForWebState:webState];
  }
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  if (navigationContext->HasCommitted() &&
      !navigationContext->IsSameDocument()) {
    [self.delegate safariDownloadMediator:self
            shouldDismissAlertForWebState:webState];
  }
}

- (void)webStateDestroyed:(web::WebState*)webState {
  _activeWebStateObservation->Reset();
  [self.delegate safariDownloadMediator:self
          shouldDismissAlertForWebState:webState];
}

@end
