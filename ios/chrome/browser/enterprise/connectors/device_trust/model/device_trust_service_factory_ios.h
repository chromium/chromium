// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_SERVICE_FACTORY_IOS_H_

#import <memory>

#import "components/enterprise/device_trust/core/device_trust_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Factory for creating `DeviceTrustService` instances on iOS.
class DeviceTrustServiceFactoryIOS final
    : public TypedProfileKeyedServiceFactoryIOS<
          DeviceTrustServiceFactoryIOS,
          enterprise_connectors::DeviceTrustService> {
 public:
  explicit DeviceTrustServiceFactoryIOS(PassKey key);

  // Returns the production factory for tests that explicitly need the real
  // `DeviceTrustService` object graph.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_SERVICE_FACTORY_IOS_H_
