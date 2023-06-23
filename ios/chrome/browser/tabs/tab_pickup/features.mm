// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_pickup/features.h"

#import "base/metrics/field_trial_params.h"
#import "base/strings/string_number_conversions.h"
#import "base/time/time.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BASE_FEATURE(kTabPickupThreshold,
             "TabPikcupThreshold",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kTabPickupThresholdParameterName[] = "variant";
const char kTabPickupThresholdTenMinutesParam[] =
    "tab-pickup-threshold-ten-minutes";
const char kTabPickupThresholdOneHourParam[] = "tab-pickup-threshold-one-hour";
const char kTabPickupThresholdTwoHoursParam[] =
    "tab-pickup-threshold-two-hours";

bool IsTabPickupEnabled() {
  return base::FeatureList::IsEnabled(kTabPickupThreshold);
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
