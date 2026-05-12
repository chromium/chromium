// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class BackendPromoService;

// Singleton that owns all BackendPromoServices and associates them with
// profiles.
class BackendPromoServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static BackendPromoService* GetForProfile(ProfileIOS* profile);
  static BackendPromoServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<BackendPromoServiceFactory>;

  BackendPromoServiceFactory();
  ~BackendPromoServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_SERVICE_FACTORY_H_
