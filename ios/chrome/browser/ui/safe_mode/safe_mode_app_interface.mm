// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/safe_mode/safe_mode_app_interface.h"

#import "ios/chrome/browser/ui/safe_mode/safe_mode_view_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SafeModeAppInterface

+ (void)presentSafeMode {
  SafeModeViewController* safeModeController =
      [[SafeModeViewController alloc] initWithDelegate:nil];
  [chrome_test_util::GetActiveViewController()
      presentViewController:safeModeController
                   animated:NO
                 completion:nil];
}

@end
