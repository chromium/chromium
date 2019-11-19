// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_sharing/handoff_manager_app_interface.h"

#import "components/handoff/handoff_manager.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation HandoffManagerAppInterface

+ (NSURL*)currentUserActivityWebPageURL {
  HandoffManager* manager =
      [chrome_test_util::GetDeviceSharingManager() handoffManager];
  return manager.userActivityWebpageURL;
}

@end
