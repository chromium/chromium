// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/verdict_cache_manager_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/safe_browsing/core/browser/sync/safe_browsing_sync_observer_impl.h"
#import "components/safe_browsing/core/browser/verdict_cache_manager.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

// static
safe_browsing::VerdictCacheManager* VerdictCacheManagerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<safe_browsing::VerdictCacheManager>(
          profile, /*create=*/true);
}

// static
VerdictCacheManagerFactory* VerdictCacheManagerFactory::GetInstance() {
  static base::NoDestructor<VerdictCacheManagerFactory> instance;
  return instance.get();
}

VerdictCacheManagerFactory::VerdictCacheManagerFactory()
    : ProfileKeyedServiceFactoryIOS("VerdictCacheManager",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::HostContentSettingsMapFactory::GetInstance());
}

std::unique_ptr<KeyedService>
VerdictCacheManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  return std::make_unique<safe_browsing::VerdictCacheManager>(
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      ios::HostContentSettingsMapFactory::GetForProfile(profile),
      profile->GetPrefs(),
      std::make_unique<safe_browsing::SafeBrowsingSyncObserverImpl>(
          SyncServiceFactory::GetForProfile(profile)));
}
