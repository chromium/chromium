// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMOS_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMOS_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class PromosManager;

// Singleton that owns all PromosManagers and associates them with a Profile.
class PromosManagerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static PromosManager* GetForProfile(ProfileIOS* profile);
  static PromosManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<PromosManagerFactory>;

  PromosManagerFactory();
  ~PromosManagerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMOS_MANAGER_FACTORY_H_
