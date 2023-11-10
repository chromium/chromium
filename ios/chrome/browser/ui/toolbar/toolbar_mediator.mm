// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_mediator.h"

#import "base/metrics/field_trial_params.h"
#import "base/metrics/histogram_functions.h"
#import "base/stl_util.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/result.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_position_metrics.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_omnibox_consumer.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

namespace {

/// The time delta for a user to be considered as a new user.
const base::TimeDelta kNewUserTimeDelta = base::Days(60);

/// Returns whether it's first run.
BOOL IsFirstRun() {
  return FirstRun::IsChromeFirstRun() ||
         experimental_flags::AlwaysDisplayFirstRun();
}

/// Returns wheter the user has seen first run recently (`kNewUserTimeDelta`).
BOOL IsNewUser() {
  // Use the first_run age to determine the user is new on this device.
  if (IsFirstRun()) {
    return YES;
  }
  absl::optional<base::File::Info> info = FirstRun::GetSentinelInfo();
  if (!info.has_value()) {
    return NO;
  }
  base::Time first_run_time = info.value().creation_time;
  BOOL isFirstRunRecent =
      base::Time::Now() - first_run_time < kNewUserTimeDelta;
  return isFirstRunRecent;
}

/// Returns whether classification `result` should have bottom omnibox by
/// default.
BOOL ShouldSwitchOmniboxToBottom(
    const segmentation_platform::ClassificationResult& result) {
  CHECK(result.status == segmentation_platform::PredictionStatus::kSucceeded);
  if (result.ordered_labels.empty()) {
    DUMP_WILL_BE_CHECK(!result.ordered_labels.empty());
    return NO;
  }

  if (!IsNewUser()) {
    return NO;
  }

  std::vector<std::string> excludedLabels = {
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel,
      segmentation_platform::DeviceSwitcherModel::kAndroidTabletLabel,
      segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel,
      segmentation_platform::DeviceSwitcherModel::kIosTabletLabel};
  std::sort(excludedLabels.begin(), excludedLabels.end());

  auto sortedLabels = std::vector<std::string>(result.ordered_labels);
  std::sort(sortedLabels.begin(), sortedLabels.end());

  std::vector<std::string> intersection =
      base::STLSetIntersection<std::vector<std::string>>(sortedLabels,
                                                         excludedLabels);
  return intersection.empty();
}

}  // namespace

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

  /// Whether SafariSwitcher should be checked on FRE.
  BOOL _shouldCheckSafariSwitcherOnFRE;
  /// Whether the NTP was shown in FRE.
  BOOL _hasEnteredNTPOnFRE;
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
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserverBridge.get());

    if (IsBottomOmniboxSteadyStateEnabled()) {
      std::string featureParam = base::GetFieldTrialParamValueByFeature(
          kBottomOmniboxDefaultSetting, kBottomOmniboxDefaultSettingParam);
      if (featureParam == kBottomOmniboxDefaultSettingParamSafariSwitcher) {
        // Device switcher data is not available in incognito.
        _shouldCheckSafariSwitcherOnFRE = !isIncognito && IsFirstRun();
      }
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
    [self updateOmniboxDefaultPosition];
    [self logOmniboxPosition];
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
  if (IsBottomOmniboxSteadyStateEnabled()) {
    if (_shouldCheckSafariSwitcherOnFRE) {
      [self checkSafariSwitcherOnFRE];
    }
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

#pragma mark Default omnibox position

/// Verifies if the user is a safari switcher on FRE.
- (void)checkSafariSwitcherOnFRE {
  CHECK(IsBottomOmniboxSteadyStateEnabled());
  CHECK(_shouldCheckSafariSwitcherOnFRE);
  CHECK(self.deviceSwitcherResultDispatcher);
  CHECK(self.prefService);

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
      if (ShouldSwitchOmniboxToBottom(result)) {
        self.prefService->SetDefaultPrefValue(prefs::kBottomOmnibox,
                                              base::Value(YES));
        self.prefService->SetBoolean(prefs::kBottomOmniboxByDefault, YES);
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
/// Used to set the default omnibox position to bottom for `IsNewUser` that are
/// not in FRE. If bottom omnibox is already default `bottomOmniboxIsDefault`,
/// still log the status as bottom as the user was classified as safari switcher
/// in a previous session.
- (BOOL)isSafariSwitcherAtStartup:(BOOL)bottomOmniboxIsDefault {
  CHECK(IsBottomOmniboxSteadyStateEnabled());
  CHECK(self.prefService);

  if (!IsNewUser()) {
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

  if (ShouldSwitchOmniboxToBottom(result)) {
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
  CHECK(IsBottomOmniboxSteadyStateEnabled());
  CHECK(self.prefService);

  // This only needs to be executed once and deviceSwitcherResult are not
  // available in incognito.
  if (!self.deviceSwitcherResultDispatcher ||
      self.prefService->GetUserPrefValue(prefs::kBottomOmnibox)) {
    return;
  }

  BOOL bottomOmniboxEnabledByDefault = NO;
  if (base::FeatureList::IsEnabled(kBottomOmniboxDefaultSetting)) {
    bottomOmniboxEnabledByDefault =
        self.prefService->GetBoolean(prefs::kBottomOmniboxByDefault);
  }

  std::string featureParam = base::GetFieldTrialParamValueByFeature(
      kBottomOmniboxDefaultSetting, kBottomOmniboxDefaultSettingParam);
  if (featureParam == kBottomOmniboxDefaultSettingParamBottom) {
    bottomOmniboxEnabledByDefault = YES;
  } else if (featureParam == kBottomOmniboxDefaultSettingParamSafariSwitcher) {
    if ([self isSafariSwitcherAtStartup:bottomOmniboxEnabledByDefault]) {
      bottomOmniboxEnabledByDefault = YES;
    }
  } else if (featureParam == kBottomOmniboxDefaultSettingParamTop) {
    bottomOmniboxEnabledByDefault = NO;
  }

  // Make sure that users who have already seen the bottom omnibox by default
  // keep it.
  if (bottomOmniboxEnabledByDefault) {
    self.prefService->SetBoolean(prefs::kBottomOmniboxByDefault, YES);
  }

  self.prefService->SetDefaultPrefValue(
      prefs::kBottomOmnibox, base::Value(bottomOmniboxEnabledByDefault));
}

/// Logs preferred omnibox position.
- (void)logOmniboxPosition {
  CHECK(IsBottomOmniboxSteadyStateEnabled());
  CHECK(self.prefService);

  static dispatch_once_t once;
  dispatch_once(&once, ^{
    BOOL isBottomOmnibox = self.prefService->GetBoolean(prefs::kBottomOmnibox);
    OmniboxPositionType positionType = isBottomOmnibox
                                           ? OmniboxPositionType::kBottom
                                           : OmniboxPositionType::kTop;
    base::UmaHistogramEnumeration(kOmniboxSteadyStatePositionAtStartup,
                                  positionType);

    if (self.prefService->GetUserPrefValue(prefs::kBottomOmnibox)) {
      base::UmaHistogramEnumeration(
          kOmniboxSteadyStatePositionAtStartupSelected, positionType);
    }
  });
}

@end
