// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_CONNECTOR_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_CONNECTOR_SERVICE_FACTORY_IOS_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace enterprise_connectors {
class DeviceTrustConnectorService;
}  // namespace enterprise_connectors

// Factory for creating `DeviceTrustConnectorService` instances on iOS.
class DeviceTrustConnectorServiceFactoryIOS
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static enterprise_connectors::DeviceTrustConnectorService* GetForProfile(
      ProfileIOS* profile);
  static DeviceTrustConnectorServiceFactoryIOS* GetInstance();

  DeviceTrustConnectorServiceFactoryIOS(
      const DeviceTrustConnectorServiceFactoryIOS&) = delete;
  DeviceTrustConnectorServiceFactoryIOS& operator=(
      const DeviceTrustConnectorServiceFactoryIOS&) = delete;

 private:
  friend class base::NoDestructor<DeviceTrustConnectorServiceFactoryIOS>;

  DeviceTrustConnectorServiceFactoryIOS();
  ~DeviceTrustConnectorServiceFactoryIOS() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_CONNECTOR_SERVICE_FACTORY_IOS_H_
