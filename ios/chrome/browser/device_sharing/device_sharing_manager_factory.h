// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_SHARING_DEVICE_SHARING_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_DEVICE_SHARING_DEVICE_SHARING_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class DeviceSharingManager;
class ChromeBrowserState;

// Keyed service factory for BrowserList.
// This factory returns the same instance for regular and OTR browser states.
class DeviceSharingManagerFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Convenience getter that typecasts the value returned to a
  // BrowserList.
  static DeviceSharingManager* GetForBrowserState(
      ChromeBrowserState* browser_state);
  // Getter for singleton instance.
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

#endif  // IOS_CHROME_BROWSER_DEVICE_SHARING_DEVICE_SHARING_MANAGER_FACTORY_H_
