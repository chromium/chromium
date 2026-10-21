// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_FACTORY_H_

#import "ios/chrome/browser/enterprise/proxy/model/proxy_service_controller.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all `ProxyServiceController` instances and associates
// them with `ProfileIOS`.
class ProxyServiceControllerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<ProxyServiceControllerFactory,
                                                ProxyServiceController> {
 public:
  explicit ProxyServiceControllerFactory(PassKey key);

  // Returns the default factory used to build `ProxyServiceController`. Can be
  // registered with `AddTestingFactory` to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  // `ProfileKeyedServiceFactoryIOS` implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_FACTORY_H_
