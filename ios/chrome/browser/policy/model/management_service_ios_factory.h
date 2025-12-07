// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_MANAGEMENT_SERVICE_IOS_FACTORY_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_MANAGEMENT_SERVICE_IOS_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace policy {

class ManagementServiceIOS;
class ManagementService;

class ManagementServiceIOSFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static ManagementServiceIOSFactory* GetInstance();
  static ManagementServiceIOS* GetForProfile(ProfileIOS* profile);
  static ManagementService* GetForPlatform();

 private:
  friend class base::NoDestructor<ManagementServiceIOSFactory>;

  ManagementServiceIOSFactory();
  ~ManagementServiceIOSFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_MANAGEMENT_SERVICE_IOS_FACTORY_H_
