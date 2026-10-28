// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/default_browser/coordinator/default_browser_screen_mediator.h"

#import <algorithm>
#import <optional>
#import <string>
#import <utility>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/metrics/metrics_reporting_choice_service.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/result.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/web_resource/web_resource_pref_names.h"
#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_screen_consumer.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/first_run/public/first_run_util.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/public/provider/chrome/browser/switcher_info/switcher_info_api.h"

namespace {

// Timeout for waiting for the device switcher signal before falling back to the
// standard Default Browser promo screen.
constexpr base::TimeDelta kDeviceSwitcherSignalWaitTimeout = base::Seconds(1);

// Returns true if `result` classifies the user as an Android switcher.
bool IsAndroidSwitcher(
    const segmentation_platform::ClassificationResult& result) {
  return result.status == segmentation_platform::PredictionStatus::kSucceeded &&
         !result.ordered_labels.empty() &&
         result.ordered_labels[0] ==
             segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel &&
         !std::ranges::contains(
             result.ordered_labels,
             segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel);
}

// Records the device switcher classification `result` and `elapsed` latency
// histograms.
void RecordDeviceSwitcherSignalMetrics(
    first_run::DefaultBrowserPromoSegmentationResult result,
    bool is_success,
    base::TimeDelta elapsed) {
  base::UmaHistogramMediumTimes(
      is_success
          ? first_run::kDefaultBrowserPromoSegmentationLatencySuccessHistogram
          : first_run::kDefaultBrowserPromoSegmentationLatencyFailureHistogram,
      elapsed);
  base::UmaHistogramEnumeration(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram, result);
}

}  // namespace

@implementation DefaultBrowserScreenMediator {
  // True if the sign-in screen will be the first screen in the FRE sequence.
  BOOL _firstScreenInFRESequence;
  // Local state pref service.
  raw_ptr<PrefService> _localState;
  // Identity manager used to check primary account sign-in state.
  raw_ptr<signin::IdentityManager> _identityManager;
  // Authentication service providing the signed-in `SystemIdentity`.
  raw_ptr<AuthenticationService> _authenticationService;
  // Dispatcher providing device switcher segmentation classification results.
  raw_ptr<segmentation_platform::DeviceSwitcherResultDispatcher>
      _deviceSwitcherResultDispatcher;
  // Timestamp captured when async device switcher signal evaluation begins.
  base::TimeTicks _evaluationStartTime;
  // Timer bounding the switcher info wait duration.
  base::OneShotTimer _switcherInfoTimer;
  // Whether promo evaluation has already notified `delegate`.
  BOOL _evaluationCompleted;
  // Whether `-disconnect` has been called.
  BOOL _disconnected;
}

#pragma mark - Public

- (instancetype)
           initWithIdentityManager:(signin::IdentityManager*)identityManager
             authenticationService:(AuthenticationService*)authenticationService
    deviceSwitcherResultDispatcher:
        (segmentation_platform::DeviceSwitcherResultDispatcher*)dispatcher {
  self = [super init];
  if (self) {
    _identityManager = identityManager;
    _authenticationService = authenticationService;
    _deviceSwitcherResultDispatcher = dispatcher;
    _UMAReportingUserChoice = kDefaultMetricsReportingCheckboxValue;
    _localState = GetApplicationContext()->GetLocalState();
    _firstScreenInFRESequence =
        !first_run::IsAnimatedDefaultBrowserPromoInFREEnabled() &&
        base::FeatureList::IsEnabled(first_run::kUpdatedFirstRunSequence) &&
        !_localState->GetBoolean(prefs::kEulaAccepted);
  }
  return self;
}

- (void)startPromoEvaluation {
  if (first_run::IsAnimatedDefaultBrowserPromoInFREEnabled() ||
      !_identityManager ||
      !_identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    [self notifyEvaluationFinishedWithAndroidSwitcherPromo:NO];
    return;
  }

  switch (
      first_run::GetAndroidSwitcherDefaultBrowserPromoInFREVariationType()) {
    case first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
        kDisabled:
      [self notifyEvaluationFinishedWithAndroidSwitcherPromo:NO];
      return;
    case first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
        kSegmentationSignal:
      [self evaluateSegmentationSignal];
      return;
    case first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
        kSwitcherInfoSignal:
      [self evaluateSwitcherInfoSignal];
      return;
  }
}

