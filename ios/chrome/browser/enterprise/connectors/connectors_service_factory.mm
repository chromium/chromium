// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"

#import <utility>

#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace enterprise_connectors {

ConnectorsServiceFactory::ConnectorsServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "ConnectorsService",
          ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService> ConnectorsServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ConnectorsService>(
      profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile),
      profile->GetUserCloudPolicyManager(), profile->GetProfileName(),
      profile->GetStatePath(), profile->IsOffTheRecord());
}

}  // namespace enterprise_connectors
