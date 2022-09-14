// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_VERDICT_CACHE_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_VERDICT_CACHE_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class KeyedService;

namespace safe_browsing {
class VerdictCacheManager;
}

namespace web {
class BrowserState;
}

// Singleton that owns VerdictCacheManager objects, one for each active
// ChromeBrowserState.
class VerdictCacheManagerFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the instance of VerdictCacheManager associated with this browser
  // state, creating one if none exists.
  static safe_browsing::VerdictCacheManager* GetForBrowserState(
      ChromeBrowserState* browser_state);

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

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_VERDICT_CACHE_MANAGER_FACTORY_H_
