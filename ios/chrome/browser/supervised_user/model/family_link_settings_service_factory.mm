// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/family_link_settings_service_factory.h"

#import "base/no_destructor.h"
#import "components/supervised_user/core/browser/family_link_settings_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace supervised_user {
// static
FamilyLinkSettingsService* FamilyLinkSettingsServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<FamilyLinkSettingsService>(
      profile, /*create=*/true);
}

// static
FamilyLinkSettingsServiceFactory*
FamilyLinkSettingsServiceFactory::GetInstance() {
  static base::NoDestructor<FamilyLinkSettingsServiceFactory> instance;
  return instance.get();
}

FamilyLinkSettingsServiceFactory::FamilyLinkSettingsServiceFactory()
    : ProfileKeyedServiceFactoryIOS("FamilyLinkSettingsService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
}

bool FamilyLinkSettingsServiceFactory::
    ServiceIsRequiredForContextInitialization() const {
  // FamilyLinkSettingsService is required to initialize the PrefService
  // of the ProfileIOS as it is part of the implementation of the
  // SupervisedUserPrefStore.
  return true;
}

std::unique_ptr<KeyedService>
FamilyLinkSettingsServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<FamilyLinkSettingsService>();
}
}  // namespace supervised_user
