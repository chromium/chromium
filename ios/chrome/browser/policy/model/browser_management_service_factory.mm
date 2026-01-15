// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/browser_management_service_factory.h"

#import "components/policy/core/common/management/platform_management_service.h"
#import "ios/chrome/browser/policy/model/browser_management_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace policy {

// static
BrowserManagementServiceFactory*
BrowserManagementServiceFactory::GetInstance() {
  static base::NoDestructor<BrowserManagementServiceFactory> instance;
  return instance.get();
}

// static
BrowserManagementService* BrowserManagementServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<BrowserManagementService>(
      profile, /*create=*/true);
}

// static
ManagementService* BrowserManagementServiceFactory::GetForPlatform() {
  return PlatformManagementService::GetInstance();
}

BrowserManagementServiceFactory::BrowserManagementServiceFactory()
    : ProfileKeyedServiceFactoryIOS("BrowserManagementService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
}

BrowserManagementServiceFactory::~BrowserManagementServiceFactory() = default;

std::unique_ptr<KeyedService>
BrowserManagementServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<BrowserManagementService>(profile);
}

}  // namespace policy
