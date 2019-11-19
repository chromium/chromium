// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_test_case_app_interface.h"

#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/signin_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromeTestCaseAppInterface

+ (void)setUpMockAuthentication {
  chrome_test_util::SetUpMockAuthentication();
  chrome_test_util::SetUpMockAccountReconcilor();
}

+ (void)tearDownMockAuthentication {
  chrome_test_util::TearDownMockAccountReconcilor();
  chrome_test_util::TearDownMockAuthentication();
}

+ (void)resetAuthentication {
  chrome_test_util::ResetSigninPromoPreferences();
  chrome_test_util::ResetMockAuthentication();
}

+ (void)removeInfoBarsAndPresentedState {
  chrome_test_util::RemoveAllInfoBars();
  chrome_test_util::ClearPresentedState();
}

@end
