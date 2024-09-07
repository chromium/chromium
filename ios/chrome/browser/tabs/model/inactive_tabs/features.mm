// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"

#import "base/metrics/field_trial_params.h"
#import "base/strings/string_number_conversions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ui/base/device_form_factor.h"

const int kInactiveTabsDisabledByUser = -1;

BASE_FEATURE(kTabInactivityThreshold,
             "TabInactivityThreshold",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kTabInactivityThresholdParameterName[] = "variant";
const char kTabInactivityThresholdOneWeekParam[] =
    "tab-inactivity-threshold-one-week";
const char kTabInactivityThresholdTwoWeeksParam[] =
    "tab-inactivity-threshold-two-weeks";
const char kTabInactivityThresholdThreeWeeksParam[] =
    "tab-inactivity-threshold-three-weeks";
const char kTabInactivityThresholdOneMinuteDemoParam[] =
    "tab-inactivity-threshold-one-minute-demo";
const char kTabInactivityThresholdImmediateDemoParam[] =
    "tab-inactivity-threshold-immediate-demo";

bool IsInactiveTabsAvailable() {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return false;
  }

  return base::FeatureList::IsEnabled(kTabInactivityThreshold);
}

bool IsInactiveTabsEnabled() {
  if (!IsInactiveTabsAvailable()) {
    return false;
  }

  return !IsInactiveTabsExplictlyDisabledByUser();
}

bool IsInactiveTabsExplictlyDisabledByUser() {
  CHECK(IsInactiveTabsAvailable());
  return GetApplicationContext()->GetLocalState()->GetInteger(
             prefs::kInactiveTabsTimeThreshold) == kInactiveTabsDisabledByUser;
}

const base::TimeDelta InactiveTabsTimeThreshold() {
  CHECK(IsInactiveTabsAvailable());

  // Preference.
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int user_preference_threshold =
      local_state->GetInteger(prefs::kInactiveTabsTimeThreshold);
  if (user_preference_threshold > 0) {
    return base::Days(user_preference_threshold);
  }

  // Feature flag.
  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kTabInactivityThreshold, kTabInactivityThresholdParameterName);
  if (feature_param == kTabInactivityThresholdOneWeekParam) {
    return base::Days(7);
  } else if (feature_param == kTabInactivityThresholdTwoWeeksParam) {
    return base::Days(14);
  } else if (feature_param == kTabInactivityThresholdThreeWeeksParam) {
    return base::Days(21);
  } else if (feature_param == kTabInactivityThresholdOneMinuteDemoParam) {
    return base::Minutes(1);
  } else if (feature_param == kTabInactivityThresholdImmediateDemoParam) {
    return base::Seconds(0);
  }
  return base::Days(21);
}

BASE_FEATURE(kInactiveTabButtonRefactoring,
             "InactiveTabButtonRefactoring",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsInactiveTabButtonRefactoringEnabled() {
  if (!IsInactiveTabsAvailable()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kInactiveTabButtonRefactoring);
}
