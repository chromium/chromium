// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_HASH_REALTIME_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_HASH_REALTIME_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "services/network/public/mojom/network_context.mojom.h"

class KeyedService;

namespace safe_browsing {
class HashRealTimeService;
}

class ChromeBrowserState;

// Singleton that owns HashRealTimeService objects, one for each active
// BrowserState. It returns nullptr for incognito BrowserStates.
class HashRealTimeServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the instance of HashRealTimeService associated with this browser
  // state, creating one if none exists.
  static safe_browsing::HashRealTimeService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  // Returns the singleton instance of HashRealTimeServiceFactory.
  static HashRealTimeServiceFactory* GetInstance();

  HashRealTimeServiceFactory(const HashRealTimeServiceFactory&) = delete;
  HashRealTimeServiceFactory& operator=(const HashRealTimeServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<HashRealTimeServiceFactory>;

  HashRealTimeServiceFactory();
  ~HashRealTimeServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_HASH_REALTIME_SERVICE_FACTORY_H_
