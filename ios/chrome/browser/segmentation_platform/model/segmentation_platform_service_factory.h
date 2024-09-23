// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace segmentation_platform {

namespace home_modules {
class HomeModulesCardRegistry;
}  // namespace home_modules

class DeviceSwitcherResultDispatcher;
class SegmentationPlatformService;

// Factory for SegmentationPlatformService.
class SegmentationPlatformServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static SegmentationPlatformService* GetForProfile(ProfileIOS* profile);

  static SegmentationPlatformServiceFactory* GetInstance();

  // Returns the dispatcher used to retrieve or store the classification result
  // for the user in the given profile. Do not call for OTR profiles.
  static DeviceSwitcherResultDispatcher* GetDispatcherForProfile(
      ProfileIOS* profile);

  // Returns the registry used to manage the home cards for the given `context`.
  // Do not call for OTR context.
  static home_modules::HomeModulesCardRegistry* GetHomeCardRegistryForProfile(
      ProfileIOS* profile);

  // Returns the default factory used to build SegmentationPlatformService. Can
  // be registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<SegmentationPlatformServiceFactory>;

  SegmentationPlatformServiceFactory();
  ~SegmentationPlatformServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace segmentation_platform

#endif  // IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_
