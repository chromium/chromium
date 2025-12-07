// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/management_service_ios_factory.h"

#import "components/policy/core/common/management/platform_management_service.h"
#import "ios/chrome/browser/policy/model/management_service_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace policy {

// static
ManagementServiceIOSFactory* ManagementServiceIOSFactory::GetInstance() {
  static base::NoDestructor<ManagementServiceIOSFactory> instance;
  return instance.get();
}

// static
ManagementServiceIOS* ManagementServiceIOSFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ManagementServiceIOS>(
      profile, /*create=*/true);
}

// static
ManagementService* ManagementServiceIOSFactory::GetForPlatform() {
  return PlatformManagementService::GetInstance();
}

ManagementServiceIOSFactory::ManagementServiceIOSFactory()
    : ProfileKeyedServiceFactoryIOS("ManagementServiceIOS",
                                    ProfileSelection::kOwnInstanceInIncognito) {
}

ManagementServiceIOSFactory::~ManagementServiceIOSFactory() = default;

std::unique_ptr<KeyedService>
ManagementServiceIOSFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ManagementServiceIOS>(profile);
}

}  // namespace policy
