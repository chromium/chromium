// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/ios_device_authenticator_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/device_reauth/ios_device_authenticator.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
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
  return static_cast<DeviceAuthenticatorProxy*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

DeviceAuthenticatorProxyFactory::DeviceAuthenticatorProxyFactory()
    : BrowserStateKeyedServiceFactory(
          "DeviceAuthenticatorProxy",
          BrowserStateDependencyManager::GetInstance()) {}

DeviceAuthenticatorProxyFactory::~DeviceAuthenticatorProxyFactory() = default;

std::unique_ptr<KeyedService>
DeviceAuthenticatorProxyFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<DeviceAuthenticatorProxy>();
}

web::BrowserState* DeviceAuthenticatorProxyFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

std::unique_ptr<IOSDeviceAuthenticator> CreateIOSDeviceAuthenticator(
    id<ReauthenticationProtocol> reauth_module,
    ProfileIOS* profile,
    const device_reauth::DeviceAuthParams& params) {
  DeviceAuthenticatorProxy* proxy =
      DeviceAuthenticatorProxyFactory::GetInstance()->GetForProfile(profile);
  CHECK(proxy);
  return std::make_unique<IOSDeviceAuthenticator>(reauth_module, proxy, params);
}
