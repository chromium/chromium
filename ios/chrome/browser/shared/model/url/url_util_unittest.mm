// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/url/url_util.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

using ChromeURLUtilTest = PlatformTest;

TEST_F(ChromeURLUtilTest, TestIsExternalFileReference) {
  GURL external_url("chrome://external-file/foo/bar");
  GURL not_external_url("chrome://foo/bar");
  GURL still_not_external_url("http://external-file/foo/bar");
  EXPECT_TRUE(UrlIsExternalFileReference(external_url));
  EXPECT_FALSE(UrlIsExternalFileReference(not_external_url));
  EXPECT_FALSE(UrlIsExternalFileReference(still_not_external_url));
}

TEST_F(ChromeURLUtilTest, TestUrlIsDownloadedFile) {
  GURL downloaded_file_url("chrome://downloads/fileName");
  GURL external_file_url("chrome://external-file/fileName");
  GURL not_downloaded_file_url("http://downloads/fileName");
  EXPECT_TRUE(UrlIsDownloadedFile(downloaded_file_url));
  EXPECT_FALSE(UrlIsDownloadedFile(external_file_url));
  EXPECT_FALSE(UrlIsDownloadedFile(not_downloaded_file_url));
}

const char* kSchemeTestData[] = {
    "http://foo.com", "https://foo.com",   "data:text/html;charset=utf-8,Hello",
    "about:blank",    "chrome://settings",
};

// Tests UrlHasChromeScheme with NSURL* parameter.
TEST_F(ChromeURLUtilTest, NSURLHasChromeScheme) {
  for (unsigned int i = 0; i < std::size(kSchemeTestData); ++i) {
    const char* url = kSchemeTestData[i];
    NSURL* nsurl = [NSURL URLWithString:base::SysUTF8ToNSString(url)];
    bool nsurl_result = UrlHasChromeScheme(nsurl);
    EXPECT_EQ(GURL(url).SchemeIs(kChromeUIScheme), nsurl_result)
        << "Scheme check failed for " << url;
  }
}

// Tests UrlHasChromeScheme with const GURL& paramter.
TEST_F(ChromeURLUtilTest, GURLHasChromeScheme) {
  for (unsigned int i = 0; i < std::size(kSchemeTestData); ++i) {
    GURL gurl(kSchemeTestData[i]);
    bool result = UrlHasChromeScheme(gurl);
    EXPECT_EQ(gurl.SchemeIs(kChromeUIScheme), result)
        << "Scheme check failed for " << gurl.spec();
  }
}

TEST_F(ChromeURLUtilTest, GetBundleURLScheme) {
  // Verifies that there is some default values.
  ChromeAppConstants* constants = [ChromeAppConstants sharedInstance];
  NSString* originalScheme = [constants bundleURLScheme];
  EXPECT_GT([originalScheme length], 0U);

  // Verifies that Chrome scheme can be reset for testing.
  [constants setCallbackSchemeForTesting:@"blah"];
  EXPECT_NSEQ(@"blah", [constants bundleURLScheme]);

  // Resets state in case of further tests.
  [constants setCallbackSchemeForTesting:originalScheme];
}

TEST_F(ChromeURLUtilTest, GetAllBundleURLSchemes) {
  // Verifies that there is at least 3 scheme (regular, secure and callback).
  ChromeAppConstants* constants = [ChromeAppConstants sharedInstance];
  NSArray* schemes = [constants allBundleURLSchemes];
  EXPECT_GT([schemes count], 2U);

  // Verifies that at least the main unit test scheme is in returned schemes.
  NSString* unittestScheme = @"ios-chrome-unittests.http";
  EXPECT_TRUE([schemes containsObject:unittestScheme]);
}

}  // namespace
