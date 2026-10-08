// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/model/ios_device_authenticator_factory.h"

#import "ios/chrome/browser/device_reauth/model/ios_device_auth_availability_checker.h"
#import "ios/chrome/browser/device_reauth/model/ios_device_authenticator.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

DeviceAuthenticatorProxyFactory::DeviceAuthenticatorProxyFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "DeviceAuthenticatorProxy",
          ProfileSelection::kRedirectedInIncognito) {}

std::unique_ptr<KeyedService>
DeviceAuthenticatorProxyFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<DeviceAuthenticatorProxy>();
}

std::unique_ptr<device_reauth::DeviceAuthAvailabilityChecker>
CreateIOSDeviceAuthAvailabilityChecker(
    id<ReauthenticationProtocol> reauth_module) {
  return std::make_unique<IOSDeviceAuthAvailabilityChecker>(reauth_module);
}

std::unique_ptr<IOSDeviceAuthenticator> CreateIOSDeviceAuthenticator(
    id<ReauthenticationProtocol> reauth_module,
    ProfileIOS* profile,
    const device_reauth::DeviceAuthParams& params) {
  DeviceAuthenticatorProxy* proxy =
      DeviceAuthenticatorProxyFactory::GetForProfile(profile);
  CHECK(proxy);
  return std::make_unique<IOSDeviceAuthenticator>(reauth_module, proxy, params);
}
