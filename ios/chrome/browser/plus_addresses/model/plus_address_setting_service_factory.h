// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_MODEL_PLUS_ADDRESS_SETTING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_MODEL_PLUS_ADDRESS_SETTING_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace plus_addresses {
class PlusAddressSettingService;
}

// Factory responsible for creating `PlusAddressSettingService`, which is
// responsible for managing settings synced via `syncer::PLUS_ADDRESS_SETTING`.
class PlusAddressSettingServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static PlusAddressSettingServiceFactory* GetInstance();
  static plus_addresses::PlusAddressSettingService* GetForBrowserState(
      ChromeBrowserState* browser_state);

 private:
  friend class base::NoDestructor<PlusAddressSettingServiceFactory>;
  PlusAddressSettingServiceFactory();
  ~PlusAddressSettingServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_MODEL_PLUS_ADDRESS_SETTING_SERVICE_FACTORY_H_
