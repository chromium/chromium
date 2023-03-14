// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_CLIENT_FACTORY_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_CLIENT_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class KidsChromeManagementClient;

class ChromeBrowserState;

// Singleton that owns KidsChromeManagementClient object and associates
// them with ChromeBrowserState.
class KidsChromeManagementClientFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static KidsChromeManagementClient* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static KidsChromeManagementClientFactory* GetInstance();

  KidsChromeManagementClientFactory(const KidsChromeManagementClientFactory&) =
      delete;
  KidsChromeManagementClientFactory& operator=(
      const KidsChromeManagementClientFactory&) = delete;

 private:
  friend class base::NoDestructor<KidsChromeManagementClientFactory>;

  KidsChromeManagementClientFactory();
  ~KidsChromeManagementClientFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  // TODO(b/264669964): Remove this after preferences are moved across
  // platforms.
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_CLIENT_FACTORY_H_
