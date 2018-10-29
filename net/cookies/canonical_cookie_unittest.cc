// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/canonical_cookie.h"

#include "base/test/metrics/histogram_tester.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

// Helper for testing BuildCookieLine
void MatchCookieLineToVector(
    const std::string& line,
    const std::vector<std::unique_ptr<CanonicalCookie>>& cookies) {
  std::vector<CanonicalCookie> list;
  for (const auto& cookie : cookies)
    list.push_back(*cookie);
  EXPECT_EQ(line, CanonicalCookie::BuildCookieLine(list));
}

}  // namespace

TEST(CanonicalCookieTest, Constructor) {
  GURL url("http://www.example.com/test");
  base::Time current_time = base::Time::Now();

  std::unique_ptr<CanonicalCookie> cookie(std::make_unique<CanonicalCookie>(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), false, false, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_EQ("A", cookie->Name());
  EXPECT_EQ("2", cookie->Value());
  EXPECT_EQ("www.example.com", cookie->Domain());
  EXPECT_EQ("/test", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());
  EXPECT_FALSE(cookie->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie->SameSite());

  std::unique_ptr<CanonicalCookie> cookie2(std::make_unique<CanonicalCookie>(
      "A", "2", ".www.example.com", "/", current_time, base::Time(),
      base::Time(), false, false, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_EQ("A", cookie2->Name());
  EXPECT_EQ("2", cookie2->Value());
  EXPECT_EQ(".www.example.com", cookie2->Domain());
  EXPECT_EQ("/", cookie2->Path());
  EXPECT_FALSE(cookie2->IsSecure());
  EXPECT_FALSE(cookie2->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie2->SameSite());
}

TEST(CanonicalCookie, CreationCornerCases) {
  base::Time creation_time = base::Time::Now();
  CookieOptions options;
  std::unique_ptr<CanonicalCookie> cookie;

  // Space in name.
  cookie = CanonicalCookie::Create(GURL("http://www.example.com/test/foo.html"),
                                   "A C=2", creation_time, options);
  EXPECT_TRUE(cookie.get());
  EXPECT_EQ("A C", cookie->Name());

  // Semicolon in path.
  cookie = CanonicalCookie::Create(GURL("http://fool/;/"), "*", creation_time,
                                   options);
  EXPECT_TRUE(cookie.get());
}

TEST(CanonicalCookieTest, Create) {
  // Test creating cookies from a cookie string.
  GURL url("http://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;

  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "A=2", creation_time, options));
  EXPECT_EQ("A", cookie->Name());
  EXPECT_EQ("2", cookie->Value());
  EXPECT_EQ("www.example.com", cookie->Domain());
  EXPECT_EQ("/test", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());

  GURL url2("http://www.foo.com");
  cookie = CanonicalCookie::Create(url2, "B=1", creation_time, options);
  EXPECT_EQ("B", cookie->Name());
  EXPECT_EQ("1", cookie->Value());
  EXPECT_EQ("www.foo.com", cookie->Domain());
  EXPECT_EQ("/", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());

  // Test creating secure cookies.
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-alone disallows
  // insecure URLs from setting secure cookies.
  cookie = CanonicalCookie::Create(url, "A=2; Secure", creation_time, options);
  EXPECT_FALSE(cookie.get());

  // Test creating http only cookies.
  cookie =
      CanonicalCookie::Create(url, "A=2; HttpOnly", creation_time, options);
  EXPECT_FALSE(cookie.get());
  CookieOptions httponly_options;
  httponly_options.set_include_httponly();
  cookie = CanonicalCookie::Create(url, "A=2; HttpOnly", creation_time,
                                   httponly_options);
  EXPECT_TRUE(cookie->IsHttpOnly());

  // Test creating SameSite cookies.
  CookieOptions same_site_options;
  same_site_options.set_same_site_cookie_mode(
      CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=Strict", creation_time,
                                   same_site_options);
  EXPECT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::STRICT_MODE, cookie->SameSite());
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=Lax", creation_time,
                                   same_site_options);

  // Test the creating cookies using specific parameter instead of a cookie
  // string.
  cookie = std::make_unique<CanonicalCookie>(
      "A", "2", ".www.example.com", "/test", creation_time, base::Time(),
      base::Time(), false, false, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_EQ("A", cookie->Name());
  EXPECT_EQ("2", cookie->Value());
  EXPECT_EQ(".www.example.com", cookie->Domain());
  EXPECT_EQ("/test", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());
  EXPECT_FALSE(cookie->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie->SameSite());

  cookie = std::make_unique<CanonicalCookie>(
      "A", "2", ".www.example.com", "/test", creation_time, base::Time(),
      base::Time(), false, false, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_EQ("A", cookie->Name());
  EXPECT_EQ("2", cookie->Value());
  EXPECT_EQ(".www.example.com", cookie->Domain());
  EXPECT_EQ("/test", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());
  EXPECT_FALSE(cookie->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie->SameSite());
}

TEST(CanonicalCookieTest, CreateNonStandardSameSite) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  std::unique_ptr<CanonicalCookie> cookie;
  CookieOptions options;

  options.set_same_site_cookie_mode(
      CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);

  // Non-standard value for the SameSite attribute.
  cookie =
      CanonicalCookie::Create(url, "A=2; SameSite=NonStandard", now, options);
  EXPECT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie->SameSite());

  // Omit value for the SameSite attribute.
  cookie = CanonicalCookie::Create(url, "A=2; SameSite", now, options);
  EXPECT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie->SameSite());
}

