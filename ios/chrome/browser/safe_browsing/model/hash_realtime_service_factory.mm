// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/hash_realtime_service_factory.h"

#import "base/feature_list.h"
#import "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#import "components/safe_browsing/core/browser/verdict_cache_manager.h"
#import "ios/chrome/browser/safe_browsing/model/ohttp_key_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/verdict_cache_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"

namespace {

network::mojom::NetworkContext* GetNetworkContext() {
  return GetApplicationContext()->GetSafeBrowsingService()->GetNetworkContext();
}

}  // namespace

// static
safe_browsing::HashRealTimeService* HashRealTimeServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<safe_browsing::HashRealTimeService>(
          profile, /*create=*/true);
}

// static
HashRealTimeServiceFactory* HashRealTimeServiceFactory::GetInstance() {
  static base::NoDestructor<HashRealTimeServiceFactory> instance;
  return instance.get();
}

HashRealTimeServiceFactory::HashRealTimeServiceFactory()
    : ProfileKeyedServiceFactoryIOS("HashRealTimeService") {
  DependsOn(VerdictCacheManagerFactory::GetInstance());
  DependsOn(OhttpKeyServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
HashRealTimeServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  if (!safe_browsing_service) {
    return nullptr;
  }
  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  return std::make_unique<safe_browsing::HashRealTimeService>(
      base::BindRepeating(&GetNetworkContext),
      VerdictCacheManagerFactory::GetForProfile(profile),
      OhttpKeyServiceFactory::GetForProfile(profile),
      /*webui_delegate=*/nullptr);
}
