// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_MODEL_BRING_ANDROID_TABS_TO_IOS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_MODEL_BRING_ANDROID_TABS_TO_IOS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class BringAndroidTabsToIOSService;
class ProfileIOS;

// Singleton that owns all BringAndroidTabsToIOSService and associates them with
// ProfileIOS.
//
// Note that as the "Bring Android Tabs" feature does not apply in incognito
// mode, the factory should only create and store services for regular browser
// states.
class BringAndroidTabsToIOSServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static BringAndroidTabsToIOSService* GetForProfile(ProfileIOS* profile);
  static BringAndroidTabsToIOSService* GetForProfileIfExists(
      ProfileIOS* profile);
  static BringAndroidTabsToIOSServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<BringAndroidTabsToIOSServiceFactory>;

  BringAndroidTabsToIOSServiceFactory();
  ~BringAndroidTabsToIOSServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_MODEL_BRING_ANDROID_TABS_TO_IOS_SERVICE_FACTORY_H_