TEST(CanonicalCookieTest, CreateInvalidHttpOnly) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  CookieOptions options;

  options.set_exclude_httponly();
  std::unique_ptr<CanonicalCookie> cookie =
      CanonicalCookie::Create(url, "A=2; HttpOnly", now, options);
  EXPECT_EQ(nullptr, cookie.get());
}

TEST(CanonicalCookieTest, CreateWithInvalidDomain) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  CookieOptions options;

  std::unique_ptr<CanonicalCookie> cookie =
      CanonicalCookie::Create(url, "A=2; Domain=wrongdomain.com", now, options);
  EXPECT_EQ(nullptr, cookie.get());
}

TEST(CanonicalCookieTest, EmptyExpiry) {
  GURL url("http://www7.ipdl.inpit.go.jp/Tokujitu/tjkta.ipdl?N0000=108");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;

  std::string cookie_line =
      "ACSTM=20130308043820420042; path=/; domain=ipdl.inpit.go.jp; Expires=";
  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, cookie_line, creation_time, options));
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time(), cookie->ExpiryDate());

  // With a stale server time
  options.set_server_time(creation_time - base::TimeDelta::FromHours(1));
  cookie = CanonicalCookie::Create(url, cookie_line, creation_time, options);
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time(), cookie->ExpiryDate());

  // With a future server time
  options.set_server_time(creation_time + base::TimeDelta::FromHours(1));
  cookie = CanonicalCookie::Create(url, cookie_line, creation_time, options);
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time(), cookie->ExpiryDate());
}

