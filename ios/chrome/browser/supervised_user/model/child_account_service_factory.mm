// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/child_account_service_factory.h"

#import "base/check_deref.h"
#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "components/supervised_user/core/browser/child_account_service.h"
#import "components/supervised_user/core/browser/family_link_settings_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/family_link_settings_service_factory.h"

// static
supervised_user::ChildAccountService* ChildAccountServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<supervised_user::ChildAccountService>(
          profile, /*create=*/true);
}

// static
ChildAccountServiceFactory* ChildAccountServiceFactory::GetInstance() {
  static base::NoDestructor<ChildAccountServiceFactory> instance;
  return instance.get();
}

ChildAccountServiceFactory::ChildAccountServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ChildAccountService") {
  // Source of truth for the supervision status.
  DependsOn(IdentityManagerFactory::GetInstance());
  // Consumer that shall be activated according to supervision status.
  DependsOn(supervised_user::FamilyLinkSettingsServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ChildAccountServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  supervised_user::FamilyLinkSettingsService& family_link_settings_service =
      CHECK_DEREF(
          supervised_user::FamilyLinkSettingsServiceFactory::GetForProfile(
              profile));
  return std::make_unique<supervised_user::ChildAccountService>(
      CHECK_DEREF(profile->GetPrefs()),
      IdentityManagerFactory::GetForProfile(profile),
      family_link_settings_service,
      // Callback relevant only for Chrome OS.
      /*check_user_child_status_callback=*/base::DoNothing());
}
