// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class ProxyServiceController;

// Singleton that owns all `ProxyServiceController` instances and associates
// them with `ProfileIOS`.
class ProxyServiceControllerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static ProxyServiceController* GetForProfile(ProfileIOS* profile);

  static ProxyServiceControllerFactory* GetInstance();

 private:
  friend class base::NoDestructor<ProxyServiceControllerFactory>;

  ProxyServiceControllerFactory();
  ~ProxyServiceControllerFactory() override;

  // `ProfileKeyedServiceFactoryIOS` implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_FACTORY_H_
