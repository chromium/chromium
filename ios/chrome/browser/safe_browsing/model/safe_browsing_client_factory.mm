// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_client_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/hash_realtime_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/real_time_url_lookup_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_client_impl.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"
#import "ios/web/public/browser_state.h"

// static
SafeBrowsingClient* SafeBrowsingClientFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<SafeBrowsingClient*>(
      GetInstance()->GetServiceForBrowserState(profile, /*create=*/true));
}

// static
SafeBrowsingClient* SafeBrowsingClientFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
SafeBrowsingClientFactory* SafeBrowsingClientFactory::GetInstance() {
  static base::NoDestructor<SafeBrowsingClientFactory> instance;
  return instance.get();
}

SafeBrowsingClientFactory::SafeBrowsingClientFactory()
    : BrowserStateKeyedServiceFactory(
          "SafeBrowsingClient",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(RealTimeUrlLookupServiceFactory::GetInstance());
  DependsOn(HashRealTimeServiceFactory::GetInstance());
  DependsOn(PrerenderServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SafeBrowsingClientFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  safe_browsing::RealTimeUrlLookupService* lookup_service =
      RealTimeUrlLookupServiceFactory::GetForProfile(profile);
  safe_browsing::HashRealTimeService* hash_real_time_service = nullptr;
  if (base::FeatureList::IsEnabled(safe_browsing::kHashPrefixRealTimeLookups)) {
    hash_real_time_service = HashRealTimeServiceFactory::GetForProfile(profile);
  }
  PrerenderService* prerender_service =
      PrerenderServiceFactory::GetForProfile(profile);
  return std::make_unique<SafeBrowsingClientImpl>(
      lookup_service, hash_real_time_service, prerender_service);
}

web::BrowserState* SafeBrowsingClientFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
