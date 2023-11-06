// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tab_pickup/features.h"

#import "base/metrics/field_trial_params.h"
#import "base/strings/string_number_conversions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

BASE_FEATURE(kTabPickupThreshold,
             "TabPickupThreshold",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabPickupMinimumDelay,
             "TabPickupMinimumDelay",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kTabPickupThresholdParameterName[] = "variant";
const char kTabPickupThresholdTenMinutesParam[] =
    "tab-pickup-threshold-ten-minutes";
const char kTabPickupThresholdOneHourParam[] = "tab-pickup-threshold-one-hour";
const char kTabPickupThresholdTwoHoursParam[] =
    "tab-pickup-threshold-two-hours";

bool IsTabPickupEnabled() {
  return base::FeatureList::IsEnabled(kTabPickupThreshold);
}

bool IsTabPickupMinimumDelayEnabled() {
  CHECK(IsTabPickupEnabled());
  return base::FeatureList::IsEnabled(kTabPickupMinimumDelay);
}

const base::TimeDelta TabPickupTimeThreshold() {
  CHECK(IsTabPickupEnabled());

  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kTabPickupThreshold, kTabPickupThresholdParameterName);
  if (feature_param == kTabPickupThresholdOneHourParam) {
    return base::Hours(1);
  } else if (feature_param == kTabPickupThresholdTwoHoursParam) {
    return base::Hours(2);
  }
  return base::Minutes(10);
}

bool IsTabPickupDisabledByUser() {
  CHECK(IsTabPickupEnabled());
  return !GetApplicationContext()->GetLocalState()->GetBoolean(
      prefs::kTabPickupEnabled);
}
