// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/canonical_cookie.h"

#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_options.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace net {

namespace {
const std::vector<std::string> kCookieableSchemes = {"http", "https", "ws",
                                                     "wss"};

// Helper for testing BuildCookieLine
void MatchCookieLineToVector(
    const std::string& line,
    const std::vector<std::unique_ptr<CanonicalCookie>>& cookies) {
  std::vector<CanonicalCookie> list;
  for (const auto& cookie : cookies)
    list.push_back(*cookie);
  EXPECT_EQ(line, CanonicalCookie::BuildCookieLine(list));
}

// Helper for checking CookieAccessResults. Should be called with matchers (or
// values) for each of the fields of a CookieAccessResult.
MATCHER_P4(MatchesCookieAccessResult,
           status,
           effective_same_site,
           access_semantics,
           is_allowed_to_access_secure_cookies,
           "") {
  return testing::ExplainMatchResult(
      testing::AllOf(
          testing::Field(&CookieAccessResult::status, status),
          testing::Field(&CookieAccessResult::effective_same_site,
                         effective_same_site),
          testing::Field(&CookieAccessResult::access_semantics,
                         access_semantics),
          testing::Field(
              &CookieAccessResult::is_allowed_to_access_secure_cookies,
              is_allowed_to_access_secure_cookies)),
      arg, result_listener);
}

// Helper for checking that status.HasDowngradeWarning() == true.
MATCHER(HasDowngradeWarning, "") {
  CookieInclusionStatus status = arg;
  return testing::ExplainMatchResult(true, status.HasDowngradeWarning(),
                                     result_listener);
}

// Helper for checking that status.HasWarningReason(reason) == true.
MATCHER_P(HasWarningReason, reason, "") {
  CookieInclusionStatus status = arg;
  return testing::ExplainMatchResult(true, status.HasWarningReason(reason),
                                     result_listener);
}

// Helper for checking that status.HasExclusionReason(reason) == true.
MATCHER_P(HasExclusionReason, reason, "") {
  CookieInclusionStatus status = arg;
  return testing::ExplainMatchResult(true, status.HasExclusionReason(reason),
                                     result_listener);
}

// Helper for checking that status.HasExactlyExclusionReasonsForTesting(reasons)
// == true.
MATCHER_P(HasExactlyExclusionReasonsForTesting, reasons, "") {
  CookieInclusionStatus status = arg;
  return testing::ExplainMatchResult(
      true, status.HasExactlyExclusionReasonsForTesting(reasons),
      result_listener);
}

// Helper for checking that status.IsInclude() == true.
MATCHER(IsInclude, "") {
  CookieInclusionStatus status = arg;
  return testing::ExplainMatchResult(true, status.IsInclude(), result_listener);
}

}  // namespace

using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::Not;
using testing::Property;

TEST(CanonicalCookieTest, Constructor) {
  base::Time current_time = base::Time::Now();

  // CreateUnsafeCookieForTesting just forwards to the constructor.
  auto cookie1 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false, CookieSourceScheme::kSecure, 443);
  EXPECT_EQ("A", cookie1->Name());
  EXPECT_EQ("2", cookie1->Value());
  EXPECT_EQ("www.example.com", cookie1->Domain());
  EXPECT_EQ("/test", cookie1->Path());
  EXPECT_FALSE(cookie1->IsSecure());
  EXPECT_FALSE(cookie1->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie1->SameSite());
  EXPECT_EQ(CookiePriority::COOKIE_PRIORITY_DEFAULT, cookie1->Priority());
  EXPECT_FALSE(cookie1->IsSameParty());
  EXPECT_EQ(cookie1->SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cookie1->SourcePort(), 443);

  auto cookie2 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", ".www.example.com", "/", current_time, base::Time(),
      base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, true, CookieSourceScheme::kNonSecure, 65536);
  EXPECT_EQ("A", cookie2->Name());
  EXPECT_EQ("2", cookie2->Value());
  EXPECT_EQ(".www.example.com", cookie2->Domain());
  EXPECT_EQ("/", cookie2->Path());
  EXPECT_FALSE(cookie2->IsSecure());
  EXPECT_FALSE(cookie2->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie2->SameSite());
  EXPECT_EQ(CookiePriority::COOKIE_PRIORITY_DEFAULT, cookie2->Priority());
  EXPECT_TRUE(cookie2->IsSameParty());
  EXPECT_EQ(cookie2->SourceScheme(), CookieSourceScheme::kNonSecure);
  // Because the port can be set explicitly in the constructor its value can be
  // independent of the other parameters. In this case, test that an invalid
  // port value is interpreted as such.
  EXPECT_EQ(cookie2->SourcePort(), url::PORT_INVALID);

  // Set Secure to true but don't specify source_scheme or port.
  auto cookie3 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", ".www.example.com", "/", current_time, base::Time(),
      base::Time(), true /* secure */, false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false);
  EXPECT_TRUE(cookie3->IsSecure());
  EXPECT_EQ(cookie3->SourceScheme(), CookieSourceScheme::kUnset);
  EXPECT_EQ(cookie3->SourcePort(), url::PORT_UNSPECIFIED);

  auto cookie4 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", ".www.example.com", "/test", current_time, base::Time(),
      base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false);
  EXPECT_EQ("A", cookie4->Name());
  EXPECT_EQ("2", cookie4->Value());
  EXPECT_EQ(".www.example.com", cookie4->Domain());
  EXPECT_EQ("/test", cookie4->Path());
  EXPECT_FALSE(cookie4->IsSecure());
  EXPECT_FALSE(cookie4->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie4->SameSite());
  EXPECT_FALSE(cookie4->IsSameParty());
  EXPECT_EQ(cookie4->SourceScheme(), CookieSourceScheme::kUnset);
  EXPECT_EQ(cookie4->SourcePort(), url::PORT_UNSPECIFIED);

  // Test some port edge cases: unspecified.
  auto cookie5 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", ".www.example.com", "/", current_time, base::Time(),
      base::Time(), true /* secure */, false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false, CookieSourceScheme::kUnset,
      url::PORT_UNSPECIFIED);
  EXPECT_EQ(cookie5->SourcePort(), url::PORT_UNSPECIFIED);

  // Test some port edge cases: invalid.
  auto cookie6 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", ".www.example.com", "/", current_time, base::Time(),
      base::Time(), true /* secure */, false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false, CookieSourceScheme::kUnset,
      url::PORT_INVALID);
  EXPECT_EQ(cookie6->SourcePort(), url::PORT_INVALID);
}

TEST(CanonicalCookie, CreationCornerCases) {
  base::Time creation_time = base::Time::Now();
  std::unique_ptr<CanonicalCookie> cookie;
  base::Optional<base::Time> server_time = base::nullopt;

  // Space in name.
  cookie = CanonicalCookie::Create(GURL("http://www.example.com/test/foo.html"),
                                   "A C=2", creation_time, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_EQ("A C", cookie->Name());

  // Semicolon in path.
  cookie = CanonicalCookie::Create(GURL("http://fool/;/"), "*", creation_time,
                                   server_time);
  EXPECT_TRUE(cookie.get());

  // Control characters in name or value.
  CookieInclusionStatus status;
  cookie =
      CanonicalCookie::Create(GURL("http://www.example.com/test/foo.html"),
                              "\b=foo", creation_time, server_time, &status);
  EXPECT_FALSE(cookie.get());
  EXPECT_TRUE(status.HasExclusionReason(
      CookieInclusionStatus::ExclusionReason::EXCLUDE_FAILURE_TO_STORE));
  cookie =
      CanonicalCookie::Create(GURL("http://www.example.com/test/foo.html"),
                              "bar=\b", creation_time, server_time, &status);
  EXPECT_FALSE(cookie.get());
  EXPECT_TRUE(status.HasExclusionReason(
      CookieInclusionStatus::ExclusionReason::EXCLUDE_FAILURE_TO_STORE));
}

TEST(CanonicalCookieTest, Create) {
  // Test creating cookies from a cookie string.
  GURL url("http://www.example.com/test/foo.html");
  GURL https_url("https://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;

  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "A=2", creation_time, server_time));
  EXPECT_EQ("A", cookie->Name());
  EXPECT_EQ("2", cookie->Value());
  EXPECT_EQ("www.example.com", cookie->Domain());
  EXPECT_EQ("/test", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kNonSecure);
  EXPECT_EQ(cookie->SourcePort(), 80);

  GURL url2("http://www.foo.com");
  cookie = CanonicalCookie::Create(url2, "B=1", creation_time, server_time);
  EXPECT_EQ("B", cookie->Name());
  EXPECT_EQ("1", cookie->Value());
  EXPECT_EQ("www.foo.com", cookie->Domain());
  EXPECT_EQ("/", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kNonSecure);
  EXPECT_EQ(cookie->SourcePort(), 80);

  // Test creating secure cookies. Secure scheme is not checked upon creation,
  // so a URL of any scheme can create a Secure cookie.
  cookie =
      CanonicalCookie::Create(url, "A=2; Secure", creation_time, server_time);
  EXPECT_TRUE(cookie->IsSecure());

  cookie = CanonicalCookie::Create(https_url, "A=2; Secure", creation_time,
                                   server_time);
  EXPECT_TRUE(cookie->IsSecure());

  GURL url3("https://www.foo.com");
  cookie =
      CanonicalCookie::Create(url3, "A=2; Secure", creation_time, server_time);
  EXPECT_TRUE(cookie->IsSecure());
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kSecure);

  cookie = CanonicalCookie::Create(url3, "A=2", creation_time, server_time);
  EXPECT_FALSE(cookie->IsSecure());
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kSecure);

  // Test creating cookie from localhost URL.
  cookie = CanonicalCookie::Create(GURL("http://localhost/path"), "A=2",
                                   creation_time, server_time);
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kNonSecure);

  cookie = CanonicalCookie::Create(GURL("http://127.0.0.1/path"), "A=2",
                                   creation_time, server_time);
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kNonSecure);

  cookie = CanonicalCookie::Create(GURL("http://[::1]/path"), "A=2",
                                   creation_time, server_time);
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kNonSecure);

  cookie = CanonicalCookie::Create(GURL("https://localhost/path"), "A=2",
                                   creation_time, server_time);
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kSecure);

  cookie = CanonicalCookie::Create(GURL("https://127.0.0.1/path"), "A=2",
                                   creation_time, server_time);
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kSecure);

  cookie = CanonicalCookie::Create(GURL("https://[::1]/path"), "A=2",
                                   creation_time, server_time);
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kSecure);

  // Test creating http only cookies. HttpOnly is not checked upon creation.
  cookie =
      CanonicalCookie::Create(url, "A=2; HttpOnly", creation_time, server_time);
  EXPECT_TRUE(cookie->IsHttpOnly());

  cookie =
      CanonicalCookie::Create(url, "A=2; HttpOnly", creation_time, server_time);
  EXPECT_TRUE(cookie->IsHttpOnly());

  // Test creating SameSite cookies. SameSite is not checked upon creation.
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=Strict", creation_time,
                                   server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::STRICT_MODE, cookie->SameSite());
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=Lax", creation_time,
                                   server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::LAX_MODE, cookie->SameSite());
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=Extended", creation_time,
                                   server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookie->SameSite());
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=None", creation_time,
                                   server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie->SameSite());
  cookie = CanonicalCookie::Create(url, "A=2", creation_time, server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookie->SameSite());

  // Test creating cookies with different ports.
  cookie = CanonicalCookie::Create(GURL("http://www.foo.com"), "B=1",
                                   creation_time, server_time);
  EXPECT_EQ(cookie->SourcePort(), 80);

  cookie = CanonicalCookie::Create(GURL("http://www.foo.com:81"), "B=1",
                                   creation_time, server_time);
  EXPECT_EQ(cookie->SourcePort(), 81);

  cookie = CanonicalCookie::Create(GURL("https://www.foo.com"), "B=1",
                                   creation_time, server_time);
  EXPECT_EQ(cookie->SourcePort(), 443);

  cookie = CanonicalCookie::Create(GURL("https://www.foo.com:1234"), "B=1",
                                   creation_time, server_time);
  EXPECT_EQ(cookie->SourcePort(), 1234);

  cookie = CanonicalCookie::Create(GURL("http://www.foo.com:443"), "B=1",
                                   creation_time, server_time);
  EXPECT_EQ(cookie->SourcePort(), 443);

  // GURL's port parsing will handle any invalid ports, but let's still make
  // sure we get the expected result anyway.
  cookie = CanonicalCookie::Create(GURL("http://www.foo.com:70000"), "B=1",
                                   creation_time, server_time);
  EXPECT_EQ(cookie->SourcePort(), url::PORT_INVALID);
}

TEST(CanonicalCookieTest, CreateNonStandardSameSite) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  std::unique_ptr<CanonicalCookie> cookie;
  base::Optional<base::Time> server_time = base::nullopt;

  // Non-standard value for the SameSite attribute.
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=NonStandard", now,
                                   server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookie->SameSite());

  // Omit value for the SameSite attribute.
  cookie = CanonicalCookie::Create(url, "A=2; SameSite", now, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookie->SameSite());
}

TEST(CanonicalCookieTest, CreateSameSiteInCrossSiteContexts) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  std::unique_ptr<CanonicalCookie> cookie;
  base::Optional<base::Time> server_time = base::nullopt;

  // A cookie can be created from any SameSiteContext regardless of SameSite
  // value (it is upon setting the cookie that the SameSiteContext comes into
  // effect).
  cookie =
      CanonicalCookie::Create(url, "A=2; SameSite=Strict", now, server_time);
  EXPECT_TRUE(cookie.get());
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=Lax", now, server_time);
  EXPECT_TRUE(cookie.get());
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=None", now, server_time);
  EXPECT_TRUE(cookie.get());
  cookie = CanonicalCookie::Create(url, "A=2;", now, server_time);
  EXPECT_TRUE(cookie.get());
}

