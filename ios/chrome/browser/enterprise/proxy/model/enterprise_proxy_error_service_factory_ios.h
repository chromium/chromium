// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_ERROR_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_ERROR_SERVICE_FACTORY_IOS_H_

#import "components/enterprise/net/core/enterprise_proxy_error_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all `EnterpriseProxyErrorService` instances and
// associates them with `ProfileIOS`.
class EnterpriseProxyErrorServiceFactoryIOS final
    : public TypedProfileKeyedServiceFactoryIOS<
          EnterpriseProxyErrorServiceFactoryIOS,
          enterprise_net::EnterpriseProxyErrorService> {
 public:
  explicit EnterpriseProxyErrorServiceFactoryIOS(PassKey key);

  // Returns the default factory used to build `EnterpriseProxyErrorService`.
  // Can be registered with `AddTestingFactory` to use real instances during
  // testing.
  static TestingFactory GetDefaultFactory();

 private:
  // `ProfileKeyedServiceFactoryIOS` implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_ERROR_SERVICE_FACTORY_IOS_H_