TEST(CanonicalCookieTest, IsEquivalent) {
  GURL url("https://www.example.com/");
  std::string cookie_name = "A";
  std::string cookie_value = "2EDA-EF";
  std::string cookie_domain = ".www.example.com";
  std::string cookie_path = "/path";
  base::Time creation_time = base::Time::Now();
  base::Time expiration_time = creation_time + base::TimeDelta::FromDays(2);
  bool secure(false);
  bool httponly(false);
  CookieSameSite same_site(CookieSameSite::NO_RESTRICTION);

  // Test that a cookie is equivalent to itself.
  std::unique_ptr<CanonicalCookie> cookie(std::make_unique<CanonicalCookie>(
      cookie_name, cookie_value, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM));
  EXPECT_TRUE(cookie->IsEquivalent(*cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Test that two identical cookies are equivalent.
  std::unique_ptr<CanonicalCookie> other_cookie(
      std::make_unique<CanonicalCookie>(
          cookie_name, cookie_value, cookie_domain, cookie_path, creation_time,
          expiration_time, base::Time(), secure, httponly, same_site,
          COOKIE_PRIORITY_MEDIUM));
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Tests that use different variations of attribute values that
  // DON'T affect cookie equivalence.
  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, "2", cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_HIGH);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  base::Time other_creation_time =
      creation_time + base::TimeDelta::FromMinutes(2);
  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, "2", cookie_domain, cookie_path, other_creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_name, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), true, httponly, same_site,
      COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_name, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, true, same_site,
      COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_name, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly,
      CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Cookies whose names mismatch are not equivalent.
  other_cookie = std::make_unique<CanonicalCookie>(
      "B", cookie_value, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_FALSE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_FALSE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // A domain cookie at 'www.example.com' is not equivalent to a host cookie
  // at the same domain. These are, however, equivalent according to the laxer
  // rules of 'IsEquivalentForSecureCookieMatching'.
  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_value, "www.example.com", cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_TRUE(cookie->IsDomainCookie());
  EXPECT_FALSE(other_cookie->IsDomainCookie());
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Likewise, a cookie on 'example.com' is not equivalent to a cookie on
  // 'www.example.com', but they are equivalent for secure cookie matching.
  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_value, ".example.com", cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Paths are a bit more complicated. 'IsEquivalent' requires an exact path
  // match, while secure cookie matching uses a more relaxed 'IsOnPath' check.
  // That is, |cookie| set on '/path' is not equivalent in either way to
  // |other_cookie| set on '/test' or '/path/subpath'. It is, however,
  // equivalent for secure cookie matching to |other_cookie| set on '/'.
  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_value, cookie_domain, "/test", creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_FALSE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_FALSE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_value, cookie_domain, cookie_path + "/subpath",
      creation_time, expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_FALSE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_value, cookie_domain, "/", creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_FALSE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));
}

TEST(CanonicalCookieTest, IsDomainMatch) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;

  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "A=2", creation_time, options));
  EXPECT_TRUE(cookie->IsHostCookie());
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));

  cookie = CanonicalCookie::Create(url, "A=2; Domain=www.example.com",
                                   creation_time, options);
  EXPECT_TRUE(cookie->IsDomainCookie());
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));

  cookie = CanonicalCookie::Create(url, "A=2; Domain=.www.example.com",
                                   creation_time, options);
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));
}

TEST(CanonicalCookieTest, IsOnPath) {
  base::Time creation_time = base::Time::Now();
  CookieOptions options;

  std::unique_ptr<CanonicalCookie> cookie(CanonicalCookie::Create(
      GURL("http://www.example.com"), "A=2", creation_time, options));
  EXPECT_TRUE(cookie->IsOnPath("/"));
  EXPECT_TRUE(cookie->IsOnPath("/test"));
  EXPECT_TRUE(cookie->IsOnPath("/test/bar.html"));

  // Test the empty string edge case.
  EXPECT_FALSE(cookie->IsOnPath(std::string()));

  cookie = CanonicalCookie::Create(GURL("http://www.example.com/test/foo.html"),
                                   "A=2", creation_time, options);
  EXPECT_FALSE(cookie->IsOnPath("/"));
  EXPECT_TRUE(cookie->IsOnPath("/test"));
  EXPECT_TRUE(cookie->IsOnPath("/test/bar.html"));
  EXPECT_TRUE(cookie->IsOnPath("/test/sample/bar.html"));
}

TEST(CanonicalCookieTest, IncludeForRequestURL) {
  GURL url("http://www.example.com");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;

  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "A=2", creation_time, options));
  EXPECT_TRUE(cookie->IncludeForRequestURL(url, options));
  EXPECT_TRUE(cookie->IncludeForRequestURL(
      GURL("http://www.example.com/foo/bar"), options));
  EXPECT_TRUE(cookie->IncludeForRequestURL(
      GURL("https://www.example.com/foo/bar"), options));
  EXPECT_FALSE(
      cookie->IncludeForRequestURL(GURL("https://sub.example.com"), options));
  EXPECT_FALSE(cookie->IncludeForRequestURL(GURL("https://sub.www.example.com"),
                                            options));

  // Test that cookie with a cookie path that does not match the url path are
  // not included.
  cookie = CanonicalCookie::Create(url, "A=2; Path=/foo/bar", creation_time,
                                   options);
  EXPECT_FALSE(cookie->IncludeForRequestURL(url, options));
  EXPECT_TRUE(cookie->IncludeForRequestURL(
      GURL("http://www.example.com/foo/bar/index.html"), options));

  // Test that a secure cookie is not included for a non secure URL.
  GURL secure_url("https://www.example.com");
  cookie = CanonicalCookie::Create(secure_url, "A=2; Secure", creation_time,
                                   options);
  EXPECT_TRUE(cookie->IsSecure());
  EXPECT_TRUE(cookie->IncludeForRequestURL(secure_url, options));
  EXPECT_FALSE(cookie->IncludeForRequestURL(url, options));

  // Test that http only cookies are only included if the include httponly flag
  // is set on the cookie options.
  options.set_include_httponly();
  cookie =
      CanonicalCookie::Create(url, "A=2; HttpOnly", creation_time, options);
  EXPECT_TRUE(cookie->IsHttpOnly());
  EXPECT_TRUE(cookie->IncludeForRequestURL(url, options));
  options.set_exclude_httponly();
  EXPECT_FALSE(cookie->IncludeForRequestURL(url, options));
}

