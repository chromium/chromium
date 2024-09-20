// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_HASH_REALTIME_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_HASH_REALTIME_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "services/network/public/mojom/network_context.mojom.h"

class KeyedService;

namespace safe_browsing {
class HashRealTimeService;
}

// Singleton that owns HashRealTimeService objects, one for each active profile.
// It returns nullptr for incognito profiles.
class HashRealTimeServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static safe_browsing::HashRealTimeService* GetForBrowserState(
      ProfileIOS* profile);

  static safe_browsing::HashRealTimeService* GetForProfile(ProfileIOS* profile);
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
