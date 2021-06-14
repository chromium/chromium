// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/mobileconfig_coordinator.h"

#include <memory>

#include "base/scoped_observation.h"
#import "ios/chrome/browser/download/mobileconfig_tab_helper.h"
#import "ios/chrome/browser/download/mobileconfig_tab_helper_delegate.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface MobileConfigCoordinator () <CRWWebStateObserver,
                                       MobileConfigTabHelperDelegate,
                                       WebStateListObserving> {
  // WebStateList observers.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<base::ScopedObservation<WebStateList, WebStateListObserver>>
      _scopedWebStateListObserver;
}

// The WebStateList being observed.
@property(nonatomic, readonly) WebStateList* webStateList;

@end

@implementation MobileConfigCoordinator

- (WebStateList*)webStateList {
  return self.browser->GetWebStateList();
}

- (void)start {
  for (int i = 0; i < self.webStateList->count(); i++) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    [self installDelegatesForWebState:webState];
  }

  _webStateListObserverBridge =
      std::make_unique<WebStateListObserverBridge>(self);
  _scopedWebStateListObserver = std::make_unique<
      base::ScopedObservation<WebStateList, WebStateListObserver>>(
      _webStateListObserverBridge.get());
  _scopedWebStateListObserver->Observe(self.webStateList);
}

- (void)stop {
  for (int i = 0; i < self.webStateList->count(); i++) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    [self uninstallDelegatesForWebState:webState];
  }
}

#pragma mark - Private

// Installs delegates for |webState|.
- (void)installDelegatesForWebState:(web::WebState*)webState {
  if (MobileConfigTabHelper::FromWebState(webState)) {
    MobileConfigTabHelper::FromWebState(webState)->set_delegate(self);
  }
}

// Uninstalls delegates for |webState|.
- (void)uninstallDelegatesForWebState:(web::WebState*)webState {
  if (MobileConfigTabHelper::FromWebState(webState)) {
    MobileConfigTabHelper::FromWebState(webState)->set_delegate(nil);
  }
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self installDelegatesForWebState:webState];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  [self uninstallDelegatesForWebState:oldWebState];
  [self installDelegatesForWebState:newWebState];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  [self uninstallDelegatesForWebState:webState];
}

#pragma mark - MobileConfigTabHelperDelegate

- (void)presentMobileConfigAlertFromURL:(NSURL*)fileURL {
  // TODO(crbug.com/781770): Present the alert.
}

@end
