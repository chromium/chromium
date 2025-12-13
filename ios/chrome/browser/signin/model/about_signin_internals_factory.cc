// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/model/about_signin_internals_factory.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/signin/model/account_reconcilor_factory.h"
#include "ios/chrome/browser/signin/model/identity_manager_factory.h"
#include "ios/chrome/browser/signin/model/signin_client_factory.h"
#include "ios/chrome/browser/signin/model/signin_error_controller_factory.h"

namespace ios {

AboutSigninInternalsFactory::AboutSigninInternalsFactory()
    : ProfileKeyedServiceFactoryIOS("AboutSigninInternals") {
  DependsOn(ios::AccountReconcilorFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SigninClientFactory::GetInstance());
  DependsOn(SigninErrorControllerFactory::GetInstance());
}

AboutSigninInternalsFactory::~AboutSigninInternalsFactory() = default;

// static
AboutSigninInternals* AboutSigninInternalsFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<AboutSigninInternals>(
      profile, /*create=*/true);
}

// static
AboutSigninInternalsFactory* AboutSigninInternalsFactory::GetInstance() {
  static base::NoDestructor<AboutSigninInternalsFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
AboutSigninInternalsFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  std::unique_ptr<AboutSigninInternals> service(new AboutSigninInternals(
      IdentityManagerFactory::GetForProfile(profile),
      SigninErrorControllerFactory::GetForProfile(profile),
      signin::AccountConsistencyMethod::kMirror,
      SigninClientFactory::GetForProfile(profile),
      ios::AccountReconcilorFactory::GetForProfile(profile)));
  return service;
}

void AboutSigninInternalsFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  AboutSigninInternals::RegisterPrefs(user_prefs);
}

}  // namespace ios
