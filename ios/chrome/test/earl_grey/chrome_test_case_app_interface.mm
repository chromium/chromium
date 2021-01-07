// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_test_case_app_interface.h"

#import "base/check.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/signin_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Stores the callback UUIDs when the callback is invoked. The UUIDs can be
// checked with +[ChromeTestCaseAppInterface isCallbackInvokedWithUUID:].
NSMutableSet* invokedCallbackUUID = nil;

}

@implementation ChromeTestCaseAppInterface

+ (void)setUpMockAuthentication {
  chrome_test_util::SetUpMockAuthentication();
}

+ (void)tearDownMockAuthentication {
  chrome_test_util::TearDownMockAuthentication();
}

+ (void)resetAuthentication {
  chrome_test_util::ResetSigninPromoPreferences();
  chrome_test_util::ResetMockAuthentication();
}

+ (void)removeInfoBarsAndPresentedStateWithCallbackUUID:(NSUUID*)callbackUUID {
  chrome_test_util::RemoveAllInfoBars();
  chrome_test_util::ClearPresentedState(^() {
    if (callbackUUID)
      [self callbackInvokedWithUUID:callbackUUID];
  });
}

+ (BOOL)isCallbackInvokedWithUUID:(NSUUID*)callbackUUID {
  if (![invokedCallbackUUID containsObject:callbackUUID])
    return NO;
  [invokedCallbackUUID removeObject:callbackUUID];
  return YES;
}

#pragma mark - Private

+ (void)callbackInvokedWithUUID:(NSUUID*)callbackUUID {
  if (!invokedCallbackUUID)
    invokedCallbackUUID = [NSMutableSet set];
  DCHECK(![invokedCallbackUUID containsObject:callbackUUID]);
  [invokedCallbackUUID addObject:callbackUUID];
}

@end
