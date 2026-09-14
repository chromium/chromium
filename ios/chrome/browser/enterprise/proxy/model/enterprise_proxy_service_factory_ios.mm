// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_service_factory_ios.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "components/enterprise/net/core/enterprise_proxy_service.h"
#import "components/enterprise/net/core/features.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_network_auth_service_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

std::unique_ptr<KeyedService> BuildEnterpriseProxyService(ProfileIOS* profile) {
  if (!enterprise_net::IsDynamicRouteFetchingEnabled()) {
    return nullptr;
  }
  enterprise_net::EnterpriseNetworkAuthService* network_auth_service =
      EnterpriseNetworkAuthServiceFactoryIOS::GetForProfile(profile);
  CHECK(network_auth_service);
  auto url_loader_factory_callback = base::BindRepeating(
      [](base::WeakPtr<ProfileIOS> weak_profile)
          -> scoped_refptr<network::SharedURLLoaderFactory> {
        return weak_profile ? weak_profile->GetSharedURLLoaderFactory()
                            : nullptr;
      },
      profile->AsWeakPtr());
  return std::make_unique<enterprise_net::EnterpriseProxyService>(
      profile->GetPrefs(), network_auth_service,
      std::move(url_loader_factory_callback),
      enterprise::ProfileIdServiceFactoryIOS::GetForProfile(profile));
}

}  // namespace

// static
enterprise_net::EnterpriseProxyService*
EnterpriseProxyServiceFactoryIOS::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<enterprise_net::EnterpriseProxyService>(
          profile, /*create=*/true);
}

// static
EnterpriseProxyServiceFactoryIOS*
EnterpriseProxyServiceFactoryIOS::GetInstance() {
  static base::NoDestructor<EnterpriseProxyServiceFactoryIOS> instance;
  return instance.get();
}

// static
EnterpriseProxyServiceFactoryIOS::TestingFactory
EnterpriseProxyServiceFactoryIOS::GetDefaultFactory() {
  return base::BindOnce(&BuildEnterpriseProxyService);
}

EnterpriseProxyServiceFactoryIOS::EnterpriseProxyServiceFactoryIOS()
    : ProfileKeyedServiceFactoryIOS("EnterpriseProxyServiceIOS",
                                    ProfileSelection::kNoInstanceInIncognito,
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(EnterpriseNetworkAuthServiceFactoryIOS::GetInstance());
  DependsOn(enterprise::ProfileIdServiceFactoryIOS::GetInstance());
}

EnterpriseProxyServiceFactoryIOS::~EnterpriseProxyServiceFactoryIOS() = default;

std::unique_ptr<KeyedService>
EnterpriseProxyServiceFactoryIOS::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildEnterpriseProxyService(profile);
}
