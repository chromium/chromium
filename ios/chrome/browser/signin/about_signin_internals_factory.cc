// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/about_signin_internals_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/account_reconcilor_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"
#include "ios/chrome/browser/signin/signin_error_controller_factory.h"

namespace ios {

AboutSigninInternalsFactory::AboutSigninInternalsFactory()
    : BrowserStateKeyedServiceFactory(
          "AboutSigninInternals",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(AccountReconcilorFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SigninClientFactory::GetInstance());
  DependsOn(SigninErrorControllerFactory::GetInstance());
}

AboutSigninInternalsFactory::~AboutSigninInternalsFactory() {}

// static
AboutSigninInternals* AboutSigninInternalsFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<AboutSigninInternals*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
AboutSigninInternalsFactory* AboutSigninInternalsFactory::GetInstance() {
  static base::NoDestructor<AboutSigninInternalsFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
AboutSigninInternalsFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);
  std::unique_ptr<AboutSigninInternals> service(new AboutSigninInternals(
      IdentityManagerFactory::GetForBrowserState(chrome_browser_state),
      SigninErrorControllerFactory::GetForBrowserState(chrome_browser_state),
      signin::AccountConsistencyMethod::kMirror,
      SigninClientFactory::GetForBrowserState(chrome_browser_state),
      AccountReconcilorFactory::GetForBrowserState(chrome_browser_state)));
  return service;
}

void AboutSigninInternalsFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  AboutSigninInternals::RegisterPrefs(user_prefs);
}

}  // namespace ios