- (void)disconnect {
  _disconnected = YES;
  _switcherInfoTimer.Stop();
  _localState = nullptr;
  _identityManager = nullptr;
  _authenticationService = nullptr;
  _deviceSwitcherResultDispatcher = nullptr;
}

- (void)setConsumer:(id<DefaultBrowserScreenConsumer>)consumer {
  _consumer = consumer;
  if (!_consumer) {
    return;
  }
  if (_firstScreenInFRESequence) {
    _consumer.hasPlatformPolicies = HasPlatformPolicies();
    BOOL metricReportingDisabled = metrics::MetricsReportingChoiceService::
        IsMetricsReportingDisabledByPolicy(_localState);
    _consumer.screenIntent =
        metricReportingDisabled
            ? DefaultBrowserScreenConsumerScreenIntent::kTOSWithoutUMA
            : DefaultBrowserScreenConsumerScreenIntent::kTOSAndUMA;
  } else {
    _consumer.screenIntent = DefaultBrowserScreenConsumerScreenIntent::kDefault;
  }
}

- (void)finishPresenting {
  if (!_firstScreenInFRESequence || !_localState) {
    return;
  }
  if (self.TOSLinkWasTapped) {
    base::RecordAction(base::UserMetricsAction("MobileFreTOSLinkTapped"));
  }
  if (self.UMALinkWasTapped) {
    base::RecordAction(base::UserMetricsAction("MobileFreUMALinkTapped"));
  }
  _localState->SetBoolean(prefs::kEulaAccepted, true);
  _localState->SetBoolean(metrics::prefs::kMetricsReportingEnabled,
                          self.UMAReportingUserChoice);
  _localState->CommitPendingWrite();
}

#pragma mark - Private

// Queries the segmentation platform device switcher classification result and
// notifies `delegate` whether to present the Android switcher promo.
- (void)evaluateSegmentationSignal {
  std::string forcedSegment =
      experimental_flags::GetSegmentForForcedDeviceSwitcherExperience();
  if (!forcedSegment.empty()) {
    BOOL isAndroidSwitcher =
        forcedSegment ==
        segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel;
    [self notifyEvaluationFinishedWithAndroidSwitcherPromo:isAndroidSwitcher];
    return;
  }

  // The dispatcher can be null in tests where SegmentationPlatformService is
  // unavailable.
  if (!_deviceSwitcherResultDispatcher) {
    [self notifyEvaluationFinishedWithAndroidSwitcherPromo:NO];
    return;
  }

  _evaluationStartTime = base::TimeTicks::Now();
  __weak __typeof(self) weakSelf = self;
  _deviceSwitcherResultDispatcher->WaitForClassificationResult(
      kDeviceSwitcherSignalWaitTimeout,
      base::BindOnce(
          ^(const segmentation_platform::ClassificationResult& result) {
            [weakSelf onSegmentationResult:result];
          }));
}

// Handles the segmentation device switcher classification `result`, records
// metrics, and notifies `delegate`.
- (void)onSegmentationResult:
    (const segmentation_platform::ClassificationResult&)result {
  if (_disconnected || _evaluationCompleted) {
    return;
  }
  base::TimeDelta elapsed = base::TimeTicks::Now() - _evaluationStartTime;

  first_run::DefaultBrowserPromoSegmentationResult segmentationResult =
      first_run::DefaultBrowserPromoSegmentationResult::kNotReady;
  BOOL isSuccess = NO;
  switch (result.status) {
    case segmentation_platform::PredictionStatus::kNotReady:
      segmentationResult =
          first_run::DefaultBrowserPromoSegmentationResult::kNotReady;
      break;
    case segmentation_platform::PredictionStatus::kFailed:
      segmentationResult =
          first_run::DefaultBrowserPromoSegmentationResult::kFailed;
      break;
    case segmentation_platform::PredictionStatus::kSucceeded:
      if (result.ordered_labels.empty()) {
        segmentationResult =
            first_run::DefaultBrowserPromoSegmentationResult::kFailed;
      } else if (std::ranges::contains(
                     result.ordered_labels,
                     segmentation_platform::DeviceSwitcherModel::
                         kNotSyncedLabel)) {
        segmentationResult =
            first_run::DefaultBrowserPromoSegmentationResult::kNotReady;
      } else if (IsAndroidSwitcher(result)) {
        segmentationResult =
            first_run::DefaultBrowserPromoSegmentationResult::kAndroidSwitcher;
        isSuccess = YES;
      } else {
        segmentationResult = first_run::DefaultBrowserPromoSegmentationResult::
            kNotAndroidSwitcher;
        isSuccess = YES;
      }
      break;
  }

  RecordDeviceSwitcherSignalMetrics(segmentationResult, isSuccess, elapsed);
  BOOL showAndroidSwitcherPromo =
      segmentationResult ==
      first_run::DefaultBrowserPromoSegmentationResult::kAndroidSwitcher;
  [self notifyEvaluationFinishedWithAndroidSwitcherPromo:
            showAndroidSwitcherPromo];
}