TEST(CanonicalCookieTest, CreateHttpOnly) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  CookieInclusionStatus status;

  // An HttpOnly cookie can be created.
  std::unique_ptr<CanonicalCookie> cookie =
      CanonicalCookie::Create(url, "A=2; HttpOnly", now, server_time, &status);
  EXPECT_TRUE(cookie->IsHttpOnly());
  EXPECT_TRUE(status.IsInclude());
}

TEST(CanonicalCookieTest, CreateWithInvalidDomain) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  CookieInclusionStatus status;

  std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::Create(
      url, "A=2; Domain=wrongdomain.com", now, server_time, &status);
  EXPECT_EQ(nullptr, cookie.get());
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));
}

TEST(CanonicalCookieTest, CreateSameParty) {
  GURL url("http://www.example.com/test/foo.html");
  GURL https_url("https://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;

  CookieInclusionStatus status;
  std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::Create(
      url, "A=2; SameParty; Secure", creation_time, server_time, &status);
  ASSERT_TRUE(cookie.get());
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsSecure());
  EXPECT_TRUE(cookie->IsSameParty());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookie->SameSite());

  cookie = CanonicalCookie::Create(url, "A=2; SameParty; SameSite=None; Secure",
                                   creation_time, server_time, &status);
  ASSERT_TRUE(cookie.get());
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsSecure());
  EXPECT_TRUE(cookie->IsSameParty());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie->SameSite());

  cookie = CanonicalCookie::Create(url, "A=2; SameParty; SameSite=Lax; Secure",
                                   creation_time, server_time, &status);
  ASSERT_TRUE(cookie.get());
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsSecure());
  EXPECT_TRUE(cookie->IsSameParty());
  EXPECT_EQ(CookieSameSite::LAX_MODE, cookie->SameSite());

  // SameParty cookie with SameSite=Strict is invalid.
  cookie =
      CanonicalCookie::Create(url, "A=2; SameParty; SameSite=Strict; Secure",
                              creation_time, server_time, &status);
  EXPECT_FALSE(cookie.get());
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_SAMEPARTY}));

  // SameParty cookie without Secure is invalid.
  cookie = CanonicalCookie::Create(url, "A=2; SameParty; SameSite=Lax",
                                   creation_time, server_time, &status);
  EXPECT_FALSE(cookie.get());
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_SAMEPARTY}));
}

TEST(CanonicalCookieTest, EmptyExpiry) {
  GURL url("http://www7.ipdl.inpit.go.jp/Tokujitu/tjkta.ipdl?N0000=108");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;

  std::string cookie_line =
      "ACSTM=20130308043820420042; path=/; domain=ipdl.inpit.go.jp; Expires=";
  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, cookie_line, creation_time, server_time));
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time(), cookie->ExpiryDate());

  // With a stale server time
  server_time = creation_time - base::TimeDelta::FromHours(1);
  cookie =
      CanonicalCookie::Create(url, cookie_line, creation_time, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time(), cookie->ExpiryDate());

  // With a future server time
  server_time = creation_time + base::TimeDelta::FromHours(1);
  cookie =
      CanonicalCookie::Create(url, cookie_line, creation_time, server_time);
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
  bool secure = false;
  bool httponly = false;
  CookieSameSite same_site = CookieSameSite::NO_RESTRICTION;
  bool same_party = false;

  // Test that a cookie is equivalent to itself.
  auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM, same_party);
  EXPECT_TRUE(cookie->IsEquivalent(*cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Test that two identical cookies are equivalent.
  auto other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM, same_party);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Tests that use different variations of attribute values that
  // DON'T affect cookie equivalence.
  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, "2", cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_HIGH, same_party);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  base::Time other_creation_time =
      creation_time + base::TimeDelta::FromMinutes(2);
  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, "2", cookie_domain, cookie_path, other_creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM, same_party);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_name, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), true, httponly, same_site,
      COOKIE_PRIORITY_LOW, same_party);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_name, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, true, same_site,
      COOKIE_PRIORITY_LOW, same_party);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_name, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly,
      CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_LOW, same_party);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Cookies whose names mismatch are not equivalent.
  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      "B", cookie_value, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM, same_party);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_FALSE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_FALSE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // A domain cookie at 'www.example.com' is not equivalent to a host cookie
  // at the same domain. These are, however, equivalent according to the laxer
  // rules of 'IsEquivalentForSecureCookieMatching'.
  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, "www.example.com", cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM, same_party);
  EXPECT_TRUE(cookie->IsDomainCookie());
  EXPECT_FALSE(other_cookie->IsDomainCookie());
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Likewise, a cookie on 'example.com' is not equivalent to a cookie on
  // 'www.example.com', but they are equivalent for secure cookie matching.
  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, ".example.com", cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM, same_party);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Paths are a bit more complicated. 'IsEquivalent' requires an exact path
  // match, while secure cookie matching uses a more relaxed 'IsOnPath' check.
  // That is, |cookie| set on '/path' is not equivalent in either way to
  // |other_cookie| set on '/test' or '/path/subpath'. It is, however,
  // equivalent for secure cookie matching to |other_cookie| set on '/'.
  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, cookie_domain, "/test", creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM, same_party);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_FALSE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_FALSE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, cookie_domain, cookie_path + "/subpath",
      creation_time, expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM, same_party);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  // The path comparison is asymmetric
  EXPECT_FALSE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, cookie_domain, "/", creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM, same_party);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_FALSE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));
}

TEST(CanonicalCookieTest, IsEquivalentForSecureCookieMatching) {
  struct {
    struct {
      const char* name;
      const char* domain;
      const char* path;
    } cookie, secure_cookie;
    bool equivalent;
    bool is_symmetric;  // Whether the reverse comparison has the same result.
  } kTests[] = {
      // Equivalent to itself
      {{"A", "a.foo.com", "/"}, {"A", "a.foo.com", "/"}, true, true},
      {{"A", ".a.foo.com", "/"}, {"A", ".a.foo.com", "/"}, true, true},
      // Names are different
      {{"A", "a.foo.com", "/"}, {"B", "a.foo.com", "/"}, false, true},
      // Host cookie and domain cookie with same hostname match
      {{"A", "a.foo.com", "/"}, {"A", ".a.foo.com", "/"}, true, true},
      // Subdomains and superdomains match
      {{"A", "a.foo.com", "/"}, {"A", ".foo.com", "/"}, true, true},
      {{"A", ".a.foo.com", "/"}, {"A", ".foo.com", "/"}, true, true},
      {{"A", "a.foo.com", "/"}, {"A", "foo.com", "/"}, true, true},
      {{"A", ".a.foo.com", "/"}, {"A", "foo.com", "/"}, true, true},
      // Different domains don't match
      {{"A", "a.foo.com", "/"}, {"A", "b.foo.com", "/"}, false, true},
      {{"A", "a.foo.com", "/"}, {"A", "ba.foo.com", "/"}, false, true},
      // Path attribute matches if it is a subdomain, but not vice versa.
      {{"A", "a.foo.com", "/sub"}, {"A", "a.foo.com", "/"}, true, false},
      // Different paths don't match
      {{"A", "a.foo.com", "/sub"}, {"A", "a.foo.com", "/other"}, false, true},
      {{"A", "a.foo.com", "/a/b"}, {"A", "a.foo.com", "/a/c"}, false, true},
  };

  for (auto test : kTests) {
    auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
        test.cookie.name, "value1", test.cookie.domain, test.cookie.path,
        base::Time(), base::Time(), base::Time(), false /* secure */, false,
        CookieSameSite::LAX_MODE, COOKIE_PRIORITY_MEDIUM, false);
    auto secure_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
        test.secure_cookie.name, "value2", test.secure_cookie.domain,
        test.secure_cookie.path, base::Time(), base::Time(), base::Time(),
        true /* secure */, false, CookieSameSite::LAX_MODE,
        COOKIE_PRIORITY_MEDIUM, false);

    EXPECT_EQ(test.equivalent,
              cookie->IsEquivalentForSecureCookieMatching(*secure_cookie));
    EXPECT_EQ(test.equivalent == test.is_symmetric,
              secure_cookie->IsEquivalentForSecureCookieMatching(*cookie));
  }
}

TEST(CanonicalCookieTest, IsDomainMatch) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;

  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "A=2", creation_time, server_time));
  EXPECT_TRUE(cookie->IsHostCookie());
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));

  cookie = CanonicalCookie::Create(url, "A=2; Domain=www.example.com",
                                   creation_time, server_time);
  EXPECT_TRUE(cookie->IsDomainCookie());
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));

  cookie = CanonicalCookie::Create(url, "A=2; Domain=.www.example.com",
                                   creation_time, server_time);
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));
}

TEST(CanonicalCookieTest, IsOnPath) {
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;

  std::unique_ptr<CanonicalCookie> cookie(CanonicalCookie::Create(
      GURL("http://www.example.com"), "A=2", creation_time, server_time));
  EXPECT_TRUE(cookie->IsOnPath("/"));
  EXPECT_TRUE(cookie->IsOnPath("/test"));
  EXPECT_TRUE(cookie->IsOnPath("/test/bar.html"));

  // Test the empty string edge case.
  EXPECT_FALSE(cookie->IsOnPath(std::string()));

  cookie = CanonicalCookie::Create(GURL("http://www.example.com/test/foo.html"),
                                   "A=2", creation_time, server_time);
  EXPECT_FALSE(cookie->IsOnPath("/"));
  EXPECT_TRUE(cookie->IsOnPath("/test"));
  EXPECT_TRUE(cookie->IsOnPath("/test/bar.html"));
  EXPECT_TRUE(cookie->IsOnPath("/test/sample/bar.html"));
}

struct EffectiveSameSiteTestCase {
  CookieSameSite same_site;
  CookieEffectiveSameSite effective_same_site;
  CookieAccessSemantics access_semantics;
};

void VerifyEffectiveSameSiteTestCases(
    base::Time creation_time,
    base::Time expiry_time,
    bool is_samesite_by_default_enabled,
    std::vector<EffectiveSameSiteTestCase> test_cases) {
  base::test::ScopedFeatureList feature_list;
  if (is_samesite_by_default_enabled) {
    feature_list.InitAndEnableFeature(features::kSameSiteByDefaultCookies);
  } else {
    feature_list.InitAndDisableFeature(features::kSameSiteByDefaultCookies);
  }

  for (const auto& test_case : test_cases) {
    auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
        "A", "2", "example.test", "/", creation_time, expiry_time, base::Time(),
        true /* secure */, false /* httponly */, test_case.same_site,
        COOKIE_PRIORITY_DEFAULT, false);
    EXPECT_EQ(
        test_case.effective_same_site,
        cookie->GetEffectiveSameSiteForTesting(test_case.access_semantics));
  }
}

TEST(CanonicalCookieTest, GetEffectiveSameSite) {
  // Test cases that are always the same, regardless of time or
  // SameSite-by-default feature status.
  const std::vector<EffectiveSameSiteTestCase> common_test_cases = {
      // Explicitly specified SameSite always has the same effective SameSite
      // regardless of the access semantics.
      {CookieSameSite::NO_RESTRICTION, CookieEffectiveSameSite::NO_RESTRICTION,
       CookieAccessSemantics::UNKNOWN},
      {CookieSameSite::LAX_MODE, CookieEffectiveSameSite::LAX_MODE,
       CookieAccessSemantics::UNKNOWN},
      {CookieSameSite::STRICT_MODE, CookieEffectiveSameSite::STRICT_MODE,
       CookieAccessSemantics::UNKNOWN},
      {CookieSameSite::NO_RESTRICTION, CookieEffectiveSameSite::NO_RESTRICTION,
       CookieAccessSemantics::LEGACY},
      {CookieSameSite::LAX_MODE, CookieEffectiveSameSite::LAX_MODE,
       CookieAccessSemantics::LEGACY},
      {CookieSameSite::STRICT_MODE, CookieEffectiveSameSite::STRICT_MODE,
       CookieAccessSemantics::LEGACY},
      {CookieSameSite::NO_RESTRICTION, CookieEffectiveSameSite::NO_RESTRICTION,
       CookieAccessSemantics::NONLEGACY},
      {CookieSameSite::LAX_MODE, CookieEffectiveSameSite::LAX_MODE,
       CookieAccessSemantics::NONLEGACY},
      {CookieSameSite::STRICT_MODE, CookieEffectiveSameSite::STRICT_MODE,
       CookieAccessSemantics::NONLEGACY},
      // UNSPECIFIED always maps to NO_RESTRICTION if LEGACY access semantics.
      {CookieSameSite::UNSPECIFIED, CookieEffectiveSameSite::NO_RESTRICTION,
       CookieAccessSemantics::LEGACY}};

  // Test cases that differ based on access semantics, feature status, and
  // whether cookie is recently created:

  std::vector<EffectiveSameSiteTestCase> enabled_recent_test_cases = {
      {CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE,
       CookieAccessSemantics::UNKNOWN},
      {CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE,
       CookieAccessSemantics::NONLEGACY}};

  std::vector<EffectiveSameSiteTestCase> enabled_not_recent_test_cases = {
      {CookieSameSite::UNSPECIFIED, CookieEffectiveSameSite::LAX_MODE,
       CookieAccessSemantics::UNKNOWN},
      {CookieSameSite::UNSPECIFIED, CookieEffectiveSameSite::LAX_MODE,
       CookieAccessSemantics::NONLEGACY}};

  std::vector<EffectiveSameSiteTestCase> disabled_recent_test_cases = {
      {CookieSameSite::UNSPECIFIED, CookieEffectiveSameSite::NO_RESTRICTION,
       CookieAccessSemantics::UNKNOWN},
      {CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE,
       CookieAccessSemantics::NONLEGACY}};

  std::vector<EffectiveSameSiteTestCase> disabled_not_recent_test_cases = {
      {CookieSameSite::UNSPECIFIED, CookieEffectiveSameSite::NO_RESTRICTION,
       CookieAccessSemantics::UNKNOWN},
      {CookieSameSite::UNSPECIFIED, CookieEffectiveSameSite::LAX_MODE,
       CookieAccessSemantics::NONLEGACY}};

  // Test GetEffectiveSameSite for recently created cookies
  // Session cookie created less than kLaxAllowUnsafeMaxAge ago.
  base::Time now = base::Time::Now();
  base::Time creation_time = now - (kLaxAllowUnsafeMaxAge / 4);
  VerifyEffectiveSameSiteTestCases(creation_time, base::Time(), false,
                                   common_test_cases);
  VerifyEffectiveSameSiteTestCases(creation_time, base::Time(), false,
                                   disabled_recent_test_cases);
  VerifyEffectiveSameSiteTestCases(creation_time, base::Time(), true,
                                   common_test_cases);
  VerifyEffectiveSameSiteTestCases(creation_time, base::Time(), true,
                                   enabled_recent_test_cases);

  // Persistent cookie with max age less than kLaxAllowUnsafeMaxAge.
  base::Time expiry_time = creation_time + (kLaxAllowUnsafeMaxAge / 4);
  VerifyEffectiveSameSiteTestCases(creation_time, expiry_time, false,
                                   common_test_cases);
  VerifyEffectiveSameSiteTestCases(creation_time, expiry_time, false,
                                   disabled_recent_test_cases);
  VerifyEffectiveSameSiteTestCases(creation_time, expiry_time, true,
                                   common_test_cases);
  VerifyEffectiveSameSiteTestCases(creation_time, expiry_time, true,
                                   enabled_recent_test_cases);

  // Test GetEffectiveSameSite for not-recently-created cookies:
  // Session cookie created more than kLaxAllowUnsafeMaxAge ago.
  creation_time = now - (kLaxAllowUnsafeMaxAge * 4);
  VerifyEffectiveSameSiteTestCases(creation_time, base::Time(), false,
                                   common_test_cases);
  VerifyEffectiveSameSiteTestCases(creation_time, base::Time(), false,
                                   disabled_not_recent_test_cases);
  VerifyEffectiveSameSiteTestCases(creation_time, base::Time(), true,
                                   common_test_cases);
  VerifyEffectiveSameSiteTestCases(creation_time, base::Time(), true,
                                   enabled_not_recent_test_cases);

  // Persistent cookie with max age more than kLaxAllowUnsafeMaxAge, created
  // more than kLaxAllowUnsafeMaxAge ago.
  expiry_time = creation_time + (kLaxAllowUnsafeMaxAge * 8);
  VerifyEffectiveSameSiteTestCases(creation_time, expiry_time, false,
                                   common_test_cases);
  VerifyEffectiveSameSiteTestCases(creation_time, expiry_time, false,
                                   disabled_not_recent_test_cases);
  VerifyEffectiveSameSiteTestCases(creation_time, expiry_time, true,
                                   common_test_cases);
  VerifyEffectiveSameSiteTestCases(creation_time, expiry_time, true,
                                   enabled_not_recent_test_cases);
}

