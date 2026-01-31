// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/persist_tab_context/model/page_content_cache_service_factory.h"

#import "base/files/file_path.h"
#import "base/no_destructor.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/page_content_cache_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/paths/paths_internal.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {
constexpr char kPersistedTabContextsDatabase[] = "persisted_tab_contexts_db";
}  // namespace

// static
PageContentCacheService* PageContentCacheServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<PageContentCacheService>(
      profile,
      /*create=*/true);
}

// static
base::FilePath PageContentCacheServiceFactory::GetStoragePathForProfile(
    ProfileIOS* profile) {
  base::FilePath cache_directory_path;
  ios::GetUserCacheDirectory(profile->GetStatePath(), &cache_directory_path);
  return cache_directory_path.Append(kPersistedTabContextsDatabase);
}

// static
PageContentCacheServiceFactory* PageContentCacheServiceFactory::GetInstance() {
  static base::NoDestructor<PageContentCacheServiceFactory> instance;
  return instance.get();
}

PageContentCacheServiceFactory::PageContentCacheServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PageContentCacheService") {}

std::unique_ptr<KeyedService>
PageContentCacheServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  CHECK(profile);

  os_crypt_async::OSCryptAsync* os_crypt_async =
      GetApplicationContext()->GetOSCryptAsync();
  CHECK(os_crypt_async);

  PrefService* prefs = profile->GetPrefs();
  base::TimeDelta max_context_age = GetPersistedContextEffectiveTTL(prefs);

  base::FilePath storage_directory_path = GetStoragePathForProfile(profile);

  return std::make_unique<PageContentCacheService>(
      os_crypt_async, storage_directory_path, max_context_age);
}
