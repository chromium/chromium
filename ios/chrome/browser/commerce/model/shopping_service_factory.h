// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_MODEL_SHOPPING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_COMMERCE_MODEL_SHOPPING_SERVICE_FACTORY_H_

#include <memory>

#include "components/commerce/core/shopping_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace commerce {

// Owns all ShoppingService instances and associates them to profiles.
class ShoppingServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<ShoppingServiceFactory,
                                                ShoppingService> {
 public:
  explicit ShoppingServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace commerce

#endif  // IOS_CHROME_BROWSER_COMMERCE_MODEL_SHOPPING_SERVICE_FACTORY_H_
