// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/field_trial_params.h"
#import "base/metrics/histogram_functions.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/result.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_position_metrics.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_position_util.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_omnibox_consumer.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface ToolbarMediator () <BooleanObserver,
                               CRWWebStateObserver,
                               WebStateListObserving>

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

  /// Observes web state activation.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;

  raw_ptr<WebStateList> _webStateList;

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

  /// Whether SafariSwitcher should be checked on FRE.
  BOOL _shouldCheckSafariSwitcherOnFRE;
  /// Whether the NTP was shown in FRE.
  BOOL _hasEnteredNTPOnFRE;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                         isIncognito:(BOOL)isIncognito {
  if ((self = [super init])) {
    _webStateList = webStateList;
    _isIncognito = isIncognito;

    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _activeWebStateObservationForwarder =
        std::make_unique<ActiveWebStateObservationForwarder>(
            webStateList, _webStateObserverBridge.get());
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserverBridge.get());

    if (IsBottomOmniboxAvailable()) {
      // Device switcher data is not available in incognito.
      _shouldCheckSafariSwitcherOnFRE = !isIncognito && IsFirstRun();

      _bottomOmniboxEnabled = [[PrefBackedBoolean alloc]
          initWithPrefService:GetApplicationContext()->GetLocalState()
                     prefName:prefs::kBottomOmnibox];
      [_bottomOmniboxEnabled setObserver:self];
      // Initialize to the correct value.
      [self booleanDidChange:_bottomOmniboxEnabled];
      [self updateOmniboxDefaultPosition];
      [self logOmniboxPosition];
    }
  }
  return self;
}

- (void)disconnect {
  _activeWebStateObservationForwarder = nullptr;
  _webStateObserverBridge = nullptr;
  _webStateList->RemoveObserver(_webStateListObserverBridge.get());
  _webStateListObserverBridge = nullptr;

  _webStateList = nullptr;
  [_bottomOmniboxEnabled stop];
  [_bottomOmniboxEnabled setObserver:nil];
  _bottomOmniboxEnabled = nil;
}

- (void)locationBarFocusChangedTo:(BOOL)focused {
  _locationBarFocused = focused;
  if (IsBottomOmniboxAvailable()) {
    [self updateOmniboxPosition];
  }
}

