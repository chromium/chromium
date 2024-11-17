// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
supervised_user::SupervisedUserSettingsService*
SupervisedUserSettingsServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<supervised_user::SupervisedUserSettingsService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
SupervisedUserSettingsServiceFactory*
SupervisedUserSettingsServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserSettingsServiceFactory> instance;
  return instance.get();
}

SupervisedUserSettingsServiceFactory::SupervisedUserSettingsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SupervisedUserSettingsService",
          BrowserStateDependencyManager::GetInstance()) {}

bool SupervisedUserSettingsServiceFactory::
    ServiceIsRequiredForContextInitialization() const {
  // SupervisedUserSettingsService is required to initialize the PrefService
  // of the ProfileIOS as it is part of the implementation of the
  // SupervisedUserPrefStore.
  return true;
}

std::unique_ptr<KeyedService>
SupervisedUserSettingsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<supervised_user::SupervisedUserSettingsService>();
}

web::BrowserState* SupervisedUserSettingsServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
