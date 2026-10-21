// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_SERVICE_FACTORY_H_

#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#include "ios/components/security_interstitials/https_only_mode/https_upgrade_service.h"

// Singleton that owns all HttpsUpgradeService and associates them with
// ProfileIOS.
class HttpsUpgradeServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<HttpsUpgradeServiceFactory,
                                                HttpsUpgradeService> {
 public:
  explicit HttpsUpgradeServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_SERVICE_FACTORY_H_
