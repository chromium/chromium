// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_QUERY_TITLE_SERVER_UTIL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_QUERY_TITLE_SERVER_UTIL_H_

#import <Foundation/Foundation.h>

class GURL;
namespace net {
namespace test_server {
class EmbeddedTestServer;
}  // namespace test_server
}  // namespace net

// Registers dynamic handler for the `/querytitle` route.
//
// The returned page is a simple HTML page whose title and body is just the
// query parameter, e.g. `/querytitle?foo` will show a page whose title is "foo"
// and body is "foo".
void RegisterQueryTitleHandler(
    net::test_server::EmbeddedTestServer* test_server);

// Returns a `/querytitle` URL with `query` as query.
//
// For example: GetQueryTitleURL(test_server, @"foo") returns "/querytitle?foo".
GURL GetQueryTitleURL(net::test_server::EmbeddedTestServer* test_server,
                      NSString* title);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_QUERY_TITLE_SERVER_UTIL_H_
