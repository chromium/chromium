// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/browser_state/model/constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

ChromeAccountManagerService*
ChromeAccountManagerServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<ChromeAccountManagerService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
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
  ChromeBrowserState* chrome_browser_state =
      static_cast<ChromeBrowserState*>(context);
  return std::make_unique<ChromeAccountManagerService>(
      GetApplicationContext()->GetLocalState(),
      chrome_browser_state->GetProfileName());
}