TEST(CanonicalCookieTest, IncludeForRequestURL) {
  GURL url("http://www.example.com");
  base::Time creation_time = base::Time::Now();
  CookieOptions options = CookieOptions::MakeAllInclusive();
  base::Optional<base::Time> server_time = base::nullopt;

  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "A=2", creation_time, server_time));
  EXPECT_TRUE(cookie
                  ->IncludeForRequestURL(
                      url, options,
                      CookieAccessParams{
                          net::CookieAccessSemantics::UNKNOWN,
                          /*delegate_treats_url_as_trustworthy=*/false,
                          CookieSamePartyStatus::kNoSamePartyEnforcement})
                  .status.IsInclude());
  EXPECT_TRUE(cookie
                  ->IncludeForRequestURL(
                      GURL("http://www.example.com/foo/bar"), options,
                      CookieAccessParams{
                          net::CookieAccessSemantics::UNKNOWN,
                          /*delegate_treats_url_as_trustworthy=*/false,
                          CookieSamePartyStatus::kNoSamePartyEnforcement})
                  .status.IsInclude());
  EXPECT_TRUE(cookie
                  ->IncludeForRequestURL(
                      GURL("https://www.example.com/foo/bar"), options,
                      CookieAccessParams{
                          net::CookieAccessSemantics::UNKNOWN,
                          /*delegate_treats_url_as_trustworthy=*/false,
                          CookieSamePartyStatus::kNoSamePartyEnforcement})
                  .status.IsInclude());
  EXPECT_TRUE(cookie
                  ->IncludeForRequestURL(
                      GURL("https://sub.example.com"), options,
                      CookieAccessParams{
                          net::CookieAccessSemantics::UNKNOWN,
                          /*delegate_treats_url_as_trustworthy=*/false,
                          CookieSamePartyStatus::kNoSamePartyEnforcement})
                  .status.HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH}));
  EXPECT_TRUE(cookie
                  ->IncludeForRequestURL(
                      GURL("https://sub.www.example.com"), options,
                      CookieAccessParams{
                          net::CookieAccessSemantics::UNKNOWN,
                          /*delegate_treats_url_as_trustworthy=*/false,
                          CookieSamePartyStatus::kNoSamePartyEnforcement})
                  .status.HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH}));

  // Test that cookie with a cookie path that does not match the url path are
  // not included.
  cookie = CanonicalCookie::Create(url, "A=2; Path=/foo/bar", creation_time,
                                   server_time);
  EXPECT_TRUE(cookie
                  ->IncludeForRequestURL(
                      url, options,
                      CookieAccessParams{
                          net::CookieAccessSemantics::UNKNOWN,
                          /*delegate_treats_url_as_trustworthy=*/false,
                          CookieSamePartyStatus::kNoSamePartyEnforcement})
                  .status.HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_NOT_ON_PATH}));
  EXPECT_TRUE(cookie
                  ->IncludeForRequestURL(
                      GURL("http://www.example.com/foo/bar/index.html"),
                      options,
                      CookieAccessParams{
                          net::CookieAccessSemantics::UNKNOWN,
                          /*delegate_treats_url_as_trustworthy=*/false,
                          CookieSamePartyStatus::kNoSamePartyEnforcement})
                  .status.IsInclude());

  // Test that a secure cookie is not included for a non secure URL.
  GURL secure_url("https://www.example.com");
  cookie = CanonicalCookie::Create(secure_url, "A=2; Secure", creation_time,
                                   server_time);
  EXPECT_TRUE(cookie->IsSecure());
  EXPECT_TRUE(cookie
                  ->IncludeForRequestURL(
                      secure_url, options,
                      CookieAccessParams{
                          net::CookieAccessSemantics::UNKNOWN,
                          /*delegate_treats_url_as_trustworthy=*/false,
                          CookieSamePartyStatus::kNoSamePartyEnforcement})
                  .status.IsInclude());
  EXPECT_TRUE(cookie
                  ->IncludeForRequestURL(
                      url, options,
                      CookieAccessParams{
                          net::CookieAccessSemantics::UNKNOWN,
                          /*delegate_treats_url_as_trustworthy=*/false,
                          CookieSamePartyStatus::kNoSamePartyEnforcement})
                  .status.HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));

  // Test that a delegate can make an exception, however, and ask for a
  // non-secure URL to be treated as trustworthy... with a warning.
  cookie =
      CanonicalCookie::Create(url, "A=2; Secure", creation_time, server_time);
  ASSERT_TRUE(cookie);
  EXPECT_TRUE(cookie->IsSecure());
  CookieAccessResult result = cookie->IncludeForRequestURL(
      url, options,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/true,
                         CookieSamePartyStatus::kNoSamePartyEnforcement});
  EXPECT_TRUE(result.status.IsInclude());
  EXPECT_TRUE(result.status.HasWarningReason(
      CookieInclusionStatus::WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC));

  // The same happens for localhost even w/o delegate intervention.
  GURL localhost_url("http://localhost/");
  cookie = CanonicalCookie::Create(localhost_url, "A=2; Secure", creation_time,
                                   server_time);
  ASSERT_TRUE(cookie);
  EXPECT_TRUE(cookie->IsSecure());
  result = cookie->IncludeForRequestURL(
      localhost_url, options,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false,
                         CookieSamePartyStatus::kNoSamePartyEnforcement});
  EXPECT_TRUE(result.status.IsInclude());
  EXPECT_TRUE(result.status.HasWarningReason(
      CookieInclusionStatus::WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC));

  // An unneeded exception doesn't add a warning, however.
  cookie = CanonicalCookie::Create(secure_url, "A=2; Secure", creation_time,
                                   server_time);
  ASSERT_TRUE(cookie);
  EXPECT_TRUE(cookie->IsSecure());
  result = cookie->IncludeForRequestURL(
      secure_url, options,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/true,
                         CookieSamePartyStatus::kNoSamePartyEnforcement});
  EXPECT_TRUE(result.status.IsInclude());
  EXPECT_FALSE(result.status.ShouldWarn());

  //

  // Test that http only cookies are only included if the include httponly flag
  // is set on the cookie options.
  options.set_include_httponly();
  cookie =
      CanonicalCookie::Create(url, "A=2; HttpOnly", creation_time, server_time);
  EXPECT_TRUE(cookie->IsHttpOnly());
  EXPECT_TRUE(cookie
                  ->IncludeForRequestURL(
                      url, options,
                      CookieAccessParams{
                          net::CookieAccessSemantics::UNKNOWN,
                          /*delegate_treats_url_as_trustworthy=*/false,
                          CookieSamePartyStatus::kNoSamePartyEnforcement})
                  .status.IsInclude());
  options.set_exclude_httponly();
  EXPECT_TRUE(cookie
                  ->IncludeForRequestURL(
                      url, options,
                      CookieAccessParams{
                          net::CookieAccessSemantics::UNKNOWN,
                          /*delegate_treats_url_as_trustworthy=*/false,
                          CookieSamePartyStatus::kNoSamePartyEnforcement})
                  .status.HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_HTTP_ONLY}));
}

struct IncludeForRequestURLTestCase {
  std::string cookie_line;
  CookieSameSite expected_samesite;
  CookieEffectiveSameSite expected_effective_samesite;
  CookieOptions::SameSiteCookieContext request_options_samesite_context;
  CookieInclusionStatus expected_inclusion_status;
  base::TimeDelta creation_time_delta = base::TimeDelta();
};

void VerifyIncludeForRequestURLTestCases(
    bool is_samesite_by_default_enabled,
    CookieAccessSemantics access_semantics,
    std::vector<IncludeForRequestURLTestCase> test_cases) {
  GURL url("https://example.test");

  for (const auto& test : test_cases) {
    base::test::ScopedFeatureList feature_list;
    if (is_samesite_by_default_enabled) {
      feature_list.InitAndEnableFeature(features::kSameSiteByDefaultCookies);
    } else {
      feature_list.InitAndDisableFeature(features::kSameSiteByDefaultCookies);
    }

    base::Time creation_time = base::Time::Now() - test.creation_time_delta;
    std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::Create(
        url, test.cookie_line, creation_time, base::nullopt /* server_time */);
    EXPECT_EQ(test.expected_samesite, cookie->SameSite());

    CookieOptions request_options;
    request_options.set_same_site_cookie_context(
        test.request_options_samesite_context);

    EXPECT_THAT(
        cookie->IncludeForRequestURL(
            url, request_options,
            CookieAccessParams{access_semantics,
                               /*delegate_treats_url_as_trustworthy=*/false,
                               CookieSamePartyStatus::kNoSamePartyEnforcement}),
        MatchesCookieAccessResult(test.expected_inclusion_status,
                                  test.expected_effective_samesite,
                                  access_semantics, true));
  }
}

