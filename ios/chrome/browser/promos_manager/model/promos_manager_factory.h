// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMOS_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMOS_MANAGER_FACTORY_H_

#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all PromosManagers and associates them with a Profile.
class PromosManagerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<PromosManagerFactory,
                                                PromosManager> {
 public:
  explicit PromosManagerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMOS_MANAGER_FACTORY_H_
