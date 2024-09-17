// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unified_consent/model/unified_consent_service_factory.h"

#import <string>
#import <vector>

#import "base/no_destructor.h"
#import "components/sync/service/sync_service.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/unified_consent/unified_consent_metrics.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

UnifiedConsentServiceFactory::UnifiedConsentServiceFactory()
    : ProfileKeyedServiceFactoryIOS("UnifiedConsentService",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

UnifiedConsentServiceFactory::~UnifiedConsentServiceFactory() = default;

// static
unified_consent::UnifiedConsentService*
UnifiedConsentServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<unified_consent::UnifiedConsentService>(
          profile, /*create=*/true);
}

// static
unified_consent::UnifiedConsentService*
UnifiedConsentServiceFactory::GetForProfileIfExists(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<unified_consent::UnifiedConsentService>(
          profile, /*create=*/false);
}

// static
UnifiedConsentServiceFactory* UnifiedConsentServiceFactory::GetInstance() {
  static base::NoDestructor<UnifiedConsentServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
UnifiedConsentServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  sync_preferences::PrefServiceSyncable* user_pref_service =
      profile->GetSyncablePrefs();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);

  // Record settings for pre- and post-UnifiedConsent users.
  unified_consent::metrics::RecordSettingsHistogram(user_pref_service);

  // List of synced prefs that can be configured during the settings opt-in
  // flow.
  std::vector<std::string> synced_service_pref_names;
  synced_service_pref_names.push_back(prefs::kSearchSuggestEnabled);

  return std::make_unique<unified_consent::UnifiedConsentService>(
      user_pref_service, identity_manager, sync_service,
      synced_service_pref_names);
}