TEST(CanonicalCookieTest, IncludeSameSiteForSameSiteURL) {
  GURL url("https://example.test");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;
  std::unique_ptr<CanonicalCookie> cookie;

  // `SameSite=Strict` cookies are included for a URL only if the options'
  // SameSiteCookieMode is INCLUDE_STRICT_AND_LAX.
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=Strict", creation_time,
                                   options);
  EXPECT_EQ(CookieSameSite::STRICT_MODE, cookie->SameSite());
  options.set_same_site_cookie_mode(
      CookieOptions::SameSiteCookieMode::DO_NOT_INCLUDE);
  EXPECT_FALSE(cookie->IncludeForRequestURL(url, options));
  options.set_same_site_cookie_mode(
      CookieOptions::SameSiteCookieMode::INCLUDE_LAX);
  EXPECT_FALSE(cookie->IncludeForRequestURL(url, options));
  options.set_same_site_cookie_mode(
      CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
  EXPECT_TRUE(cookie->IncludeForRequestURL(url, options));

  // `SameSite=Lax` cookies are included for a URL only if the options'
  // SameSiteCookieMode is INCLUDE_STRICT_AND_LAX.
  cookie =
      CanonicalCookie::Create(url, "A=2; SameSite=Lax", creation_time, options);
  EXPECT_EQ(CookieSameSite::LAX_MODE, cookie->SameSite());
  options.set_same_site_cookie_mode(
      CookieOptions::SameSiteCookieMode::DO_NOT_INCLUDE);
  EXPECT_FALSE(cookie->IncludeForRequestURL(url, options));
  options.set_same_site_cookie_mode(
      CookieOptions::SameSiteCookieMode::INCLUDE_LAX);
  EXPECT_TRUE(cookie->IncludeForRequestURL(url, options));
  options.set_same_site_cookie_mode(
      CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
  EXPECT_TRUE(cookie->IncludeForRequestURL(url, options));
}

TEST(CanonicalCookieTest, PartialCompare) {
  GURL url("http://www.example.com");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;
  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "a=b", creation_time, options));
  std::unique_ptr<CanonicalCookie> cookie_different_path(
      CanonicalCookie::Create(url, "a=b; path=/foo", creation_time, options));
  std::unique_ptr<CanonicalCookie> cookie_different_value(
      CanonicalCookie::Create(url, "a=c", creation_time, options));

  // Cookie is equivalent to itself.
  EXPECT_FALSE(cookie->PartialCompare(*cookie));

  // Changing the path affects the ordering.
  EXPECT_TRUE(cookie->PartialCompare(*cookie_different_path));
  EXPECT_FALSE(cookie_different_path->PartialCompare(*cookie));

  // Changing the value does not affect the ordering.
  EXPECT_FALSE(cookie->PartialCompare(*cookie_different_value));
  EXPECT_FALSE(cookie_different_value->PartialCompare(*cookie));

  // Cookies identical for PartialCompare() are equivalent.
  EXPECT_TRUE(cookie->IsEquivalent(*cookie_different_value));
  EXPECT_TRUE(cookie->IsEquivalent(*cookie));
}

