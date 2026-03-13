// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/eg_tests/inttest/composebox_inttest_earl_grey.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/test/earl_grey/chrome_coordinator_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

using base::test::ios::WaitUntilConditionOrTimeout;

@implementation ComposeboxInttestEarlGreyImpl

- (void)assertSearchLoaded:(NSString*)searchTerms {
  GURL lastURLLoaded = net::GURLWithNSURL([self waitForURLLoad]);
  std::string loadedSearchTerms = "";
  net::GetValueForKeyInQuery(lastURLLoaded, "q", &loadedSearchTerms);
  GREYAssertEqualObjects([NSString cr_fromString:loadedSearchTerms],
                         searchTerms, @"search terms should have loaded.");
}

#pragma mark - Private

/// Waits for the composebox to load an URL and returns it.
- (NSURL*)waitForURLLoad {
  NSString* errorString = @"Failed waiting for the composebox to load an URL.";

  GREYCondition* waitCondition = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    return !![ChromeCoordinatorAppInterface lastURLLoaded];
                  }];
  bool conditionMet = [waitCondition
      waitWithTimeout:base::test::ios::kWaitForPageLoadTimeout.InSecondsF()];

  EG_TEST_HELPER_ASSERT_TRUE(conditionMet, errorString);
  return [ChromeCoordinatorAppInterface lastURLLoaded];
}

@end
