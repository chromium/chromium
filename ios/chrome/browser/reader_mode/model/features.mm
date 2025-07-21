// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/features.h"

#import "base/feature_list.h"
#import "base/json/values_util.h"
#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

namespace {
// The default number of days to span for determining Reading Mode default
// browser eligibility.
constexpr int kReaderModeDefaultBrowserPromoNumDaysCriteria = 14;

// The default number of days a user should be active to display the default
// browser promo.
constexpr int kReaderModeDefaultBrowserPromoActiveDaysCriteria = 2;

// Name to configure the number of days a user should be active in Reading Mode
// to display a default browser promo.
const char kReaderModeDefaultBrowserActiveDaysCriteriaStringName[] =
    "reader-mode-default-browser-active-days";

// Name to configure the number of days to span for determining the Reading Mode
// default browser eligibility criteria.
const char kReaderModeDefaultBrowserNumDaysCriteriaStringName[] =
    "reader-mode-default-browser-num-days";

}  // namespace

BASE_FEATURE(kEnableReaderMode,
             "EnableReaderMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReadabilityHeuristic,
             "EnableReadabilityHeuristic",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModePageEligibilityForToolsMenu,
             "EnableReaderModePageEligibilityForToolsMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModeDebugInfo,
             "EnableReaderModeDebugInfo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModeDefaultBrowserPromo,
             "EnableReaderModeDefaultBrowserPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kReaderModeHeuristicPageLoadDelayDurationStringName[] =
    "reader-mode-heuristic-page-load-delay-duration-string";

const char kReaderModeDistillationTimeoutDurationStringName[] =
    "reader-mode-distillation-timeout-duration-string";

const base::TimeDelta ReaderModeDistillationTimeout() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kEnableReaderMode,
      /*name=*/kReaderModeDistillationTimeoutDurationStringName,
      /*default_value=*/kReaderModeDistillationTimeout);
}

const base::TimeDelta ReaderModeHeuristicPageLoadDelay() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kEnableReaderMode,
      /*name=*/kReaderModeHeuristicPageLoadDelayDurationStringName,
      /*default_value=*/kReaderModeHeuristicPageLoadDelay);
}

bool IsReaderModeAvailable() {
  return base::FeatureList::IsEnabled(kEnableReaderMode);
}

bool IsReaderModeSnackbarEnabled() {
  return base::FeatureList::IsEnabled(kEnableReaderModeDebugInfo);
}

int ReaderModeDefaultBrowserActiveDaysCriteria() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kEnableReaderModeDefaultBrowserPromo,
      /*name=*/kReaderModeDefaultBrowserActiveDaysCriteriaStringName,
      /*default_value=*/kReaderModeDefaultBrowserPromoActiveDaysCriteria);
}

int ReaderModeDefaultBrowserNumDaysCriteria() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kEnableReaderModeDefaultBrowserPromo,
      /*name=*/kReaderModeDefaultBrowserNumDaysCriteriaStringName,
      /*default_value=*/kReaderModeDefaultBrowserPromoNumDaysCriteria);
}
