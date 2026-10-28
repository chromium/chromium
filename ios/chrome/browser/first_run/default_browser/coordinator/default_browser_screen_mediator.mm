// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/default_browser/coordinator/default_browser_screen_mediator.h"

#import <algorithm>
#import <string>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
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
  // Dispatcher providing device switcher segmentation classification results.
  raw_ptr<segmentation_platform::DeviceSwitcherResultDispatcher>
      _deviceSwitcherResultDispatcher;
  // Timestamp captured when async device switcher signal evaluation begins.
  base::TimeTicks _evaluationStartTime;
  // Whether `-disconnect` has been called.
  BOOL _disconnected;
}

#pragma mark - Public

- (instancetype)
           initWithIdentityManager:(signin::IdentityManager*)identityManager
    deviceSwitcherResultDispatcher:
        (segmentation_platform::DeviceSwitcherResultDispatcher*)dispatcher {
  self = [super init];
  if (self) {
    _identityManager = identityManager;
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
      !_identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin) ||
      first_run::GetAndroidSwitcherDefaultBrowserPromoInFREVariationType() !=
          first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
              kSegmentationSignal) {
    [self notifyEvaluationFinishedWithAndroidSwitcherPromo:NO];
    return;
  }

  [self evaluateSegmentationSignal];
}

- (void)disconnect {
  _disconnected = YES;
  _localState = nullptr;
  _identityManager = nullptr;
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
  if (_disconnected) {
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

// Notifies `delegate` with `showAndroidSwitcherPromo`.
- (void)notifyEvaluationFinishedWithAndroidSwitcherPromo:
    (BOOL)showAndroidSwitcherPromo {
  [self.delegate defaultBrowserScreenMediator:self
      didFinishEvaluationWithAndroidSwitcherPromo:showAndroidSwitcherPromo];
}

@end
