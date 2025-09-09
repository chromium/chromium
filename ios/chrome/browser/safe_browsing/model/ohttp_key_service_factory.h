// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_OHTTP_KEY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_OHTTP_KEY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace safe_browsing {
class OhttpKeyService;
}

// Singleton that owns OhttpKeyService objects, one for each active
// ProfileIOS. It returns nullptr for incognito ProfileIOS.
class OhttpKeyServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the instance of OhttpKeyService associated with this profile,
  // creating one if none exists.
  static safe_browsing::OhttpKeyService* GetForProfile(ProfileIOS* profile);

  // Returns the singleton instance of OhttpKeyServiceFactory.
  static OhttpKeyServiceFactory* GetInstance();

  // Returns the default factory. Can be used to force instantiation during
  // testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<OhttpKeyServiceFactory>;

  OhttpKeyServiceFactory();
  ~OhttpKeyServiceFactory() override = default;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_OHTTP_KEY_SERVICE_FACTORY_H_
