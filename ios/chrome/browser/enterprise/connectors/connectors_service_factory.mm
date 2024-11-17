// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace enterprise_connectors {

// static
ConnectorsServiceFactory* ConnectorsServiceFactory::GetInstance() {
  static base::NoDestructor<ConnectorsServiceFactory> instance;
  return instance.get();
}

// static
ConnectorsService* ConnectorsServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ConnectorsService>(
      profile, /*create=*/true);
}

ConnectorsServiceFactory::ConnectorsServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ConnectorsService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
}

ConnectorsServiceFactory::~ConnectorsServiceFactory() = default;

std::unique_ptr<KeyedService> ConnectorsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  auto* profile = ProfileIOS::FromBrowserState(browser_state);
  return std::make_unique<ConnectorsService>(
      profile->IsOffTheRecord(), profile->GetPrefs(),
      profile->GetUserCloudPolicyManager());
}

}  // namespace enterprise_connectors
