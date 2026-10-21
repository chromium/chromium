// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_error_service_factory_ios.h"

#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "components/enterprise/net/core/enterprise_proxy_error_service.h"
#import "components/enterprise/net/core/features.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_service_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

std::unique_ptr<KeyedService> BuildEnterpriseProxyErrorService(
    ProfileIOS* profile) {
  if (!enterprise_net::IsDynamicRouteFetchingEnabled()) {
    return nullptr;
  }
  enterprise_net::EnterpriseProxyService* proxy_service =
      EnterpriseProxyServiceFactoryIOS::GetForProfile(profile);
  CHECK(proxy_service);
  return std::make_unique<enterprise_net::EnterpriseProxyErrorService>(
      proxy_service);
}

}  // namespace

// static
EnterpriseProxyErrorServiceFactoryIOS::TestingFactory
EnterpriseProxyErrorServiceFactoryIOS::GetDefaultFactory() {
  return base::BindOnce(&BuildEnterpriseProxyErrorService);
}

EnterpriseProxyErrorServiceFactoryIOS::EnterpriseProxyErrorServiceFactoryIOS(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "EnterpriseProxyErrorServiceIOS",
          ProfileSelection::kNoInstanceInIncognito,
          ServiceCreation::kCreateWithProfile,
          TestingCreation::kNoServiceForTests) {
  DependsOn(EnterpriseProxyServiceFactoryIOS::GetInstance());
}

std::unique_ptr<KeyedService>
EnterpriseProxyErrorServiceFactoryIOS::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildEnterpriseProxyErrorService(profile);
}
