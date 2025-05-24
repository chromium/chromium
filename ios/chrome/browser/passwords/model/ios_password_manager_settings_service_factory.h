// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_MANAGER_SETTINGS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_MANAGER_SETTINGS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace password_manager {
class PasswordManagerSettingsService;
}  // namespace password_manager

// Factory for retrieving password manager settings.
class IOSPasswordManagerSettingsServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static password_manager::PasswordManagerSettingsService* GetForProfile(
      ProfileIOS* profile);
  static IOSPasswordManagerSettingsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSPasswordManagerSettingsServiceFactory>;

  IOSPasswordManagerSettingsServiceFactory();
  ~IOSPasswordManagerSettingsServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_MANAGER_SETTINGS_SERVICE_FACTORY_H_
