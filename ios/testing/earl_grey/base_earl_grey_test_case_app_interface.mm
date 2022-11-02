// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/base_earl_grey_test_case_app_interface.h"

#import <UIKit/UIKit.h>

#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UIApplication (Testing)
- (void)_terminateWithStatus:(int)status;
@end

@implementation BaseEarlGreyTestCaseAppInterface

+ (void)logMessage:(NSString*)message {
  DLOG(WARNING) << base::SysNSStringToUTF8(message);
}

+ (void)enableFastAnimation {
  for (UIWindow* window in [UIApplication sharedApplication].windows) {
    [[window layer] setSpeed:100];
  }
}

+ (void)gracefulTerminate {
  dispatch_async(dispatch_get_main_queue(), ^{
    UIApplication* application = UIApplication.sharedApplication;
    [application _terminateWithStatus:0];
    exit(0);
  });
}

@end
