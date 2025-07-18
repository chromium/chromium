// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/badges/ui_bundled/incognito_badge_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_item.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_static_item.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_type_util.h"
#import "ios/chrome/browser/badges/ui_bundled/incognito_badge_consumer.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface IncognitoBadgeMediator () <CRWWebStateObserver,
                                      WebStateListObserving>
@end

@implementation IncognitoBadgeMediator {
  // Used to observe the active WebState.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<base::ScopedObservation<web::WebState, web::WebStateObserver>>
      _webStateObservation;

  // Used to observe the WebStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<base::ScopedObservation<WebStateList, WebStateListObserver>>
      _webStateListObservation;

  // The WebStateList that this mediator listens for any changes on the active
  // web state.
  raw_ptr<WebStateList> _webStateList;
  // The WebStateList's active WebState.
  raw_ptr<web::WebState> _webState;
  // The OTR badge item.
  id<BadgeItem> _offTheRecordBadge;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _offTheRecordBadge =
        [[BadgeStaticItem alloc] initWithBadgeType:kBadgeTypeIncognito];
    // Set up the WebStateList and its observer.
    _webStateList = webStateList;
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _webStateListObservation = std::make_unique<
        base::ScopedObservation<WebStateList, WebStateListObserver>>(
        _webStateListObserverBridge.get());
    _webStateListObservation->Observe(_webStateList);

    // Set up the WebState and its observer.
    _webState = _webStateList->GetActiveWebState();
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webStateObservation = std::make_unique<
        base::ScopedObservation<web::WebState, web::WebStateObserver>>(
        _webStateObserverBridge.get());
    if (_webState) {
      _webStateObservation->Observe(_webState);
    }
  }
  return self;
}

- (void)dealloc {
  // `-disconnect` must be called before deallocation.
  DCHECK(!_webStateList);
}

- (void)disconnect {
  self.consumer = nil;
  [self disconnectWebState];
  [self disconnectWebStateList];
}

#pragma mark - Disconnect helpers

- (void)disconnectWebState {
  if (_webState) {
    _webStateObservation.reset();
    _webState = nullptr;
  }
}

- (void)disconnectWebStateList {
  if (_webStateList) {
    _webStateListObservation.reset();
    _webStateList = nullptr;
  }
}

#pragma mark - Accessors

- (void)setConsumer:(id<IncognitoBadgeConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  if (!_consumer) {
    return;
  }

  // Update the consumer with the new badge item.
  [_consumer setupWithIncognitoBadge:_offTheRecordBadge];
}

- (void)setWebState:(web::WebState*)webState {
  if (_webState == webState) {
    return;
  }
  _webStateObservation->Reset();
  if (webState) {
    _webStateObservation->Observe(webState);
  }
  _webState = webState;
  [self maybeUpdateIncognitoBadgeVisibility];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self maybeUpdateIncognitoBadgeVisibility];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(webState, _webState);
  [self disconnectWebState];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change()) {
    self.webState = status.new_active_web_state;
  }
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  [self disconnectWebStateList];
}

#pragma mark - Private

- (BOOL)isCurrentWebStateShowingNTP {
  if (!_webStateList || !_webStateList->GetActiveWebState()) {
    return NO;
  }

  return IsUrlNtp(_webStateList->GetActiveWebState()->GetVisibleURL());
}

- (void)maybeUpdateIncognitoBadgeVisibility {
  if (!base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV2)) {
    return;
  }
  _consumer.disabled = [self isCurrentWebStateShowingNTP];
}

@end
