// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_MODEL_PLUS_ADDRESS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_MODEL_PLUS_ADDRESS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace plus_addresses {
class PlusAddressService;
}

// Associates PlusAddressService instance with ProfileIOS.
class PlusAddressServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the PlusAddressService associated with `profile`.
  static plus_addresses::PlusAddressService* GetForProfile(ProfileIOS* profile);

  // Returns the factory instance.
  static PlusAddressServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<PlusAddressServiceFactory>;

  PlusAddressServiceFactory();
  ~PlusAddressServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_MODEL_PLUS_ADDRESS_SERVICE_FACTORY_H_
