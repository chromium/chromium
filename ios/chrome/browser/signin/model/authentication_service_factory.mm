// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

#import <memory>
#import <utility>

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_delegate_impl.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace {

std::unique_ptr<KeyedService> BuildAuthenticationService(
    std::unique_ptr<AuthenticationServiceDelegate> delegate,
    ProfileIOS* profile) {
  auto service = std::make_unique<AuthenticationService>(
      profile, profile->GetPrefs(),
      ChromeAccountManagerServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile));
  service->Initialize(std::move(delegate));
  DCHECK(service->initialized());
  return service;
}

}  // namespace

// static
AuthenticationService* AuthenticationServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<AuthenticationService>(
      profile, /*create=*/true);
}

// static
AuthenticationServiceFactory* AuthenticationServiceFactory::GetInstance() {
  static base::NoDestructor<AuthenticationServiceFactory> instance;
  return instance.get();
}

// static
AuthenticationServiceFactory::TestingFactory
AuthenticationServiceFactory::GetFactoryWithDelegate(
    std::unique_ptr<AuthenticationServiceDelegate> delegate) {
  return GetFactoryWithDelegateForTesting(std::move(delegate));
}

// static
AuthenticationServiceFactory::TestingFactory
AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
    std::unique_ptr<AuthenticationServiceDelegate> delegate) {
  return base::BindOnce(&BuildAuthenticationService, std::move(delegate));
}

AuthenticationServiceFactory::AuthenticationServiceFactory()
    : ProfileKeyedServiceFactoryIOS("AuthenticationService",
                                    ProfileSelection::kNoInstanceInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(BrowsingDataRemoverFactory::GetInstance());
}

AuthenticationServiceFactory::~AuthenticationServiceFactory() {}

std::unique_ptr<KeyedService>
AuthenticationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildAuthenticationService(
      std::make_unique<AuthenticationServiceDelegateImpl>(
          BrowsingDataRemoverFactory::GetForProfile(profile),
          profile->GetPrefs()),
      profile);
}

void AuthenticationServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AuthenticationService::RegisterPrefs(registry);
}
