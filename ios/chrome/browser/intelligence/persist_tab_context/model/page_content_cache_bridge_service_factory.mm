// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/persist_tab_context/model/page_content_cache_bridge_service_factory.h"

#import "base/no_destructor.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/page_content_cache_bridge_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
PageContentCacheBridgeService*
PageContentCacheBridgeServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<PageContentCacheBridgeService>(
      profile,
      /*create=*/true);
}

// static
PageContentCacheBridgeServiceFactory*
PageContentCacheBridgeServiceFactory::GetInstance() {
  static base::NoDestructor<PageContentCacheBridgeServiceFactory> instance;
  return instance.get();
}

PageContentCacheBridgeServiceFactory::PageContentCacheBridgeServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PageContentCacheBridgeService") {}

std::unique_ptr<KeyedService>
PageContentCacheBridgeServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  os_crypt_async::OSCryptAsync* os_crypt_async =
      GetApplicationContext()->GetOSCryptAsync();
  PrefService* prefs = profile->GetPrefs();
  base::TimeDelta max_context_age = GetPersistedContextEffectiveTTL(prefs);
  return std::make_unique<PageContentCacheBridgeService>(
      os_crypt_async, profile->GetStatePath(), max_context_age);
}
