// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/browser_management_service_factory.h"

#import <utility>

#import "components/policy/core/common/management/platform_management_service.h"
#import "ios/chrome/browser/policy/model/browser_management_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace policy {

// static
ManagementService* BrowserManagementServiceFactory::GetForPlatform() {
  return PlatformManagementService::GetInstance();
}

BrowserManagementServiceFactory::BrowserManagementServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "BrowserManagementService",
          ProfileSelection::kOwnInstanceInIncognito) {}

std::unique_ptr<KeyedService>
BrowserManagementServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<BrowserManagementService>(profile);
}

}  // namespace policy
