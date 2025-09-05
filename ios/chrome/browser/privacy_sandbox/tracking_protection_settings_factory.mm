// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/privacy_sandbox/tracking_protection_prefs.h"
#import "components/privacy_sandbox/tracking_protection_settings.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/policy/model/management_service_ios.h"
#import "ios/chrome/browser/policy/model/management_service_ios_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
TrackingProtectionSettingsFactory*
TrackingProtectionSettingsFactory::GetInstance() {
  static base::NoDestructor<TrackingProtectionSettingsFactory> instance;
  return instance.get();
}

// static
privacy_sandbox::TrackingProtectionSettings*
TrackingProtectionSettingsFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<privacy_sandbox::TrackingProtectionSettings>(
          profile, /*create=*/true);
}

void TrackingProtectionSettingsFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kFingerprintingProtectionEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

TrackingProtectionSettingsFactory::TrackingProtectionSettingsFactory()
    : ProfileKeyedServiceFactoryIOS("TrackingProtectionSettings",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(ios::HostContentSettingsMapFactory::GetInstance());
  DependsOn(policy::ManagementServiceIOSFactory::GetInstance());
}

TrackingProtectionSettingsFactory::~TrackingProtectionSettingsFactory() =
    default;

std::unique_ptr<KeyedService>
TrackingProtectionSettingsFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<privacy_sandbox::TrackingProtectionSettings>(
      profile->GetPrefs(),
      ios::HostContentSettingsMapFactory::GetForProfile(profile),
      policy::ManagementServiceIOSFactory::GetForProfile(profile),
      profile->IsOffTheRecord());
}
