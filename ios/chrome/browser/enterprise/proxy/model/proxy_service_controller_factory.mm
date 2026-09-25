// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/proxy_service_controller_factory.h"

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "components/enterprise/net/core/features.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_error_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/proxy/model/proxy_service_controller.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/proxy/proxy_configuration_provider.h"

namespace {

std::unique_ptr<KeyedService> BuildProxyServiceController(ProfileIOS* profile) {
  if (!enterprise_net::IsDynamicRouteFetchingEnabled()) {
    return nullptr;
  }
  return std::make_unique<ProxyServiceController>(
      EnterpriseProxyServiceFactoryIOS::GetForProfile(profile),
      EnterpriseProxyErrorServiceFactoryIOS::GetForProfile(profile),
      &web::ProxyConfigurationProvider::FromBrowserState(profile));
}

}  // namespace

// static
ProxyServiceController* ProxyServiceControllerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ProxyServiceController>(
      profile, /*create=*/true);
}

// static
ProxyServiceControllerFactory* ProxyServiceControllerFactory::GetInstance() {
  static base::NoDestructor<ProxyServiceControllerFactory> instance;
  return instance.get();
}

// static
ProxyServiceControllerFactory::TestingFactory
ProxyServiceControllerFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildProxyServiceController);
}

ProxyServiceControllerFactory::ProxyServiceControllerFactory()
    : ProfileKeyedServiceFactoryIOS("ProxyServiceController",
                                    ProfileSelection::kNoInstanceInIncognito,
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(EnterpriseProxyServiceFactoryIOS::GetInstance());
  DependsOn(EnterpriseProxyErrorServiceFactoryIOS::GetInstance());
}

ProxyServiceControllerFactory::~ProxyServiceControllerFactory() = default;

std::unique_ptr<KeyedService>
ProxyServiceControllerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildProxyServiceController(profile);
}
