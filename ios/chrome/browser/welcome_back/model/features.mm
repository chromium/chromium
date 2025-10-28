// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/welcome_back/model/features.h"

#import "base/metrics/field_trial_params.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/welcome_back/model/welcome_back_prefs.h"

BASE_FEATURE(kWelcomeBack, base::FEATURE_DISABLED_BY_DEFAULT);

const char kWelcomeBackParam[] = "WelcomeBackParam";

bool IsWelcomeBackEnabled() {
  return base::FeatureList::IsEnabled(kWelcomeBack) &&
         !base::FeatureList::IsEnabled(
             first_run::kBestFeaturesScreenInFirstRun);
}

void MarkWelcomeBackFeatureUsed(BestFeaturesItemType item_type) {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int pref = static_cast<int>(item_type);
  ScopedListPrefUpdate update(local_state, kWelcomeBackEligibleItems);
  update->EraseValue(base::Value(pref));
}

WelcomeBackScreenVariationType GetWelcomeBackScreenVariationType() {
  if (!base::FeatureList::IsEnabled(kWelcomeBack)) {
    return WelcomeBackScreenVariationType::kDisabled;
  }
  return static_cast<WelcomeBackScreenVariationType>(
      base::GetFieldTrialParamByFeatureAsInt(kWelcomeBack, kWelcomeBackParam,
                                             1));
}
