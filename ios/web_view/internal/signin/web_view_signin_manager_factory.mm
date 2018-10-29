// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/web_view_signin_manager_factory.h"

#include <utility>

#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/signin/ios_web_view_signin_client.h"
#include "ios/web_view/internal/signin/web_view_account_tracker_service_factory.h"
#include "ios/web_view/internal/signin/web_view_gaia_cookie_manager_service_factory.h"
#include "ios/web_view/internal/signin/web_view_oauth2_token_service_factory.h"
#include "ios/web_view/internal/signin/web_view_signin_client_factory.h"
#include "ios/web_view/internal/signin/web_view_signin_error_controller_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewSigninManagerFactory::WebViewSigninManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "SigninManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewSigninClientFactory::GetInstance());
  DependsOn(WebViewGaiaCookieManagerServiceFactory::GetInstance());
  DependsOn(WebViewOAuth2TokenServiceFactory::GetInstance());
  DependsOn(WebViewAccountTrackerServiceFactory::GetInstance());
  DependsOn(WebViewSigninErrorControllerFactory::GetInstance());
}

// static
SigninManager* WebViewSigninManagerFactory::GetForBrowserState(
    ios_web_view::WebViewBrowserState* browser_state) {
  return static_cast<SigninManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
SigninManager* WebViewSigninManagerFactory::GetForBrowserStateIfExists(
    ios_web_view::WebViewBrowserState* browser_state) {
  return static_cast<SigninManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
WebViewSigninManagerFactory* WebViewSigninManagerFactory::GetInstance() {
  return base::Singleton<WebViewSigninManagerFactory>::get();
}

void WebViewSigninManagerFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SigninManagerBase::RegisterProfilePrefs(registry);
}

std::unique_ptr<KeyedService>
WebViewSigninManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);

  // Clearing the sign in state on start up greatly simplifies the management of
  // ChromeWebView's signin state.
  PrefService* pref_service = browser_state->GetPrefs();
  pref_service->ClearPref(prefs::kGoogleServicesAccountId);
  pref_service->ClearPref(prefs::kGoogleServicesUsername);
  pref_service->ClearPref(prefs::kGoogleServicesUserAccountId);

  std::unique_ptr<SigninManager> service = std::make_unique<SigninManager>(
      WebViewSigninClientFactory::GetForBrowserState(browser_state),
      WebViewOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
      WebViewAccountTrackerServiceFactory::GetForBrowserState(browser_state),
      WebViewGaiaCookieManagerServiceFactory::GetForBrowserState(browser_state),
      WebViewSigninErrorControllerFactory::GetForBrowserState(browser_state),
      signin::AccountConsistencyMethod::kDisabled);
  service->Initialize(ApplicationContext::GetInstance()->GetLocalState());
  return service;
}

void WebViewSigninManagerFactory::BrowserStateShutdown(
    web::BrowserState* context) {
  BrowserStateKeyedServiceFactory::BrowserStateShutdown(context);
}

}  // namespace ios_web_view
