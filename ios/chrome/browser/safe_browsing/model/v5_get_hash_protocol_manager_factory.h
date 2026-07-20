// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_V5_GET_HASH_PROTOCOL_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_V5_GET_HASH_PROTOCOL_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace safe_browsing {
class V5GetHashProtocolManager;
}

// Singleton that owns V5GetHashProtocolManager objects, one for each active
// profile.
class V5GetHashProtocolManagerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static safe_browsing::V5GetHashProtocolManager* GetForProfile(
      ProfileIOS* profile);

  // Returns the singleton instance of V5GetHashProtocolManagerFactory.
  static V5GetHashProtocolManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<V5GetHashProtocolManagerFactory>;

  V5GetHashProtocolManagerFactory();
  ~V5GetHashProtocolManagerFactory() override = default;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_V5_GET_HASH_PROTOCOL_MANAGER_FACTORY_H_
