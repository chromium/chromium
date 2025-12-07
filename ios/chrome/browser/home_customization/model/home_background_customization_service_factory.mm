// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_customization_service_factory.h"

#import "base/no_destructor.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_image_service_factory.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
HomeBackgroundCustomizationService*
HomeBackgroundCustomizationServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<HomeBackgroundCustomizationService>(
          profile, /*create=*/true);
}

// static
HomeBackgroundCustomizationServiceFactory*
HomeBackgroundCustomizationServiceFactory::GetInstance() {
  static base::NoDestructor<HomeBackgroundCustomizationServiceFactory> instance;
  return instance.get();
}

HomeBackgroundCustomizationServiceFactory::
    HomeBackgroundCustomizationServiceFactory()
    : ProfileKeyedServiceFactoryIOS("HomeBackgroundCustomizationService") {
  DependsOn(UserUploadedImageManagerFactory::GetInstance());
  DependsOn(HomeBackgroundImageServiceFactory::GetInstance());
}

HomeBackgroundCustomizationServiceFactory::
    ~HomeBackgroundCustomizationServiceFactory() {}

std::unique_ptr<KeyedService>
HomeBackgroundCustomizationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<HomeBackgroundCustomizationService>(
      profile->GetPrefs(),
      UserUploadedImageManagerFactory::GetForProfile(profile),
      HomeBackgroundImageServiceFactory::GetForProfile(profile));
}

void HomeBackgroundCustomizationServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  HomeBackgroundCustomizationService::RegisterProfilePrefs(registry);
}
