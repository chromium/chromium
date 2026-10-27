// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_MANAGER_FACTORY_H_

#import "ios/chrome/browser/device_sharing/model/device_sharing_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Keyed service factory for BrowserList.
// This factory returns the same instance for regular and OTR profiles.
class DeviceSharingManagerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<DeviceSharingManagerFactory,
                                                DeviceSharingManager> {
 public:
  explicit DeviceSharingManagerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS& profile) const final;
};

#endif  // IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_MANAGER_FACTORY_H_
