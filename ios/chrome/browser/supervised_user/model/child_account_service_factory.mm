// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/child_account_service_factory.h"

#import "base/check_deref.h"
#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/list_family_members_service_factory.h"

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
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ListFamilyMembersServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ChildAccountServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<supervised_user::ChildAccountService>(
      CHECK_DEREF(profile->GetPrefs()),
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory(),
      // Callback relevant only for Chrome OS.
      /*check_user_child_status_callback=*/base::DoNothing(),
      CHECK_DEREF(ListFamilyMembersServiceFactory::GetForProfile(profile)));
}