// Queries the switcher info signal for the signed-in user and notifies
// `delegate` whether to present the Android switcher promo.
- (void)evaluateSwitcherInfoSignal {
  // `GetPrimaryIdentity()` resolves the primary account's Gaia ID via
  // `ChromeAccountManagerService`, which can return `nil` if the system
  // identity was removed from the device before `IdentityManager` finished
  // clearing the primary account.
  id<SystemIdentity> identity =
      _authenticationService ? _authenticationService->GetPrimaryIdentity()
                             : nil;
  if (!identity) {
    [self notifyEvaluationFinishedWithAndroidSwitcherPromo:NO];
    return;
  }

  _evaluationStartTime = base::TimeTicks::Now();
  __weak __typeof(self) weakSelf = self;
  _switcherInfoTimer.Start(
      FROM_HERE, kDeviceSwitcherSignalWaitTimeout, base::BindOnce(^{
        [weakSelf onSwitcherInfoResult:std::nullopt timedOut:YES];
      }));
  ios::provider::GetSwitcherInfo(
      identity, base::BindOnce(^(
                    std::optional<ios::provider::SwitcherInfoResult> result) {
        [weakSelf onSwitcherInfoResult:std::move(result) timedOut:NO];
      }));
}

// Handles the switcher info `result` (or timeout), records metrics, and
// notifies `delegate`.
- (void)onSwitcherInfoResult:
            (std::optional<ios::provider::SwitcherInfoResult>)result
                    timedOut:(BOOL)timedOut {
  if (_disconnected || _evaluationCompleted) {
    return;
  }
  _switcherInfoTimer.Stop();
  base::TimeDelta elapsed = base::TimeTicks::Now() - _evaluationStartTime;

  first_run::DefaultBrowserPromoSegmentationResult switcherInfoResult =
      timedOut ? first_run::DefaultBrowserPromoSegmentationResult::kNotReady
               : first_run::DefaultBrowserPromoSegmentationResult::kFailed;
  BOOL isSuccess = NO;
  if (!timedOut && result.has_value()) {
    switch (result->switcher_status) {
      case ios::provider::SwitcherStatus::kUnknown:
        switcherInfoResult =
            first_run::DefaultBrowserPromoSegmentationResult::kNotReady;
        break;
      case ios::provider::SwitcherStatus::kNotSwitcher:
        switcherInfoResult = first_run::DefaultBrowserPromoSegmentationResult::
            kNotAndroidSwitcher;
        isSuccess = YES;
        break;
      case ios::provider::SwitcherStatus::kSwitcher:
        switcherInfoResult =
            first_run::DefaultBrowserPromoSegmentationResult::kAndroidSwitcher;
        isSuccess = YES;
        break;
    }
  }

  RecordDeviceSwitcherSignalMetrics(switcherInfoResult, isSuccess, elapsed);
  BOOL showAndroidSwitcherPromo =
      switcherInfoResult ==
      first_run::DefaultBrowserPromoSegmentationResult::kAndroidSwitcher;
  [self notifyEvaluationFinishedWithAndroidSwitcherPromo:
            showAndroidSwitcherPromo];
}

// Notifies `delegate` with `showAndroidSwitcherPromo`.
- (void)notifyEvaluationFinishedWithAndroidSwitcherPromo:
    (BOOL)showAndroidSwitcherPromo {
  _evaluationCompleted = YES;
  [self.delegate defaultBrowserScreenMediator:self
      didFinishEvaluationWithAndroidSwitcherPromo:showAndroidSwitcherPromo];
}

@end
