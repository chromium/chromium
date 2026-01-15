// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_MANAGEMENT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_MANAGEMENT_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace policy {

class BrowserManagementService;
class ManagementService;

class BrowserManagementServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static BrowserManagementServiceFactory* GetInstance();
  static BrowserManagementService* GetForProfile(ProfileIOS* profile);
  static ManagementService* GetForPlatform();

 private:
  friend class base::NoDestructor<BrowserManagementServiceFactory>;

  BrowserManagementServiceFactory();
  ~BrowserManagementServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_MANAGEMENT_SERVICE_FACTORY_H_
