// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_mediator.h"

#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarMediator () <BooleanObserver, CRWWebStateObserver>

@end

@implementation ToolbarMediator {
  /// Bridges C++ WebStateObserver methods to this mediator.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  /// Forwards observer methods for active WebStates in the WebStateList to
  /// this mediator.
  std::unique_ptr<ActiveWebStateObservationForwarder>
      _activeWebStateObservationForwarder;

  WebStateList* _webStateList;

  /// Pref tracking if bottom omnibox is enabled.
  PrefBackedBoolean* _bottomOmniboxEnabled;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  if (self = [super init]) {
    _webStateList = webStateList;

    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _activeWebStateObservationForwarder =
        std::make_unique<ActiveWebStateObservationForwarder>(
            webStateList, _webStateObserverBridge.get());
  }
  return self;
}

- (void)disconnect {
  _activeWebStateObservationForwarder = nullptr;
  _webStateObserverBridge = nullptr;

  _webStateList = nullptr;
}

- (void)setPrefService:(PrefService*)prefService {
  _prefService = prefService;
  if (IsBottomOmniboxSteadyStateEnabled() && _prefService) {
    _bottomOmniboxEnabled =
        [[PrefBackedBoolean alloc] initWithPrefService:_prefService
                                              prefName:prefs::kBottomOmnibox];
    [_bottomOmniboxEnabled setObserver:self];
  }
}

#pragma mark - Boolean Observer

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _bottomOmniboxEnabled) {
    // TODO(crbug.com/1453279): Do something here.
  }
}

#pragma mark - CRWWebStateObserver methods.

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [self.delegate updateToolbar];
}

@end
