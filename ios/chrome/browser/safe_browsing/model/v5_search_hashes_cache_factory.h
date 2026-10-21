// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_V5_SEARCH_HASHES_CACHE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_V5_SEARCH_HASHES_CACHE_FACTORY_H_

#import "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns V5SearchHashesCache objects, one for each active profile.
class V5SearchHashesCacheFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          V5SearchHashesCacheFactory,
          safe_browsing::V5SearchHashesCache> {
 public:
  explicit V5SearchHashesCacheFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_V5_SEARCH_HASHES_CACHE_FACTORY_H_
