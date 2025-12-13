// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_VERDICT_CACHE_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_VERDICT_CACHE_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace safe_browsing {
class VerdictCacheManager;
}

// Singleton that owns VerdictCacheManager objects, one for each active
// profile.
class VerdictCacheManagerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static safe_browsing::VerdictCacheManager* GetForProfile(ProfileIOS* profile);
  // Returns the singleton instance of VerdictCacheManagerFactory.
  static VerdictCacheManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<VerdictCacheManagerFactory>;

  VerdictCacheManagerFactory();
  ~VerdictCacheManagerFactory() override = default;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_VERDICT_CACHE_MANAGER_FACTORY_H_
