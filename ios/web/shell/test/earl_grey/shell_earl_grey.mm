// Copyright 2016 The Chromium Authors
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
  BOOL pageLoaded =
      [condition waitWithTimeout:kWaitForPageLoadTimeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(pageLoaded, loadingErrorDescription);

  // Ensure any UI elements handled by EarlGrey become idle for any subsequent
  // EarlGrey steps.
  GREYWaitForAppToIdle(@"App failed to idle");
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

  BOOL containsText =
      [condition waitWithTimeout:kWaitForPageLoadTimeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(containsText, description);
}

- (void)waitForUIElementToDisappearWithMatcher:(id<GREYMatcher>)matcher {
  [self waitForUIElementToDisappearWithMatcher:matcher
                                       timeout:kWaitForUIElementTimeout];
}

- (void)waitForUIElementToDisappearWithMatcher:(id<GREYMatcher>)matcher
                                       timeout:(base::TimeDelta)timeout {
  NSString* errorDescription = [NSString
      stringWithFormat:
          @"Failed waiting for element with matcher %@ to disappear", matcher];

  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_nil()
                                                             error:&error];
    return error == nil;
  };

  bool matched = WaitUntilConditionOrTimeout(timeout, condition);
  EG_TEST_HELPER_ASSERT_TRUE(matched, errorDescription);
}

@end
