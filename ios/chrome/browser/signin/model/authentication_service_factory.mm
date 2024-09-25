// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

#import <memory>
#import <utility>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace {

std::unique_ptr<KeyedService> BuildAuthenticationService(
    web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<AuthenticationService>(
      profile->GetPrefs(),
      ChromeAccountManagerServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile));
}

}  // namespace

// static
AuthenticationService* AuthenticationServiceFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
AuthenticationService* AuthenticationServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  AuthenticationService* service = static_cast<AuthenticationService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
  CHECK(!service || service->initialized());
  return service;
}

// static
AuthenticationServiceFactory* AuthenticationServiceFactory::GetInstance() {
  static base::NoDestructor<AuthenticationServiceFactory> instance;
  return instance.get();
}

void AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
    ProfileIOS* profile,
    std::unique_ptr<AuthenticationServiceDelegate> delegate) {
  CreateAndInitializeForProfile(profile, std::move(delegate));
}

// static
void AuthenticationServiceFactory::CreateAndInitializeForProfile(
    ProfileIOS* profile,
    std::unique_ptr<AuthenticationServiceDelegate> delegate) {
  AuthenticationService* service = static_cast<AuthenticationService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
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
