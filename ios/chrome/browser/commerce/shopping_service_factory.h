// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_SHOPPING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_COMMERCE_SHOPPING_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace commerce {

class ShoppingService;

class ShoppingServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  ShoppingServiceFactory(const ShoppingServiceFactory&) = delete;
  ShoppingServiceFactory& operator=(const ShoppingServiceFactory&) = delete;

  static ShoppingServiceFactory* GetInstance();

  static ShoppingService* GetForBrowserState(web::BrowserState* state);
  static ShoppingService* GetForBrowserStateIfExists(web::BrowserState* state);

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

#endif  // IOS_CHROME_BROWSER_COMMERCE_SHOPPING_SERVICE_FACTORY_H_
