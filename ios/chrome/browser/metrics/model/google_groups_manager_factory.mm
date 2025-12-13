// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/google_groups_manager_factory.h"

#import "components/variations/service/google_groups_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
GoogleGroupsManager* GoogleGroupsManagerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<GoogleGroupsManager>(
      profile, /*create=*/true);
}

// static
GoogleGroupsManagerFactory* GoogleGroupsManagerFactory::GetInstance() {
  static base::NoDestructor<GoogleGroupsManagerFactory> instance;
  return instance.get();
}

GoogleGroupsManagerFactory::GoogleGroupsManagerFactory()
    : ProfileKeyedServiceFactoryIOS("GoogleGroupsManager",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {}

std::unique_ptr<KeyedService>
GoogleGroupsManagerFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return std::make_unique<GoogleGroupsManager>(
      *GetApplicationContext()->GetLocalState(), profile->GetProfileName(),
      *profile->GetPrefs());
}

void GoogleGroupsManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  GoogleGroupsManager::RegisterProfilePrefs(registry);
}
