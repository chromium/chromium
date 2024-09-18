// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_VERDICT_CACHE_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_VERDICT_CACHE_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class KeyedService;

namespace safe_browsing {
class VerdictCacheManager;
}

namespace web {
class BrowserState;
}

// Singleton that owns VerdictCacheManager objects, one for each active
// profile.
class VerdictCacheManagerFactory : public BrowserStateKeyedServiceFactory {
 public:
  static safe_browsing::VerdictCacheManager* GetForProfile(ProfileIOS* profile);
  // Returns the singleton instance of VerdictCacheManagerFactory.
  static VerdictCacheManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<VerdictCacheManagerFactory>;

  VerdictCacheManagerFactory();
  ~VerdictCacheManagerFactory() override = default;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* browser_state) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_VERDICT_CACHE_MANAGER_FACTORY_H_
