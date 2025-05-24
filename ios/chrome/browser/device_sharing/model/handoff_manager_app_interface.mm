// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_sharing/model/handoff_manager_app_interface.h"

#import "components/handoff/handoff_manager.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager_factory.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager_impl.h"
#import "ios/chrome/test/app/chrome_test_util.h"

NSURL* GetCurrentUserActivityURL(ProfileIOS* profile) {
  DeviceSharingManagerImpl* sharing_manager =
      static_cast<DeviceSharingManagerImpl*>(
          DeviceSharingManagerFactory::GetForProfile(profile));
  return [sharing_manager->handoff_manager_ userActivityWebpageURL];
}

@implementation HandoffManagerAppInterface

+ (NSURL*)currentUserActivityWebPageURL {
  return GetCurrentUserActivityURL(chrome_test_util::GetOriginalProfile());
}

@end
