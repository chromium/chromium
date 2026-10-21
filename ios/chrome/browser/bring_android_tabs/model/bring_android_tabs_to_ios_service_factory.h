// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_MODEL_BRING_ANDROID_TABS_TO_IOS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_MODEL_BRING_ANDROID_TABS_TO_IOS_SERVICE_FACTORY_H_

#import "ios/chrome/browser/bring_android_tabs/model/bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all BringAndroidTabsToIOSService and associates them with
// ProfileIOS.
//
// Note that as the "Bring Android Tabs" feature does not apply in incognito
// mode, the factory should only create and store services for regular browser
// states.
class BringAndroidTabsToIOSServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          BringAndroidTabsToIOSServiceFactory,
          BringAndroidTabsToIOSService> {
 public:
  explicit BringAndroidTabsToIOSServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_MODEL_BRING_ANDROID_TABS_TO_IOS_SERVICE_FACTORY_H_
