// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/ios_device_authenticator_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/device_reauth/ios_device_authenticator.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

// Singleton that owns all DeviceAuthenticatorProxy and associates them with
// ChromeBrowserState.
class DeviceAuthenticatorProxyFactory : public BrowserStateKeyedServiceFactory {
 public:
  static DeviceAuthenticatorProxyFactory* GetInstance() {
    static base::NoDestructor<DeviceAuthenticatorProxyFactory> instance;
    return instance.get();
  }

  static DeviceAuthenticatorProxy* GetForBrowserState(
      ChromeBrowserState* browser_state) {
    return static_cast<DeviceAuthenticatorProxy*>(
        GetInstance()->GetServiceForBrowserState(browser_state, true));
  }

  DeviceAuthenticatorProxyFactory(const DeviceAuthenticatorProxyFactory&) =
      delete;
  DeviceAuthenticatorProxyFactory& operator=(
      const DeviceAuthenticatorProxyFactory&) = delete;

 private:
  friend class base::NoDestructor<DeviceAuthenticatorProxyFactory>;

  DeviceAuthenticatorProxyFactory()
      : BrowserStateKeyedServiceFactory(
            "DeviceAuthenticatorProxy",
            BrowserStateDependencyManager::GetInstance()) {}
  ~DeviceAuthenticatorProxyFactory() override = default;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override {
    return std::make_unique<DeviceAuthenticatorProxy>();
  }

  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override {
    return GetBrowserStateRedirectedInIncognito(context);
  }
};

std::unique_ptr<IOSDeviceAuthenticator> CreateIOSDeviceAuthenticator(
    id<ReauthenticationProtocol> reauth_module,
    ChromeBrowserState* browser_state,
    const device_reauth::DeviceAuthParams& params) {
  DeviceAuthenticatorProxy* proxy =
      DeviceAuthenticatorProxyFactory::GetInstance()->GetForBrowserState(
          browser_state);
  CHECK(proxy);
  return std::make_unique<IOSDeviceAuthenticator>(reauth_module, proxy, params);
}
