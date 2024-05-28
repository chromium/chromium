// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EARL_GREY_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EARL_GREY_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/omnibox/omnibox_test_util.h"
#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

namespace net {
namespace test_server {
class EmbeddedTestServer;
}
}  // namespace net

#define OmniboxEarlGrey \
  [OmniboxEarlGreyImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

/// Methods used for the EarlGrey tests.
@interface OmniboxEarlGreyImpl : BaseEGTestHelperImpl

/// Wait for the shortcuts backend to initialize.
- (void)waitForShortcutsBackendInitialization;

/// Wait for the number of shortcuts in the database to be equal to
/// `numberOfShortcuts`.
- (void)waitForNumberOfShortcutsInDatabase:(NSInteger)numberOfShortcuts;

/// Navigates to `page` using `testServer`.
- (void)openPage:(omnibox::Page)page
      testServer:(net::test_server::EmbeddedTestServer*)testServer;

/// Populates history by navigating to `numberOfPages` pages.
- (void)populateHistory:(net::test_server::EmbeddedTestServer*)testServer
          numberOfPages:(NSUInteger)numberOfPages;

/// Add `shortcutCount`.
// List of shortcuts <input> -> <URL>:
// - <omnibox::PageTitle(1)> -> <omnibox::PageURL(1)>
// - ...
// - <omnibox::PageTitle(N)> -> <omnibox::PageURL(N)>
// N = `shortcutCount`.
- (void)addShorcuts:(NSUInteger)shortcutCount
       toTestServer:(net::test_server::EmbeddedTestServer*)testServer;

/// Returns a matcher for a valid URL.
- (id<GREYMatcher>)isURLMatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EARL_GREY_H_
