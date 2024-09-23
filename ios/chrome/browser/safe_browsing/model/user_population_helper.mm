// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/user_population_helper.h"

#import "components/safe_browsing/core/browser/sync/sync_utils.h"
#import "components/safe_browsing/core/browser/user_population.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

safe_browsing::ChromeUserPopulation GetUserPopulationForProfile(
    ProfileIOS* profile) {
  syncer::SyncService* sync = SyncServiceFactory::GetForProfile(profile);
  bool is_history_sync_active =
      sync && !sync->IsLocalSyncEnabled() &&
      sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  bool is_signed_in =
      identity_manager &&
      safe_browsing::SyncUtils::IsPrimaryAccountSignedIn(identity_manager);
  return safe_browsing::GetUserPopulation(
      profile->GetPrefs(), profile->IsOffTheRecord(), is_history_sync_active,
      is_signed_in,
      /*is_under_advanced_protection=*/false,
      GetApplicationContext()->GetBrowserPolicyConnector(),
      /*num_profiles=*/std::nullopt,
      /*num_loaded_profiles=*/std::nullopt,
      /*num_open_profiles=*/std::nullopt);
}
