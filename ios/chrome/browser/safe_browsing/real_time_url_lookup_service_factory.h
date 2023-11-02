// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_REAL_TIME_URL_LOOKUP_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_REAL_TIME_URL_LOOKUP_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class KeyedService;

namespace safe_browsing {
class RealTimeUrlLookupService;
}

namespace web {
class BrowserState;
}

// Singleton that owns RealTimeUrlLookupService objects, one for each active
// ChromeBrowserState. It returns nullptr for Incognito browser states.
class RealTimeUrlLookupServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the instance of RealTimeUrlLookupService associated with this
  // browser state, creating one if none exists and the given browser state is
  // not in Incognito mode.
  static safe_browsing::RealTimeUrlLookupService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  // Returns the singleton instance of RealTimeUrlLookupServiceFactory.
  static RealTimeUrlLookupServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<RealTimeUrlLookupServiceFactory>;

  RealTimeUrlLookupServiceFactory();
  ~RealTimeUrlLookupServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_REAL_TIME_URL_LOOKUP_SERVICE_FACTORY_H_
