// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"

#import "base/metrics/field_trial_params.h"
#import "base/strings/string_number_conversions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ui/base/device_form_factor.h"

const int kInactiveTabsDisabledByUser = -1;

bool IsInactiveTabsExplicitlyDisabledByUser(PrefService* prefs) {
  return IsInactiveTabsExplicitlyDisabledByUser(
      prefs->GetInteger(prefs::kInactiveTabsTimeThreshold));
}

bool IsInactiveTabsExplicitlyDisabledByUser(int raw_threshold_value) {
  return raw_threshold_value == kInactiveTabsDisabledByUser;
}

const base::TimeDelta InactiveTabsTimeThreshold(PrefService* prefs) {
  if (experimental_flags::ShouldUseInactiveTabsTestThreshold()) {
    return base::Seconds(0);
  }

  if (experimental_flags::ShouldUseInactiveTabsDemoThreshold()) {
    return base::Minutes(1);
  }

  // Preference.
  int user_preference_threshold =
      prefs->GetInteger(prefs::kInactiveTabsTimeThreshold);
  if (user_preference_threshold > 0) {
    return base::Days(user_preference_threshold);
  }

  return base::Days(21);
}
