// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_SERVICE_FACTORY_H_

#import "ios/chrome/browser/backend_promo/model/backend_promo_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all BackendPromoServices and associates them with
// profiles.
class BackendPromoServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<BackendPromoServiceFactory,
                                                BackendPromoService> {
 public:
  explicit BackendPromoServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_SERVICE_FACTORY_H_
