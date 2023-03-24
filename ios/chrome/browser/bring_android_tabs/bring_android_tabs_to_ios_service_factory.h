// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_BRING_ANDROID_TABS_TO_IOS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_BRING_ANDROID_TABS_TO_IOS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class BringAndroidTabsToIOSService;

// Singleton that owns all BringAndroidTabsToIOSService and associates them with
// ChromeBrowserState.
//
// Note that as the "Bring Android Tabs" feature does not apply in incognito
// mode, the factory should only create and store services for regular browser
// states.
class BringAndroidTabsToIOSServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static BringAndroidTabsToIOSService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static BringAndroidTabsToIOSService* GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state);
  static BringAndroidTabsToIOSServiceFactory* GetInstance();

  BringAndroidTabsToIOSServiceFactory(
      const BringAndroidTabsToIOSServiceFactory&) = delete;
  BringAndroidTabsToIOSServiceFactory& operator=(
      const BringAndroidTabsToIOSServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<BringAndroidTabsToIOSServiceFactory>;

  BringAndroidTabsToIOSServiceFactory();
  ~BringAndroidTabsToIOSServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_BRING_ANDROID_TABS_TO_IOS_SERVICE_FACTORY_H_
