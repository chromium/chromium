// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

#import <memory>
#import <utility>

#import "base/check.h"
#import "base/feature_list.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace {

std::unique_ptr<KeyedService> BuildAuthenticationService(ProfileIOS* profile) {
  auto service = std::make_unique<AuthenticationService>(
      profile, profile->GetPrefs(),
      ChromeAccountManagerServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile));
  service->Initialize();
  DCHECK(service->initialized());
  return service;
}

std::unique_ptr<KeyedService> BuildAuthenticationServiceForTesting(
    ProfileIOS* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  // In production OnNetworkInitialized is called by
  // ProfileManagerIOSImpl::DoFinalInitForServices, explicitly call it in
  // tests that utilize AuthenticationService.
  if (base::FeatureList::IsEnabled(switches::kUseAccountInfoFetcherIos)) {
    identity_manager->OnNetworkInitialized();
  }

  return BuildAuthenticationService(profile);
}

}  // namespace

// static
AuthenticationServiceFactory::TestingFactory
AuthenticationServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildAuthenticationServiceForTesting);
}

AuthenticationServiceFactory::AuthenticationServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "AuthenticationService",
          ProfileSelection::kNoInstanceInIncognito,
          TestingCreation::kNoServiceForTests) {
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
AuthenticationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildAuthenticationService(profile);
}

void AuthenticationServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AuthenticationService::RegisterPrefs(registry);
}
