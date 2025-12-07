// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

// static
ChromeAccountManagerService* ChromeAccountManagerServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ChromeAccountManagerService>(
      profile, /*create=*/true);
}

ChromeAccountManagerServiceFactory*
ChromeAccountManagerServiceFactory::GetInstance() {
  static base::NoDestructor<ChromeAccountManagerServiceFactory> instance;
  return instance.get();
}

ChromeAccountManagerServiceFactory::ChromeAccountManagerServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ChromeAccountManagerService") {}

ChromeAccountManagerServiceFactory::~ChromeAccountManagerServiceFactory() =
    default;

std::unique_ptr<KeyedService>
ChromeAccountManagerServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ChromeAccountManagerService>(
      GetApplicationContext()->GetLocalState(), profile->GetProfileName());
}
