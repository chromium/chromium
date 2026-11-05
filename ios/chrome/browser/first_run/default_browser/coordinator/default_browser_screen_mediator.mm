// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/default_browser/coordinator/default_browser_screen_mediator.h"

#import <algorithm>
#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/barrier_callback.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
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
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
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

// Resolves the segmentation `result` into a
// `DefaultBrowserPromoSegmentationResult`.
first_run::DefaultBrowserPromoSegmentationResult ResolveSegmentationResult(
    const segmentation_platform::ClassificationResult& result) {
  switch (result.status) {
    case segmentation_platform::PredictionStatus::kNotReady:
      return first_run::DefaultBrowserPromoSegmentationResult::kNotReady;
    case segmentation_platform::PredictionStatus::kFailed:
      return first_run::DefaultBrowserPromoSegmentationResult::kFailed;
    case segmentation_platform::PredictionStatus::kSucceeded:
      if (result.ordered_labels.empty()) {
        return first_run::DefaultBrowserPromoSegmentationResult::kFailed;
      }
      if (std::ranges::contains(
              result.ordered_labels,
              segmentation_platform::DeviceSwitcherModel::kNotSyncedLabel)) {
        return first_run::DefaultBrowserPromoSegmentationResult::kNotReady;
      }
      return IsAndroidSwitcher(result)
                 ? first_run::DefaultBrowserPromoSegmentationResult::
                       kAndroidSwitcher
                 : first_run::DefaultBrowserPromoSegmentationResult::
                       kNotAndroidSwitcher;
  }
}

// Resolves the aggregated switcher info `results` across identities into a
// `DefaultBrowserPromoSegmentationResult`.
first_run::DefaultBrowserPromoSegmentationResult ResolveSwitcherInfoResult(
    const std::vector<std::optional<ios::provider::SwitcherInfoResult>>&
        results) {
  bool has_not_switcher = false;
  bool has_unknown = false;
  for (const std::optional<ios::provider::SwitcherInfoResult>& result :
       results) {
    if (!result.has_value()) {
      continue;
    }
    switch (result->switcher_status) {
      case ios::provider::SwitcherStatus::kSwitcher:
        return first_run::DefaultBrowserPromoSegmentationResult::
            kAndroidSwitcher;
      case ios::provider::SwitcherStatus::kNotSwitcher:
        has_not_switcher = true;
        break;
      case ios::provider::SwitcherStatus::kUnknown:
        has_unknown = true;
        break;
    }
  }
  if (has_not_switcher) {
    return first_run::DefaultBrowserPromoSegmentationResult::
        kNotAndroidSwitcher;
  }
  return has_unknown
             ? first_run::DefaultBrowserPromoSegmentationResult::kNotReady
             : first_run::DefaultBrowserPromoSegmentationResult::kFailed;
}

// Handles a single identity's `SwitcherInfoResult`, invoking
// `on_switcher_found` immediately on a `kSwitcher` match and forwarding
// `result` to `barrier_callback`.
void OnIdentitySwitcherInfoResult(
    base::OnceClosure on_switcher_found,
    base::OnceCallback<void(std::optional<ios::provider::SwitcherInfoResult>)>
        barrier_callback,
    std::optional<ios::provider::SwitcherInfoResult> result) {
  if (result.has_value() &&
      result->switcher_status == ios::provider::SwitcherStatus::kSwitcher) {
    std::move(on_switcher_found).Run();
  }
  std::move(barrier_callback).Run(std::move(result));
}

