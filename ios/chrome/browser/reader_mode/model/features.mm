// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/features.h"

#import "base/feature_list.h"
#import "base/json/values_util.h"
#import "base/metrics/field_trial_params.h"
#import "base/strings/string_util.h"
#import "components/variations/service/variations_service_utils.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

namespace {

// Returns whether the user's current country code is US.
bool IsUSCountryCode() {
  return base::ToLowerASCII(GetCurrentCountryCode(
             GetApplicationContext()->GetVariationsService())) == "us";
}

}  // namespace

BASE_FEATURE(kEnableReaderMode, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModeInUS, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModeOmniboxEntryPoint,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModeTranslation, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModeTranslationWithInfobar,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReadabilityHeuristic, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModePageEligibilityForToolsMenu,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModeOptimizationGuideEligibility,
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
  static const bool is_reader_mode_available = [] {
    if (IsDiamondPrototypeEnabled()) {
      return true;
    }
    if (IsUSCountryCode() &&
        !experimental_flags::ShouldIgnoreDeviceLocaleConditions()) {
      return base::FeatureList::IsEnabled(kEnableReaderMode) &&
             base::FeatureList::IsEnabled(kEnableReaderModeInUS);
    }
    return base::FeatureList::IsEnabled(kEnableReaderMode);
  }();
  return is_reader_mode_available;
}

bool IsReaderModeOmniboxEntryPointEnabled() {
  return base::FeatureList::IsEnabled(kEnableReaderModeOmniboxEntryPoint);
}

bool IsReaderModeTranslationAvailable() {
  return base::FeatureList::IsEnabled(kEnableReaderModeTranslation) ||
         base::FeatureList::IsEnabled(kEnableReaderModeTranslationWithInfobar);
}

bool IsReaderModeOptimizationGuideEligibilityAvailable() {
  return base::FeatureList::IsEnabled(
      kEnableReaderModeOptimizationGuideEligibility);
}
