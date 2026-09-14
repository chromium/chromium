// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_SERVICE_FACTORY_IOS_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace enterprise_net {
class EnterpriseProxyService;
}  // namespace enterprise_net

// Singleton that owns all `EnterpriseProxyService` instances and associates
// them with `ProfileIOS`.
class EnterpriseProxyServiceFactoryIOS : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the `EnterpriseProxyService` for the given `profile`, or `nullptr`
  // for Incognito profiles or if dynamic route fetching is disabled.
  static enterprise_net::EnterpriseProxyService* GetForProfile(
      ProfileIOS* profile);

  // Returns the singleton instance of `EnterpriseProxyServiceFactoryIOS`.
  static EnterpriseProxyServiceFactoryIOS* GetInstance();

  // Returns the default factory used to build `EnterpriseProxyService`. Can be
  // registered with `AddTestingFactory` to use real instances during testing.
  static TestingFactory GetDefaultFactory();

  EnterpriseProxyServiceFactoryIOS(const EnterpriseProxyServiceFactoryIOS&) =
      delete;
  EnterpriseProxyServiceFactoryIOS& operator=(
      const EnterpriseProxyServiceFactoryIOS&) = delete;

 private:
  friend class base::NoDestructor<EnterpriseProxyServiceFactoryIOS>;

  EnterpriseProxyServiceFactoryIOS();
  ~EnterpriseProxyServiceFactoryIOS() override;

  // `ProfileKeyedServiceFactoryIOS` implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_SERVICE_FACTORY_IOS_H_
