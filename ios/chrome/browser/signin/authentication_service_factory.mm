// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/authentication_service_factory.h"

#import <memory>
#import <utility>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_delegate.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_setup_service_factory.h"

namespace {

std::unique_ptr<KeyedService> BuildAuthenticationService(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<AuthenticationService>(
      browser_state->GetPrefs(),
      SyncSetupServiceFactory::GetForBrowserState(browser_state),
      ChromeAccountManagerServiceFactory::GetForBrowserState(browser_state),
      IdentityManagerFactory::GetForBrowserState(browser_state),
      SyncServiceFactory::GetForBrowserState(browser_state));
}

}  // namespace

// static
AuthenticationService* AuthenticationServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  AuthenticationService* service = static_cast<AuthenticationService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
  CHECK(!service || service->initialized());
  return service;
}

// static
AuthenticationServiceFactory* AuthenticationServiceFactory::GetInstance() {
  static base::NoDestructor<AuthenticationServiceFactory> instance;
  return instance.get();
}

// static
void AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
    ChromeBrowserState* browser_state,
    std::unique_ptr<AuthenticationServiceDelegate> delegate) {
  AuthenticationService* service = static_cast<AuthenticationService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
  CHECK(service && !service->initialized());
  service->Initialize(std::move(delegate));
}

// static
AuthenticationServiceFactory::TestingFactory
AuthenticationServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildAuthenticationService);
}

AuthenticationServiceFactory::AuthenticationServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "AuthenticationService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncSetupServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

AuthenticationServiceFactory::~AuthenticationServiceFactory() {}

std::unique_ptr<KeyedService>
AuthenticationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildAuthenticationService(context);
}

void AuthenticationServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AuthenticationService::RegisterPrefs(registry);
}

bool AuthenticationServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
