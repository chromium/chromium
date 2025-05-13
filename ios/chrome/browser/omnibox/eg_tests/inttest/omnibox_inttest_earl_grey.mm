// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/eg_tests/inttest/omnibox_inttest_earl_grey.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/omnibox/eg_tests/inttest/omnibox_inttest_app_interface.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

using base::test::ios::WaitUntilConditionOrTimeout;

@implementation OmniboxInttestEarlGreyImpl

- (void)focusOmnibox {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_tap()];
}

- (void)focusOmniboxAndType:(NSString*)text {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_tap()];
  if (text.length) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
        performAction:grey_replaceText(text)];
  }
}

- (void)addURLShortcutMatch:(NSString*)shortcutText
             destinationURL:(const GURL&)shortcutURL {
  GREYAssertTrue(shortcutURL.is_valid(), @"shortcutURL should be valid.");
  [OmniboxInttestAppInterface
       addURLShortcutMatch:shortcutText
      destinationURLString:[NSString cr_fromString:shortcutURL.spec()]];
}

- (void)assertURLLoaded:(const GURL&)URL {
  NSURL* loadedURL = [self waitForURLLoad];
  GREYAssertEqualObjects(loadedURL, net::NSURLWithGURL(URL),
                         @"URL should have loaded.");
}

- (void)assertSearchLoaded:(NSString*)searchTerms {
  GURL lastURLLoaded = net::GURLWithNSURL([self waitForURLLoad]);
  std::string loadedSearchTerms = "";
  net::GetValueForKeyInQuery(lastURLLoaded, "q", &loadedSearchTerms);
  GREYAssertEqualObjects([NSString cr_fromString:loadedSearchTerms],
                         searchTerms, @"search terms should have loaded.");
}

#pragma mark - Private

/// Waits for the omnibox to load an URL and returns it.
- (NSURL*)waitForURLLoad {
  NSString* errorString = @"Failed waiting for the omnibox to load an URL.";

  GREYCondition* waitCondition = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    return !![OmniboxInttestAppInterface lastURLLoaded];
                  }];
  bool conditionMet = [waitCondition
      waitWithTimeout:base::test::ios::kWaitForPageLoadTimeout.InSecondsF()];

  EG_TEST_HELPER_ASSERT_TRUE(conditionMet, errorString);
  return [OmniboxInttestAppInterface lastURLLoaded];
}

@end
