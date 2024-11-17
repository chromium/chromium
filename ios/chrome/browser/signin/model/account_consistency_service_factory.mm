// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_consistency_service_factory.h"

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "components/content_settings/core/browser/cookie_settings.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#import "ios/chrome/browser/content_settings/model/cookie_settings_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/account_reconcilor_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace ios {

AccountConsistencyServiceFactory::AccountConsistencyServiceFactory()
    : ProfileKeyedServiceFactoryIOS("AccountConsistencyService",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(ios::AccountReconcilorFactory::GetInstance());
  DependsOn(ios::CookieSettingsFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

AccountConsistencyServiceFactory::~AccountConsistencyServiceFactory() {}

// static
AccountConsistencyService* AccountConsistencyServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<AccountConsistencyService>(
      profile, /*create=*/true);
}

// static
AccountConsistencyServiceFactory*
AccountConsistencyServiceFactory::GetInstance() {
  static base::NoDestructor<AccountConsistencyServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
AccountConsistencyServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  // The base::Unretained(profile) is safe since the callback is only called
  // from the returned AccountConsistencyService instance which is owned by
  // the Profile object (as it is a KeyedService).
  auto cookie_manager_callback = base::BindRepeating(
      &web::BrowserState::GetCookieManager, base::Unretained(profile));

  return std::make_unique<AccountConsistencyService>(
      std::move(cookie_manager_callback),
      ios::AccountReconcilorFactory::GetForProfile(profile),
      ios::CookieSettingsFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile));
}

}  // namespace ios
