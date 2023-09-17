// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/account_consistency_service_factory.h"

#import "base/no_destructor.h"
#import "components/content_settings/core/browser/cookie_settings.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#import "ios/chrome/browser/content_settings/model/cookie_settings_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/account_reconcilor_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"

namespace ios {

AccountConsistencyServiceFactory::AccountConsistencyServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "AccountConsistencyService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::AccountReconcilorFactory::GetInstance());
  DependsOn(ios::CookieSettingsFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

AccountConsistencyServiceFactory::~AccountConsistencyServiceFactory() {}

// static
AccountConsistencyService* AccountConsistencyServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<AccountConsistencyService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
AccountConsistencyServiceFactory*
AccountConsistencyServiceFactory::GetInstance() {
  static base::NoDestructor<AccountConsistencyServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
AccountConsistencyServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<AccountConsistencyService>(
      chrome_browser_state,
      ios::AccountReconcilorFactory::GetForBrowserState(chrome_browser_state),
      ios::CookieSettingsFactory::GetForBrowserState(chrome_browser_state),
      IdentityManagerFactory::GetForBrowserState(chrome_browser_state));
}

}  // namespace ios
