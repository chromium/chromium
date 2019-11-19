// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_earl_grey.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/shell/test/earl_grey/shell_earl_grey_app_interface.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(ShellEarlGreyAppInterface)
#endif

@implementation ShellEarlGreyImpl

- (void)loadURL:(const GURL&)URL {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ShellEarlGreyAppInterface startLoadingURL:spec];

  NSString* loadingErrorDescription = [NSString
      stringWithFormat:@"Current WebState did not finish loading %@ URL", spec];
  GREYCondition* condition = [GREYCondition
      conditionWithName:loadingErrorDescription
                  block:^{
                    return !
                        [ShellEarlGreyAppInterface isCurrentWebStateLoading];
                  }];
  BOOL pageLoaded = [condition waitWithTimeout:kWaitForPageLoadTimeout];
  EG_TEST_HELPER_ASSERT_TRUE(pageLoaded, loadingErrorDescription);

  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ShellEarlGreyAppInterface waitForWindowIDInjectedInCurrentWebState]);

  // Ensure any UI elements handled by EarlGrey become idle for any subsequent
  // EarlGrey steps.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (void)waitForWebStateContainingText:(NSString*)text {
  NSString* description = [NSString
      stringWithFormat:@"Current WebState does not contain: '%@'", text];
  GREYCondition* condition =
      [GREYCondition conditionWithName:description
                                 block:^{
                                   return [ShellEarlGreyAppInterface
                                       currentWebStateContainsText:text];
                                 }];

  BOOL containsText = [condition waitWithTimeout:kWaitForPageLoadTimeout];
  EG_TEST_HELPER_ASSERT_TRUE(containsText, description);
}

@end
