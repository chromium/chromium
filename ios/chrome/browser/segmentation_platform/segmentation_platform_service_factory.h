// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace segmentation_platform {

class DeviceSwitcherResultDispatcher;
class SegmentationPlatformService;

// Factory for SegmentationPlatformService.
class SegmentationPlatformServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static SegmentationPlatformService* GetForBrowserState(
      ChromeBrowserState* context);

  static SegmentationPlatformServiceFactory* GetInstance();

  SegmentationPlatformServiceFactory(SegmentationPlatformServiceFactory&) =
      delete;
  SegmentationPlatformServiceFactory& operator=(
      SegmentationPlatformServiceFactory&) = delete;

  // Returns the dispatcher used to retrieve or store the classification result
  // for the user in the given browser state.
  static DeviceSwitcherResultDispatcher* GetDispatcherForBrowserState(
      ChromeBrowserState* context);

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
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace segmentation_platform

#endif  // IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_