TEST(CanonicalCookieTest, FullCompare) {
  GURL url("http://www.example.com");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;
  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "a=b", creation_time, options));
  std::unique_ptr<CanonicalCookie> cookie_different_path(
      CanonicalCookie::Create(url, "a=b; path=/foo", creation_time, options));
  std::unique_ptr<CanonicalCookie> cookie_different_value(
      CanonicalCookie::Create(url, "a=c", creation_time, options));

  // Cookie is equivalent to itself.
  EXPECT_FALSE(cookie->FullCompare(*cookie));

  // Changing the path affects the ordering.
  EXPECT_TRUE(cookie->FullCompare(*cookie_different_path));
  EXPECT_FALSE(cookie_different_path->FullCompare(*cookie));

  // Changing the value affects the ordering.
  EXPECT_TRUE(cookie->FullCompare(*cookie_different_value));
  EXPECT_FALSE(cookie_different_value->FullCompare(*cookie));

  // FullCompare() implies PartialCompare().
  auto check_consistency =
      [](const CanonicalCookie& a, const CanonicalCookie& b) {
        if (a.FullCompare(b))
          EXPECT_FALSE(b.PartialCompare(a));
        else if (b.FullCompare(a))
          EXPECT_FALSE(a.PartialCompare(b));
      };

  check_consistency(*cookie, *cookie_different_path);
  check_consistency(*cookie, *cookie_different_value);
  check_consistency(*cookie_different_path, *cookie_different_value);
}

TEST(CanonicalCookieTest, SecureCookiePrefix) {
  GURL https_url("https://www.example.test");
  GURL http_url("http://www.example.test");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;

  // A __Secure- cookie must be Secure.
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Secure-A=B", creation_time,
                                       options));
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Secure-A=B; httponly",
                                       creation_time, options));

  // A typoed prefix does not have to be Secure.
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__secure-A=B; Secure",
                                      creation_time, options));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__secure-A=C;", creation_time,
                                      options));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__SecureA=B; Secure",
                                      creation_time, options));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__SecureA=C;", creation_time,
                                      options));

  // A __Secure- cookie can't be set on a non-secure origin.
  EXPECT_FALSE(CanonicalCookie::Create(http_url, "__Secure-A=B; Secure",
                                       creation_time, options));
}

TEST(CanonicalCookieTest, HostCookiePrefix) {
  GURL https_url("https://www.example.test");
  GURL http_url("http://www.example.test");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;
  std::string domain = https_url.host();

  // A __Host- cookie must be Secure.
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Host-A=B;", creation_time,
                                       options));
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "__Host-A=B; Domain=" + domain + "; Path=/;", creation_time,
      options));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Host-A=B; Path=/; Secure;",
                                      creation_time, options));

  // A __Host- cookie must be set from a secure scheme.
  EXPECT_FALSE(CanonicalCookie::Create(
      http_url, "__Host-A=B; Domain=" + domain + "; Path=/; Secure;",
      creation_time, options));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Host-A=B; Path=/; Secure;",
                                      creation_time, options));

  // A __Host- cookie can't have a Domain.
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "__Host-A=B; Domain=" + domain + "; Path=/; Secure;",
      creation_time, options));
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "__Host-A=B; Domain=" + domain + "; Secure;", creation_time,
      options));

  // A __Host- cookie must have a Path of "/".
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "__Host-A=B; Path=/foo; Secure;", creation_time, options));
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Host-A=B; Secure;",
                                       creation_time, options));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Host-A=B; Secure; Path=/;",
                                      creation_time, options));

  // Rules don't apply for a typoed prefix.
  EXPECT_TRUE(CanonicalCookie::Create(
      http_url, "__host-A=B; Domain=" + domain + "; Path=/;", creation_time,
      options));
  EXPECT_TRUE(CanonicalCookie::Create(
      https_url, "__HostA=B; Domain=" + domain + "; Secure;", creation_time,
      options));
}

TEST(CanonicalCookieTest, EnforceSecureCookiesRequireSecureScheme) {
  GURL http_url("http://www.example.com");
  GURL https_url("https://www.example.com");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;

  std::unique_ptr<CanonicalCookie> http_cookie_no_secure(
      CanonicalCookie::Create(http_url, "a=b", creation_time, options));
  std::unique_ptr<CanonicalCookie> http_cookie_secure(
      CanonicalCookie::Create(http_url, "a=b; Secure", creation_time, options));
  std::unique_ptr<CanonicalCookie> https_cookie_no_secure(
      CanonicalCookie::Create(https_url, "a=b", creation_time, options));
  std::unique_ptr<CanonicalCookie> https_cookie_secure(CanonicalCookie::Create(
      https_url, "a=b; Secure", creation_time, options));

  EXPECT_TRUE(http_cookie_no_secure.get());
  EXPECT_FALSE(http_cookie_secure.get());
  EXPECT_TRUE(https_cookie_no_secure.get());
  EXPECT_TRUE(https_cookie_secure.get());
}

