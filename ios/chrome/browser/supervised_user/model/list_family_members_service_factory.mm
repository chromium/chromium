// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/list_family_members_service_factory.h"

#import <utility>

#import "base/check_deref.h"
#import "components/supervised_user/core/browser/list_family_members_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace supervised_user {

ListFamilyMembersServiceFactory::ListFamilyMembersServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "ListFamilyMembersService",
          ServiceCreation::kCreateWithProfile,
          // Note: this is a leaf service that is not automatically created in
          // tests that use TestProfileIOS (i.e. unit tests). It is however
          // created integration tests that bootstrap a real profile.
          TestingCreation::kNoServiceForTests) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ListFamilyMembersServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    // Match lifecycle of the identity manager. No identity means no family.
    return nullptr;
  }
  return std::make_unique<ListFamilyMembersService>(
      CHECK_DEREF(identity_manager), profile->GetSharedURLLoaderFactory(),
      CHECK_DEREF(profile->GetPrefs()));
}

}  // namespace supervised_user
