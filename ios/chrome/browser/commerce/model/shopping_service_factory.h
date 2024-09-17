// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_MODEL_SHOPPING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_COMMERCE_MODEL_SHOPPING_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace commerce {

class ShoppingService;

class ShoppingServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  ShoppingServiceFactory(const ShoppingServiceFactory&) = delete;
  ShoppingServiceFactory& operator=(const ShoppingServiceFactory&) = delete;

  static ShoppingServiceFactory* GetInstance();

  static ShoppingService* GetForProfile(ProfileIOS* profile);
  static ShoppingService* GetForProfileIfExists(ProfileIOS* profile);

  // Deprecated: use GetForProfile(...)/GetForProfileIfExists(...).
  static ShoppingService* GetForBrowserState(ProfileIOS* profile);
  static ShoppingService* GetForBrowserStateIfExists(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<ShoppingServiceFactory>;

  ShoppingServiceFactory();
  ~ShoppingServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* state) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace commerce

#endif  // IOS_CHROME_BROWSER_COMMERCE_MODEL_SHOPPING_SERVICE_FACTORY_H_
