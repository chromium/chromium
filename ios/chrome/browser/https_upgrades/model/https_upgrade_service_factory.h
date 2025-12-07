// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/https_upgrades/model/https_upgrade_service_impl.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

// Singleton that owns all HttpsUpgradeService and associates them with
// ProfileIOS.
class HttpsUpgradeServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static HttpsUpgradeService* GetForProfile(ProfileIOS* profile);
  static HttpsUpgradeServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<HttpsUpgradeServiceFactory>;

  HttpsUpgradeServiceFactory();
  ~HttpsUpgradeServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_SERVICE_FACTORY_H_
