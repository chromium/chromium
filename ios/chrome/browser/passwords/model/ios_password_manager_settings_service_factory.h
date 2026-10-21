// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_MANAGER_SETTINGS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_MANAGER_SETTINGS_SERVICE_FACTORY_H_

#import "components/password_manager/core/browser/password_manager_settings_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Factory for retrieving password manager settings.
class IOSPasswordManagerSettingsServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSPasswordManagerSettingsServiceFactory,
          password_manager::PasswordManagerSettingsService> {
 public:
  explicit IOSPasswordManagerSettingsServiceFactory(PassKey key);

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_MANAGER_SETTINGS_SERVICE_FACTORY_H_