TEST(CanonicalCookieTest, IncludeForRequestURLSameSite) {
  const base::TimeDelta kLongAge = kLaxAllowUnsafeMaxAge * 4;
  const base::TimeDelta kShortAge = kLaxAllowUnsafeMaxAge / 4;

  using SameSiteCookieContext = CookieOptions::SameSiteCookieContext;

  // Test cases that are the same regardless of feature status or access
  // semantics. For Schemeful Same-Site this means that the context downgrade is
  // a no-op (such as for NO_RESTRICTION cookies) or that there is no downgrade:
  std::vector<IncludeForRequestURLTestCase> common_test_cases = {
      // Strict cookies:
      {"Common=1;SameSite=Strict", CookieSameSite::STRICT_MODE,
       CookieEffectiveSameSite::STRICT_MODE,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus(CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)},
      {"Common=2;SameSite=Strict", CookieSameSite::STRICT_MODE,
       CookieEffectiveSameSite::STRICT_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE),
       CookieInclusionStatus(CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)},
      {"Common=3;SameSite=Strict", CookieSameSite::STRICT_MODE,
       CookieEffectiveSameSite::STRICT_MODE,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX),
       CookieInclusionStatus(CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)},
      {"Common=4;SameSite=Strict", CookieSameSite::STRICT_MODE,
       CookieEffectiveSameSite::STRICT_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
       CookieInclusionStatus()},
      // Lax cookies:
      {"Common=5;SameSite=Lax", CookieSameSite::LAX_MODE,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus(CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)},
      {"Common=6;SameSite=Lax", CookieSameSite::LAX_MODE,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE),
       CookieInclusionStatus(CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)},
      {"Common=7;SameSite=Lax", CookieSameSite::LAX_MODE,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX),
       CookieInclusionStatus()},
      {"Common=8;SameSite=Lax", CookieSameSite::LAX_MODE,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
       CookieInclusionStatus()},
      // Lax cookies with downgrade:
      {"Common=9;SameSite=Lax", CookieSameSite::LAX_MODE,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
           SameSiteCookieContext::ContextType::SAME_SITE_LAX),
       CookieInclusionStatus()},
      // None and Secure cookies:
      {"Common=10;SameSite=None;Secure", CookieSameSite::NO_RESTRICTION,
       CookieEffectiveSameSite::NO_RESTRICTION,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus()},
      {"Common=11;SameSite=None;Secure", CookieSameSite::NO_RESTRICTION,
       CookieEffectiveSameSite::NO_RESTRICTION,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE),
       CookieInclusionStatus()},
      {"Common=12;SameSite=None;Secure", CookieSameSite::NO_RESTRICTION,
       CookieEffectiveSameSite::NO_RESTRICTION,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX),
       CookieInclusionStatus()},
      {"Common=13;SameSite=None;Secure", CookieSameSite::NO_RESTRICTION,
       CookieEffectiveSameSite::NO_RESTRICTION,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
       CookieInclusionStatus()},
      // Because NO_RESTRICTION cookies are always sent, the schemeful context
      // downgrades shouldn't matter.
      {"Common=14;SameSite=None;Secure", CookieSameSite::NO_RESTRICTION,
       CookieEffectiveSameSite::NO_RESTRICTION,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
           SameSiteCookieContext::ContextType::SAME_SITE_LAX),
       CookieInclusionStatus()},
      {"Common=15;SameSite=None;Secure", CookieSameSite::NO_RESTRICTION,
       CookieEffectiveSameSite::NO_RESTRICTION,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
           SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE),
       CookieInclusionStatus()},
      {"Common=16;SameSite=None;Secure", CookieSameSite::NO_RESTRICTION,
       CookieEffectiveSameSite::NO_RESTRICTION,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
           SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus()},
      {"Common=17;SameSite=None;Secure", CookieSameSite::NO_RESTRICTION,
       CookieEffectiveSameSite::NO_RESTRICTION,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX,
                             SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus()},
      {"Common=18;SameSite=None;Secure", CookieSameSite::NO_RESTRICTION,
       CookieEffectiveSameSite::NO_RESTRICTION,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE,
           SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus()},
  };

  // Test cases where the default is None (either access semantics is LEGACY, or
  // semantics is UNKNOWN and SameSiteByDefaultCookies feature is disabled):
  std::vector<IncludeForRequestURLTestCase> default_none_test_cases = {
      // Unspecified cookies (without SameSite-by-default):
      {"DefaultNone=1", CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::NO_RESTRICTION,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           std::vector<CookieInclusionStatus::ExclusionReason>(),
           {CookieInclusionStatus::
                WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT})},
      {"DefaultNone=2", CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::NO_RESTRICTION,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           std::vector<CookieInclusionStatus::ExclusionReason>(),
           {CookieInclusionStatus::
                WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT})},
      {"DefaultNone=3", CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::NO_RESTRICTION,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX),
       CookieInclusionStatus()},
      {"DefaultNone=4", CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::NO_RESTRICTION,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
       CookieInclusionStatus()}};

  // Test cases where the default is Lax (either access semantics is NONLEGACY,
  // or access semantics is UNKNOWN and SameSiteByDefaultCookies feature is
  // enabled):
  std::vector<IncludeForRequestURLTestCase> default_lax_test_cases = {
      // Unspecified recently-created cookies (with SameSite-by-default):
      {"DefaultLax=1", CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus(
           CookieInclusionStatus::EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
           CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT),
       kShortAge},
      {"DefaultLax=2", CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           std::vector<CookieInclusionStatus::ExclusionReason>(),
           {CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE}),
       kShortAge},
      {"DefaultLax=3", CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX),
       CookieInclusionStatus(), kShortAge},
      {"DefaultLax=4", CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
       CookieInclusionStatus(), kShortAge},
      // Unspecified not-recently-created cookies (with SameSite-by-default):
      {"DefaultLax=5", CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus(
           CookieInclusionStatus::EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
           CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT),
       kLongAge},
      {"DefaultLax=6", CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE),
       CookieInclusionStatus(
           CookieInclusionStatus::EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
           CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT),
       kLongAge},
      {"DefaultLax=7", CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX),
       CookieInclusionStatus(), kLongAge},
      {"DefaultLax=8", CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
       CookieInclusionStatus(), kLongAge},
  };

  // Test cases that require LEGACY semantics or Schemeful Same-Site to be
  // disabled.
  std::vector<IncludeForRequestURLTestCase> schemeful_disabled_test_cases = {
      {"LEGACY_Schemeful=1;SameSite=Strict", CookieSameSite::STRICT_MODE,
       CookieEffectiveSameSite::STRICT_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
           SameSiteCookieContext::ContextType::SAME_SITE_LAX),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           std::vector<CookieInclusionStatus::ExclusionReason>(),
           {CookieInclusionStatus::WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE})},
      {"LEGACY_Schemeful=2;SameSite=Strict", CookieSameSite::STRICT_MODE,
       CookieEffectiveSameSite::STRICT_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
           SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           std::vector<CookieInclusionStatus::ExclusionReason>(),
           {CookieInclusionStatus::
                WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE})},
      {"LEGACY_Schemeful=3;SameSite=Strict", CookieSameSite::STRICT_MODE,
       CookieEffectiveSameSite::STRICT_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
           SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           std::vector<CookieInclusionStatus::ExclusionReason>(),
           {CookieInclusionStatus::
                WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE})},
      {"LEGACY_Schemeful=4;SameSite=Lax", CookieSameSite::LAX_MODE,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
           SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           std::vector<CookieInclusionStatus::ExclusionReason>(),
           {CookieInclusionStatus::WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE})},
      {"LEGACY_Schemeful=5;SameSite=Lax", CookieSameSite::LAX_MODE,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
           SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           std::vector<CookieInclusionStatus::ExclusionReason>(),
           {CookieInclusionStatus::WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE})},
      {"LEGACY_Schemeful=6;SameSite=Lax", CookieSameSite::LAX_MODE,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX,
                             SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           std::vector<CookieInclusionStatus::ExclusionReason>(),
           {CookieInclusionStatus::WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE})},
  };

  // Test cases that require NONLEGACY or UNKNOWN semantics with Schemeful
  // Same-Site enabled
  std::vector<IncludeForRequestURLTestCase> schemeful_enabled_test_cases = {
      {"NONLEGACY_Schemeful=1;SameSite=Strict", CookieSameSite::STRICT_MODE,
       CookieEffectiveSameSite::STRICT_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
           SameSiteCookieContext::ContextType::SAME_SITE_LAX),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           {CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT},
           {CookieInclusionStatus::WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE})},
      {"NONLEGACY_Schemeful=2;SameSite=Strict", CookieSameSite::STRICT_MODE,
       CookieEffectiveSameSite::STRICT_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
           SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           {CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT},
           {CookieInclusionStatus::
                WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE})},
      {"NONLEGACY_Schemeful=3;SameSite=Strict", CookieSameSite::STRICT_MODE,
       CookieEffectiveSameSite::STRICT_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
           SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           {CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT},
           {CookieInclusionStatus::
                WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE})},
      {"NONLEGACY_Schemeful=4;SameSite=Lax", CookieSameSite::LAX_MODE,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
           SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           {CookieInclusionStatus::EXCLUDE_SAMESITE_LAX},
           {CookieInclusionStatus::WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE})},
      {"NONLEGACY_Schemeful=5;SameSite=Lax", CookieSameSite::LAX_MODE,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(
           SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
           SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           {CookieInclusionStatus::EXCLUDE_SAMESITE_LAX},
           {CookieInclusionStatus::WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE})},
      {"NONLEGACY_Schemeful=6;SameSite=Lax", CookieSameSite::LAX_MODE,
       CookieEffectiveSameSite::LAX_MODE,
       SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX,
                             SameSiteCookieContext::ContextType::CROSS_SITE),
       CookieInclusionStatus::MakeFromReasonsForTesting(
           {CookieInclusionStatus::EXCLUDE_SAMESITE_LAX},
           {CookieInclusionStatus::WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE})},
  };

  auto SchemefulIndependentCases = [&]() {
    // Run the test cases that are independent of Schemeful Same-Site.
    VerifyIncludeForRequestURLTestCases(true, CookieAccessSemantics::UNKNOWN,
                                        common_test_cases);
    VerifyIncludeForRequestURLTestCases(true, CookieAccessSemantics::UNKNOWN,
                                        default_lax_test_cases);
    VerifyIncludeForRequestURLTestCases(true, CookieAccessSemantics::LEGACY,
                                        common_test_cases);
    VerifyIncludeForRequestURLTestCases(true, CookieAccessSemantics::LEGACY,
                                        default_none_test_cases);
    VerifyIncludeForRequestURLTestCases(true, CookieAccessSemantics::NONLEGACY,
                                        common_test_cases);
    VerifyIncludeForRequestURLTestCases(true, CookieAccessSemantics::NONLEGACY,
                                        default_lax_test_cases);
    VerifyIncludeForRequestURLTestCases(false, CookieAccessSemantics::UNKNOWN,
                                        common_test_cases);
    VerifyIncludeForRequestURLTestCases(false, CookieAccessSemantics::UNKNOWN,
                                        default_none_test_cases);
    VerifyIncludeForRequestURLTestCases(false, CookieAccessSemantics::LEGACY,
                                        common_test_cases);
    VerifyIncludeForRequestURLTestCases(false, CookieAccessSemantics::LEGACY,
                                        default_none_test_cases);
    VerifyIncludeForRequestURLTestCases(false, CookieAccessSemantics::NONLEGACY,
                                        common_test_cases);
    VerifyIncludeForRequestURLTestCases(false, CookieAccessSemantics::NONLEGACY,
                                        default_lax_test_cases);
  };

  {
    // Schemeful Same-Site disabled.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kSchemefulSameSite);

    SchemefulIndependentCases();

    VerifyIncludeForRequestURLTestCases(false, CookieAccessSemantics::LEGACY,
                                        schemeful_disabled_test_cases);
    VerifyIncludeForRequestURLTestCases(false, CookieAccessSemantics::NONLEGACY,
                                        schemeful_disabled_test_cases);
    VerifyIncludeForRequestURLTestCases(false, CookieAccessSemantics::UNKNOWN,
                                        schemeful_disabled_test_cases);
  }
  {
    // Schemeful Same-Site enabled.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kSchemefulSameSite);

    SchemefulIndependentCases();

    // With LEGACY access the cases should act as if schemeful is disabled, even
    // when it's not.
    VerifyIncludeForRequestURLTestCases(false, CookieAccessSemantics::LEGACY,
                                        schemeful_disabled_test_cases);

    VerifyIncludeForRequestURLTestCases(false, CookieAccessSemantics::NONLEGACY,
                                        schemeful_enabled_test_cases);
    VerifyIncludeForRequestURLTestCases(false, CookieAccessSemantics::UNKNOWN,
                                        schemeful_enabled_test_cases);
  }
}

// Test that non-SameSite, insecure cookies are excluded if both
// SameSiteByDefaultCookies and CookiesWithoutSameSiteMustBeSecure are enabled.
TEST(CanonicalCookieTest, IncludeCookiesWithoutSameSiteMustBeSecure) {
  GURL url("https://www.example.com");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  CookieOptions options;
  std::unique_ptr<CanonicalCookie> cookie;

  // Create the cookie without the experimental options enabled.
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=None", creation_time,
                                   server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsSecure());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie->SameSite());
  EXPECT_EQ(CookieEffectiveSameSite::NO_RESTRICTION,
            cookie->GetEffectiveSameSiteForTesting());

  // Test SameSite=None must be Secure.
  // Features on:
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kSameSiteByDefaultCookies,
         features::kCookiesWithoutSameSiteMustBeSecure} /* enabled_features */,
        {} /* disabled_features */);

    EXPECT_TRUE(
        cookie
            ->IncludeForRequestURL(
                url, options,
                CookieAccessParams{
                    CookieAccessSemantics::UNKNOWN,
                    /*delegate_treats_url_as_trustworthy=*/false,
                    CookieSamePartyStatus::kNoSamePartyEnforcement})
            .status.HasExactlyExclusionReasonsForTesting(
                {CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE}));
    EXPECT_TRUE(cookie
                    ->IncludeForRequestURL(
                        url, options,
                        CookieAccessParams{
                            CookieAccessSemantics::LEGACY,
                            /*delegate_treats_url_as_trustworthy=*/false,
                            CookieSamePartyStatus::kNoSamePartyEnforcement})
                    .status.IsInclude());
    EXPECT_TRUE(
        cookie
            ->IncludeForRequestURL(
                url, options,
                CookieAccessParams{
                    CookieAccessSemantics::NONLEGACY,
                    /*delegate_treats_url_as_trustworthy=*/false,
                    CookieSamePartyStatus::kNoSamePartyEnforcement})
            .status.HasExactlyExclusionReasonsForTesting(
                {CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE}));
  }
  // Features off:
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {} /* enabled_features */,
        {features::kSameSiteByDefaultCookies,
         features::
             kCookiesWithoutSameSiteMustBeSecure} /* disabled_features */);

    EXPECT_TRUE(cookie
                    ->IncludeForRequestURL(
                        url, options,
                        CookieAccessParams{
                            CookieAccessSemantics::UNKNOWN,
                            /*delegate_treats_url_as_trustworthy=*/false,
                            CookieSamePartyStatus::kNoSamePartyEnforcement})
                    .status.IsInclude());
    EXPECT_TRUE(cookie
                    ->IncludeForRequestURL(
                        url, options,
                        CookieAccessParams{
                            CookieAccessSemantics::LEGACY,
                            /*delegate_treats_url_as_trustworthy=*/false,
                            CookieSamePartyStatus::kNoSamePartyEnforcement})
                    .status.IsInclude());
    // If the semantics is Nonlegacy, only reject the cookie if the
    // SameSite=None-must-be-Secure feature is enabled.
    EXPECT_TRUE(cookie
                    ->IncludeForRequestURL(
                        url, options,
                        CookieAccessParams{
                            CookieAccessSemantics::NONLEGACY,
                            /*delegate_treats_url_as_trustworthy=*/false,
                            CookieSamePartyStatus::kNoSamePartyEnforcement})
                    .status.IsInclude());
  }
}

