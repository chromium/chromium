// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/zero_suggest_prefetch_helper.h"

#import "base/feature_list.h"
#import "components/omnibox/browser/omnibox_edit_model.h"
#import "ios/chrome/browser/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

- (void)dealloc {
  /// Reset the web state observation forwarder, which will remove
  /// `_webStateObserverBridge` from the relevant observer list.
  _activeWebStateObservationForwarder.reset();
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserverBridge.get());
    _webStateListObserverBridge.reset();
    _webStateList = nullptr;
  }
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                           editModel:(OmniboxEditModel*)editModel {
  self = [super init];
  if (self) {
    DCHECK(webStateList);
    DCHECK(editModel);

    _webStateList = webStateList;
    _editModel = editModel;
    _webStateObserverBridge = std::make_unique<WebStateObserverBridge>(self);
    _activeWebStateObservationForwarder =
        std::make_unique<ActiveWebStateObservationForwarder>(
            webStateList, _webStateObserverBridge.get());
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserverBridge.get());

    [self startPrefetchIfNecessary];
  }
  return self;
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  [self startPrefetchIfNecessary];
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  if (newWebState) {
    [self startPrefetchIfNecessary];
  }
}

#pragma mark - private

/// Tell edit model to start prefetching whenever there's an active web state.
/// The prefetching is expected on navigation, tab switch, and page reload.
- (void)startPrefetchIfNecessary {
  WebState* activeWebState = _webStateList->GetActiveWebState();
  if (activeWebState == nullptr) {
    return;
  }

  self.editModel->StartPrefetch();
}

@end
