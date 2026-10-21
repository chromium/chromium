// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_REGIONAL_CAPABILITIES_MODEL_REGIONAL_CAPABILITIES_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_REGIONAL_CAPABILITIES_MODEL_REGIONAL_CAPABILITIES_SERVICE_FACTORY_H_

#import "components/regional_capabilities/regional_capabilities_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {

class RegionalCapabilitiesServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          RegionalCapabilitiesServiceFactory,
          regional_capabilities::RegionalCapabilitiesService> {
 public:
  explicit RegionalCapabilitiesServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_REGIONAL_CAPABILITIES_MODEL_REGIONAL_CAPABILITIES_SERVICE_FACTORY_H_
