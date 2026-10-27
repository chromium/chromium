// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_investigator_factory.h"

#import <utility>

#import "components/pref_registry/pref_registry_syncable.h"
#import "components/signin/core/browser/account_investigator.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace ios {

AccountInvestigatorFactory::AccountInvestigatorFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "AccountInvestigator",
                                         ServiceCreation::kCreateWithProfile,
                                         TestingCreation::kNoServiceForTests) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
AccountInvestigatorFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  std::unique_ptr<AccountInvestigator> investigator =
      std::make_unique<AccountInvestigator>(
          profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile));
  investigator->Initialize();
  return std::move(investigator);
}

void AccountInvestigatorFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AccountInvestigator::RegisterPrefs(registry);
}

}  // namespace ios
