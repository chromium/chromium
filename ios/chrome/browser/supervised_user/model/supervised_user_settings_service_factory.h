// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SETTINGS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SETTINGS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace supervised_user {
class SupervisedUserSettingsService;
}  // namespace supervised_user

// Singleton that owns SupervisedUserSettingsService object and associates
// them with ProfileIOS.
class SupervisedUserSettingsServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static supervised_user::SupervisedUserSettingsService* GetForProfile(
      ProfileIOS* profile);
  static SupervisedUserSettingsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SupervisedUserSettingsServiceFactory>;

  SupervisedUserSettingsServiceFactory();
  ~SupervisedUserSettingsServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  bool ServiceIsRequiredForContextInitialization() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SETTINGS_SERVICE_FACTORY_H_
