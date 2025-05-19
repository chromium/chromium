// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_mediator.h"

#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/web_resource/web_resource_pref_names.h"
#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_consumer.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ui/base/l10n/l10n_util.h"

@implementation DefaultBrowserScreenMediator {
  // True if the sign-in screen will be the first screen in the FRE sequence.
  BOOL _firstScreenInFRESequence;
  // Local state pref service.
  raw_ptr<PrefService> _localState;
}

#pragma mark - Public

- (instancetype)init {
  self = [super init];
  if (self) {
    _UMAReportingUserChoice = kDefaultMetricsReportingCheckboxValue;
    _localState = GetApplicationContext()->GetLocalState();
    _firstScreenInFRESequence = !_localState->GetBoolean(prefs::kEulaAccepted);
  }
  return self;
}

- (void)disconnect {
  _localState = nullptr;
}

- (void)setConsumer:(id<DefaultBrowserScreenConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
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

@end
