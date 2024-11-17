// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_password_manager_settings_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/password_manager/core/browser/password_manager_settings_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state.h"

IOSPasswordManagerSettingsServiceFactory::
    IOSPasswordManagerSettingsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordManagerSettingsService",
          BrowserStateDependencyManager::GetInstance()) {}

IOSPasswordManagerSettingsServiceFactory::
    ~IOSPasswordManagerSettingsServiceFactory() = default;

// static
IOSPasswordManagerSettingsServiceFactory*
IOSPasswordManagerSettingsServiceFactory::GetInstance() {
  static base::NoDestructor<IOSPasswordManagerSettingsServiceFactory> instance;
  return instance.get();
}

// static
password_manager::PasswordManagerSettingsService*
IOSPasswordManagerSettingsServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<password_manager::PasswordManagerSettingsService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

std::unique_ptr<KeyedService>
IOSPasswordManagerSettingsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  return std::make_unique<password_manager::PasswordManagerSettingsServiceImpl>(
      profile->GetPrefs());
}
