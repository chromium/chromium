// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_VERDICT_CACHE_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_VERDICT_CACHE_MANAGER_FACTORY_H_

#import "components/safe_browsing/core/browser/verdict_cache_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns VerdictCacheManager objects, one for each active
// profile.
class VerdictCacheManagerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          VerdictCacheManagerFactory,
          safe_browsing::VerdictCacheManager> {
 public:
  explicit VerdictCacheManagerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_VERDICT_CACHE_MANAGER_FACTORY_H_
