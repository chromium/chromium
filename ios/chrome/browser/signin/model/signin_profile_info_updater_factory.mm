// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_profile_info_updater_factory.h"

#import <utility>

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/signin_error_controller_factory.h"
#import "ios/chrome/browser/signin/model/signin_profile_info_updater.h"

SigninProfileInfoUpdaterFactory::SigninProfileInfoUpdaterFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "SigninProfileInfoUpdater",
                                         ServiceCreation::kCreateWithProfile,
                                         TestingCreation::kNoServiceForTests) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ios::SigninErrorControllerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SigninProfileInfoUpdaterFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<SigninProfileInfoUpdater>(
      IdentityManagerFactory::GetForProfile(profile),
      ios::SigninErrorControllerFactory::GetForProfile(profile),
      profile->GetProfileName());
}
