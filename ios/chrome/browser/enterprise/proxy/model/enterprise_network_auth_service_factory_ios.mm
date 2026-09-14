// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/enterprise_network_auth_service_factory_ios.h"

#import "base/functional/bind.h"
#import "components/enterprise/net/core/enterprise_network_auth_service.h"
#import "components/enterprise/net/core/features.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace {

std::unique_ptr<KeyedService> BuildEnterpriseNetworkAuthService(
    ProfileIOS* profile) {
  if (!enterprise_net::IsDynamicRouteFetchingEnabled()) {
    return nullptr;
  }
  return std::make_unique<enterprise_net::EnterpriseNetworkAuthService>(
      IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs(),
      enterprise::ProfileIdServiceFactoryIOS::GetForProfile(profile));
}

}  // namespace

// static
enterprise_net::EnterpriseNetworkAuthService*
EnterpriseNetworkAuthServiceFactoryIOS::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<enterprise_net::EnterpriseNetworkAuthService>(
          profile, /*create=*/true);
}

// static
EnterpriseNetworkAuthServiceFactoryIOS*
EnterpriseNetworkAuthServiceFactoryIOS::GetInstance() {
  static base::NoDestructor<EnterpriseNetworkAuthServiceFactoryIOS> instance;
  return instance.get();
}

// static
EnterpriseNetworkAuthServiceFactoryIOS::TestingFactory
EnterpriseNetworkAuthServiceFactoryIOS::GetDefaultFactory() {
  return base::BindOnce(&BuildEnterpriseNetworkAuthService);
}

EnterpriseNetworkAuthServiceFactoryIOS::EnterpriseNetworkAuthServiceFactoryIOS()
    : ProfileKeyedServiceFactoryIOS("EnterpriseNetworkAuthServiceIOS",
                                    ProfileSelection::kNoInstanceInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(enterprise::ProfileIdServiceFactoryIOS::GetInstance());
}

EnterpriseNetworkAuthServiceFactoryIOS::
    ~EnterpriseNetworkAuthServiceFactoryIOS() = default;

std::unique_ptr<KeyedService>
EnterpriseNetworkAuthServiceFactoryIOS::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildEnterpriseNetworkAuthService(profile);
}
