// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/profile/model/constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

// static
ChromeAccountManagerService*
ChromeAccountManagerServiceFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
ChromeAccountManagerService* ChromeAccountManagerServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<ChromeAccountManagerService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

ChromeAccountManagerServiceFactory*
ChromeAccountManagerServiceFactory::GetInstance() {
  static base::NoDestructor<ChromeAccountManagerServiceFactory> instance;
  return instance.get();
}

ChromeAccountManagerServiceFactory::ChromeAccountManagerServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ChromeAccountManagerService",
          BrowserStateDependencyManager::GetInstance()) {}

ChromeAccountManagerServiceFactory::~ChromeAccountManagerServiceFactory() =
    default;

std::unique_ptr<KeyedService>
ChromeAccountManagerServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<ChromeAccountManagerService>(
      GetApplicationContext()->GetLocalState());
}