TEST(CanonicalCookieTest, IncludeForRequestURLSameParty) {
  GURL url("https://www.example.com");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  CookieOptions options;

  // SameSite is not specified.
  std::unique_ptr<CanonicalCookie> cookie_samesite_unspecified =
      CanonicalCookie::Create(url, "A=2; SameParty; Secure", creation_time,
                              server_time);
  ASSERT_TRUE(cookie_samesite_unspecified.get());
  EXPECT_TRUE(cookie_samesite_unspecified->IsSecure());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED,
            cookie_samesite_unspecified->SameSite());
  EXPECT_EQ(CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE,
            cookie_samesite_unspecified->GetEffectiveSameSiteForTesting());
  EXPECT_TRUE(cookie_samesite_unspecified->IsSameParty());

  // SameSite=None.
  std::unique_ptr<CanonicalCookie> cookie_samesite_none =
      CanonicalCookie::Create(url, "A=2; SameSite=None; SameParty; Secure",
                              creation_time, server_time);
  ASSERT_TRUE(cookie_samesite_none.get());
  EXPECT_TRUE(cookie_samesite_none->IsSecure());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie_samesite_none->SameSite());
  EXPECT_EQ(CookieEffectiveSameSite::NO_RESTRICTION,
            cookie_samesite_none->GetEffectiveSameSiteForTesting());
  EXPECT_TRUE(cookie_samesite_none->IsSameParty());

  // SameSite=Lax.
  std::unique_ptr<CanonicalCookie> cookie_samesite_lax =
      CanonicalCookie::Create(url, "A=2; SameSite=Lax; SameParty; Secure",
                              creation_time, server_time);
  ASSERT_TRUE(cookie_samesite_lax.get());
  EXPECT_TRUE(cookie_samesite_lax->IsSecure());
  EXPECT_EQ(CookieSameSite::LAX_MODE, cookie_samesite_lax->SameSite());
  EXPECT_EQ(CookieEffectiveSameSite::LAX_MODE,
            cookie_samesite_lax->GetEffectiveSameSiteForTesting());
  EXPECT_TRUE(cookie_samesite_lax->IsSameParty());

  for (const CanonicalCookie* cookie : {
           cookie_samesite_unspecified.get(),
           cookie_samesite_none.get(),
           cookie_samesite_lax.get(),
       }) {
    // SameParty cookies that should be excluded result in the appropriate
    // exclusion reason, and removes SAMESITE exclusion reasons.
    for (CookieAccessSemantics access_semantics : {
             CookieAccessSemantics::UNKNOWN,
             CookieAccessSemantics::LEGACY,
             CookieAccessSemantics::NONLEGACY,
         }) {
      EXPECT_THAT(cookie->IncludeForRequestURL(
                      url, options,
                      CookieAccessParams{
                          access_semantics,
                          /*delegate_treats_url_as_trustworthy=*/false,
                          CookieSamePartyStatus::kEnforceSamePartyExclude}),
                  MatchesCookieAccessResult(
                      HasExactlyExclusionReasonsForTesting(
                          std::vector<CookieInclusionStatus::ExclusionReason>(
                              {CookieInclusionStatus::
                                   EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT})),
                      _, _, true))
          << "SameSite = " << static_cast<int>(cookie->SameSite())
          << ", access_semantics = " << static_cast<int>(access_semantics);
    }
  }
}

TEST(CanonicalCookieTest, MultipleExclusionReasons) {
  GURL url("http://www.not-secure.com/foo");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  CookieOptions options;
  options.set_exclude_httponly();
  options.set_same_site_cookie_context(CookieOptions::SameSiteCookieContext(
      CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE));

  // Test IncludeForRequestURL()
  // Note: This is a cookie that should never exist normally, because Create()
  // would weed it out.
  auto cookie1 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "name", "value", "other-domain.com", "/bar", creation_time, base::Time(),
      base::Time(), true /* secure */, true /* httponly */,
      CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_DEFAULT, false);
  EXPECT_THAT(
      cookie1->IncludeForRequestURL(
          url, options,
          CookieAccessParams{CookieAccessSemantics::UNKNOWN,
                             /*delegate_treats_url_as_trustworthy=*/false,
                             CookieSamePartyStatus::kNoSamePartyEnforcement}),
      MatchesCookieAccessResult(
          CookieInclusionStatus::MakeFromReasonsForTesting({
              CookieInclusionStatus::EXCLUDE_HTTP_ONLY,
              CookieInclusionStatus::EXCLUDE_SECURE_ONLY,
              CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH,
              CookieInclusionStatus::EXCLUDE_NOT_ON_PATH,
              CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT,
          }),
          _, _, false));

  // Test Create()
  CookieInclusionStatus create_status;
  auto cookie2 = CanonicalCookie::Create(
      url, "__Secure-notactuallysecure=value;Domain=some-other-domain.com",
      creation_time, server_time, &create_status);
  ASSERT_FALSE(cookie2);
  EXPECT_TRUE(create_status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX,
       CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));

  // Test IsSetPermittedInContext()
  auto cookie3 = CanonicalCookie::Create(
      url, "name=value;HttpOnly;SameSite=Lax", creation_time, server_time);
  ASSERT_TRUE(cookie3);
  EXPECT_THAT(
      cookie3->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */,
                             CookieSamePartyStatus::kNoSamePartyEnforcement),
          kCookieableSchemes),
      MatchesCookieAccessResult(
          CookieInclusionStatus::MakeFromReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_HTTP_ONLY,
               CookieInclusionStatus::EXCLUDE_SAMESITE_LAX}),
          _, _, false));
}

TEST(CanonicalCookieTest, PartialCompare) {
  GURL url("http://www.example.com");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "a=b", creation_time, server_time));
  std::unique_ptr<CanonicalCookie> cookie_different_path(
      CanonicalCookie::Create(url, "a=b; path=/foo", creation_time,
                              server_time));
  std::unique_ptr<CanonicalCookie> cookie_different_value(
      CanonicalCookie::Create(url, "a=c", creation_time, server_time));

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

TEST(CanonicalCookieTest, SecureCookiePrefix) {
  GURL https_url("https://www.example.test");
  GURL http_url("http://www.example.test");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  CookieInclusionStatus status;

  // A __Secure- cookie must be Secure.
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Secure-A=B", creation_time,
                                       server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Secure-A=B; httponly",
                                       creation_time, server_time, &status));
  // (EXCLUDE_HTTP_ONLY would be fine, too)
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // A typoed prefix does not have to be Secure.
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__secure-A=B; Secure",
                                      creation_time, server_time));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__secure-A=C;", creation_time,
                                      server_time));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__SecureA=B; Secure",
                                      creation_time, server_time));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__SecureA=C;", creation_time,
                                      server_time));

  // A __Secure- cookie can't be set on a non-secure origin.
  EXPECT_FALSE(CanonicalCookie::Create(http_url, "__Secure-A=B; Secure",
                                       creation_time, server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
}

TEST(CanonicalCookieTest, HostCookiePrefix) {
  GURL https_url("https://www.example.test");
  GURL http_url("http://www.example.test");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  std::string domain = https_url.host();
  CookieInclusionStatus status;

  // A __Host- cookie must be Secure.
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Host-A=B;", creation_time,
                                       server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "__Host-A=B; Domain=" + domain + "; Path=/;", creation_time,
      server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Host-A=B; Path=/; Secure;",
                                      creation_time, server_time));

  // A __Host- cookie must be set from a secure scheme.
  EXPECT_FALSE(CanonicalCookie::Create(
      http_url, "__Host-A=B; Domain=" + domain + "; Path=/; Secure;",
      creation_time, server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Host-A=B; Path=/; Secure;",
                                      creation_time, server_time));

  // A __Host- cookie can't have a Domain.
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "__Host-A=B; Domain=" + domain + "; Path=/; Secure;",
      creation_time, server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "__Host-A=B; Domain=" + domain + "; Secure;", creation_time,
      server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // A __Host- cookie may have a domain if it's an IP address that matches the
  // URL.
  EXPECT_TRUE(
      CanonicalCookie::Create(GURL("https://127.0.0.1"),
                              "__Host-A=B; Domain=127.0.0.1; Path=/; Secure;",
                              creation_time, server_time, &status));
  // A __Host- cookie with an IP address domain does not need the domain
  // attribute specified explicitly (just like a normal domain).
  EXPECT_TRUE(CanonicalCookie::Create(GURL("https://127.0.0.1"),
                                      "__Host-A=B; Domain=; Path=/; Secure;",
                                      creation_time, server_time, &status));

  // A __Host- cookie must have a Path of "/".
  EXPECT_FALSE(CanonicalCookie::Create(https_url,
                                       "__Host-A=B; Path=/foo; Secure;",
                                       creation_time, server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Host-A=B; Secure;",
                                       creation_time, server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Host-A=B; Secure; Path=/;",
                                      creation_time, server_time));

  // Rules don't apply for a typoed prefix.
  EXPECT_TRUE(CanonicalCookie::Create(
      http_url, "__host-A=B; Domain=" + domain + "; Path=/;", creation_time,
      server_time));
  EXPECT_TRUE(CanonicalCookie::Create(
      https_url, "__HostA=B; Domain=" + domain + "; Secure;", creation_time,
      server_time));
}

TEST(CanonicalCookieTest, CanCreateSecureCookiesFromAnyScheme) {
  GURL http_url("http://www.example.com");
  GURL https_url("https://www.example.com");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;

  std::unique_ptr<CanonicalCookie> http_cookie_no_secure(
      CanonicalCookie::Create(http_url, "a=b", creation_time, server_time));
  std::unique_ptr<CanonicalCookie> http_cookie_secure(CanonicalCookie::Create(
      http_url, "a=b; Secure", creation_time, server_time));
  std::unique_ptr<CanonicalCookie> https_cookie_no_secure(
      CanonicalCookie::Create(https_url, "a=b", creation_time, server_time));
  std::unique_ptr<CanonicalCookie> https_cookie_secure(CanonicalCookie::Create(
      https_url, "a=b; Secure", creation_time, server_time));

  EXPECT_TRUE(http_cookie_no_secure.get());
  EXPECT_TRUE(http_cookie_secure.get());
  EXPECT_TRUE(https_cookie_no_secure.get());
  EXPECT_TRUE(https_cookie_secure.get());
}

TEST(CanonicalCookieTest, IsCanonical) {
  // Base correct template.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "x.y", "/path", base::Time(), base::Time(),
                  base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                  COOKIE_PRIORITY_LOW, false)
                  ->IsCanonical());

  // Newline in name.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A\n", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Carriage return in name.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A\r", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Null character in name.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   std::string("A\0Z", 3), "B", "x.y", "/path", base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Name begins with whitespace.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   " A", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Name ends with whitespace.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A ", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Empty name.  (Note this is against the spec but compatible with other
  // browsers.)
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "", "B", "x.y", "/path", base::Time(), base::Time(),
                  base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                  COOKIE_PRIORITY_LOW, false)
                  ->IsCanonical());

  // Space in name
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A C", "B", "x.y", "/path", base::Time(), base::Time(),
                  base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                  COOKIE_PRIORITY_LOW, false)
                  ->IsCanonical());

  // Extra space suffixing name.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A ", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // '=' character in name.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A=", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Separator in name.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A;", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // '=' character in value.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B=", "x.y", "/path", base::Time(), base::Time(),
                  base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                  COOKIE_PRIORITY_LOW, false)
                  ->IsCanonical());

  // Separator in value.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B;", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Separator in domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", ";x.y", "/path", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Garbage in domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "@:&", "/path", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Space in domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "x.y ", "/path", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Empty domain.  (This is against cookie spec, but needed for Chrome's
  // out-of-spec use of cookies for extensions; see http://crbug.com/730633.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "", "/path", base::Time(), base::Time(),
                  base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                  COOKIE_PRIORITY_LOW, false)
                  ->IsCanonical());

  // Path does not start with a "/".
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "x.y", "path", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Empty path.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "x.y", "", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Simple IPv4 address as domain.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "1.2.3.4", "/path", base::Time(), base::Time(),
                  base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                  COOKIE_PRIORITY_LOW, false)
                  ->IsCanonical());

  // NOn-canonical IPv4 address as domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "01.2.03.4", "/path", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Null IPv6 address as domain.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "[::]", "/path", base::Time(), base::Time(),
                  base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                  COOKIE_PRIORITY_LOW, false)
                  ->IsCanonical());

  // Localhost IPv6 address as domain.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "[::1]", "/path", base::Time(), base::Time(),
                  base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                  COOKIE_PRIORITY_LOW, false)
                  ->IsCanonical());

  // Fully speced IPv6 address as domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "[2001:0DB8:AC10:FE01:0000:0000:0000:0000]",
                   "/path", base::Time(), base::Time(), base::Time(), false,
                   false, CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW,
                   false)
                   ->IsCanonical());

  // Zero abbreviated IPv6 address as domain.  Not canonical because of leading
  // zeros & uppercase hex letters.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "[2001:0DB8:AC10:FE01::]", "/path", base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Zero prefixes removed IPv6 address as domain.  Not canoncial because of
  // uppercase hex letters.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "[2001:DB8:AC10:FE01::]", "/path", base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Lowercased hex IPv6 address as domain.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "[2001:db8:ac10:fe01::]", "/path", base::Time(),
                  base::Time(), base::Time(), false, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW, false)
                  ->IsCanonical());

  // Properly formatted host cookie.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "__Host-A", "B", "x.y", "/", base::Time(), base::Time(),
                  base::Time(), true, false, CookieSameSite::NO_RESTRICTION,
                  COOKIE_PRIORITY_LOW, false)
                  ->IsCanonical());

  // Insecure host cookie.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "__Host-A", "B", "x.y", "/", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Host cookie with non-null path.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "__Host-A", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), true, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Host cookie with empty domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "__Host-A", "B", "", "/", base::Time(), base::Time(),
                   base::Time(), true, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Host cookie with period prefixed domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "__Host-A", "B", ".x.y", "/", base::Time(), base::Time(),
                   base::Time(), true, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // Properly formatted secure cookie.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "__Secure-A", "B", "x.y", "/", base::Time(), base::Time(),
                  base::Time(), true, false, CookieSameSite::NO_RESTRICTION,
                  COOKIE_PRIORITY_LOW, false)
                  ->IsCanonical());

  // Insecure secure cookie.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "__Secure-A", "B", "x.y", "/", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW, false)
                   ->IsCanonical());

  // SameParty attribute used correctly (with Secure and non-Strict SameSite).
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "x.y", "/", base::Time(), base::Time(),
                  base::Time(), true, false, CookieSameSite::NO_RESTRICTION,
                  COOKIE_PRIORITY_LOW, true)
                  ->IsCanonical());
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "x.y", "/", base::Time(), base::Time(),
                  base::Time(), true, false, CookieSameSite::UNSPECIFIED,
                  COOKIE_PRIORITY_LOW, true)
                  ->IsCanonical());
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "x.y", "/", base::Time(), base::Time(),
                  base::Time(), true, false, CookieSameSite::LAX_MODE,
                  COOKIE_PRIORITY_LOW, true)
                  ->IsCanonical());

  // SameParty without Secure is not canonical.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "x.y", "/", base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::LAX_MODE,
                   COOKIE_PRIORITY_LOW, true)
                   ->IsCanonical());

  // SameParty with SameSite=Strict is not canonical.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "x.y", "/", base::Time(), base::Time(),
                   base::Time(), true, false, CookieSameSite::STRICT_MODE,
                   COOKIE_PRIORITY_LOW, true)
                   ->IsCanonical());
}

