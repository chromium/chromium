// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/list_family_members_service_factory.h"

#import "base/check_deref.h"
#import "base/no_destructor.h"
#import "components/supervised_user/core/browser/list_family_members_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

// static
supervised_user::ListFamilyMembersService*
ListFamilyMembersServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<supervised_user::ListFamilyMembersService>(
          profile, /*create=*/true);
}

// static
ListFamilyMembersServiceFactory*
ListFamilyMembersServiceFactory::GetInstance() {
  static base::NoDestructor<ListFamilyMembersServiceFactory> instance;
  return instance.get();
}

ListFamilyMembersServiceFactory::ListFamilyMembersServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ListFamilyMembersService") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ListFamilyMembersServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<supervised_user::ListFamilyMembersService>(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory(), CHECK_DEREF(profile->GetPrefs()));
}
