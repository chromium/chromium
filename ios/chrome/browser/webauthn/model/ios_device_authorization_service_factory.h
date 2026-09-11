// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace webauthn {
class DeviceAuthorizationService;
}  // namespace webauthn

// Singleton that associates `DeviceAuthorizationService` to Profiles.
class IOSDeviceAuthorizationServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static webauthn::DeviceAuthorizationService* GetForProfile(
      ProfileIOS* profile);
  static IOSDeviceAuthorizationServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSDeviceAuthorizationServiceFactory>;

  IOSDeviceAuthorizationServiceFactory();
  ~IOSDeviceAuthorizationServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_SERVICE_FACTORY_H_
