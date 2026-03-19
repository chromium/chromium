// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/web_app_url_config.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace webapps {

TEST(WebAppUrlConfigTest, HttpUrlIsEligible) {
  EXPECT_TRUE(IsUrlEligibleForWebApp(GURL("http://example.com")));
  EXPECT_TRUE(IsUrlEligibleForWebApp(GURL("http://chromium.org")));
  EXPECT_TRUE(IsUrlEligibleForWebApp(GURL("http://www.chromium.org")));
  EXPECT_TRUE(IsUrlEligibleForWebApp(
      GURL("http://www.chromium.org/path/to/page.html")));
}

TEST(WebAppUrlConfigTest, HttpsUrlIsEligible) {
  EXPECT_TRUE(IsUrlEligibleForWebApp(GURL("https://example.com")));
  EXPECT_TRUE(IsUrlEligibleForWebApp(GURL("https://chromium.org")));
  EXPECT_TRUE(IsUrlEligibleForWebApp(GURL("https://www.chromium.org")));
  EXPECT_TRUE(IsUrlEligibleForWebApp(
      GURL("https://www.chromium.org/path/to/page.html")));
}

TEST(WebAppUrlConfigTest, HttpsUrlWithPathIsEligible) {
  EXPECT_TRUE(
      IsUrlEligibleForWebApp(GURL("https://www.chromium.org/path/to/page")));
}

TEST(WebAppUrlConfigTest, HttpsUrlWithQueryIsEligible) {
  EXPECT_TRUE(IsUrlEligibleForWebApp(GURL("https://example.com/foo?bar")));
}

TEST(WebAppUrlConfigTest, HttpsUrlWithFragmentIsEligible) {
  EXPECT_TRUE(IsUrlEligibleForWebApp(GURL("https://example.com/foo#bar")));
}

TEST(WebAppUrlConfigTest, AboutBlankIsNotEligible) {
  EXPECT_FALSE(IsUrlEligibleForWebApp(GURL("about:blank")));
}

TEST(WebAppUrlConfigTest, EmptyUrlIsNotEligible) {
  EXPECT_FALSE(IsUrlEligibleForWebApp(GURL()));
}

TEST(WebAppUrlConfigTest, InvalidUrlIsNotEligible) {
  EXPECT_FALSE(IsUrlEligibleForWebApp(GURL("not a valid url")));
}

TEST(WebAppUrlConfigTest, BlobUrlIsNotEligible) {
  EXPECT_FALSE(
      IsUrlEligibleForWebApp(GURL("blob:http://example.com/some-guid")));
}

TEST(WebAppUrlConfigTest, FilesystemUrlIsNotEligible) {
  EXPECT_FALSE(IsUrlEligibleForWebApp(
      GURL("filesystem:http://example.com/path/file.html")));
}

TEST(WebAppUrlConfigTest, FtpUrlIsNotEligible) {
  EXPECT_FALSE(IsUrlEligibleForWebApp(GURL("ftp://www.chromium.org")));
}

TEST(WebAppUrlConfigTest, FileUrlIsNotEligible) {
  EXPECT_FALSE(
      IsUrlEligibleForWebApp(GURL("file://mhjfbmdgcfjbbpaeojofohoefgiehjai")));
}

TEST(WebAppUrlConfigTest, DataUrlIsNotEligible) {
  EXPECT_FALSE(IsUrlEligibleForWebApp(GURL("data:text/html,hello")));
}

TEST(WebAppUrlConfigTest, ChromeExtensionUrlPlatformDependent) {
  bool expected = true;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  expected = false;
#endif
  EXPECT_EQ(expected,
            IsUrlEligibleForWebApp(
                GURL("chrome-extension://oafaagfgbdpldilgjjfjocjglfbolmac")));
}

TEST(WebAppUrlConfigTest, ChromePasswordManagerIsEligible) {
  EXPECT_TRUE(IsUrlEligibleForWebApp(GURL("chrome://password-manager")));
}

TEST(WebAppUrlConfigTest, ChromePasswordManagerWithPathIsEligible) {
  EXPECT_TRUE(
      IsUrlEligibleForWebApp(GURL("chrome://password-manager/passwords")));
}

TEST(WebAppUrlConfigTest, ChromeSettingsIsNotEligible) {
  EXPECT_FALSE(IsUrlEligibleForWebApp(GURL("chrome://settings")));
}

TEST(WebAppUrlConfigTest, ChromeFlagsIsNotEligible) {
  EXPECT_FALSE(IsUrlEligibleForWebApp(GURL("chrome://flags")));
}

TEST(WebAppUrlConfigTest, ChromeExtensionsPageIsNotEligible) {
  EXPECT_FALSE(IsUrlEligibleForWebApp(GURL("chrome://extensions")));
}

TEST(WebAppUrlConfigTest, ChromeUntrustedIsNotEligible) {
  EXPECT_FALSE(IsUrlEligibleForWebApp(GURL("chrome-untrusted://foo")));
}

TEST(WebAppUrlConfigTest, DevtoolsIsNotEligible) {
  EXPECT_FALSE(IsUrlEligibleForWebApp(GURL("devtools://devtools")));
}

TEST(WebAppUrlConfigTest, ChromeNewTabIsNotEligible) {
  EXPECT_FALSE(IsUrlEligibleForWebApp(GURL("chrome://newtab")));
}

TEST(WebAppUrlConfigTest, TestRegisteredHostIsEligible) {
  EXPECT_FALSE(IsUrlEligibleForWebApp(GURL("chrome://test-host")));
  {
    auto scoped = AddValidChromeUrlHostForTesting("test-host");
    EXPECT_TRUE(IsUrlEligibleForWebApp(GURL("chrome://test-host")));
  }
  EXPECT_FALSE(IsUrlEligibleForWebApp(GURL("chrome://test-host")));
}

}  // namespace webapps
