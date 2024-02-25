// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/cookie_cache.h"

#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

using net::CanonicalCookie;

namespace {

CanonicalCookie MakeCookie(const GURL& url,
                           const std::string& name,
                           const std::string& value) {
  return *CanonicalCookie::CreateUnsafeCookieForTesting(
      name, value, url.host(), url.path(), base::Time(), base::Time(),
      base::Time(), base::Time(), false, false,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT);
}

}  // namespace

using CookieCacheTest = PlatformTest;

TEST_F(CookieCacheTest, UpdateAddsCookieAllowsnullptr) {
  CookieCache cache;
  const GURL test_url("http://www.google.com");
  std::vector<CanonicalCookie> cookies;
  cookies.push_back(MakeCookie(test_url, "abc", "def"));
  EXPECT_TRUE(cache.Update(test_url, "abc", cookies, nullptr, nullptr));
  EXPECT_FALSE(cache.Update(test_url, "abc", cookies, nullptr, nullptr));
}

TEST_F(CookieCacheTest, UpdateAddsCookie) {
  CookieCache cache;
  const GURL test_url("http://www.google.com");
  std::vector<CanonicalCookie> cookies;
  cookies.push_back(MakeCookie(test_url, "abc", "def"));
  std::vector<net::CanonicalCookie> removed;
  std::vector<net::CanonicalCookie> changed;

  EXPECT_TRUE(cache.Update(test_url, "abc", cookies, &removed, &changed));
  EXPECT_TRUE(removed.empty());
  EXPECT_EQ(1U, changed.size());
  EXPECT_EQ("abc", changed[0].Name());
  EXPECT_EQ("def", changed[0].Value());
  removed.clear();
  changed.clear();

  EXPECT_FALSE(cache.Update(test_url, "abc", cookies, &removed, &changed));
  EXPECT_TRUE(removed.empty());
  EXPECT_TRUE(changed.empty());
}

TEST_F(CookieCacheTest, UpdateAddsDistinctCookie) {
  CookieCache cache;
  const GURL test_url("http://www.google.com");
  const GURL test_url_path("http://www.google.com/foo");
  const GURL test_url_path_long("http://www.google.com/foo/bar");
  std::vector<CanonicalCookie> cookies;
  std::vector<net::CanonicalCookie> removed;
  std::vector<net::CanonicalCookie> changed;

  cookies.push_back(MakeCookie(test_url, "abc", "def"));
  EXPECT_TRUE(
      cache.Update(test_url_path_long, "abc", cookies, &removed, &changed));
  EXPECT_TRUE(removed.empty());
  EXPECT_EQ(1U, changed.size());
  removed.clear();
  changed.clear();

  cookies.push_back(MakeCookie(test_url_path, "abc", "def"));
  EXPECT_TRUE(
      cache.Update(test_url_path_long, "abc", cookies, &removed, &changed));
  EXPECT_TRUE(removed.empty());
  EXPECT_EQ(1U, changed.size());
  removed.clear();
  changed.clear();

  cookies.push_back(MakeCookie(test_url_path_long, "abc", "def"));
  EXPECT_TRUE(
      cache.Update(test_url_path_long, "abc", cookies, &removed, &changed));
  EXPECT_TRUE(removed.empty());
  EXPECT_EQ(1U, changed.size());
}

TEST_F(CookieCacheTest, UpdateValueChanged) {
  CookieCache cache;
  const GURL test_url("http://www.google.com");
  std::vector<CanonicalCookie> cookies;

  cookies.push_back(MakeCookie(test_url, "abc", "def"));
  EXPECT_TRUE(cache.Update(test_url, "abc", cookies, nullptr, nullptr));

  std::vector<net::CanonicalCookie> removed;
  std::vector<net::CanonicalCookie> changed;
  cookies[0] = MakeCookie(test_url, "abc", "ghi");
  EXPECT_TRUE(cache.Update(test_url, "abc", cookies, &removed, &changed));
  EXPECT_EQ(1U, removed.size());
  EXPECT_EQ("abc", removed[0].Name());
  EXPECT_EQ("def", removed[0].Value());

  EXPECT_EQ(1U, changed.size());
  EXPECT_EQ("abc", changed[0].Name());
  EXPECT_EQ("ghi", changed[0].Value());
}

TEST_F(CookieCacheTest, UpdateDeletedCookie) {
  CookieCache cache;
  const GURL test_url("http://www.google.com");
  std::vector<CanonicalCookie> cookies;
  cookies.push_back(MakeCookie(test_url, "abc", "def"));
  EXPECT_TRUE(cache.Update(test_url, "abc", cookies, nullptr, nullptr));
  cookies.clear();

  std::vector<net::CanonicalCookie> removed;
  std::vector<net::CanonicalCookie> changed;
  EXPECT_TRUE(cache.Update(test_url, "abc", cookies, &removed, &changed));
  EXPECT_EQ(1U, removed.size());
  EXPECT_TRUE(changed.empty());
}

TEST_F(CookieCacheTest, UpdatePathChanged) {
  CookieCache cache;
  const GURL test_url("http://www.google.com");
  const GURL test_url_path("http://www.google.com/foo");
  std::vector<CanonicalCookie> cookies;
  cookies.push_back(MakeCookie(test_url, "abc", "def"));
  EXPECT_TRUE(cache.Update(test_url, "abc", cookies, nullptr, nullptr));

  std::vector<net::CanonicalCookie> removed;
  std::vector<net::CanonicalCookie> changed;
  cookies[0] = MakeCookie(test_url_path, "abc", "def");
  EXPECT_TRUE(cache.Update(test_url, "abc", cookies, &removed, &changed));
  EXPECT_EQ(1U, removed.size());
  EXPECT_EQ(1U, changed.size());
}

TEST_F(CookieCacheTest, MultipleDomains) {
  CookieCache cache;
  const GURL test_url_a("http://www.google.com");
  const GURL test_url_b("http://test.google.com");
  const GURL cookieurl("http://google.com");
  std::vector<CanonicalCookie> cookies;
  cookies.push_back(MakeCookie(cookieurl, "abc", "def"));
  EXPECT_TRUE(cache.Update(test_url_a, "abc", cookies, nullptr, nullptr));
  EXPECT_FALSE(cache.Update(test_url_a, "abc", cookies, nullptr, nullptr));
  EXPECT_TRUE(cache.Update(test_url_b, "abc", cookies, nullptr, nullptr));
  EXPECT_FALSE(cache.Update(test_url_b, "abc", cookies, nullptr, nullptr));
}

TEST_F(CookieCacheTest, MultipleNames) {
  CookieCache cache;
  const GURL cookieurl("http://google.com");
  std::vector<CanonicalCookie> cookies;
  cookies.push_back(MakeCookie(cookieurl, "abc", "def"));
  EXPECT_TRUE(cache.Update(cookieurl, "abc", cookies, nullptr, nullptr));
  EXPECT_FALSE(cache.Update(cookieurl, "abc", cookies, nullptr, nullptr));
  cookies[0] = MakeCookie(cookieurl, "def", "def");
  EXPECT_TRUE(cache.Update(cookieurl, "def", cookies, nullptr, nullptr));
  EXPECT_FALSE(cache.Update(cookieurl, "def", cookies, nullptr, nullptr));
  cookies[0] = MakeCookie(cookieurl, "abc", "def");
  EXPECT_FALSE(cache.Update(cookieurl, "abc", cookies, nullptr, nullptr));
}

}  // namespace net
