// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_

#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace segmentation_platform {

namespace home_modules {
class HomeModulesCardRegistry;
}  // namespace home_modules

class DeviceSwitcherResultDispatcher;

// Factory for SegmentationPlatformService.
class SegmentationPlatformServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          SegmentationPlatformServiceFactory,
          SegmentationPlatformService> {
 public:
  explicit SegmentationPlatformServiceFactory(PassKey key);

  // Returns the dispatcher used to retrieve or store the classification result
  // for the user in the given profile. Do not call for OTR profiles.
  static DeviceSwitcherResultDispatcher* GetDispatcherForProfile(
      ProfileIOS* profile);

  // Returns the registry used to manage the home cards for the given `context`.
  // Do not call for OTR context.
  static home_modules::HomeModulesCardRegistry* GetHomeCardRegistryForProfile(
      ProfileIOS* profile);

  // Returns the default factory used to build SegmentationPlatformService. Can
  // be registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
};

}  // namespace segmentation_platform

#endif  // IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_