TEST(CanonicalCookieTest, TestSetCreationDate) {
  auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(), false,
      false, CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW, false);
  EXPECT_TRUE(cookie->CreationDate().is_null());

  base::Time now(base::Time::Now());
  cookie->SetCreationDate(now);
  EXPECT_EQ(now, cookie->CreationDate());
}

TEST(CanonicalCookieTest, TestPrefixHistograms) {
  base::HistogramTester histograms;
  const char kCookiePrefixHistogram[] = "Cookie.CookiePrefix";
  const char kCookiePrefixBlockedHistogram[] = "Cookie.CookiePrefixBlocked";
  GURL https_url("https://www.example.test");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;

  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Host-A=B;", creation_time,
                                       server_time));

  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 1);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 1);

  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Host-A=B; Path=/; Secure",
                                      creation_time, server_time));
  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 2);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 1);
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__HostA=B; Path=/; Secure",
                                      creation_time, server_time));
  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 2);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 1);

  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Secure-A=B;",
                                       creation_time, server_time));

  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 1);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 1);
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Secure-A=B; Path=/; Secure",
                                      creation_time, server_time));
  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 2);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 1);
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__SecureA=B; Path=/; Secure",
                                      creation_time, server_time));
  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 2);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 1);
}

TEST(CanonicalCookieTest, BuildCookieLine) {
  std::vector<std::unique_ptr<CanonicalCookie>> cookies;
  GURL url("https://example.com/");
  base::Time now = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  MatchCookieLineToVector("", cookies);

  cookies.push_back(CanonicalCookie::Create(url, "A=B", now, server_time));
  MatchCookieLineToVector("A=B", cookies);
  // Nameless cookies are sent back without a prefixed '='.
  cookies.push_back(CanonicalCookie::Create(url, "C", now, server_time));
  MatchCookieLineToVector("A=B; C", cookies);
  // Cookies separated by ';'.
  cookies.push_back(CanonicalCookie::Create(url, "D=E", now, server_time));
  MatchCookieLineToVector("A=B; C; D=E", cookies);
  // BuildCookieLine doesn't reorder the list, it relies on the caller to do so.
  cookies.push_back(CanonicalCookie::Create(
      url, "F=G", now - base::TimeDelta::FromSeconds(1), server_time));
  MatchCookieLineToVector("A=B; C; D=E; F=G", cookies);
  // BuildCookieLine doesn't deduplicate.
  cookies.push_back(CanonicalCookie::Create(
      url, "D=E", now - base::TimeDelta::FromSeconds(2), server_time));
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
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/);
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
  EXPECT_FALSE(cc->IsSameParty());
  EXPECT_FALSE(cc->IsDomainCookie());

  // Creation date
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      two_hours_ago, base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/);
  EXPECT_TRUE(cc);
  EXPECT_EQ(two_hours_ago, cc->CreationDate());

  // Last access date
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      two_hours_ago, base::Time(), one_hour_ago, false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/);
  EXPECT_TRUE(cc);
  EXPECT_EQ(one_hour_ago, cc->LastAccessDate());

  // Expiry
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/);
  EXPECT_TRUE(cc);
  EXPECT_EQ(one_hour_from_now, cc->ExpiryDate());

  // Secure
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), true /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(cc->IsSecure());

  // Httponly
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      true /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(cc->IsHttpOnly());

  // Same site
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT,
      false /*same_party*/);
  EXPECT_TRUE(cc);
  EXPECT_EQ(CookieSameSite::LAX_MODE, cc->SameSite());

  // Priority
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW,
      false /*same_party*/);
  EXPECT_TRUE(cc);
  EXPECT_EQ(COOKIE_PRIORITY_LOW, cc->Priority());

  // Domain cookie
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", "www.foo.com", "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());

  // SameParty
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), true /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW,
      true /*same_party*/);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(cc->IsSameParty());
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
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/bar"), "C", "D", "www.foo.com", "/",
      two_hours_ago, base::Time(), one_hour_ago, false /*secure*/,
      true /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "E", "F", std::string(), std::string(),
      base::Time(), base::Time(), base::Time(), true /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));

  // Test the file:// protocol.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("file:///"), "A", "B", std::string(), "/foo", one_hour_ago,
      one_hour_from_now, base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      false /*same_party*/));
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("file:///home/user/foo.txt"), "A", "B", std::string(), "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("file:///home/user/foo.txt"), "A", "B", "home", "/foo", one_hour_ago,
      one_hour_from_now, base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      false /*same_party*/));

  // Test that malformed attributes fail to set the cookie.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), " A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A;", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A=", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A\x07", "B", std::string(), "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", " B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "\x0fZ", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", "www.foo.com ", "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", "foo.ozzzzzzle", "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", std::string(), "foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo ",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", "%2Efoo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://domaintest.%E3%81%BF%E3%82%93%E3%81%AA"), "A", "B",
      "domaintest.%E3%81%BF%E3%82%93%E3%81%AA", "/foo", base::Time(),
      base::Time(), base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      false /*same_party*/));

  std::unique_ptr<CanonicalCookie> cc;

  // Confirm that setting domain cookies with or without leading periods,
  // or on domains different from the URL's, functions correctly.
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", "www.foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/);
  ASSERT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
  EXPECT_EQ(".www.foo.com", cc->Domain());

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", ".www.foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/);
  ASSERT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
  EXPECT_EQ(".www.foo.com", cc->Domain());

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", ".foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/);
  ASSERT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
  EXPECT_EQ(".foo.com", cc->Domain());

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", ".www2.www.foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/);
  EXPECT_FALSE(cc);

  // Secure/URL Scheme mismatch.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo ",
      base::Time(), base::Time(), base::Time(), true /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/));

  // Null creation date/non-null last access date conflict.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo", base::Time(),
      base::Time(), base::Time::Now(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      false /*same_party*/));

  // Domain doesn't match URL
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", "www.bar.com", "/", base::Time(),
      base::Time(), base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      false /*same_party*/));

  // Path with unusual characters escaped.
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false /*same_party*/);
  ASSERT_TRUE(cc);
  EXPECT_EQ("/foo%7F", cc->Path());

  // Empty name and value.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "", "", std::string(), "/", base::Time(),
      base::Time(), base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      false /*same_party*/));

  // A __Secure- cookie must be Secure.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Secure-A", "B", ".www.foo.com", "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      false));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Secure-A", "B", ".www.foo.com", "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, false, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      false));

  // A __Host- cookie must be Secure.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      false));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, false, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      false));

  // A __Host- cookie must have path "/".
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      false));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/foo",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      false));

  // A __Host- cookie must not specify a domain.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      false));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", ".www.foo.com", "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      false));
  // Without __Host- prefix, this is a valid host cookie because it does not
  // specify a domain.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      false));
  // Without __Host- prefix, this is a valid domain (not host) cookie.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", ".www.foo.com", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      false));

  // The __Host- prefix should not prevent otherwise-valid host cookies from
  // being accepted.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://127.0.0.1"), "A", "B", std::string(), "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      false));
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://127.0.0.1"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      false));
  // Host cookies should not specify domain unless it is an IP address that
  // matches the URL.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://127.0.0.1"), "A", "B", "127.0.0.1", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      false));
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://127.0.0.1"), "__Host-A", "B", "127.0.0.1", "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      false));

  // SameParty attribute requires Secure and forbids SameSite=Strict.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", ".www.foo.com", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true /*secure*/, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      true /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", ".www.foo.com", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, false /*secure*/, false,
      CookieSameSite::LAX_MODE, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      true /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", ".www.foo.com", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true /*secure*/, false,
      CookieSameSite::STRICT_MODE, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      true /*same_party*/));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", ".www.foo.com", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, false /*secure*/, false,
      CookieSameSite::STRICT_MODE, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      true /*same_party*/));

  // Check that CreateSanitizedCookie can gracefully fail on inputs that would
  // crash cookie_util::GetCookieDomainWithString due to failing
  // DCHECKs. Specifically, GetCookieDomainWithString requires that if the
  // domain is empty or the URL's host matches the domain, then the URL's host
  // must pass DomainIsHostOnly; it must not begin with a period.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://..."), "A", "B", "...", "/", base::Time(), base::Time(),
      base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, false));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://."), "A", "B", std::string(), "/", base::Time(),
      base::Time(), base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, false));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://.chromium.org"), "A", "B", ".chromium.org", "/",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, false));

  // Check that a file URL with an IPv6 host, and matching IPv6 domain, are
  // valid.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("file://[A::]"), "A", "B", "[A::]", "", base::Time(), base::Time(),
      base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, false));

  // On Windows, URLs beginning with two backslashes are considered file
  // URLs. On other platforms, they are invalid.
  auto double_backslash_ipv6_cookie = CanonicalCookie::CreateSanitizedCookie(
      GURL("\\\\[A::]"), "A", "B", "[A::]", "", base::Time(), base::Time(),
      base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, false);
#if defined(OS_WIN)
  EXPECT_TRUE(double_backslash_ipv6_cookie);
  EXPECT_TRUE(double_backslash_ipv6_cookie->IsCanonical());
#else
  EXPECT_FALSE(double_backslash_ipv6_cookie);
#endif
}

TEST(CanonicalCookieTest, FromStorage) {
  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);
  base::Time one_hour_ago = base::Time::Now() - base::TimeDelta::FromHours(1);
  base::Time one_hour_from_now =
      base::Time::Now() + base::TimeDelta::FromHours(1);

  std::unique_ptr<CanonicalCookie> cc = CanonicalCookie::FromStorage(
      "A", "B", "www.foo.com", "/bar", two_hours_ago, one_hour_from_now,
      one_hour_ago, false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      false /*same_party*/, CookieSourceScheme::kSecure, 87);
  EXPECT_TRUE(cc);
  EXPECT_EQ("A", cc->Name());
  EXPECT_EQ("B", cc->Value());
  EXPECT_EQ("www.foo.com", cc->Domain());
  EXPECT_EQ("/bar", cc->Path());
  EXPECT_EQ(two_hours_ago, cc->CreationDate());
  EXPECT_EQ(one_hour_ago, cc->LastAccessDate());
  EXPECT_EQ(one_hour_from_now, cc->ExpiryDate());
  EXPECT_FALSE(cc->IsSecure());
  EXPECT_FALSE(cc->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cc->SameSite());
  EXPECT_EQ(COOKIE_PRIORITY_MEDIUM, cc->Priority());
  EXPECT_EQ(CookieSourceScheme::kSecure, cc->SourceScheme());
  EXPECT_FALSE(cc->IsDomainCookie());
  EXPECT_EQ(cc->SourcePort(), 87);

  // Should return nullptr when the cookie is not canonical.
  // In this case the cookie is not canonical because its name attribute
  // contains a newline character.
  EXPECT_FALSE(CanonicalCookie::FromStorage(
      "A\n", "B", "www.foo.com", "/bar", two_hours_ago, one_hour_from_now,
      one_hour_ago, false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      false /*same_party*/, CookieSourceScheme::kSecure, 80));

  // If the port information gets corrupted out of the valid range
  // FromStorage() should result in a PORT_INVALID.
  std::unique_ptr<CanonicalCookie> cc2 = CanonicalCookie::FromStorage(
      "A", "B", "www.foo.com", "/bar", two_hours_ago, one_hour_from_now,
      one_hour_ago, false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      false /*same_party*/, CookieSourceScheme::kSecure, 80000);

  EXPECT_EQ(cc2->SourcePort(), url::PORT_INVALID);

  // Test port edge cases: unspecified.
  std::unique_ptr<CanonicalCookie> cc3 = CanonicalCookie::FromStorage(
      "A", "B", "www.foo.com", "/bar", two_hours_ago, one_hour_from_now,
      one_hour_ago, false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      false /*same_party*/, CookieSourceScheme::kSecure, url::PORT_UNSPECIFIED);
  EXPECT_EQ(cc3->SourcePort(), url::PORT_UNSPECIFIED);

  // Test port edge cases: invalid.
  std::unique_ptr<CanonicalCookie> cc4 = CanonicalCookie::FromStorage(
      "A", "B", "www.foo.com", "/bar", two_hours_ago, one_hour_from_now,
      one_hour_ago, false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      false /*same_party*/, CookieSourceScheme::kSecure, url::PORT_INVALID);
  EXPECT_EQ(cc4->SourcePort(), url::PORT_INVALID);
}

