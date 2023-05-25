// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_TABS_EGTEST_UTIL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_TABS_EGTEST_UTIL_H_

#import <Foundation/Foundation.h>

@protocol GREYMatcher;
namespace net {
namespace test_server {
class EmbeddedTestServer;
}  // namespace test_server
}  // namespace net

// Creates a regular tab with `title` using `test_server`.
void CreateRegularTab(net::test_server::EmbeddedTestServer* test_server,
                      NSString* title);

// Creates `tabs_count` of regular tabs.
void CreateRegularTabs(int tabs_count,
                       net::test_server::EmbeddedTestServer* test_server);

// Creates `tabs_count` of pinned tabs.
void CreatePinnedTabs(int tabs_count,
                      net::test_server::EmbeddedTestServer* test_server);

// Matcher for a tab grid cell.
id<GREYMatcher> TabGridCell();

// Matcher for a tab grid cell with the given `title`.
id<GREYMatcher> TabWithTitle(NSString* title);

// Matcher for a tab grid cell with the given `title`, at the given `index`.
id<GREYMatcher> TabWithTitleAndIndex(NSString* title, unsigned int index);

// Taps on the item defined by `matcher` and waits for a snack bar with the
// given `snackbarLabel`.
void WaitForSnackbarTriggeredByTappingItem(NSString* snackbarLabel,
                                           id<GREYMatcher> matcher);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_TABS_EGTEST_UTIL_H_
