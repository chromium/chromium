// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_CONNECTOR_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_CONNECTOR_SERVICE_FACTORY_IOS_H_

#import <memory>

#import "components/enterprise/device_trust/core/device_trust_connector_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Factory for creating `DeviceTrustConnectorService` instances on iOS.
class DeviceTrustConnectorServiceFactoryIOS final
    : public TypedProfileKeyedServiceFactoryIOS<
          DeviceTrustConnectorServiceFactoryIOS,
          enterprise_connectors::DeviceTrustConnectorService> {
 public:
  explicit DeviceTrustConnectorServiceFactoryIOS(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_CONNECTOR_SERVICE_FACTORY_IOS_H_