TEST(CanonicalCookieTest, IsSetPermittedInContext) {
  GURL url("https://www.example.com/test");
  GURL insecure_url("http://www.example.com/test");
  base::Time current_time = base::Time::Now();

  auto cookie_scriptable = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), true /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, false);
  auto cookie_httponly = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), true /*secure*/, true /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, false);

  CookieOptions context_script;
  CookieOptions context_network;
  context_network.set_include_httponly();

  EXPECT_THAT(
      cookie_scriptable->IsSetPermittedInContext(
          GURL("file://foo/bar.txt"), context_network,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */,
                             CookieSamePartyStatus::kNoSamePartyEnforcement),
          kCookieableSchemes),
      MatchesCookieAccessResult(
          CookieInclusionStatus::MakeFromReasonsForTesting({
              CookieInclusionStatus::EXCLUDE_NONCOOKIEABLE_SCHEME,
              CookieInclusionStatus::EXCLUDE_SECURE_ONLY,
          }),
          _, _, false));

  EXPECT_THAT(
      cookie_scriptable->IsSetPermittedInContext(
          insecure_url, context_network,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */,
                             CookieSamePartyStatus::kNoSamePartyEnforcement),
          kCookieableSchemes),
      MatchesCookieAccessResult(
          CookieInclusionStatus::MakeFromReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_SECURE_ONLY}),
          _, _, false));
  EXPECT_THAT(
      cookie_scriptable->IsSetPermittedInContext(
          url, context_network,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */,
                             CookieSamePartyStatus::kNoSamePartyEnforcement),
          kCookieableSchemes),
      MatchesCookieAccessResult(IsInclude(), _, _, true));
  EXPECT_THAT(
      cookie_scriptable->IsSetPermittedInContext(
          url, context_script,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */,
                             CookieSamePartyStatus::kNoSamePartyEnforcement),
          kCookieableSchemes),
      MatchesCookieAccessResult(IsInclude(), _, _, true));

  EXPECT_THAT(
      cookie_httponly->IsSetPermittedInContext(
          url, context_network,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */,
                             CookieSamePartyStatus::kNoSamePartyEnforcement),
          kCookieableSchemes),
      MatchesCookieAccessResult(IsInclude(), _, _, true));
  EXPECT_THAT(
      cookie_httponly->IsSetPermittedInContext(
          url, context_script,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */,
                             CookieSamePartyStatus::kNoSamePartyEnforcement),
          kCookieableSchemes),
      MatchesCookieAccessResult(
          CookieInclusionStatus::MakeFromReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_HTTP_ONLY}),
          _, _, true));

  CookieOptions context_cross_site;
  CookieOptions context_same_site_lax;
  context_same_site_lax.set_same_site_cookie_context(
      CookieOptions::SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX));
  CookieOptions context_same_site_strict;
  context_same_site_strict.set_same_site_cookie_context(
      CookieOptions::SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT));

  CookieOptions context_same_site_strict_to_lax;
  context_same_site_strict_to_lax.set_same_site_cookie_context(
      CookieOptions::SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX));

  CookieOptions context_same_site_strict_to_cross;
  context_same_site_strict_to_cross.set_same_site_cookie_context(
      CookieOptions::SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE));

  CookieOptions context_same_site_lax_to_cross;
  context_same_site_lax_to_cross.set_same_site_cookie_context(
      CookieOptions::SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX,
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE));

  {
    auto cookie_same_site_unrestricted =
        CanonicalCookie::CreateUnsafeCookieForTesting(
            "A", "2", "www.example.com", "/test", current_time, base::Time(),
            base::Time(), true /*secure*/, false /*httponly*/,
            CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, false);

    EXPECT_THAT(
        cookie_same_site_unrestricted->IsSetPermittedInContext(
            url, context_cross_site,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unrestricted->IsSetPermittedInContext(
            url, context_same_site_lax,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unrestricted->IsSetPermittedInContext(
            url, context_same_site_strict,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));

    {
      // Schemeful Same-Site disabled.
      base::test::ScopedFeatureList feature_list;
      feature_list.InitAndDisableFeature(features::kSchemefulSameSite);

      EXPECT_THAT(
          cookie_same_site_unrestricted->IsSetPermittedInContext(
              url, context_same_site_strict_to_lax,
              CookieAccessParams(
                  CookieAccessSemantics::UNKNOWN,
                  false /* delegate_treats_url_as_trustworthy */,
                  CookieSamePartyStatus::kNoSamePartyEnforcement),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasDowngradeWarning())), _, _, true));
      EXPECT_THAT(
          cookie_same_site_unrestricted->IsSetPermittedInContext(
              url, context_same_site_strict_to_cross,
              CookieAccessParams(
                  CookieAccessSemantics::UNKNOWN,
                  false /* delegate_treats_url_as_trustworthy */,
                  CookieSamePartyStatus::kNoSamePartyEnforcement),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasDowngradeWarning())), _, _, true));
      EXPECT_THAT(
          cookie_same_site_unrestricted->IsSetPermittedInContext(
              url, context_same_site_lax_to_cross,
              CookieAccessParams(
                  CookieAccessSemantics::UNKNOWN,
                  false /* delegate_treats_url_as_trustworthy */,
                  CookieSamePartyStatus::kNoSamePartyEnforcement),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasDowngradeWarning())), _, _, true));
    }
    {
      // Schemeful Same-Site enabled.
      base::test::ScopedFeatureList feature_list;
      feature_list.InitAndEnableFeature(features::kSchemefulSameSite);

      EXPECT_THAT(
          cookie_same_site_unrestricted->IsSetPermittedInContext(
              url, context_same_site_strict_to_lax,
              CookieAccessParams(
                  CookieAccessSemantics::UNKNOWN,
                  false /* delegate_treats_url_as_trustworthy */,
                  CookieSamePartyStatus::kNoSamePartyEnforcement),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasDowngradeWarning())), _, _, true));
      EXPECT_THAT(
          cookie_same_site_unrestricted->IsSetPermittedInContext(
              url, context_same_site_strict_to_cross,
              CookieAccessParams(
                  CookieAccessSemantics::UNKNOWN,
                  false /* delegate_treats_url_as_trustworthy */,
                  CookieSamePartyStatus::kNoSamePartyEnforcement),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasDowngradeWarning())), _, _, true));
      EXPECT_THAT(
          cookie_same_site_unrestricted->IsSetPermittedInContext(
              url, context_same_site_lax_to_cross,
              CookieAccessParams(
                  CookieAccessSemantics::UNKNOWN,
                  false /* delegate_treats_url_as_trustworthy */,
                  CookieSamePartyStatus::kNoSamePartyEnforcement),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasDowngradeWarning())), _, _, true));
    }
  }

  {
    auto cookie_same_site_lax = CanonicalCookie::CreateUnsafeCookieForTesting(
        "A", "2", "www.example.com", "/test", current_time, base::Time(),
        base::Time(), true /*secure*/, false /*httponly*/,
        CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT, false);

    EXPECT_THAT(
        cookie_same_site_lax->IsSetPermittedInContext(
            url, context_cross_site,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(
            CookieInclusionStatus::MakeFromReasonsForTesting(
                {CookieInclusionStatus::EXCLUDE_SAMESITE_LAX}),
            _, _, true));
    EXPECT_THAT(
        cookie_same_site_lax->IsSetPermittedInContext(
            url, context_same_site_lax,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_lax->IsSetPermittedInContext(
            url, context_same_site_strict,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));

    {
      // Schemeful Same-Site disabled.
      base::test::ScopedFeatureList feature_list;
      feature_list.InitAndDisableFeature(features::kSchemefulSameSite);

      EXPECT_THAT(
          cookie_same_site_lax->IsSetPermittedInContext(
              url, context_same_site_strict_to_lax,
              CookieAccessParams(
                  CookieAccessSemantics::UNKNOWN,
                  false /* delegate_treats_url_as_trustworthy */,
                  CookieSamePartyStatus::kNoSamePartyEnforcement),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasDowngradeWarning())), _, _, true));
      EXPECT_THAT(cookie_same_site_lax->IsSetPermittedInContext(
                      url, context_same_site_strict_to_cross,
                      CookieAccessParams(
                          CookieAccessSemantics::UNKNOWN,
                          false /* delegate_treats_url_as_trustworthy */,
                          CookieSamePartyStatus::kNoSamePartyEnforcement),
                      kCookieableSchemes),
                  MatchesCookieAccessResult(
                      AllOf(IsInclude(),
                            HasWarningReason(
                                CookieInclusionStatus::
                                    WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE)),
                      _, _, true));
      EXPECT_THAT(cookie_same_site_lax->IsSetPermittedInContext(
                      url, context_same_site_lax_to_cross,
                      CookieAccessParams(
                          CookieAccessSemantics::UNKNOWN,
                          false /* delegate_treats_url_as_trustworthy */,
                          CookieSamePartyStatus::kNoSamePartyEnforcement),
                      kCookieableSchemes),
                  MatchesCookieAccessResult(
                      AllOf(IsInclude(),
                            HasWarningReason(
                                CookieInclusionStatus::
                                    WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE)),
                      _, _, true));
    }
    {
      // Schemeful Same-Site enabled.
      base::test::ScopedFeatureList feature_list;
      feature_list.InitAndEnableFeature(features::kSchemefulSameSite);

      EXPECT_THAT(
          cookie_same_site_lax->IsSetPermittedInContext(
              url, context_same_site_strict_to_lax,
              CookieAccessParams(
                  CookieAccessSemantics::UNKNOWN,
                  false /* delegate_treats_url_as_trustworthy */,
                  CookieSamePartyStatus::kNoSamePartyEnforcement),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasDowngradeWarning())), _, _, true));
      EXPECT_THAT(cookie_same_site_lax->IsSetPermittedInContext(
                      url, context_same_site_strict_to_cross,
                      CookieAccessParams(
                          CookieAccessSemantics::UNKNOWN,
                          false /* delegate_treats_url_as_trustworthy */,
                          CookieSamePartyStatus::kNoSamePartyEnforcement),
                      kCookieableSchemes),
                  MatchesCookieAccessResult(
                      AllOf(Not(IsInclude()),
                            HasWarningReason(
                                CookieInclusionStatus::
                                    WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE),
                            HasExclusionReason(
                                CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)),
                      _, _, true));
      EXPECT_THAT(
          cookie_same_site_lax->IsSetPermittedInContext(
              url, context_same_site_lax_to_cross,
              CookieAccessParams(
                  CookieAccessSemantics::UNKNOWN,
                  false /* delegate_treats_url_as_trustworthy */,
                  CookieSamePartyStatus::kNoSamePartyEnforcement),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(Not(IsInclude()),
                    HasWarningReason(CookieInclusionStatus::
                                         WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE),
                    HasExclusionReason(
                        CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)),
              _, _, true));
    }
  }

  {
    auto cookie_same_site_strict =
        CanonicalCookie::CreateUnsafeCookieForTesting(
            "A", "2", "www.example.com", "/test", current_time, base::Time(),
            base::Time(), true /*secure*/, false /*httponly*/,
            CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_DEFAULT, false);

    // TODO(morlovich): Do compatibility testing on whether set of strict in lax
    // context really should be accepted.
    EXPECT_THAT(
        cookie_same_site_strict->IsSetPermittedInContext(
            url, context_cross_site,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(
            CookieInclusionStatus::MakeFromReasonsForTesting(
                {CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT}),
            _, _, true));
    EXPECT_THAT(
        cookie_same_site_strict->IsSetPermittedInContext(
            url, context_same_site_lax,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_strict->IsSetPermittedInContext(
            url, context_same_site_strict,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));

    {
      // Schemeful Same-Site disabled.
      base::test::ScopedFeatureList feature_list;
      feature_list.InitAndDisableFeature(features::kSchemefulSameSite);

      EXPECT_THAT(
          cookie_same_site_strict->IsSetPermittedInContext(
              url, context_same_site_strict_to_lax,
              CookieAccessParams(
                  CookieAccessSemantics::UNKNOWN,
                  false /* delegate_treats_url_as_trustworthy */,
                  CookieSamePartyStatus::kNoSamePartyEnforcement),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasDowngradeWarning())), _, _, true));
      EXPECT_THAT(
          cookie_same_site_strict->IsSetPermittedInContext(
              url, context_same_site_strict_to_cross,
              CookieAccessParams(
                  CookieAccessSemantics::UNKNOWN,
                  false /* delegate_treats_url_as_trustworthy */,
                  CookieSamePartyStatus::kNoSamePartyEnforcement),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(),
                    HasWarningReason(
                        CookieInclusionStatus::
                            WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE)),
              _, _, true));
      EXPECT_THAT(cookie_same_site_strict->IsSetPermittedInContext(
                      url, context_same_site_lax_to_cross,
                      CookieAccessParams(
                          CookieAccessSemantics::UNKNOWN,
                          false /* delegate_treats_url_as_trustworthy */,
                          CookieSamePartyStatus::kNoSamePartyEnforcement),
                      kCookieableSchemes),
                  MatchesCookieAccessResult(
                      AllOf(IsInclude(),
                            HasWarningReason(
                                CookieInclusionStatus::
                                    WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE)),
                      _, _, true));
    }
    {
      // Schemeful Same-Site enabled.
      base::test::ScopedFeatureList feature_list;
      feature_list.InitAndEnableFeature(features::kSchemefulSameSite);

      EXPECT_THAT(
          cookie_same_site_strict->IsSetPermittedInContext(
              url, context_same_site_strict_to_lax,
              CookieAccessParams(
                  CookieAccessSemantics::UNKNOWN,
                  false /* delegate_treats_url_as_trustworthy */,
                  CookieSamePartyStatus::kNoSamePartyEnforcement),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasDowngradeWarning())), _, _, true));
      EXPECT_THAT(
          cookie_same_site_strict->IsSetPermittedInContext(
              url, context_same_site_strict_to_cross,
              CookieAccessParams(
                  CookieAccessSemantics::UNKNOWN,
                  false /* delegate_treats_url_as_trustworthy */,
                  CookieSamePartyStatus::kNoSamePartyEnforcement),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(Not(IsInclude()),
                    HasWarningReason(
                        CookieInclusionStatus::
                            WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE),
                    HasExclusionReason(
                        CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)),
              _, _, true));
      EXPECT_THAT(
          cookie_same_site_strict->IsSetPermittedInContext(
              url, context_same_site_lax_to_cross,
              CookieAccessParams(
                  CookieAccessSemantics::UNKNOWN,
                  false /* delegate_treats_url_as_trustworthy */,
                  CookieSamePartyStatus::kNoSamePartyEnforcement),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(Not(IsInclude()),
                    HasWarningReason(
                        CookieInclusionStatus::
                            WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE),
                    HasExclusionReason(
                        CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)),
              _, _, true));
    }

    // Even with Schemeful Same-Site enabled, cookies semantics could change the
    // inclusion.
    {
      base::test::ScopedFeatureList feature_list;
      feature_list.InitAndEnableFeature(features::kSchemefulSameSite);

      EXPECT_THAT(cookie_same_site_strict->IsSetPermittedInContext(
                      url, context_same_site_strict_to_cross,
                      CookieAccessParams(
                          CookieAccessSemantics::UNKNOWN,
                          false /* delegate_treats_url_as_trustworthy */,
                          CookieSamePartyStatus::kNoSamePartyEnforcement),
                      kCookieableSchemes),
                  MatchesCookieAccessResult(Not(IsInclude()), _, _, true));
      EXPECT_THAT(cookie_same_site_strict->IsSetPermittedInContext(
                      url, context_same_site_strict_to_cross,
                      CookieAccessParams(
                          CookieAccessSemantics::NONLEGACY,
                          false /* delegate_treats_url_as_trustworthy */,
                          CookieSamePartyStatus::kNoSamePartyEnforcement),
                      kCookieableSchemes),
                  MatchesCookieAccessResult(Not(IsInclude()), _, _, true));
      // LEGACY semantics should allow cookies which Schemeful Same-Site would
      // normally block.
      EXPECT_THAT(cookie_same_site_strict->IsSetPermittedInContext(
                      url, context_same_site_strict_to_cross,
                      CookieAccessParams(
                          CookieAccessSemantics::LEGACY,
                          false /* delegate_treats_url_as_trustworthy */,
                          CookieSamePartyStatus::kNoSamePartyEnforcement),
                      kCookieableSchemes),
                  MatchesCookieAccessResult(IsInclude(), _, _, true));
    }
  }

  // Behavior of UNSPECIFIED depends on an experiment and CookieAccessSemantics.
  auto cookie_same_site_unspecified =
      CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "2", "www.example.com", "/test", current_time, base::Time(),
          base::Time(), true /*secure*/, false /*httponly*/,
          CookieSameSite::UNSPECIFIED, COOKIE_PRIORITY_DEFAULT, false);

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kSameSiteByDefaultCookies);

    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_cross_site,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_same_site_lax,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_same_site_strict,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_cross_site,
            CookieAccessParams(CookieAccessSemantics::LEGACY,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_same_site_lax,
            CookieAccessParams(CookieAccessSemantics::LEGACY,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_same_site_strict,
            CookieAccessParams(CookieAccessSemantics::LEGACY,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_cross_site,
            CookieAccessParams(CookieAccessSemantics::NONLEGACY,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(
            HasExactlyExclusionReasonsForTesting(
                std::vector<CookieInclusionStatus::ExclusionReason>(
                    {CookieInclusionStatus::
                         EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX})),
            _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_same_site_lax,
            CookieAccessParams(CookieAccessSemantics::NONLEGACY,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_same_site_strict,
            CookieAccessParams(CookieAccessSemantics::NONLEGACY,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kSameSiteByDefaultCookies);

    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_cross_site,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(
            HasExactlyExclusionReasonsForTesting(
                std::vector<CookieInclusionStatus::ExclusionReason>(
                    {CookieInclusionStatus::
                         EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX})),
            _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_same_site_lax,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_same_site_strict,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_cross_site,
            CookieAccessParams(CookieAccessSemantics::LEGACY,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_same_site_lax,
            CookieAccessParams(CookieAccessSemantics::LEGACY,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_same_site_strict,
            CookieAccessParams(CookieAccessSemantics::LEGACY,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_cross_site,
            CookieAccessParams(CookieAccessSemantics::NONLEGACY,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(
            HasExactlyExclusionReasonsForTesting(
                std::vector<CookieInclusionStatus::ExclusionReason>(
                    {CookieInclusionStatus::
                         EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX})),
            _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_same_site_lax,
            CookieAccessParams(CookieAccessSemantics::NONLEGACY,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unspecified->IsSetPermittedInContext(
            url, context_same_site_strict,
            CookieAccessParams(CookieAccessSemantics::NONLEGACY,
                               false /* delegate_treats_url_as_trustworthy */,
                               CookieSamePartyStatus::kNoSamePartyEnforcement),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
  }
}

TEST(CanonicalCookieTest, IsSetPermittedEffectiveSameSite) {
  GURL url("http://www.example.com/test");
  base::Time current_time = base::Time::Now();
  CookieOptions options;

  // Test IsSetPermitted CookieEffectiveSameSite for
  // CanonicalCookie with CookieSameSite::NO_RESTRICTION.
  auto cookie_no_restriction = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), true /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, false);

  EXPECT_THAT(
      cookie_no_restriction->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */,
                             CookieSamePartyStatus::kNoSamePartyEnforcement),
          kCookieableSchemes),
      MatchesCookieAccessResult(_, CookieEffectiveSameSite::NO_RESTRICTION, _,
                                false));

  // Test IsSetPermitted CookieEffectiveSameSite for
  // CanonicalCookie with CookieSameSite::LAX_MODE.
  auto cookie_lax = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), true /*secure*/, false /*httponly*/,
      CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT, false);

  EXPECT_THAT(
      cookie_lax->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */,
                             CookieSamePartyStatus::kNoSamePartyEnforcement),
          kCookieableSchemes),
      MatchesCookieAccessResult(_, CookieEffectiveSameSite::LAX_MODE, _,
                                false));

  // Test IsSetPermitted CookieEffectiveSameSite for
  // CanonicalCookie with CookieSameSite::STRICT_MODE.
  auto cookie_strict = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), true /*secure*/, false /*httponly*/,
      CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_DEFAULT, false);

  EXPECT_THAT(
      cookie_strict->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */,
                             CookieSamePartyStatus::kNoSamePartyEnforcement),
          kCookieableSchemes),
      MatchesCookieAccessResult(_, CookieEffectiveSameSite::STRICT_MODE, _,
                                false));

  // Test IsSetPermitted CookieEffectiveSameSite for
  // CanonicalCookie with CookieSameSite::UNSPECIFIED.
  base::Time creation_time = base::Time::Now() - (kLaxAllowUnsafeMaxAge * 4);
  auto cookie_old_unspecified = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", creation_time, base::Time(),
      base::Time(), true /*secure*/, false /*httponly*/,
      CookieSameSite::UNSPECIFIED, COOKIE_PRIORITY_DEFAULT, false);
  auto cookie_unspecified = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), true /*secure*/, false /*httponly*/,
      CookieSameSite::UNSPECIFIED, COOKIE_PRIORITY_DEFAULT, false);

  EXPECT_THAT(
      cookie_old_unspecified->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */,
                             CookieSamePartyStatus::kNoSamePartyEnforcement),
          kCookieableSchemes),
      MatchesCookieAccessResult(_, CookieEffectiveSameSite::LAX_MODE, _,
                                false));

  EXPECT_THAT(
      cookie_unspecified->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */,
                             CookieSamePartyStatus::kNoSamePartyEnforcement),
          kCookieableSchemes),
      MatchesCookieAccessResult(
          _, CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE, _, false));

  EXPECT_THAT(
      cookie_unspecified->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::NONLEGACY,
                             false /* delegate_treats_url_as_trustworthy */,
                             CookieSamePartyStatus::kNoSamePartyEnforcement),
          kCookieableSchemes),
      MatchesCookieAccessResult(
          _, CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE, _, false));

  EXPECT_THAT(
      cookie_unspecified->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::LEGACY,
                             false /* delegate_treats_url_as_trustworthy */,
                             CookieSamePartyStatus::kNoSamePartyEnforcement),
          kCookieableSchemes),
      MatchesCookieAccessResult(_, CookieEffectiveSameSite::NO_RESTRICTION, _,
                                false));
}

