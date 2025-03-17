// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_mediator.h"

#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/result.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/web_resource/web_resource_pref_names.h"
#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_consumer.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ui/base/l10n/l10n_util.h"

@implementation DefaultBrowserScreenMediator {
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      _segmentationService;
  raw_ptr<segmentation_platform::DeviceSwitcherResultDispatcher>
      _deviceSwitcherResultDispatcher;
  segmentation_platform::DefaultBrowserUserSegment _userSegment;
  // True if the sign-in screen will be the first screen in the FRE sequence.
  BOOL _firstScreenInFRESequence;
  // Local state pref service.
  raw_ptr<PrefService> _localState;
}

#pragma mark - Public

- (instancetype)initWithSegmentationService:
                    (segmentation_platform::SegmentationPlatformService*)
                        segmentationService
             deviceSwitcherResultDispatcher:
                 (segmentation_platform::DeviceSwitcherResultDispatcher*)
                     dispatcher {
  self = [super init];
  if (self) {
    _segmentationService = segmentationService;
    _deviceSwitcherResultDispatcher = dispatcher;
    _UMAReportingUserChoice = kDefaultMetricsReportingCheckboxValue;
    _localState = GetApplicationContext()->GetLocalState();
    _firstScreenInFRESequence = !_localState->GetBoolean(prefs::kEulaAccepted);
    if (IsSegmentedDefaultBrowserPromoEnabled()) {
      [self retrieveUserSegment];
    }
  }
  return self;
}

- (void)disconnect {
  _segmentationService = nullptr;
  _deviceSwitcherResultDispatcher = nullptr;
  _localState = nullptr;
}

- (void)setConsumer:(id<DefaultBrowserScreenConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    // Sets the Default Browser screen view title to the consumer with targeted
    // messaging based on the user's segment.
    if (IsSegmentedDefaultBrowserPromoEnabled()) {
      [_consumer setPromoTitle:l10n_util::GetNSString(
                                   GetFirstRunDefaultBrowserScreenTitleStringID(
                                       _userSegment))];
      [_consumer
          setPromoSubtitle:l10n_util::GetNSString(
                               GetFirstRunDefaultBrowserScreenSubtitleStringID(
                                   _userSegment))];
    }

    if (_firstScreenInFRESequence) {
      _consumer.hasPlatformPolicies = HasPlatformPolicies();
      BOOL metricReportingDisabled =
          _localState->IsManagedPreference(
              metrics::prefs::kMetricsReportingEnabled) &&
          !_localState->GetBoolean(metrics::prefs::kMetricsReportingEnabled);
      _consumer.screenIntent =
          metricReportingDisabled
              ? DefaultBrowserScreenConsumerScreenIntent::kTOSWithoutUMA
              : DefaultBrowserScreenConsumerScreenIntent::kTOSAndUMA;
    } else {
      _consumer.screenIntent =
          DefaultBrowserScreenConsumerScreenIntent::kDefault;
    }
  }
}

- (void)finishPresenting {
  if (_firstScreenInFRESequence) {
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
}

#pragma mark - Private

// Retrieves user segmentation data from the Segmentation Platform.
- (void)retrieveUserSegment {
  CHECK(_segmentationService);
  CHECK(_deviceSwitcherResultDispatcher);

  __weak __typeof(self) weakSelf = self;
  _deviceSwitcherResultDispatcher->WaitForClassificationResult(
      segmentation_platform::kDeviceSwitcherWaitTimeout,
      base::BindOnce(^(const segmentation_platform::ClassificationResult&
                           deviceSwitcherResult) {
        [weakSelf
            didReceiveDeviceSwitcherSegmentationResult:deviceSwitcherResult];
      }));
}

// Sets the user's highest priority targeted segment retrieved from the
// Segmentation Platform.
- (void)didReceiveDeviceSwitcherSegmentationResult:
    (const segmentation_platform::ClassificationResult&)result {
  _userSegment = GetDefaultBrowserUserSegment(&result, nullptr);
}

@end
