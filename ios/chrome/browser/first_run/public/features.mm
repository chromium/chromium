// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/public/features.h"

#import "base/metrics/field_trial_params.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/first_run/ui_bundled/welcome_back/model/welcome_back_prefs.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

bool IsWelcomeBackInFirstRunEnabled() {
  return base::FeatureList::IsEnabled(first_run::kWelcomeBackInFirstRun) &&
         !base::FeatureList::IsEnabled(
             first_run::kBestFeaturesScreenInFirstRun);
}

void MarkWelcomeBackFeatureUsed(BestFeaturesItemType item_type) {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int pref = static_cast<int>(item_type);
  ScopedListPrefUpdate update(local_state, kWelcomeBackEligibleItems);
  update->EraseValue(base::Value(pref));
}
