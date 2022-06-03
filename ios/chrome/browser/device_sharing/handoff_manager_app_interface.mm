// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_sharing/handoff_manager_app_interface.h"

#import "components/handoff/handoff_manager.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager_factory.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager_impl.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSURL* DeviceSharingAppInterfaceWrapper::GetCurrentUserActivityURL(
    ChromeBrowserState* browser_state) {
  DeviceSharingManagerImpl* sharing_manager =
      static_cast<DeviceSharingManagerImpl*>(
          DeviceSharingManagerFactory::GetForBrowserState(browser_state));
  return [sharing_manager->handoff_manager_ userActivityWebpageURL];
}

@implementation HandoffManagerAppInterface

+ (NSURL*)currentUserActivityWebPageURL {
  return DeviceSharingAppInterfaceWrapper::GetCurrentUserActivityURL(
      chrome_test_util::GetOriginalBrowserState());
}

@end
