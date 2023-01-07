// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/tab_events_mediator.h"

#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabEventsMediator () <CRWWebStateObserver>

@end

@implementation TabEventsMediator {
  // Bridges C++ WebStateObserver methods to this mediator.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Forwards observer methods for all WebStates in the WebStateList to
  // this mediator.
  std::unique_ptr<AllWebStateObservationForwarder>
      _allWebStateObservationForwarder;

  WebStateList* _webStateList;
  __weak NewTabPageCoordinator* _ntpCoordinator;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                      ntpCoordinator:(NewTabPageCoordinator*)ntpCoordinator {
  if (self = [super init]) {
    _webStateList = webStateList;
    // TODO(crbug.com/1348459): Stop lazy loading in NTPCoordinator and remove
    // this dependency.
    _ntpCoordinator = ntpCoordinator;

    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _allWebStateObservationForwarder =
        std::make_unique<AllWebStateObservationForwarder>(
            _webStateList, _webStateObserverBridge.get());
  }
  return self;
}

- (void)disconnect {
  _allWebStateObservationForwarder = nullptr;
  _webStateObserverBridge = nullptr;

  _webStateList = nullptr;
  _ntpCoordinator = nil;
}

#pragma mark - CRWWebStateObserver methods.

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  web::WebState* currentWebState = _webStateList->GetActiveWebState();

  // If there is no first responder, try to make the webview or the NTP first
  // responder to have it answer keyboard commands (e.g. space bar to scroll).
  if (!GetFirstResponder() && currentWebState) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(webState);
    if (NTPHelper && NTPHelper->IsActive()) {
      // TODO(crbug.com/1348459): Stop lazy loading in NTPCoordinator and remove
      // this dependency.
      UIViewController* viewController = _ntpCoordinator.viewController;
      [viewController becomeFirstResponder];
    } else {
      [currentWebState->GetWebViewProxy() becomeFirstResponder];
    }
  }
}

@end
