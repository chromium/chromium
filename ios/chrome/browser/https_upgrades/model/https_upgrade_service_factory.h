// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_impl.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"

// Singleton that owns all HttpsUpgradeService and associates them with
// ChromeBrowserState.
class HttpsUpgradeServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static HttpsUpgradeService* GetForBrowserState(
      web::BrowserState* browser_state);
  static HttpsUpgradeServiceFactory* GetInstance();

  HttpsUpgradeServiceFactory(const HttpsUpgradeServiceFactory&) = delete;
  HttpsUpgradeServiceFactory& operator=(const HttpsUpgradeServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<HttpsUpgradeServiceFactory>;

  HttpsUpgradeServiceFactory();
  ~HttpsUpgradeServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_SERVICE_FACTORY_H_
