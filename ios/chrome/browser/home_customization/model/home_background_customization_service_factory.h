// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class HomeBackgroundCustomizationService;

// Singleton that owns all HomeBackgroundCustomizationServices and associates
// them with profiles.
class HomeBackgroundCustomizationServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static HomeBackgroundCustomizationService* GetForProfile(ProfileIOS* profile);
  static HomeBackgroundCustomizationServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<HomeBackgroundCustomizationServiceFactory>;

  HomeBackgroundCustomizationServiceFactory();
  ~HomeBackgroundCustomizationServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_FACTORY_H_
