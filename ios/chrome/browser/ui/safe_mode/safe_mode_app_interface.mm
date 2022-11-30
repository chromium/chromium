// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/safe_mode/safe_mode_app_interface.h"

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

+ (void)setFailedStartupAttemptCount:(int)count {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setInteger:count forKey:@"AppStartupFailureCount"];
}

@end
