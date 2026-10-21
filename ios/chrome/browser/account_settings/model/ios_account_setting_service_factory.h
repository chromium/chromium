// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_SETTINGS_MODEL_IOS_ACCOUNT_SETTING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_ACCOUNT_SETTINGS_MODEL_IOS_ACCOUNT_SETTING_SERVICE_FACTORY_H_

#import "components/account_settings/account_setting_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all AccountSettingServices and associates them with
// ProfileIOS.
class IOSAccountSettingServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSAccountSettingServiceFactory,
          account_settings::AccountSettingService> {
 public:
  explicit IOSAccountSettingServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_ACCOUNT_SETTINGS_MODEL_IOS_ACCOUNT_SETTING_SERVICE_FACTORY_H_