TEST(CanonicalCookieTest, IsSetPermitted_AllowedToAccessSecureCookies) {
  GURL url("https://www.example.com/test");
  GURL insecure_url("http://www.example.com/test");
  GURL localhost_url("http://localhost/test");
  base::Time current_time = base::Time::Now();
  CookieOptions options;

  for (bool secure : {false, true}) {
    for (CookieSameSite same_site : {
             CookieSameSite::UNSPECIFIED,
             CookieSameSite::NO_RESTRICTION,
             CookieSameSite::LAX_MODE,
             CookieSameSite::STRICT_MODE,
         }) {
      for (bool same_party : {false, true}) {
        // Skip setting SameParty and SameSite=Strict, since that is invalid.
        if (same_party && same_site == CookieSameSite::STRICT_MODE)
          continue;
        auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
            "A", "2", "www.example.com", "/test", current_time, base::Time(),
            base::Time(), secure, false /*httponly*/, same_site,
            COOKIE_PRIORITY_DEFAULT, same_party);

        for (bool delegate_treats_url_as_trustworthy : {false, true}) {
          for (CookieAccessSemantics access_semantics : {
                   CookieAccessSemantics::UNKNOWN,
                   CookieAccessSemantics::LEGACY,
                   CookieAccessSemantics::NONLEGACY,
               }) {
            for (CookieSamePartyStatus same_party_status : {
                     CookieSamePartyStatus::kNoSamePartyEnforcement,
                     CookieSamePartyStatus::kEnforceSamePartyInclude,
                     CookieSamePartyStatus::kEnforceSamePartyExclude,
                 }) {
              // Skip invalid combinations of `same_party` and
              // `same_party_status`.
              bool has_same_party_enforcement =
                  same_party_status !=
                  CookieSamePartyStatus::kNoSamePartyEnforcement;
              if (has_same_party_enforcement != same_party) {
                continue;
              }
              EXPECT_THAT(
                  cookie->IsSetPermittedInContext(
                      url, options,
                      CookieAccessParams(access_semantics,
                                         delegate_treats_url_as_trustworthy,
                                         same_party_status),
                      kCookieableSchemes),
                  MatchesCookieAccessResult(_, _, _, true));
              EXPECT_THAT(
                  cookie->IsSetPermittedInContext(
                      insecure_url, options,
                      CookieAccessParams(access_semantics,
                                         delegate_treats_url_as_trustworthy,
                                         same_party_status),
                      kCookieableSchemes),
                  MatchesCookieAccessResult(
                      _, _, _, delegate_treats_url_as_trustworthy));
              EXPECT_THAT(
                  cookie->IsSetPermittedInContext(
                      localhost_url, options,
                      CookieAccessParams(access_semantics,
                                         delegate_treats_url_as_trustworthy,
                                         same_party_status),
                      kCookieableSchemes),
                  MatchesCookieAccessResult(_, _, _, true));
            }
          }
        }
      }
    }
  }
}

TEST(CanonicalCookieTest, IsSetPermitted_SameParty) {
  GURL url("https://www.example.com/test");
  base::Time current_time = base::Time::Now();
  CookieOptions options;
  options.set_same_site_cookie_context(CookieOptions::SameSiteCookieContext(
      CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE));
  options.set_same_party_cookie_context_type(
      CookieOptions::SamePartyCookieContextType::kSameParty);

  {
    bool delegate_treats_url_as_trustworthy = false;
    auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
        "A", "2", "www.example.com", "/test", current_time, base::Time(),
        base::Time(), true /* secure */, false /*httponly*/,
        CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT,
        true /* same_party */);

    // The following access would normally be excluded due to SameSite=Lax, but
    // SameParty overrides SameSite.
    EXPECT_THAT(
        cookie->IsSetPermittedInContext(
            url, options,
            CookieAccessParams(CookieAccessSemantics::LEGACY,
                               delegate_treats_url_as_trustworthy,
                               CookieSamePartyStatus::kEnforceSamePartyExclude),
            kCookieableSchemes),
        MatchesCookieAccessResult(
            CookieInclusionStatus::MakeFromReasonsForTesting(
                {CookieInclusionStatus::EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT},
                {CookieInclusionStatus::WARN_TREATED_AS_SAMEPARTY}),
            _, _, true));
  }

  for (CookieSameSite same_site : {
           CookieSameSite::UNSPECIFIED,
           CookieSameSite::NO_RESTRICTION,
           CookieSameSite::LAX_MODE,
       }) {
    auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
        "A", "2", "www.example.com", "/test", current_time, base::Time(),
        base::Time(), true /* secure */, false /*httponly*/, same_site,
        COOKIE_PRIORITY_DEFAULT, true /* same_party */);

    for (bool delegate_treats_url_as_trustworthy : {false, true}) {
      for (CookieAccessSemantics access_semantics : {
               CookieAccessSemantics::UNKNOWN,
               CookieAccessSemantics::LEGACY,
               CookieAccessSemantics::NONLEGACY,
           }) {
        EXPECT_THAT(
            cookie->IsSetPermittedInContext(
                url, options,
                CookieAccessParams(
                    access_semantics, delegate_treats_url_as_trustworthy,
                    CookieSamePartyStatus::kEnforceSamePartyInclude),
                kCookieableSchemes),
            MatchesCookieAccessResult(IsInclude(), _, _, true));
      }
    }
  }
}

}  // namespace net
