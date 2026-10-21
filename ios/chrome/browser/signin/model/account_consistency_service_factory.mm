// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_consistency_service_factory.h"

#import <utility>

#import "base/functional/bind.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/account_reconcilor_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace ios {

AccountConsistencyServiceFactory::AccountConsistencyServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "AccountConsistencyService",
                                         ServiceCreation::kCreateWithProfile,
                                         TestingCreation::kNoServiceForTests) {
  DependsOn(ios::AccountReconcilorFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
AccountConsistencyServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  // The base::Unretained(profile) is safe since the callback is only called
  // from the returned AccountConsistencyService instance which is owned by
  // the Profile object (as it is a KeyedService).
  auto cookie_manager_callback = base::BindRepeating(
      &web::BrowserState::GetCookieManager, base::Unretained(profile));

  return std::make_unique<AccountConsistencyService>(
      std::move(cookie_manager_callback),
      ios::AccountReconcilorFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs());
}

}  // namespace ios