TEST(CanonicalCookieTest, IsCanonical) {
  // Base correct template.
  EXPECT_TRUE(CanonicalCookie("A", "B", "x.y", "/path", base::Time(),
                              base::Time(), base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Newline in name.
  EXPECT_FALSE(CanonicalCookie("A\n", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Carriage return in name.
  EXPECT_FALSE(CanonicalCookie("A\r", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Null character in name.
  EXPECT_FALSE(CanonicalCookie(std::string("A\0Z", 3), "B", "x.y", "/path",
                               base::Time(), base::Time(), base::Time(), false,
                               false, CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Name begins with whitespace.
  EXPECT_FALSE(CanonicalCookie(" A", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Name ends with whitespace.
  EXPECT_FALSE(CanonicalCookie("A ", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Empty name.  (Note this is against the spec but compatible with other
  // browsers.)
  EXPECT_TRUE(CanonicalCookie("", "B", "x.y", "/path", base::Time(),
                              base::Time(), base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Space in name
  EXPECT_TRUE(CanonicalCookie("A C", "B", "x.y", "/path", base::Time(),
                              base::Time(), base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Extra space suffixing name.
  EXPECT_FALSE(CanonicalCookie("A ", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // '=' character in name.
  EXPECT_FALSE(CanonicalCookie("A=", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Separator in name.
  EXPECT_FALSE(CanonicalCookie("A;", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // '=' character in value.
  EXPECT_TRUE(CanonicalCookie("A", "B=", "x.y", "/path", base::Time(),
                              base::Time(), base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Separator in value.
  EXPECT_FALSE(CanonicalCookie("A", "B;", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Separator in domain.
  EXPECT_FALSE(CanonicalCookie("A", "B", ";x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Garbage in domain.
  EXPECT_FALSE(CanonicalCookie("A", "B", "@:&", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Space in domain.
  EXPECT_FALSE(CanonicalCookie("A", "B", "x.y ", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Empty domain.  (This is against cookie spec, but needed for Chrome's
  // out-of-spec use of cookies for extensions; see http://crbug.com/730633.
  EXPECT_TRUE(CanonicalCookie("A", "B", "", "/path", base::Time(), base::Time(),
                              base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Path does not start with a "/".
  EXPECT_FALSE(CanonicalCookie("A", "B", "x.y", "path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Empty path.
  EXPECT_FALSE(CanonicalCookie("A", "B", "x.y", "", base::Time(), base::Time(),
                               base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Simple IPv4 address as domain.
  EXPECT_TRUE(CanonicalCookie("A", "B", "1.2.3.4", "/path", base::Time(),
                              base::Time(), base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // NOn-canonical IPv4 address as domain.
  EXPECT_FALSE(CanonicalCookie("A", "B", "01.2.03.4", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Null IPv6 address as domain.
  EXPECT_TRUE(CanonicalCookie("A", "B", "[::]", "/path", base::Time(),
                              base::Time(), base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Localhost IPv6 address as domain.
  EXPECT_TRUE(CanonicalCookie("A", "B", "[::1]", "/path", base::Time(),
                              base::Time(), base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Fully speced IPv6 address as domain.
  EXPECT_FALSE(CanonicalCookie(
                   "A", "B", "[2001:0DB8:AC10:FE01:0000:0000:0000:0000]",
                   "/path", base::Time(), base::Time(), base::Time(), false,
                   false, CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Zero abbreviated IPv6 address as domain.  Not canonical because of leading
  // zeros & uppercase hex letters.
  EXPECT_FALSE(CanonicalCookie("A", "B", "[2001:0DB8:AC10:FE01::]", "/path",
                               base::Time(), base::Time(), base::Time(), false,
                               false, CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Zero prefixes removed IPv6 address as domain.  Not canoncial because of
  // uppercase hex letters.
  EXPECT_FALSE(CanonicalCookie("A", "B", "[2001:DB8:AC10:FE01::]", "/path",
                               base::Time(), base::Time(), base::Time(), false,
                               false, CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Lowercased hex IPv6 address as domain.
  EXPECT_TRUE(CanonicalCookie("A", "B", "[2001:db8:ac10:fe01::]", "/path",
                              base::Time(), base::Time(), base::Time(), false,
                              false, CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Properly formatted host cookie.
  EXPECT_TRUE(CanonicalCookie("__Host-A", "B", "x.y", "/", base::Time(),
                              base::Time(), base::Time(), true, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Insecure host cookie.
  EXPECT_FALSE(CanonicalCookie("__Host-A", "B", "x.y", "/", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Host cookie with non-null path.
  EXPECT_FALSE(CanonicalCookie("__Host-A", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), true, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Host cookie with empty domain.
  EXPECT_FALSE(CanonicalCookie("__Host-A", "B", "", "/", base::Time(),
                               base::Time(), base::Time(), true, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Host cookie with period prefixed domain.
  EXPECT_FALSE(CanonicalCookie("__Host-A", "B", ".x.y", "/", base::Time(),
                               base::Time(), base::Time(), true, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Properly formatted secure cookie.
  EXPECT_TRUE(CanonicalCookie("__Secure-A", "B", "x.y", "/", base::Time(),
                              base::Time(), base::Time(), true, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Insecure secure cookie.
  EXPECT_FALSE(CanonicalCookie("__Secure-A", "B", "x.y", "/", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());
}

TEST(CanonicalCookieTest, TestSetCreationDate) {
  CanonicalCookie cookie("A", "B", "x.y", "/path", base::Time(), base::Time(),
                         base::Time(), false, false,
                         CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cookie.CreationDate().is_null());

  base::Time now(base::Time::Now());
  cookie.SetCreationDate(now);
  EXPECT_EQ(now, cookie.CreationDate());
}

TEST(CanonicalCookieTest, TestPrefixHistograms) {
  base::HistogramTester histograms;
  const char kCookiePrefixHistogram[] = "Cookie.CookiePrefix";
  const char kCookiePrefixBlockedHistogram[] = "Cookie.CookiePrefixBlocked";
  GURL https_url("https://www.example.test");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;

  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Host-A=B;", creation_time,
                                       options));

  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 1);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 1);

  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Host-A=B; Path=/; Secure",
                                      creation_time, options));
  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 2);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 1);
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__HostA=B; Path=/; Secure",
                                      creation_time, options));
  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 2);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 1);

  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Secure-A=B;",
                                       creation_time, options));

  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 1);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 1);
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Secure-A=B; Path=/; Secure",
                                      creation_time, options));
  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 2);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 1);
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__SecureA=B; Path=/; Secure",
                                      creation_time, options));
  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 2);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 1);
}

TEST(CanonicalCookieTest, BuildCookieLine) {
  std::vector<std::unique_ptr<CanonicalCookie>> cookies;
  GURL url("https://example.com/");
  CookieOptions options;
  base::Time now = base::Time::Now();
  MatchCookieLineToVector("", cookies);

  cookies.push_back(CanonicalCookie::Create(url, "A=B", now, options));
  MatchCookieLineToVector("A=B", cookies);
  // Nameless cookies are sent back without a prefixed '='.
  cookies.push_back(CanonicalCookie::Create(url, "C", now, options));
  MatchCookieLineToVector("A=B; C", cookies);
  // Cookies separated by ';'.
  cookies.push_back(CanonicalCookie::Create(url, "D=E", now, options));
  MatchCookieLineToVector("A=B; C; D=E", cookies);
  // BuildCookieLine doesn't reorder the list, it relies on the caller to do so.
  cookies.push_back(CanonicalCookie::Create(
      url, "F=G", now - base::TimeDelta::FromSeconds(1), options));
  MatchCookieLineToVector("A=B; C; D=E; F=G", cookies);
  // BuildCookieLine doesn't deduplicate.
  cookies.push_back(CanonicalCookie::Create(
      url, "D=E", now - base::TimeDelta::FromSeconds(2), options));
  MatchCookieLineToVector("A=B; C; D=E; F=G; D=E", cookies);
}

// Confirm that input arguments are reflected in the output cookie.
TEST(CanonicalCookieTest, CreateSanitizedCookie_Inputs) {
  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);
  base::Time one_hour_ago = base::Time::Now() - base::TimeDelta::FromHours(1);
  base::Time one_hour_from_now =
      base::Time::Now() + base::TimeDelta::FromHours(1);

  std::unique_ptr<CanonicalCookie> cc;
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_EQ("A", cc->Name());
  EXPECT_EQ("B", cc->Value());
  EXPECT_EQ("www.foo.com", cc->Domain());
  EXPECT_EQ("/foo", cc->Path());
  EXPECT_EQ(base::Time(), cc->CreationDate());
  EXPECT_EQ(base::Time(), cc->LastAccessDate());
  EXPECT_EQ(base::Time(), cc->ExpiryDate());
  EXPECT_FALSE(cc->IsSecure());
  EXPECT_FALSE(cc->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cc->SameSite());
  EXPECT_EQ(COOKIE_PRIORITY_MEDIUM, cc->Priority());
  EXPECT_FALSE(cc->IsDomainCookie());

  // Creation date
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      two_hours_ago, base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_EQ(two_hours_ago, cc->CreationDate());

  // Last access date
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      two_hours_ago, base::Time(), one_hour_ago, false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_EQ(one_hour_ago, cc->LastAccessDate());

  // Expiry
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_EQ(one_hour_from_now, cc->ExpiryDate());

  // Secure
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), true /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(cc->IsSecure());

  // Httponly
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      true /*httponly*/, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(cc->IsHttpOnly());

  // Same site
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_EQ(CookieSameSite::LAX_MODE, cc->SameSite());

  // Priority
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cc);
  EXPECT_EQ(COOKIE_PRIORITY_LOW, cc->Priority());

  // Domain cookie
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", "www.foo.com", "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
}

// Make sure sanitization and blocking of cookies works correctly.
TEST(CanonicalCookieTest, CreateSanitizedCookie_Logic) {
  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);
  base::Time one_hour_ago = base::Time::Now() - base::TimeDelta::FromHours(1);
  base::Time one_hour_from_now =
      base::Time::Now() + base::TimeDelta::FromHours(1);

  // Simple path and domain variations.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", std::string(), "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/bar"), "C", "D", "www.foo.com", "/",
      two_hours_ago, base::Time(), one_hour_ago, false /*secure*/,
      true /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "E", "F", std::string(), std::string(),
      base::Time(), base::Time(), base::Time(), true /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));

  // Test that malformed attributes fail to set the cookie.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), " A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A;", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A=", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", " B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", "www.foo.com ", "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", "foo.ozzzzzzle", "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", std::string(), "foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo ",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", "%2Efoo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://domaintest.%E3%81%BF%E3%82%93%E3%81%AA"), "A", "B",
      "domaintest.%E3%81%BF%E3%82%93%E3%81%AA", "/foo", base::Time(),
      base::Time(), base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT));

  std::unique_ptr<CanonicalCookie> cc;

  // Confirm that setting domain cookies with or without leading periods,
  // or on domains different from the URL's, functions correctly.
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", "www.foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT);
  ASSERT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
  EXPECT_EQ(".www.foo.com", cc->Domain());

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", ".www.foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT);
  ASSERT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
  EXPECT_EQ(".www.foo.com", cc->Domain());

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", ".foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT);
  ASSERT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
  EXPECT_EQ(".foo.com", cc->Domain());

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", ".www2.www.foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_FALSE(cc);

  // Secure/URL Scheme mismatch.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo ",
      base::Time(), base::Time(), base::Time(), true /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT));

  // Null creation date/non-null last access date conflict.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo", base::Time(),
      base::Time(), base::Time::Now(), false /*secure*/, false /*httponly*/,
      CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT));

  // Domain doesn't match URL
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", "www.bar.com", "/", base::Time(),
      base::Time(), base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::DEFAULT_MODE, COOKIE_PRIORITY_DEFAULT));

  // Path with unusual characters escaped.
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::DEFAULT_MODE,
      COOKIE_PRIORITY_DEFAULT);
  ASSERT_TRUE(cc);
  EXPECT_EQ("/foo%7F", cc->Path());
}

}  // namespace net
