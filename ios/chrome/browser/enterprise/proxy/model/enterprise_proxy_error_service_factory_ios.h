// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_ERROR_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_ERROR_SERVICE_FACTORY_IOS_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace enterprise_net {
class EnterpriseProxyErrorService;
}  // namespace enterprise_net

// Singleton that owns all `EnterpriseProxyErrorService` instances and
// associates them with `ProfileIOS`.
class EnterpriseProxyErrorServiceFactoryIOS
    : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the `EnterpriseProxyErrorService` for the given `profile`, or
  // `nullptr` for Incognito profiles or if dynamic route fetching is disabled.
  static enterprise_net::EnterpriseProxyErrorService* GetForProfile(
      ProfileIOS* profile);

  // Returns the singleton instance of `EnterpriseProxyErrorServiceFactoryIOS`.
  static EnterpriseProxyErrorServiceFactoryIOS* GetInstance();

  // Returns the default factory used to build `EnterpriseProxyErrorService`.
  // Can be registered with `AddTestingFactory` to use real instances during
  // testing.
  static TestingFactory GetDefaultFactory();

  EnterpriseProxyErrorServiceFactoryIOS(
      const EnterpriseProxyErrorServiceFactoryIOS&) = delete;
  EnterpriseProxyErrorServiceFactoryIOS& operator=(
      const EnterpriseProxyErrorServiceFactoryIOS&) = delete;

 private:
  friend class base::NoDestructor<EnterpriseProxyErrorServiceFactoryIOS>;

  EnterpriseProxyErrorServiceFactoryIOS();
  ~EnterpriseProxyErrorServiceFactoryIOS() override;

  // `ProfileKeyedServiceFactoryIOS` implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_ERROR_SERVICE_FACTORY_IOS_H_
