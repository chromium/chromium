// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/model/browser_about_rewriter.h"

#include "base/test/gtest_util.h"
#include "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

using BrowserAboutRewriterTest = PlatformTest;

// Test that chrome://newtab is re-written to about://newtab/,
// but that about://newtab/ is not re-written twice.
TEST_F(BrowserAboutRewriterTest, NtpTest) {
  GURL url = GURL(kChromeUINewTabURL);
  EXPECT_TRUE(WillHandleWebBrowserAboutURL(&url, /*profile=*/nullptr));
  EXPECT_EQ(url, GURL(kChromeUIAboutNewTabURL));
  EXPECT_FALSE(WillHandleWebBrowserAboutURL(&url, /*profile=*/nullptr));
}

// Test that about|chrome://about is rewritten to chrome-urls and about:blank
// is not.
TEST_F(BrowserAboutRewriterTest, AboutTest) {
  GURL url = GURL("about:about");
  EXPECT_FALSE(WillHandleWebBrowserAboutURL(&url, /*profile=*/nullptr));
  EXPECT_EQ(url, GURL("chrome://chrome-urls/"));

  url = GURL("chrome://about/");
  EXPECT_FALSE(WillHandleWebBrowserAboutURL(&url, /*profile=*/nullptr));
  EXPECT_EQ(url, GURL("chrome://chrome-urls/"));

  url = GURL("about:blank?for=");
  EXPECT_FALSE(WillHandleWebBrowserAboutURL(&url, /*profile=*/nullptr));
  EXPECT_EQ(url, GURL("about:blank?for="));
}

// Test that about|chrome://sync is rewritten to sync-internals.
TEST_F(BrowserAboutRewriterTest, SyncTest) {
  GURL url = GURL("about:sync");
  EXPECT_FALSE(WillHandleWebBrowserAboutURL(&url, /*profile=*/nullptr));
  EXPECT_EQ(url, GURL("chrome://sync-internals/"));

  url = GURL("chrome://sync/");
  EXPECT_FALSE(WillHandleWebBrowserAboutURL(&url, /*profile=*/nullptr));
  EXPECT_EQ(url, GURL("chrome://sync-internals/"));
}
