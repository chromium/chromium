// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_service_factory.h"

#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/chrome_tailored_security_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

// static
safe_browsing::TailoredSecurityService*
TailoredSecurityServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<safe_browsing::TailoredSecurityService>(
          profile, /*create=*/true);
}

// static
TailoredSecurityServiceFactory* TailoredSecurityServiceFactory::GetInstance() {
  static base::NoDestructor<TailoredSecurityServiceFactory> instance;
  return instance.get();
}

TailoredSecurityServiceFactory::TailoredSecurityServiceFactory()
    : ProfileKeyedServiceFactoryIOS("TailoredSecurityService") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
TailoredSecurityServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<safe_browsing::ChromeTailoredSecurityService>(
      profile, IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile));
}
