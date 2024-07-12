// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/browser_state/model/constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
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
  ChromeAccountManagerService::VisibleIdentities visible_identities =
      ChromeAccountManagerService::VisibleIdentities::kAll;
  if (experimental_flags::DisplaySwitchProfile().has_value()) {
    // When the multiple profile featuer is enabled, the default profile should
    // list only non managed identities. And other profiles should list only
    // managed identities.
    // TODO(crbug.com/331783685): Need a better implementation to avoid using
    // the profile name and a better mapping account<->profile mapping.
    ChromeBrowserState* chrome_browser_stsate =
        static_cast<ChromeBrowserState*>(context);
    std::string profile_name = chrome_browser_stsate->GetBrowserStateName();
    if (profile_name == kIOSChromeInitialBrowserState) {
      visible_identities =
          ChromeAccountManagerService::VisibleIdentities::kNonManagedOnly;
    } else {
      visible_identities =
          ChromeAccountManagerService::VisibleIdentities::kManagedOnly;
    }
  }
  return std::make_unique<ChromeAccountManagerService>(
      GetApplicationContext()->GetLocalState(), visible_identities);
}
