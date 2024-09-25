// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_profile_info_updater_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/signin_error_controller_factory.h"
#import "ios/chrome/browser/signin/model/signin_profile_info_updater.h"

// static
SigninProfileInfoUpdater* SigninProfileInfoUpdaterFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<SigninProfileInfoUpdater*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
SigninProfileInfoUpdaterFactory*
SigninProfileInfoUpdaterFactory::GetInstance() {
  static base::NoDestructor<SigninProfileInfoUpdaterFactory> instance;
  return instance.get();
}

SigninProfileInfoUpdaterFactory::SigninProfileInfoUpdaterFactory()
    : BrowserStateKeyedServiceFactory(
          "SigninProfileInfoUpdater",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ios::SigninErrorControllerFactory::GetInstance());
}

SigninProfileInfoUpdaterFactory::~SigninProfileInfoUpdaterFactory() {}

std::unique_ptr<KeyedService>
SigninProfileInfoUpdaterFactory::BuildServiceInstanceFor(
    web::BrowserState* state) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(state);
  return std::make_unique<SigninProfileInfoUpdater>(
      IdentityManagerFactory::GetForProfile(profile),
      ios::SigninErrorControllerFactory::GetForProfile(profile),
      profile->GetProfileName());
}

bool SigninProfileInfoUpdaterFactory::ServiceIsCreatedWithBrowserState() const {
  return true;
}

bool SigninProfileInfoUpdaterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
