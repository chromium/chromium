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
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ui/base/device_form_factor.h"

const int kInactiveTabsDisabledByUser = -1;

BASE_FEATURE(kInactiveTabsIPadFeature,
             "InactiveTabsIPadFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsInactiveTabsAvailable() {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    return true;
  }

  return base::FeatureList::IsEnabled(kInactiveTabsIPadFeature);
}

bool IsInactiveTabsEnabled() {
  if (!IsInactiveTabsAvailable()) {
    return false;
  }

  return !IsInactiveTabsExplicitlyDisabledByUser();
}

bool IsInactiveTabsExplicitlyDisabledByUser() {
  CHECK(IsInactiveTabsAvailable());
  return GetApplicationContext()->GetLocalState()->GetInteger(
             prefs::kInactiveTabsTimeThreshold) == kInactiveTabsDisabledByUser;
}

const base::TimeDelta InactiveTabsTimeThreshold() {
  CHECK(IsInactiveTabsAvailable());

  if (experimental_flags::ShouldUseInactiveTabsTestThreshold()) {
    return base::Seconds(0);
  }

  if (experimental_flags::ShouldUseInactiveTabsDemoThreshold()) {
    return base::Minutes(1);
  }

  // Preference.
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int user_preference_threshold =
      local_state->GetInteger(prefs::kInactiveTabsTimeThreshold);
  if (user_preference_threshold > 0) {
    return base::Days(user_preference_threshold);
  }

  return base::Days(21);
}
