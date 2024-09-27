// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class DeviceSharingManager;

// Keyed service factory for BrowserList.
// This factory returns the same instance for regular and OTR profiles.
class DeviceSharingManagerFactory : public BrowserStateKeyedServiceFactory {
 public:
  static DeviceSharingManager* GetForProfile(ProfileIOS* profile);
  static DeviceSharingManagerFactory* GetInstance();

  // Returns the default factory used to build DeviceSharingManagers. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

  // Not copyable or moveable.
  DeviceSharingManagerFactory(const DeviceSharingManagerFactory&) = delete;
  DeviceSharingManagerFactory& operator=(const DeviceSharingManagerFactory&) =
      delete;

 private:
  friend class base::NoDestructor<DeviceSharingManagerFactory>;

  DeviceSharingManagerFactory();

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_MANAGER_FACTORY_H_
