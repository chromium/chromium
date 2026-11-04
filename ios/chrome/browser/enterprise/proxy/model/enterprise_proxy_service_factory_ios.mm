// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_service_factory_ios.h"

#import <type_traits>
#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "components/enterprise/net/core/enterprise_proxy_service.h"
#import "components/enterprise/net/core/features.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_network_auth_service_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/web_client.h"
#import "net/log/net_log.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

std::unique_ptr<KeyedService> BuildEnterpriseProxyService(ProfileIOS& profile) {
  if (!enterprise_net::IsDynamicRouteFetchingEnabled()) {
    return nullptr;
  }
  enterprise_net::EnterpriseNetworkAuthService* network_auth_service =
      EnterpriseNetworkAuthServiceFactoryIOS::GetForProfile(profile);
  CHECK(network_auth_service);
  return std::make_unique<enterprise_net::EnterpriseProxyService>(
      profile.GetPrefs(), network_auth_service,
      base::ReturnValueRepeating(profile.GetSharedURLLoaderFactory()),
      enterprise::ProfileIdServiceFactoryIOS::GetForProfile(profile),
      net::NetLog::Get(),
      // WebKit sends no User-Agent header with proxy CONNECT requests, so the
      // proxy gets the mobile user agent that Chrome's own network stack sends
      // on iOS.
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE));
}

}  // namespace

// static
EnterpriseProxyServiceFactoryIOS::TestingFactoryByRef
EnterpriseProxyServiceFactoryIOS::GetDefaultFactory() {
  return base::BindOnce(&BuildEnterpriseProxyService);
}

EnterpriseProxyServiceFactoryIOS::EnterpriseProxyServiceFactoryIOS(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "EnterpriseProxyServiceIOS",
          ProfileSelection::kNoInstanceInIncognito,
          ServiceCreation::kCreateWithProfile,
          TestingCreation::kNoServiceForTests) {
  DependsOn(EnterpriseNetworkAuthServiceFactoryIOS::GetInstance());
  DependsOn(enterprise::ProfileIdServiceFactoryIOS::GetInstance());
}

std::unique_ptr<KeyedService>
EnterpriseProxyServiceFactoryIOS::BuildServiceInstanceFor(
    ProfileIOS& profile) const {
  return BuildEnterpriseProxyService(profile);
}
