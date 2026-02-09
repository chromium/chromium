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
  static const bool is_us_country_code = [] {
    return base::ToLowerASCII(GetCurrentCountryCode(
               GetApplicationContext()->GetVariationsService())) == "us";
  }();
  return is_us_country_code;
}

}  // namespace

BASE_FEATURE(kEnableReaderModeInUS, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModeOmniboxEntryPointInUS,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModeTranslationWithInfobar,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReadabilityHeuristic, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModeOptimizationGuideEligibility,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableContentSettingsOptionForLinks,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsReaderModeAvailable() {
  if (IsUSCountryCode() &&
      !experimental_flags::ShouldIgnoreDeviceLocaleConditions()) {
    return base::FeatureList::IsEnabled(kEnableReaderModeInUS);
  }
  return true;
}

bool IsReaderModeOmniboxEntryPointEnabled() {
  if (IsUSCountryCode() &&
      !experimental_flags::ShouldIgnoreDeviceLocaleConditions()) {
    return base::FeatureList::IsEnabled(kEnableReaderModeOmniboxEntryPointInUS);
  }
  return true;
}

bool IsReaderModeOptimizationGuideEligibilityAvailable() {
  return base::FeatureList::IsEnabled(
      kEnableReaderModeOptimizationGuideEligibility);
}

bool IsReaderModeContentSettingsForLinkEnabled() {
  return base::FeatureList::IsEnabled(kEnableContentSettingsOptionForLinks);
}
