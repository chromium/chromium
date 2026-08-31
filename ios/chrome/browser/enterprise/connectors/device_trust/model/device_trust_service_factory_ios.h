// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_SERVICE_FACTORY_IOS_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace enterprise_connectors {
class DeviceTrustService;
}  // namespace enterprise_connectors

// Factory for creating `DeviceTrustService` instances on iOS.
class DeviceTrustServiceFactoryIOS : public ProfileKeyedServiceFactoryIOS {
 public:
  static enterprise_connectors::DeviceTrustService* GetForProfile(
      ProfileIOS* profile);
  static DeviceTrustServiceFactoryIOS* GetInstance();

  // Returns the production factory for tests that explicitly need the real
  // `DeviceTrustService` object graph.
  static TestingFactory GetDefaultFactory();

  DeviceTrustServiceFactoryIOS(const DeviceTrustServiceFactoryIOS&) = delete;
  DeviceTrustServiceFactoryIOS& operator=(const DeviceTrustServiceFactoryIOS&) =
      delete;

 private:
  friend class base::NoDestructor<DeviceTrustServiceFactoryIOS>;

  DeviceTrustServiceFactoryIOS();
  ~DeviceTrustServiceFactoryIOS() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_SERVICE_FACTORY_IOS_H_
