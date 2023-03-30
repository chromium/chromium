// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/url_utils.h"

#include "build/build_config.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

GURL CreateValidURL(const std::string& str) {
  GURL url(str);
  EXPECT_TRUE(url.is_valid()) << str;
  return url;
}

TEST(UrlUtilsTest, HasWebUIScheme) {
  EXPECT_TRUE(HasWebUIScheme(CreateValidURL("chrome://test")));
  EXPECT_TRUE(HasWebUIScheme(CreateValidURL("chrome-untrusted://test")));
  EXPECT_TRUE(HasWebUIScheme(CreateValidURL("devtools://test")));

  // Other chromium schemes not considered WebUI schemes.
  EXPECT_FALSE(HasWebUIScheme(CreateValidURL("chrome-error://test")));
  EXPECT_FALSE(HasWebUIScheme(CreateValidURL("googlechrome://test")));

  EXPECT_FALSE(HasWebUIScheme(CreateValidURL("http://foo/bar.html")));
  EXPECT_FALSE(HasWebUIScheme(CreateValidURL("https://foo/bar.html")));
  EXPECT_FALSE(HasWebUIScheme(CreateValidURL("data://foo")));
}

TEST(UrlUtilsTest, IsURLHandledByNetworkStack) {
  EXPECT_TRUE(
      IsURLHandledByNetworkStack(CreateValidURL("http://foo/bar.html")));
  EXPECT_TRUE(
      IsURLHandledByNetworkStack(CreateValidURL("https://foo/bar.html")));
  EXPECT_TRUE(IsURLHandledByNetworkStack(CreateValidURL("data://foo")));
  EXPECT_TRUE(IsURLHandledByNetworkStack(CreateValidURL("cid:foo@bar")));
  EXPECT_TRUE(IsURLHandledByNetworkStack(CreateValidURL("chrome://test")));
  EXPECT_TRUE(IsURLHandledByNetworkStack(CreateValidURL("devtools://test")));
  EXPECT_TRUE(
      IsURLHandledByNetworkStack(CreateValidURL("chrome-untrusted://test")));

  EXPECT_FALSE(IsURLHandledByNetworkStack(CreateValidURL("about:blank")));
  EXPECT_FALSE(IsURLHandledByNetworkStack(CreateValidURL("about:srcdoc")));
  EXPECT_FALSE(IsURLHandledByNetworkStack(CreateValidURL("javascript:foo.js")));
  EXPECT_FALSE(IsURLHandledByNetworkStack(GURL()));
}

TEST(UrlUtilsTest, IsSafeRedirectTarget) {
  EXPECT_FALSE(IsSafeRedirectTarget(
      GURL(), CreateValidURL(GetWebUIURLString("foo/bar.html"))));
  EXPECT_TRUE(
      IsSafeRedirectTarget(GURL(), CreateValidURL("http://foo/bar.html")));
  EXPECT_FALSE(
      IsSafeRedirectTarget(GURL(), CreateValidURL("file:///foo/bar/")));
  EXPECT_FALSE(IsSafeRedirectTarget(GURL(), CreateValidURL("about:blank")));
  EXPECT_FALSE(IsSafeRedirectTarget(
      GURL(), CreateValidURL("filesystem:http://foo.com/bar")));
#if !defined(CHROMECAST_BUILD)
  EXPECT_FALSE(
      IsSafeRedirectTarget(GURL(), CreateValidURL("data:text/plain,foo")));
#else
  EXPECT_TRUE(
      IsSafeRedirectTarget(GURL(), CreateValidURL("data:text/plain,foo")));
#endif
  EXPECT_FALSE(
      IsSafeRedirectTarget(GURL(), CreateValidURL("blob:https://foo.com/bar")));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(
      IsSafeRedirectTarget(GURL(), CreateValidURL("content://foo.bar")));
#endif
  EXPECT_TRUE(IsSafeRedirectTarget(CreateValidURL("file:///foo/bar"),
                                   CreateValidURL("file:///foo/bar/")));
  EXPECT_TRUE(
      IsSafeRedirectTarget(CreateValidURL("filesystem:http://foo.com/bar"),
                           CreateValidURL("filesystem:http://foo.com/bar")));
  EXPECT_TRUE(
      IsSafeRedirectTarget(GURL(), CreateValidURL("unknown://foo/bar/")));
  EXPECT_FALSE(IsSafeRedirectTarget(CreateValidURL("http://foo/bar.html"),
                                    CreateValidURL("file:///foo/bar/")));
  EXPECT_TRUE(IsSafeRedirectTarget(CreateValidURL("file:///foo/bar/"),
                                   CreateValidURL("http://foo/bar.html")));

  // WebUI schemes
  EXPECT_FALSE(IsSafeRedirectTarget(GURL(), CreateValidURL("chrome://test")));
  EXPECT_FALSE(IsSafeRedirectTarget(GURL(), CreateValidURL("devtools://test")));
  EXPECT_FALSE(
      IsSafeRedirectTarget(GURL(), CreateValidURL("chrome-untrusted://test")));

  // TODO(cmumford): Capturing current behavior, but should probably prevent
  //                 redirect to invalid URL.
  EXPECT_TRUE(IsSafeRedirectTarget(GURL(), GURL()));
}

}  // namespace content
