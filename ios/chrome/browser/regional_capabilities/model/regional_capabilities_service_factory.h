// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_REGIONAL_CAPABILITIES_MODEL_REGIONAL_CAPABILITIES_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_REGIONAL_CAPABILITIES_MODEL_REGIONAL_CAPABILITIES_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace regional_capabilities {
class RegionalCapabilitiesService;
}

namespace ios {

class RegionalCapabilitiesServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static regional_capabilities::RegionalCapabilitiesService* GetForProfile(
      ProfileIOS* profile);
  static RegionalCapabilitiesServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<RegionalCapabilitiesServiceFactory>;

  RegionalCapabilitiesServiceFactory();
  ~RegionalCapabilitiesServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_REGIONAL_CAPABILITIES_MODEL_REGIONAL_CAPABILITIES_SERVICE_FACTORY_H_