// Records the device switcher classification `result` and `elapsed` latency
// histograms.
void RecordDeviceSwitcherSignalMetrics(
    first_run::DefaultBrowserPromoSegmentationResult result,
    base::TimeDelta elapsed) {
  bool is_success =
      result ==
          first_run::DefaultBrowserPromoSegmentationResult::kAndroidSwitcher ||
      result ==
          first_run::DefaultBrowserPromoSegmentationResult::kNotAndroidSwitcher;
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
  // Authentication service for the profile.
  raw_ptr<AuthenticationService> _authenticationService;
  // Account manager service providing on-device identities for the profile.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
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
     initWithAuthenticationService:(AuthenticationService*)authenticationService
             accountManagerService:
                 (ChromeAccountManagerService*)accountManagerService
    deviceSwitcherResultDispatcher:
        (segmentation_platform::DeviceSwitcherResultDispatcher*)dispatcher {
  self = [super init];
  if (self) {
    _authenticationService = authenticationService;
    _accountManagerService = accountManagerService;
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
      !_accountManagerService || !_accountManagerService->HasIdentities()) {
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
  _authenticationService = nullptr;
  _accountManagerService = nullptr;
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

// Completes signal evaluation and records metrics for a forced device switcher
// segment.
- (void)completeForcedDeviceSwitcherEvaluation:(BOOL)isAndroidSwitcher {
  _evaluationStartTime = base::TimeTicks::Now();
  first_run::DefaultBrowserPromoSegmentationResult result =
      isAndroidSwitcher
          ? first_run::DefaultBrowserPromoSegmentationResult::kAndroidSwitcher
          : first_run::DefaultBrowserPromoSegmentationResult::
                kNotAndroidSwitcher;
  [self completeSignalEvaluationWithResult:result];
}

// Queries the segmentation platform device switcher classification result and
// notifies `delegate` whether to present the Android switcher promo.
- (void)evaluateSegmentationSignal {
  if (std::optional<bool> isAndroidSwitcher =
          experimental_flags::IsForcedDeviceSwitcherAndroidPhone()) {
    [self completeForcedDeviceSwitcherEvaluation:*isAndroidSwitcher];
    return;
  }

  // Segmentation classification relies on Sync DeviceInfo, which requires a
  // signed-in primary account. The dispatcher can also be null in tests where
  // SegmentationPlatformService is unavailable.
  if (!_authenticationService ||
      !_authenticationService->HasPrimaryIdentity() ||
      !_deviceSwitcherResultDispatcher) {
    [self notifyEvaluationFinishedWithAndroidSwitcherPromo:NO];
    return;
  }

  _evaluationStartTime = base::TimeTicks::Now();
  __weak __typeof(self) weakSelf = self;
  _deviceSwitcherResultDispatcher->WaitForClassificationResult(
      kDeviceSwitcherSignalWaitTimeout,
      base::BindOnce(^(
          const segmentation_platform::ClassificationResult& result) {
        [weakSelf completeSignalEvaluationWithResult:ResolveSegmentationResult(
                                                         result)];
      }));
}

// Queries the switcher info signal across on-device identities in the profile
// and notifies `delegate` whether to present the Android switcher promo.
- (void)evaluateSwitcherInfoSignal {
  if (std::optional<bool> isAndroidSwitcher =
          experimental_flags::IsForcedDeviceSwitcherAndroidPhone()) {
    [self completeForcedDeviceSwitcherEvaluation:*isAndroidSwitcher];
    return;
  }

  NSArray<id<SystemIdentity>>* identities =
      _accountManagerService ? _accountManagerService->GetAllIdentities() : nil;
  if (identities.count == 0) {
    [self notifyEvaluationFinishedWithAndroidSwitcherPromo:NO];
    return;
  }

  _evaluationStartTime = base::TimeTicks::Now();
  __weak __typeof(self) weakSelf = self;
  _switcherInfoTimer.Start(
      FROM_HERE, kDeviceSwitcherSignalWaitTimeout, base::BindOnce(^{
        [weakSelf
            completeSignalEvaluationWithResult:
                first_run::DefaultBrowserPromoSegmentationResult::kNotReady];
      }));
  base::RepeatingCallback<void(
      std::optional<ios::provider::SwitcherInfoResult>)>
      barrierCallback = base::BarrierCallback<
          std::optional<ios::provider::SwitcherInfoResult>>(
          identities.count,
          base::BindOnce(^(
              std::vector<std::optional<ios::provider::SwitcherInfoResult>>
                  results) {
            [weakSelf
                completeSignalEvaluationWithResult:ResolveSwitcherInfoResult(
                                                       results)];
          }));
  base::RepeatingClosure onSwitcherFound = base::BindRepeating(^{
    [weakSelf
        completeSignalEvaluationWithResult:
            first_run::DefaultBrowserPromoSegmentationResult::kAndroidSwitcher];
  });
  for (id<SystemIdentity> identity in identities) {
    ios::provider::GetSwitcherInfo(
        identity, base::BindOnce(&OnIdentitySwitcherInfoResult, onSwitcherFound,
                                 barrierCallback));
  }
}

// Completes async device switcher signal evaluation with `result`, records
// metrics, and notifies `delegate`.
- (void)completeSignalEvaluationWithResult:
    (first_run::DefaultBrowserPromoSegmentationResult)result {
  if (_disconnected || _evaluationCompleted) {
    return;
  }
  _switcherInfoTimer.Stop();
  base::TimeDelta elapsed = base::TimeTicks::Now() - _evaluationStartTime;
  RecordDeviceSwitcherSignalMetrics(result, elapsed);
  BOOL showAndroidSwitcherPromo =
      result ==
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
