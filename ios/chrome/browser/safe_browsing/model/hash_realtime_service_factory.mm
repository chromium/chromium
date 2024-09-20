// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/hash_realtime_service_factory.h"

#import "base/feature_list.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
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
safe_browsing::HashRealTimeService*
HashRealTimeServiceFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
safe_browsing::HashRealTimeService* HashRealTimeServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<safe_browsing::HashRealTimeService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
HashRealTimeServiceFactory* HashRealTimeServiceFactory::GetInstance() {
  static base::NoDestructor<HashRealTimeServiceFactory> instance;
  return instance.get();
}

HashRealTimeServiceFactory::HashRealTimeServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "HashRealTimeService",
          BrowserStateDependencyManager::GetInstance()) {
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
