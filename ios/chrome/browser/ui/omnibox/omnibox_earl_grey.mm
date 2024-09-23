// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_earl_grey.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_app_interface.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_matchers.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::test::ios::WaitUntilConditionOrTimeout;

@implementation OmniboxEarlGreyImpl

- (void)waitForShortcutsBackendInitialization {
  NSString* errorString = [NSString
      stringWithFormat:@"Failed waiting for shortcuts backend initialization."];

  GREYCondition* waitCondition = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    return [OmniboxAppInterface shortcutsBackendInitialized];
                  }];
  bool conditionMet = [waitCondition
      waitWithTimeout:base::test::ios::kWaitForFileOperationTimeout
                          .InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(conditionMet, errorString);
}

- (void)waitForNumberOfShortcutsInDatabase:(NSInteger)numberOfShortcuts {
  NSString* errorString =
      [NSString stringWithFormat:
                    @"Failed waiting for shortcut database size equal to %ld.",
                    numberOfShortcuts];

  GREYCondition* waitCondition = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    return [OmniboxAppInterface numberOfShortcutsInDatabase] ==
                           numberOfShortcuts;
                  }];
  bool conditionMet = [waitCondition
      waitWithTimeout:base::test::ios::kWaitForFileOperationTimeout
                          .InSecondsF()];

  EG_TEST_HELPER_ASSERT_TRUE(conditionMet, errorString);
}

- (void)openPage:(omnibox::Page)page
      testServer:(net::test_server::EmbeddedTestServer*)testServer {
  // Page is limited to two digits by the `kPageURLScheme`.
  DCHECK(page < 100u);

  GURL pageURL = testServer->GetURL(omnibox::PageURL(page));
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:omnibox::PageContent(page)];
}

- (void)populateHistory:(net::test_server::EmbeddedTestServer*)testServer
          numberOfPages:(NSUInteger)numberOfPages {
  // Navigates to pages `Page(1)` to `Page(numberOfPages)`.
  for (NSUInteger i = 1; i <= numberOfPages; ++i) {
    [OmniboxEarlGrey openPage:i testServer:testServer];
  }
}

- (void)addShorcuts:(NSUInteger)shortcutCount
       toTestServer:(net::test_server::EmbeddedTestServer*)testServer {
  [OmniboxEarlGrey waitForShortcutsBackendInitialization];

  for (NSUInteger i = 1; i <= shortcutCount; ++i) {
    omnibox::Page pageI = omnibox::Page(i);

    // Shortcut are added by:
    // 1. Navigating to the <URL> to add it to history.
    // 2. Typing the shortcut <input> (page title here) in the omnibox.
    // 3. Selecting the shortcut <URL> in the suggestions.
    [OmniboxEarlGrey openPage:i testServer:testServer];
    GURL URL = testServer->GetURL(omnibox::PageURL(pageI));
    [ChromeEarlGreyUI
        focusOmniboxAndReplaceText:base::SysUTF8ToNSString(
                                       omnibox::PageTitle(pageI))];
    [[EarlGrey selectElementWithMatcher:omnibox::PopupRowWithUrlMatcher(URL)]
        performAction:grey_tap()];
    [ChromeEarlGrey waitForWebStateContainingText:omnibox::PageContent(pageI)];
  }
}

- (id<GREYMatcher>)isURLMatcher {
  GREYMatchesBlock matches = ^BOOL(id element) {
    return [OmniboxAppInterface isElementURL:element];
  };
  NSString* descriptionString =
      [NSString stringWithFormat:@"Element is a valid URL."];
  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:descriptionString];
  };
  id<GREYMatcher> URLMatcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return URLMatcher;
}

@end
