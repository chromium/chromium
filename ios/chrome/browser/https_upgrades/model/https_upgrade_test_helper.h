// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_TEST_HELPER_H_
#define IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_TEST_HELPER_H_

#include <memory>
#include <set>
#include <string>

#import "ios/chrome/test/earl_grey/chrome_test_case.h"

namespace net {
namespace test_server {
class EmbeddedTestServer;
}
}  // namespace net

// Base test class for HTTPS upgrade related features (HTTPS-Only Mode,
// HTTPS Upgrades and Typed Omnibox Navigation Upgrades).
// The tests here use WKWebView and don't go through Chrome's net stack. Due to
// this, we can't serve valid HTTPS over the test server due to platform
// limitations on iOS. Instead, we use a faux-HTTPS server (goodHTTPSServer)
// which is just another HTTP_SERVER but runs on a different port and returns a
// different text than self.testServer. badHTTPSServer is a proper HTTPS_SERVER
// that just serves bad HTTPS responses. slowServer is a server that serves hung
// responses.
@interface HttpsUpgradeTestCaseBase : ChromeTestCase {
  // Counts the number of HTTP responses returned by the test server. Doesn't
  // count the faux-HTTPS or bad-HTTPS responses. Used to check if prerender
  // navigations are successfully cancelled (the server shouldn't return a
  // response for them).
  int _HTTPResponseCounter;
  std::unique_ptr<net::test_server::EmbeddedTestServer> _goodHTTPSServer;
  std::unique_ptr<net::test_server::EmbeddedTestServer> _badHTTPSServer;
  std::unique_ptr<net::test_server::EmbeddedTestServer> _slowServer;
}

// The EmbeddedTestServer instance that serves faux-good HTTPS responses for
// tests.
@property(nonatomic, readonly)
    net::test_server::EmbeddedTestServer* goodHTTPSServer;

// The EmbeddedTestServer instance that serves actual bad HTTPS responses for
// tests.
@property(nonatomic, readonly)
    net::test_server::EmbeddedTestServer* badHTTPSServer;

// The EmbeddedTestServer instance that serves a hung response for tests.
@property(nonatomic, readonly) net::test_server::EmbeddedTestServer* slowServer;

@end

#endif  // IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_TEST_HELPER_H_
