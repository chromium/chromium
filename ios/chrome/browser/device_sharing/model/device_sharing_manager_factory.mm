// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_sharing/model/device_sharing_manager_factory.h"

#import <utility>

#import "base/check.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

// Build an instance of DeviceSharingManagerImpl.
std::unique_ptr<KeyedService> BuildDeviceSharingManager(ProfileIOS& profile) {
  CHECK(!profile.IsOffTheRecord());
  return std::make_unique<DeviceSharingManagerImpl>(profile.GetPrefs());
}

}  // namespace

DeviceSharingManagerFactory::DeviceSharingManagerFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "DeviceSharingManager",
          ProfileSelection::kRedirectedInIncognito) {}

std::unique_ptr<KeyedService>
DeviceSharingManagerFactory::BuildServiceInstanceFor(
    ProfileIOS& profile) const {
  return BuildDeviceSharingManager(profile);
}
