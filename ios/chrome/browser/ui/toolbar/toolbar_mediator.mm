// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_mediator.h"

#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_omnibox_consumer.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarMediator () <BooleanObserver, CRWWebStateObserver>

/// Type of toolbar containing the omnibox. Unlike
/// `steadyStateOmniboxPosition`, this tracks the omnibox position at all
/// time.
@property(nonatomic, assign) ToolbarType omniboxPosition;
/// Type of the toolbar that contains the omnibox when it's not focused. The
/// animation of focusing/defocusing the omnibox changes depending on this
/// position.
@property(nonatomic, assign) ToolbarType steadyStateOmniboxPosition;

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
  /// Whether the omnibox is currently focused.
  BOOL _locationBarFocused;
  /// Whether the browser is incognito.
  BOOL _isIncognito;
  /// Whether the last navigated web state is NTP.
  BOOL _isNTP;
  /// Last trait collection of the toolbars.
  UITraitCollection* _toolbarTraitCollection;
  /// Preferred toolbar to contain the omnibox.
  ToolbarType _preferredOmniboxPosition;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                         isIncognito:(BOOL)isIncognito {
  if (self = [super init]) {
    _webStateList = webStateList;
    _isIncognito = isIncognito;

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
    // Initialize to the correct value.
    [self booleanDidChange:_bottomOmniboxEnabled];
  }
}

- (void)locationBarFocusChangedTo:(BOOL)focused {
  _locationBarFocused = focused;
  if (IsBottomOmniboxSteadyStateEnabled()) {
    [self updateOmniboxPosition];
  }
}

- (void)toolbarTraitCollectionChangedTo:(UITraitCollection*)traitCollection {
  _toolbarTraitCollection = traitCollection;
  if (IsBottomOmniboxSteadyStateEnabled()) {
    [self updateOmniboxPosition];
  }
}

- (void)setInitialOmniboxPosition {
  [self updateOmniboxPosition];
  [self.delegate transitionOmniboxToToolbarType:self.omniboxPosition];
  [self.delegate transitionSteadyStateOmniboxToToolbarType:
                     self.steadyStateOmniboxPosition];
  [self.omniboxConsumer
      steadyStateOmniboxMovedToToolbar:self.steadyStateOmniboxPosition];
}

- (void)didNavigateToNTPOnActiveWebState {
  _isNTP = YES;
  if (IsBottomOmniboxSteadyStateEnabled()) {
    [self updateOmniboxPosition];
  }
}

#pragma mark - Setters

- (void)setOmniboxPosition:(ToolbarType)omniboxPosition {
  if (_omniboxPosition != omniboxPosition) {
    _omniboxPosition = omniboxPosition;
    [self.delegate transitionOmniboxToToolbarType:omniboxPosition];
  }
}

- (void)setSteadyStateOmniboxPosition:(ToolbarType)steadyStateOmniboxPosition {
  if (_steadyStateOmniboxPosition != steadyStateOmniboxPosition) {
    _steadyStateOmniboxPosition = steadyStateOmniboxPosition;
    [self.delegate
        transitionSteadyStateOmniboxToToolbarType:steadyStateOmniboxPosition];
    [self.omniboxConsumer
        steadyStateOmniboxMovedToToolbar:steadyStateOmniboxPosition];
  }
}

#pragma mark - Boolean Observer

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _bottomOmniboxEnabled) {
    _preferredOmniboxPosition = _bottomOmniboxEnabled.value
                                    ? ToolbarType::kSecondary
                                    : ToolbarType::kPrimary;
    [self updateOmniboxPosition];
  }
}

#pragma mark - CRWWebStateObserver methods.

- (void)webStateWasShown:(web::WebState*)webState {
  [self updateForWebState:webState];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [self updateForWebState:webState];
}

#pragma mark - Private

/// Updates the state variables and toolbars with `webState`.
- (void)updateForWebState:(web::WebState*)webState {
  [self.delegate updateToolbar];
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  _isNTP = NTPHelper && NTPHelper->IsActive();
  if (IsBottomOmniboxSteadyStateEnabled()) {
    [self updateOmniboxPosition];
  }
}

/// Computes the toolbar that should contain the unfocused omnibox in the
/// current state.
- (ToolbarType)steadyStateOmniboxPositionInCurrentState {
  CHECK(IsBottomOmniboxSteadyStateEnabled());
  if (_preferredOmniboxPosition == ToolbarType::kPrimary ||
      !IsSplitToolbarMode(_toolbarTraitCollection)) {
    return ToolbarType::kPrimary;
  }
  if (_isNTP && !_isIncognito) {
    return ToolbarType::kPrimary;
  }
  return _preferredOmniboxPosition;
}

/// Computes the toolbar that should contain the omnibox in the current state.
- (ToolbarType)omniboxPositionInCurrentState {
  CHECK(IsBottomOmniboxSteadyStateEnabled());
  if (_locationBarFocused) {
    return ToolbarType::kPrimary;
  } else {
    return [self steadyStateOmniboxPositionInCurrentState];
  }
}

/// Updates the omnibox position to the correct toolbar.
- (void)updateOmniboxPosition {
  if (!IsBottomOmniboxSteadyStateEnabled()) {
    [self.delegate transitionOmniboxToToolbarType:ToolbarType::kPrimary];
    return;
  }

  self.omniboxPosition = [self omniboxPositionInCurrentState];
  self.steadyStateOmniboxPosition =
      [self steadyStateOmniboxPositionInCurrentState];
}

@end
