// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/management_service_ios_factory.h"

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

ManagementServiceIOSFactory::ManagementServiceIOSFactory()
    : ProfileKeyedServiceFactoryIOS("ManagementServiceIOS",
                                    ProfileSelection::kOwnInstanceInIncognito) {
}

ManagementServiceIOSFactory::~ManagementServiceIOSFactory() = default;

std::unique_ptr<KeyedService>
ManagementServiceIOSFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  return std::make_unique<ManagementServiceIOS>(
      ProfileIOS::FromBrowserState(browser_state));
}

}  // namespace policy
