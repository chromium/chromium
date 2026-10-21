// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_SERVICE_FACTORY_H_

#import <memory>

#import "components/webauthn/core/browser/device_authorization/device_authorization_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that associates `DeviceAuthorizationService` to Profiles.
class IOSDeviceAuthorizationServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSDeviceAuthorizationServiceFactory,
          webauthn::DeviceAuthorizationService> {
 public:
  explicit IOSDeviceAuthorizationServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_SERVICE_FACTORY_H_
