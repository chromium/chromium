// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/base_earl_grey_test_case_app_interface.h"

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#import "base/apple/foundation_util.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"

@interface UIApplication (Testing)
- (void)_terminateWithStatus:(int)status;
@end

@implementation BaseEarlGreyTestCaseAppInterface

+ (void)logMessage:(NSString*)message {
  DLOG(WARNING) << base::SysNSStringToUTF8(message);
}

+ (void)enableFastAnimation {
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    UIWindowScene* windowScene =
        base::apple::ObjCCastStrict<UIWindowScene>(scene);
    for (UIWindow* window in windowScene.windows) {
      [[window layer] setSpeed:100];
    }
  }
}

+ (void)disableFastAnimation {
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    UIWindowScene* windowScene =
        base::apple::ObjCCastStrict<UIWindowScene>(scene);
    for (UIWindow* window in windowScene.windows) {
      [[window layer] setSpeed:1];
    }
  }
}

+ (BOOL)swizzledInputUIOOP {
  return NO;
}

+ (void)swizzleKeyboardOOP {
  Class klass = NSClassFromString(@"UIKeyboard");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundeclared-selector"
  Method originalMethod = class_getClassMethod(klass, @selector(inputUIOOP));
#pragma clang diagnostic pop

  Method swizzledMethod =
      class_getClassMethod([self class], @selector(swizzledInputUIOOP));
  method_exchangeImplementations(originalMethod, swizzledMethod);
}

+ (void)gracefulTerminate {
  dispatch_async(dispatch_get_main_queue(), ^{
    UIApplication* application = UIApplication.sharedApplication;
    [application _terminateWithStatus:0];
    exit(0);
  });
}

@end
