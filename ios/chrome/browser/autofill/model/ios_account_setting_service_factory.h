// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_ACCOUNT_SETTING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_ACCOUNT_SETTING_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace autofill {
class AccountSettingService;
}

// Singleton that owns all AccountSettingServices and associates them with
// ProfileIOS.
class IOSAccountSettingServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static IOSAccountSettingServiceFactory* GetInstance();
  static autofill::AccountSettingService* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<IOSAccountSettingServiceFactory>;

  IOSAccountSettingServiceFactory();
  ~IOSAccountSettingServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_ACCOUNT_SETTING_SERVICE_FACTORY_H_
