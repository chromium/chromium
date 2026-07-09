// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/v5_search_hashes_cache_factory.h"

#import "base/no_destructor.h"
#import "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
safe_browsing::V5SearchHashesCache* V5SearchHashesCacheFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<safe_browsing::V5SearchHashesCache>(
          profile, /*create=*/true);
}

// static
V5SearchHashesCacheFactory* V5SearchHashesCacheFactory::GetInstance() {
  static base::NoDestructor<V5SearchHashesCacheFactory> instance;
  return instance.get();
}

V5SearchHashesCacheFactory::V5SearchHashesCacheFactory()
    : ProfileKeyedServiceFactoryIOS("V5SearchHashesCache") {}

std::unique_ptr<KeyedService>
V5SearchHashesCacheFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return std::make_unique<safe_browsing::V5SearchHashesCache>();
}
