// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_password_manager_settings_service_factory.h"

#import <utility>

#import "components/password_manager/core/browser/password_manager_settings_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

IOSPasswordManagerSettingsServiceFactory::
    IOSPasswordManagerSettingsServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "PasswordManagerSettingsService") {}

std::unique_ptr<KeyedService>
IOSPasswordManagerSettingsServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<password_manager::PasswordManagerSettingsServiceImpl>(
      profile->GetPrefs());
}
