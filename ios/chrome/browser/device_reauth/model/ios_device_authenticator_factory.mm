// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/model/ios_device_authenticator_factory.h"

#import "ios/chrome/browser/device_reauth/model/ios_device_authenticator.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
DeviceAuthenticatorProxyFactory*
DeviceAuthenticatorProxyFactory::GetInstance() {
  static base::NoDestructor<DeviceAuthenticatorProxyFactory> instance;
  return instance.get();
}

// static
DeviceAuthenticatorProxy* DeviceAuthenticatorProxyFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<DeviceAuthenticatorProxy>(
      profile, /*create=*/true);
}

DeviceAuthenticatorProxyFactory::DeviceAuthenticatorProxyFactory()
    : ProfileKeyedServiceFactoryIOS("DeviceAuthenticatorProxy",
                                    ProfileSelection::kRedirectedInIncognito) {}

DeviceAuthenticatorProxyFactory::~DeviceAuthenticatorProxyFactory() = default;

std::unique_ptr<KeyedService>
DeviceAuthenticatorProxyFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<DeviceAuthenticatorProxy>();
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
