// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"

#import "components/enterprise/browser/identifiers/profile_id_service.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_delegate_ios_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace enterprise {
namespace {
std::unique_ptr<KeyedService> BuildProfileIdService(ProfileIOS* profile) {
  return std::make_unique<ProfileIdService>(
      std::make_unique<ProfileIdDelegateIOSImpl>(profile), profile->GetPrefs());
}
}  // namespace

// static
ProfileIdServiceFactoryIOS* ProfileIdServiceFactoryIOS::GetInstance() {
  static base::NoDestructor<ProfileIdServiceFactoryIOS> instance;
  return instance.get();
}

// static
ProfileIdService* ProfileIdServiceFactoryIOS::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ProfileIdService>(
      profile, /*create=*/true);
}

// static
ProfileIdServiceFactoryIOS::TestingFactory
ProfileIdServiceFactoryIOS::GetDefaultFactory() {
  return base::BindOnce(&BuildProfileIdService);
}

ProfileIdServiceFactoryIOS::ProfileIdServiceFactoryIOS()
    : ProfileKeyedServiceFactoryIOS("ProfileIdServiceIOS",
                                    ProfileSelection::kNoInstanceInIncognito) {}

ProfileIdServiceFactoryIOS::~ProfileIdServiceFactoryIOS() = default;

std::unique_ptr<KeyedService>
ProfileIdServiceFactoryIOS::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return BuildProfileIdService(profile);
}

}  // namespace enterprise