- (void)toolbarTraitCollectionChangedTo:(UITraitCollection*)traitCollection {
  _toolbarTraitCollection = traitCollection;
  if (IsBottomOmniboxAvailable()) {
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
  if (IsBottomOmniboxAvailable()) {
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

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change() && status.new_active_web_state) {
    [self updateForWebState:status.new_active_web_state];
  }
}

#pragma mark - Private

/// Updates the state variables and toolbars with `webState`.
- (void)updateForWebState:(web::WebState*)webState {
  [self.delegate updateToolbar];
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  _isNTP = NTPHelper && NTPHelper->IsActive();
  if (IsBottomOmniboxAvailable()) {
    if (_shouldCheckSafariSwitcherOnFRE) {
      [self checkSafariSwitcherOnFRE];
    }
    [self updateOmniboxPosition];
  }
}

/// Computes the toolbar that should contain the unfocused omnibox in the
/// current state.
- (ToolbarType)steadyStateOmniboxPositionInCurrentState {
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
  if (_locationBarFocused) {
    return ToolbarType::kPrimary;
  } else {
    return [self steadyStateOmniboxPositionInCurrentState];
  }
}

/// Updates the omnibox position to the correct toolbar.
- (void)updateOmniboxPosition {
  if (!IsBottomOmniboxAvailable()) {
    [self.delegate transitionOmniboxToToolbarType:ToolbarType::kPrimary];
    return;
  }

  self.omniboxPosition = [self omniboxPositionInCurrentState];
  self.steadyStateOmniboxPosition =
      [self steadyStateOmniboxPositionInCurrentState];
}

#pragma mark Default omnibox position

/// Verifies if the user is a safari switcher on FRE.
- (void)checkSafariSwitcherOnFRE {
  CHECK(_shouldCheckSafariSwitcherOnFRE);
  CHECK(self.deviceSwitcherResultDispatcher);

  if (_isNTP) {
    _hasEnteredNTPOnFRE = YES;
  } else if (_hasEnteredNTPOnFRE) {
    // Check device switcher data when the user leaves NTP on FRE, as data is
    // only available on sync and takes time to fetch. This is only executed
    // once, if the data is unavailable user may still see the bottom omnibox on
    // next restart (see. `updateOmniboxDefaultPosition`).
    _shouldCheckSafariSwitcherOnFRE = NO;
    segmentation_platform::ClassificationResult result =
        self.deviceSwitcherResultDispatcher->GetCachedClassificationResult();
    if (result.status == segmentation_platform::PredictionStatus::kSucceeded) {
      if (omnibox::IsSafariSwitcher(result)) {
        std::string featureParam = base::GetFieldTrialParamValueByFeature(
            kBottomOmniboxDefaultSetting, kBottomOmniboxDefaultSettingParam);
        if (featureParam == kBottomOmniboxDefaultSettingParamSafariSwitcher) {
          PrefService* localState = GetApplicationContext()->GetLocalState();
          localState->SetDefaultPrefValue(prefs::kBottomOmnibox,
                                          base::Value(YES));
          localState->SetBoolean(prefs::kBottomOmniboxByDefault, YES);
        }
        base::UmaHistogramEnumeration(
            kOmniboxDeviceSwitcherResultAtFRE,
            OmniboxDeviceSwitcherResult::kBottomOmnibox);
      } else {
        base::UmaHistogramEnumeration(kOmniboxDeviceSwitcherResultAtFRE,
                                      OmniboxDeviceSwitcherResult::kTopOmnibox);
      }
    } else {
      base::UmaHistogramEnumeration(kOmniboxDeviceSwitcherResultAtFRE,
                                    OmniboxDeviceSwitcherResult::kUnavailable);
    }
  }
}

/// Returns whether user is a safari switcher at startup.
/// Used to set the default omnibox position to bottom for `IsNewUser`
/// that are not in FRE. If bottom omnibox is already default
/// `bottomOmniboxIsDefault`, still log the status as bottom as the user was
/// classified as safari switcher in a previous session.
- (BOOL)isSafariSwitcherAtStartup:(BOOL)bottomOmniboxIsDefault {

  if (!omnibox::IsNewUser()) {
    base::UmaHistogramEnumeration(kOmniboxDeviceSwitcherResultAtStartup,
                                  OmniboxDeviceSwitcherResult::kNotNewUser);
    return NO;
  }

  if (bottomOmniboxIsDefault) {
    base::UmaHistogramEnumeration(kOmniboxDeviceSwitcherResultAtStartup,
                                  OmniboxDeviceSwitcherResult::kBottomOmnibox);
    return YES;
  }

  segmentation_platform::ClassificationResult result =
      self.deviceSwitcherResultDispatcher->GetCachedClassificationResult();
  if (result.status != segmentation_platform::PredictionStatus::kSucceeded) {
    base::UmaHistogramEnumeration(kOmniboxDeviceSwitcherResultAtStartup,
                                  OmniboxDeviceSwitcherResult::kUnavailable);
    return NO;
  }

  if (omnibox::IsSafariSwitcher(result)) {
    base::UmaHistogramEnumeration(kOmniboxDeviceSwitcherResultAtStartup,
                                  OmniboxDeviceSwitcherResult::kBottomOmnibox);
    return YES;
  }
  base::UmaHistogramEnumeration(kOmniboxDeviceSwitcherResultAtStartup,
                                OmniboxDeviceSwitcherResult::kTopOmnibox);
  return NO;
}

/// Updates the default setting for bottom omnibox.
- (void)updateOmniboxDefaultPosition {
  PrefService* localState = GetApplicationContext()->GetLocalState();

  // This only needs to be executed once and deviceSwitcherResult are not
  // available in incognito.
  if (!self.deviceSwitcherResultDispatcher ||
      localState->GetUserPrefValue(prefs::kBottomOmnibox)) {
    return;
  }

  BOOL bottomOmniboxEnabledByDefault = NO;
  if (localState->GetUserPrefValue(prefs::kBottomOmniboxByDefault)) {
    bottomOmniboxEnabledByDefault =
        localState->GetBoolean(prefs::kBottomOmniboxByDefault);
  }

  std::string featureParam = base::GetFieldTrialParamValueByFeature(
      kBottomOmniboxDefaultSetting, kBottomOmniboxDefaultSettingParam);
  if (featureParam == kBottomOmniboxDefaultSettingParamBottom) {
    bottomOmniboxEnabledByDefault = YES;
  } else if (featureParam == kBottomOmniboxDefaultSettingParamTop) {
    bottomOmniboxEnabledByDefault = NO;
  }

  // Call `isSafariSwitcherAtStartup` in all cases to collect metrics on the
  // device switcher result availability.
  if ([self isSafariSwitcherAtStartup:bottomOmniboxEnabledByDefault] &&
      featureParam == kBottomOmniboxDefaultSettingParamSafariSwitcher) {
    bottomOmniboxEnabledByDefault = YES;
  }

  // Make sure that users who have already seen the bottom omnibox by default
  // keep it.
  if (bottomOmniboxEnabledByDefault) {
    localState->SetBoolean(prefs::kBottomOmniboxByDefault, YES);
  }

  localState->SetDefaultPrefValue(prefs::kBottomOmnibox,
                                  base::Value(bottomOmniboxEnabledByDefault));
}

/// Logs preferred omnibox position.
- (void)logOmniboxPosition {

  static dispatch_once_t once;
  dispatch_once(&once, ^{
    PrefService* localState = GetApplicationContext()->GetLocalState();
    const BOOL isBottomOmnibox = localState->GetBoolean(prefs::kBottomOmnibox);
    OmniboxPositionType positionType = isBottomOmnibox
                                           ? OmniboxPositionType::kBottom
                                           : OmniboxPositionType::kTop;
    base::UmaHistogramEnumeration(kOmniboxSteadyStatePositionAtStartup,
                                  positionType);

    if (localState->GetUserPrefValue(prefs::kBottomOmnibox)) {
      base::UmaHistogramEnumeration(
          kOmniboxSteadyStatePositionAtStartupSelected, positionType);
    }
  });
}

@end
