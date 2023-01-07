// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/user_population_helper.h"

#import "components/safe_browsing/core/browser/sync/sync_utils.h"
#import "components/safe_browsing/core/browser/user_population.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

safe_browsing::ChromeUserPopulation GetUserPopulationForBrowserState(
    ChromeBrowserState* browser_state) {
  syncer::SyncService* sync =
      SyncServiceFactory::GetForBrowserState(browser_state);
  bool is_history_sync_enabled =
      sync && sync->IsSyncFeatureActive() && !sync->IsLocalSyncEnabled() &&
      sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  bool is_signed_in =
      identity_manager &&
      safe_browsing::SyncUtils::IsPrimaryAccountSignedIn(identity_manager);
  return safe_browsing::GetUserPopulation(
      browser_state->GetPrefs(), browser_state->IsOffTheRecord(),
      is_history_sync_enabled, is_signed_in,
      /*is_under_advanced_protection=*/false,
      GetApplicationContext()->GetBrowserPolicyConnector(),
      /*num_profiles=*/absl::nullopt,
      /*num_loaded_profiles=*/absl::nullopt,
      /*num_open_profiles=*/absl::nullopt);
}
