// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/user_population.h"

#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/sync/driver/sync_service.h"
#include "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using safe_browsing::ChromeUserPopulation;

ChromeUserPopulation GetUserPopulation(ChromeBrowserState* browser_state) {
  ChromeUserPopulation population;
  if (browser_state->GetPrefs()) {
    const PrefService& prefs = *browser_state->GetPrefs();
    if (safe_browsing::IsEnhancedProtectionEnabled(prefs)) {
      population.set_user_population(ChromeUserPopulation::ENHANCED_PROTECTION);
    } else if (safe_browsing::IsExtendedReportingEnabled(prefs)) {
      population.set_user_population(ChromeUserPopulation::EXTENDED_REPORTING);
    } else if (safe_browsing::IsSafeBrowsingEnabled(prefs)) {
      population.set_user_population(ChromeUserPopulation::SAFE_BROWSING);
    }

    population.set_is_mbb_enabled(prefs.GetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  }

  population.set_is_incognito(browser_state->IsOffTheRecord());

  syncer::SyncService* sync =
      SyncServiceFactory::GetForBrowserState(browser_state);
  bool is_history_sync_enabled =
      sync && sync->IsSyncFeatureActive() && !sync->IsLocalSyncEnabled() &&
      sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
  population.set_is_history_sync_enabled(is_history_sync_enabled);

  population.set_profile_management_status(
      safe_browsing::GetProfileManagementStatus(
          GetApplicationContext()->GetBrowserPolicyConnector()));

  return population;
}
