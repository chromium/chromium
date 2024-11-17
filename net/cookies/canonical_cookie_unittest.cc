// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/canonical_cookie.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/parsed_cookie.h"
#include "net/http/http_util.h"
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
      base::Time(), base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt, CookieSourceScheme::kSecure, 443);
  EXPECT_EQ("A", cookie1->Name());
  EXPECT_EQ("2", cookie1->Value());
  EXPECT_EQ("www.example.com", cookie1->Domain());
  EXPECT_EQ("/test", cookie1->Path());
  EXPECT_FALSE(cookie1->SecureAttribute());
  EXPECT_FALSE(cookie1->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie1->SameSite());
  EXPECT_EQ(CookiePriority::COOKIE_PRIORITY_DEFAULT, cookie1->Priority());
  EXPECT_FALSE(cookie1->IsPartitioned());
  EXPECT_EQ(cookie1->SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cookie1->SourcePort(), 443);

  auto cookie2 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", ".www.example.com", "/", current_time, base::Time(),
      base::Time(), base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT,
      CookiePartitionKey::FromURLForTesting(GURL("https://foo.com")),
      CookieSourceScheme::kNonSecure, 65536);
  EXPECT_EQ("A", cookie2->Name());
  EXPECT_EQ("2", cookie2->Value());
  EXPECT_EQ(".www.example.com", cookie2->Domain());
  EXPECT_EQ("/", cookie2->Path());
  EXPECT_FALSE(cookie2->SecureAttribute());
  EXPECT_FALSE(cookie2->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie2->SameSite());
  EXPECT_EQ(CookiePriority::COOKIE_PRIORITY_DEFAULT, cookie2->Priority());
  EXPECT_TRUE(cookie2->IsPartitioned());
  EXPECT_EQ(cookie2->SourceScheme(), CookieSourceScheme::kNonSecure);
  // Because the port can be set explicitly in the constructor its value can be
  // independent of the other parameters. In this case, test that an out of
  // range port is kept out of range.
  EXPECT_EQ(cookie2->SourcePort(), 65536);

  // Set Secure to true but don't specify source_scheme or port.
  auto cookie3 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", ".www.example.com", "/", current_time, base::Time(),
      base::Time(), base::Time(), true /* secure */, false,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cookie3->SecureAttribute());
  EXPECT_EQ(cookie3->SourceScheme(), CookieSourceScheme::kUnset);
  EXPECT_EQ(cookie3->SourcePort(), url::PORT_UNSPECIFIED);

  auto cookie4 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", ".www.example.com", "/test", current_time, base::Time(),
      base::Time(), base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_EQ("A", cookie4->Name());
  EXPECT_EQ("2", cookie4->Value());
  EXPECT_EQ(".www.example.com", cookie4->Domain());
  EXPECT_EQ("/test", cookie4->Path());
  EXPECT_FALSE(cookie4->SecureAttribute());
  EXPECT_FALSE(cookie4->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie4->SameSite());
  EXPECT_FALSE(cookie4->IsPartitioned());
  EXPECT_EQ(cookie4->SourceScheme(), CookieSourceScheme::kUnset);
  EXPECT_EQ(cookie4->SourcePort(), url::PORT_UNSPECIFIED);

  // Test some port edge cases: unspecified.
  auto cookie5 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", ".www.example.com", "/", current_time, base::Time(),
      base::Time(), base::Time(), true /* secure */, false,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, std::nullopt,
      CookieSourceScheme::kUnset, url::PORT_UNSPECIFIED);
  EXPECT_EQ(cookie5->SourcePort(), url::PORT_UNSPECIFIED);

  // Test some port edge cases: invalid.
  auto cookie6 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", ".www.example.com", "/", current_time, base::Time(),
      base::Time(), base::Time(), true /* secure */, false,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, std::nullopt,
      CookieSourceScheme::kUnset, url::PORT_INVALID);
  EXPECT_EQ(cookie6->SourcePort(), url::PORT_INVALID);
}

TEST(CanonicalCookieTest, CreationCornerCases) {
  base::Time creation_time = base::Time::Now();
  std::unique_ptr<CanonicalCookie> cookie;
  std::optional<base::Time> server_time = std::nullopt;

  // Space in name.
  cookie = CanonicalCookie::CreateForTesting(
      GURL("http://www.example.com/test/foo.html"), "A C=2", creation_time,
      server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_EQ("A C", cookie->Name());

  // Semicolon in path.
  cookie = CanonicalCookie::CreateForTesting(GURL("http://fool/;/"), "*",
                                             creation_time, server_time);
  EXPECT_TRUE(cookie.get());

  // Control characters in name or value.
  CookieInclusionStatus status;
  cookie = CanonicalCookie::Create(GURL("http://www.example.com/test/foo.html"),
                                   "\b=foo", creation_time, server_time,
                                   /*cookie_partition_key=*/std::nullopt,
                                   CookieSourceType::kUnknown, &status);
  EXPECT_FALSE(cookie.get());
  EXPECT_TRUE(status.HasExclusionReason(
      CookieInclusionStatus::ExclusionReason::EXCLUDE_DISALLOWED_CHARACTER));
  cookie = CanonicalCookie::Create(GURL("http://www.example.com/test/foo.html"),
                                   "bar=\b", creation_time, server_time,
                                   /*cookie_partition_key=*/std::nullopt,
                                   CookieSourceType::kUnknown, &status);
  EXPECT_FALSE(cookie.get());
  EXPECT_TRUE(status.HasExclusionReason(
      CookieInclusionStatus::ExclusionReason::EXCLUDE_DISALLOWED_CHARACTER));

  // The ParsedCookie constructor unit tests cover many edge cases related to
  // invalid sizes when parsing a cookie line, and since CanonicalCookie::Create
  // creates a ParsedCookie immediately, there's no point in replicating all
  // of those tests here.  We should test that the corresponding ExclusionReason
  // gets passed back correctly, though.
  std::string too_long_value(ParsedCookie::kMaxCookieNamePlusValueSize + 1,
                             'a');

  cookie = CanonicalCookie::Create(GURL("http://www.example.com/test/foo.html"),
                                   too_long_value, creation_time, server_time,
                                   /*cookie_partition_key=*/std::nullopt,
                                   CookieSourceType::kUnknown, &status);
  EXPECT_FALSE(cookie.get());
  EXPECT_TRUE(
      status.HasExclusionReason(CookieInclusionStatus::ExclusionReason::
                                    EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE));
}

TEST(CanonicalCookieTest, Create) {
  // Test creating cookies from a cookie string.
  GURL url("http://www.example.com/test/foo.html");
  GURL https_url("https://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;

  std::unique_ptr<CanonicalCookie> cookie(CanonicalCookie::CreateForTesting(
      url, "A=2", creation_time, server_time));
  EXPECT_EQ("A", cookie->Name());
  EXPECT_EQ("2", cookie->Value());
  EXPECT_EQ("www.example.com", cookie->Domain());
  EXPECT_EQ("/test", cookie->Path());
  EXPECT_FALSE(cookie->SecureAttribute());
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kNonSecure);
  EXPECT_EQ(cookie->SourcePort(), 80);

  GURL url2("http://www.foo.com");
  cookie = CanonicalCookie::CreateForTesting(url2, "B=1", creation_time,
                                             server_time);
  EXPECT_EQ("B", cookie->Name());
  EXPECT_EQ("1", cookie->Value());
  EXPECT_EQ("www.foo.com", cookie->Domain());
  EXPECT_EQ("/", cookie->Path());
  EXPECT_FALSE(cookie->SecureAttribute());
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kNonSecure);
  EXPECT_EQ(cookie->SourcePort(), 80);

  // Test creating secure cookies. Secure scheme is not checked upon creation,
  // so a URL of any scheme can create a Secure cookie.
  cookie = CanonicalCookie::CreateForTesting(url, "A=2; Secure", creation_time,
                                             server_time);
  EXPECT_TRUE(cookie->SecureAttribute());

  cookie = CanonicalCookie::CreateForTesting(https_url, "A=2; Secure",
                                             creation_time, server_time);
  EXPECT_TRUE(cookie->SecureAttribute());

  GURL url3("https://www.foo.com");
  cookie = CanonicalCookie::CreateForTesting(url3, "A=2; Secure", creation_time,
                                             server_time);
  EXPECT_TRUE(cookie->SecureAttribute());
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kSecure);

  cookie = CanonicalCookie::CreateForTesting(url3, "A=2", creation_time,
                                             server_time);
  EXPECT_FALSE(cookie->SecureAttribute());
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kSecure);

  // Test creating cookie from localhost URL.
  cookie = CanonicalCookie::CreateForTesting(GURL("http://localhost/path"),
                                             "A=2", creation_time, server_time);
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kNonSecure);

  cookie = CanonicalCookie::CreateForTesting(GURL("http://127.0.0.1/path"),
                                             "A=2", creation_time, server_time);
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kNonSecure);

  cookie = CanonicalCookie::CreateForTesting(GURL("http://[::1]/path"), "A=2",
                                             creation_time, server_time);
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kNonSecure);

  cookie = CanonicalCookie::CreateForTesting(GURL("https://localhost/path"),
                                             "A=2", creation_time, server_time);
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kSecure);

  cookie = CanonicalCookie::CreateForTesting(GURL("https://127.0.0.1/path"),
                                             "A=2", creation_time, server_time);
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kSecure);

  cookie = CanonicalCookie::CreateForTesting(GURL("https://[::1]/path"), "A=2",
                                             creation_time, server_time);
  EXPECT_EQ(cookie->SourceScheme(), CookieSourceScheme::kSecure);

  // Test creating http only cookies. HttpOnly is not checked upon creation.
  cookie = CanonicalCookie::CreateForTesting(url, "A=2; HttpOnly",
                                             creation_time, server_time);
  EXPECT_TRUE(cookie->IsHttpOnly());

  cookie = CanonicalCookie::CreateForTesting(url, "A=2; HttpOnly",
                                             creation_time, server_time);
  EXPECT_TRUE(cookie->IsHttpOnly());

  // Test creating SameSite cookies. SameSite is not checked upon creation.
  cookie = CanonicalCookie::CreateForTesting(url, "A=2; SameSite=Strict",
                                             creation_time, server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::STRICT_MODE, cookie->SameSite());
  cookie = CanonicalCookie::CreateForTesting(url, "A=2; SameSite=Lax",
                                             creation_time, server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::LAX_MODE, cookie->SameSite());
  cookie = CanonicalCookie::CreateForTesting(url, "A=2; SameSite=Extended",
                                             creation_time, server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookie->SameSite());
  cookie = CanonicalCookie::CreateForTesting(url, "A=2; SameSite=None",
                                             creation_time, server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie->SameSite());
  cookie =
      CanonicalCookie::CreateForTesting(url, "A=2", creation_time, server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookie->SameSite());

  // Test creating cookies with different ports.
  cookie = CanonicalCookie::CreateForTesting(GURL("http://www.foo.com"), "B=1",
                                             creation_time, server_time);
  EXPECT_EQ(cookie->SourcePort(), 80);

  cookie = CanonicalCookie::CreateForTesting(GURL("http://www.foo.com:81"),
                                             "B=1", creation_time, server_time);
  EXPECT_EQ(cookie->SourcePort(), 81);

  cookie = CanonicalCookie::CreateForTesting(GURL("https://www.foo.com"), "B=1",
                                             creation_time, server_time);
  EXPECT_EQ(cookie->SourcePort(), 443);

  cookie = CanonicalCookie::CreateForTesting(GURL("https://www.foo.com:1234"),
                                             "B=1", creation_time, server_time);
  EXPECT_EQ(cookie->SourcePort(), 1234);

  cookie = CanonicalCookie::CreateForTesting(GURL("http://www.foo.com:443"),
                                             "B=1", creation_time, server_time);
  EXPECT_EQ(cookie->SourcePort(), 443);

  // An invalid port leads to an invalid GURL, which causes cookie creation
  // to fail.
  CookieInclusionStatus status;
  cookie = CanonicalCookie::Create(GURL("http://www.foo.com:70000"), "B=1",
                                   creation_time, server_time,
                                   /*cookie_partition_key=*/std::nullopt,
                                   CookieSourceType::kUnknown, &status);
  EXPECT_FALSE(cookie.get());
  EXPECT_TRUE(status.HasExclusionReason(
      CookieInclusionStatus::ExclusionReason::EXCLUDE_FAILURE_TO_STORE));
}

TEST(CanonicalCookieTest, CreateInvalidUrl) {
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  CookieInclusionStatus status;
  std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::Create(
      GURL("http://.127.0.0.1/path"), "A=2", creation_time, server_time,
      /*cookie_partition_key=*/std::nullopt, CookieSourceType::kUnknown,
      &status);
  EXPECT_FALSE(cookie.get());
  EXPECT_TRUE(status.HasExclusionReason(
      CookieInclusionStatus::ExclusionReason::EXCLUDE_FAILURE_TO_STORE));
}

// Test that a cookie string with an empty domain attribute generates a
// canonical host cookie.
TEST(CanonicalCookieTest, CreateHostCookieFromString) {
  // Create a new canonical host cookie via empty string domain in the
  // cookie_line.
  GURL url("http://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  std::unique_ptr<CanonicalCookie> cookie(CanonicalCookie::CreateForTesting(
      url, "A=2; domain=; Secure", creation_time, server_time,
      std::nullopt /*cookie_partition_key*/));
  EXPECT_EQ("www.example.com", cookie->Domain());
  EXPECT_TRUE(cookie->IsHostCookie());
}

TEST(CanonicalCookieTest, CreateNonStandardSameSite) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  std::unique_ptr<CanonicalCookie> cookie;
  std::optional<base::Time> server_time = std::nullopt;

  // Non-standard value for the SameSite attribute.
  cookie = CanonicalCookie::CreateForTesting(url, "A=2; SameSite=NonStandard",
                                             now, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookie->SameSite());

  // Omit value for the SameSite attribute.
  cookie =
      CanonicalCookie::CreateForTesting(url, "A=2; SameSite", now, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookie->SameSite());
}

TEST(CanonicalCookieTest, CreateSameSiteInCrossSiteContexts) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  std::unique_ptr<CanonicalCookie> cookie;
  std::optional<base::Time> server_time = std::nullopt;

  // A cookie can be created from any SameSiteContext regardless of SameSite
  // value (it is upon setting the cookie that the SameSiteContext comes into
  // effect).
  cookie = CanonicalCookie::CreateForTesting(url, "A=2; SameSite=Strict", now,
                                             server_time);
  EXPECT_TRUE(cookie.get());
  cookie = CanonicalCookie::CreateForTesting(url, "A=2; SameSite=Lax", now,
                                             server_time);
  EXPECT_TRUE(cookie.get());
  cookie = CanonicalCookie::CreateForTesting(url, "A=2; SameSite=None", now,
                                             server_time);
  EXPECT_TRUE(cookie.get());
  cookie = CanonicalCookie::CreateForTesting(url, "A=2;", now, server_time);
  EXPECT_TRUE(cookie.get());
}

TEST(CanonicalCookieTest, CreateHttpOnly) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  CookieInclusionStatus status;

  // An HttpOnly cookie can be created.
  std::unique_ptr<CanonicalCookie> cookie =
      CanonicalCookie::Create(url, "A=2; HttpOnly", now, server_time,
                              /*cookie_partition_key=*/std::nullopt,
                              CookieSourceType::kUnknown, &status);
  EXPECT_TRUE(cookie->IsHttpOnly());
  EXPECT_TRUE(status.IsInclude());
}

TEST(CanonicalCookieTest, CreateWithInvalidDomain) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  CookieInclusionStatus status;

  std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::Create(
      url, "A=2; Domain=wrongdomain.com", now, server_time,
      /*cookie_partition_key=*/std::nullopt, CookieSourceType::kUnknown,
      &status);
  EXPECT_EQ(nullptr, cookie.get());
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));
}

// Creating a cookie for an eTLD is possible, but it must match the hostname and
// be a host cookie.
TEST(CanonicalCookieTest, CreateFromPublicSuffix) {
  GURL url("http://com/path");
  base::Time now = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  CookieInclusionStatus status;

  // Host cookie can be created for an eTLD.
  std::unique_ptr<CanonicalCookie> cookie =
      CanonicalCookie::Create(url, "A=2", now, server_time, std::nullopt,
                              CookieSourceType::kUnknown, &status);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsHostCookie());
  EXPECT_EQ("com", cookie->Domain());

  // Attempting to create a domain cookie still yields a valid cookie, but only
  // if the domain attribute is the same as the URL's host, and it becomes a
  // host cookie only.
  cookie = CanonicalCookie::Create(url, "A=2; domain=com", now, server_time,
                                   std::nullopt, CookieSourceType::kUnknown,
                                   &status);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsHostCookie());
  EXPECT_EQ("com", cookie->Domain());

  // Same thing if the domain attribute is specified with a dot.
  cookie = CanonicalCookie::Create(url, "A=2; domain=.com", now, server_time,
                                   std::nullopt, CookieSourceType::kUnknown,
                                   &status);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsHostCookie());
  EXPECT_EQ("com", cookie->Domain());

  // Capitalization is ok because everything is canonicalized.
  cookie = CanonicalCookie::Create(url, "A=2; domain=CoM", now, server_time,
                                   std::nullopt, CookieSourceType::kUnknown,
                                   &status);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsHostCookie());
  EXPECT_EQ("com", cookie->Domain());

  // Test an eTLD that is more than one label.
  // If the domain attribute minus any leading dot is the same as the url's
  // host, allow it to become a host cookie.
  GURL multilabel_url = GURL("http://co.uk/path");
  cookie = CanonicalCookie::Create(multilabel_url, "A=2", now, server_time,
                                   std::nullopt, CookieSourceType::kUnknown,
                                   &status);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsHostCookie());
  EXPECT_EQ("co.uk", cookie->Domain());

  cookie = CanonicalCookie::Create(multilabel_url, "A=2; domain=co.uk", now,
                                   server_time, std::nullopt,
                                   CookieSourceType::kUnknown, &status);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsHostCookie());
  EXPECT_EQ("co.uk", cookie->Domain());

  cookie = CanonicalCookie::Create(multilabel_url, "A=2; domain=.co.uk", now,
                                   server_time, std::nullopt,
                                   CookieSourceType::kUnknown, &status);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsHostCookie());
  EXPECT_EQ("co.uk", cookie->Domain());

  // Don't allow setting a domain cookie from a public suffix for a superdomain.
  cookie = CanonicalCookie::Create(multilabel_url, "A=2; domain=uk", now,
                                   server_time, std::nullopt,
                                   CookieSourceType::kUnknown, &status);
  EXPECT_EQ(nullptr, cookie.get());
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));

  cookie = CanonicalCookie::Create(multilabel_url, "A=2; domain=.uk", now,
                                   server_time, std::nullopt,
                                   CookieSourceType::kUnknown, &status);
  EXPECT_EQ(nullptr, cookie.get());
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));

  // Don't allow setting a domain cookie for an unrelated domain.
  cookie = CanonicalCookie::Create(multilabel_url, "A=2; domain=foo.com", now,
                                   server_time, std::nullopt,
                                   CookieSourceType::kUnknown, &status);
  EXPECT_EQ(nullptr, cookie.get());
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));

  // Don't allow setting a domain cookie for some other domain with no
  // registrable domain.
  cookie = CanonicalCookie::Create(multilabel_url, "A=2; domain=com", now,
                                   server_time, std::nullopt,
                                   CookieSourceType::kUnknown, &status);
  EXPECT_EQ(nullptr, cookie.get());
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));
}

TEST(CanonicalCookieTest, CreateWithNonASCIIDomain) {
  GURL url("http://www.xn--xample-9ua.com/test/foo.html");
  base::Time now = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;

  // Test with feature flag enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kCookieDomainRejectNonASCII);
    CookieInclusionStatus status;

    // Test that non-ascii characters are rejected.
    std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::Create(
        url, "A=1; Domain=\xC3\xA9xample.com", now, server_time,
        /*cookie_partition_key=*/std::nullopt, CookieSourceType::kUnknown,
        &status);
    EXPECT_EQ(nullptr, cookie.get());
    EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
        {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN,
         CookieInclusionStatus::EXCLUDE_DOMAIN_NON_ASCII}));
    EXPECT_FALSE(
        status.HasWarningReason(CookieInclusionStatus::WARN_DOMAIN_NON_ASCII));
  }

  // Test with feature flag disabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kCookieDomainRejectNonASCII);
    CookieInclusionStatus status2;

    std::unique_ptr<CanonicalCookie> cookie2 = CanonicalCookie::Create(
        url, "A=2; Domain=\xC3\xA9xample.com", now, server_time,
        /*cookie_partition_key=*/std::nullopt, CookieSourceType::kUnknown,
        &status2);

    EXPECT_TRUE(cookie2.get());
    EXPECT_TRUE(status2.IsInclude());
    EXPECT_TRUE(
        status2.HasWarningReason(CookieInclusionStatus::WARN_DOMAIN_NON_ASCII));
  }

  // Test that regular ascii punycode still works.
  CookieInclusionStatus status3;
  std::unique_ptr<CanonicalCookie> cookie3 = CanonicalCookie::Create(
      url, "A=3; Domain=xn--xample-9ua.com", now, server_time,
      /*cookie_partition_key=*/std::nullopt, CookieSourceType::kUnknown,
      &status3);
  EXPECT_TRUE(cookie3.get());
  EXPECT_TRUE(status3.IsInclude());
  EXPECT_FALSE(
      status3.HasWarningReason(CookieInclusionStatus::WARN_DOMAIN_NON_ASCII));
}

TEST(CanonicalCookieTest, CreateWithDomainAsIP) {
  GURL url("http://1.1.1.1");
  GURL url6("http://[2606:2800:220:1:248:1893:25c8:1946]");

  base::Time now = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  CookieInclusionStatus status;

  const struct {
    const GURL url;
    const std::string cookie_line;
    const bool expectedResult;
  } kTests[] = {
      {url, "d=1;Domain=1.1.1.1;", true},
      {url, "dd=1;Domain=.1.1.1.1;", true},
      {url, "ds=1;Domain=1.1.1;", false},
      {url, "dsd=1;Domain=.1.1.1;", false},
      {url, "dx=1;Domain=0x01.0x1.0x1.0x1;", false},
      {url, "dxd=1;Domain=.0x01.0x1.0x1.0x1;", false},
      {url, "do=1;Domain=0001.0001.0001.0001;", false},
      {url, "d10=1;Domain=16843009;", false},
      {url, "d16=value;Domain=0x1010101;", false},
      {url, "d8=1;Domain=0100200401;", false},
      {url, "dm=1;Domain=00001.0x01.1.001;", false},
      {url6, "d1ipv6=1;Domain=[2606:2800:220:1:248:1893:25c8:1946];", true},
      {url6, "dd1ipv6=1;Domain=.[2606:2800:220:1:248:1893:25c8:1946];", true},
      {url6, "dc1ipv6=1;Domain=[2606:2800:220:1:248:1893:25C8:1946];", true},
      {url6, "d2ipv6=1;Domain=2606:2800:220:1:248:1893:25c8:1946;", false},
      {url6, "dd2ipv6=1;Domain=.2606:2800:220:1:248:1893:25c8:1946;", false},
      {url6, "dc2ipv6=1;Domain=2606:2800:220:1:248:1893:25C8:1946;", false},
  };

  for (const auto& test : kTests) {
    std::unique_ptr<CanonicalCookie> cookie =
        CanonicalCookie::Create(test.url, test.cookie_line, now, server_time,
                                /*cookie_partition_key=*/std::nullopt,
                                CookieSourceType::kUnknown, &status);
    if (test.expectedResult) {
      ASSERT_TRUE(cookie.get());
      EXPECT_EQ(test.url.host(), cookie->Domain());
    } else {
      EXPECT_EQ(nullptr, cookie.get());
      EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
          {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));
    }
  }
}

TEST(CanonicalCookieTest, CreateWithPartitioned) {
  GURL url("https://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  auto partition_key =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"));
  CookieInclusionStatus status;

  // Valid Partitioned attribute
  std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::Create(
      url, "__Host-A=2; Partitioned; Path=/; Secure", creation_time,
      server_time, partition_key, CookieSourceType::kUnknown, &status);
  ASSERT_TRUE(cookie.get());
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->SecureAttribute());
  EXPECT_TRUE(cookie->IsPartitioned());
  EXPECT_EQ(partition_key, cookie->PartitionKey());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookie->SameSite());

  // Create() without Partitioned in the cookie line should not result in a
  // partitioned cookie.
  status = CookieInclusionStatus();
  cookie = CanonicalCookie::Create(url, "__Host-A=2; Path=/; Secure",
                                   creation_time, server_time, partition_key,
                                   CookieSourceType::kUnknown, &status);
  ASSERT_TRUE(cookie.get());
  EXPECT_TRUE(status.IsInclude());
  EXPECT_FALSE(cookie->IsPartitioned());
  EXPECT_FALSE(cookie->PartitionKey());

  // Partitioned cookies with no __Host- prefix are still valid if they still
  // have Secure, Path=/, and no Domain.
  status = CookieInclusionStatus();
  cookie = CanonicalCookie::Create(url, "A=2; Partitioned; Path=/; Secure",
                                   creation_time, server_time, partition_key,
                                   CookieSourceType::kUnknown, &status);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsPartitioned());
  EXPECT_EQ(partition_key, cookie->PartitionKey());

  // Invalid Partitioned attribute: No Secure attribute.
  status = CookieInclusionStatus();
  cookie = CanonicalCookie::Create(url, "A=2; Partitioned; Path=/",
                                   creation_time, server_time, partition_key,
                                   CookieSourceType::kUnknown, &status);
  EXPECT_FALSE(cookie.get());
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PARTITIONED}));

  // Partitioned attribute: No Path attribute.
  status = CookieInclusionStatus();
  cookie = CanonicalCookie::Create(url, "A=2; Partitioned; Secure",
                                   creation_time, server_time, partition_key,
                                   CookieSourceType::kUnknown, &status);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsPartitioned());
  EXPECT_EQ(partition_key, cookie->PartitionKey());

  // Partitioned attribute: Path attribute not equal to "/".
  status = CookieInclusionStatus();
  cookie = CanonicalCookie::Create(
      url, "A=2; Partitioned; Path=/foobar; Secure", creation_time, server_time,
      partition_key, CookieSourceType::kUnknown, &status);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsPartitioned());
  EXPECT_EQ(partition_key, cookie->PartitionKey());

  // Partitioned attribute: Domain cookie.
  status = CookieInclusionStatus();
  cookie = CanonicalCookie::Create(
      url, "A=2; Partitioned; Path=/; Secure; Domain=example.com",
      creation_time, server_time, partition_key, CookieSourceType::kUnknown,
      &status);
  EXPECT_TRUE(cookie.get());
  LOG(ERROR) << status;
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsPartitioned());
  EXPECT_EQ(partition_key, cookie->PartitionKey());

  // No Partitioned attribute but with a nonce.
  status = CookieInclusionStatus();
  auto partition_key_with_nonce =
      std::make_optional(CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite.com"),
          CookiePartitionKey::AncestorChainBit::kCrossSite,
          base::UnguessableToken::Create()));
  cookie = CanonicalCookie::Create(
      url, "__Host-A=2; Path=/; Secure", creation_time, server_time,
      partition_key_with_nonce, CookieSourceType::kUnknown, &status);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->IsPartitioned());
  EXPECT_EQ(partition_key_with_nonce, cookie->PartitionKey());
}

TEST(CanonicalCookieTest, CreateWithPartitioned_Localhost) {
  GURL url("http://localhost:8000/foo/bar.html");
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  auto partition_key =
      CookiePartitionKey::FromURLForTesting(GURL("http://localhost:8000"));
  CookieInclusionStatus status;

  std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::Create(
      url, "foo=bar; Path=/; Secure; Partitioned", creation_time, server_time,
      partition_key, CookieSourceType::kUnknown, &status);
  ASSERT_TRUE(cookie.get());
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(cookie->SecureAttribute());
  EXPECT_TRUE(cookie->IsPartitioned());
  EXPECT_EQ(partition_key, cookie->PartitionKey());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookie->SameSite());
}

TEST(CanonicalCookieTest, CreateWithMaxAge) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;

  // Max-age with positive integer.
  std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::CreateForTesting(
      url, "A=1; max-age=60", creation_time, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Seconds(60) + creation_time, cookie->ExpiryDate());
  EXPECT_TRUE(cookie->IsCanonical());

  // Max-age with expires (max-age should take precedence).
  cookie = CanonicalCookie::CreateForTesting(
      url, "A=1; expires=01-Jan-1970, 00:00:00 GMT; max-age=60", creation_time,
      server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Seconds(60) + creation_time, cookie->ExpiryDate());
  EXPECT_TRUE(cookie->IsCanonical());

  // Max-age=0 should create an expired cookie with expiry equal to the earliest
  // representable time.
  cookie = CanonicalCookie::CreateForTesting(url, "A=1; max-age=0",
                                             creation_time, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(cookie->IsPersistent());
  EXPECT_TRUE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time::Min(), cookie->ExpiryDate());
  EXPECT_TRUE(cookie->IsCanonical());

  // Negative max-age should create an expired cookie with expiry equal to the
  // earliest representable time.
  cookie = CanonicalCookie::CreateForTesting(url, "A=1; max-age=-1",
                                             creation_time, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(cookie->IsPersistent());
  EXPECT_TRUE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time::Min(), cookie->ExpiryDate());
  EXPECT_TRUE(cookie->IsCanonical());

  // Max-age with whitespace (should be trimmed out).
  cookie = CanonicalCookie::CreateForTesting(url, "A=1; max-age = 60  ; Secure",
                                             creation_time, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Seconds(60) + creation_time, cookie->ExpiryDate());
  EXPECT_TRUE(cookie->IsCanonical());

  // Max-age with non-integer should be ignored.
  cookie = CanonicalCookie::CreateForTesting(url, "A=1; max-age=abcd",
                                             creation_time, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_TRUE(cookie->IsCanonical());

  // Overflow max-age should be clipped.
  cookie = CanonicalCookie::CreateForTesting(
      url,
      "A=1; "
      "max-age="
      "9999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999",
      creation_time, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(creation_time + base::Days(400), cookie->ExpiryDate());
  EXPECT_TRUE(cookie->IsCanonical());

  // Underflow max-age should be clipped.
  cookie = CanonicalCookie::CreateForTesting(
      url,
      "A=1; "
      "max-age=-"
      "9999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999",
      creation_time, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(cookie->IsPersistent());
  EXPECT_TRUE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time::Min(), cookie->ExpiryDate());
  EXPECT_TRUE(cookie->IsCanonical());
}

TEST(CanonicalCookieTest, CreateWithExpires) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;

  // Expires in the past
  base::Time past_date = base::Time::Now() - base::Days(10);
  std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::CreateForTesting(
      url, "A=1; expires=" + HttpUtil::TimeFormatHTTP(past_date), creation_time,
      server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(cookie->IsPersistent());
  EXPECT_TRUE(cookie->IsExpired(creation_time));
  EXPECT_TRUE((past_date - cookie->ExpiryDate()).magnitude() <
              base::Seconds(1));
  EXPECT_TRUE(cookie->IsCanonical());

  // Expires in the future
  base::Time future_date = base::Time::Now() + base::Days(10);
  cookie = CanonicalCookie::CreateForTesting(
      url, "A=1; expires=" + HttpUtil::TimeFormatHTTP(future_date),
      creation_time, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_TRUE((future_date - cookie->ExpiryDate()).magnitude() <
              base::Seconds(1));
  EXPECT_TRUE(cookie->IsCanonical());

  // Expires in the far future
  future_date = base::Time::Now() + base::Days(800);
  cookie = CanonicalCookie::CreateForTesting(
      url, "A=1; expires=" + HttpUtil::TimeFormatHTTP(future_date),
      creation_time, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_TRUE(
      (cookie->ExpiryDate() - creation_time - base::Days(400)).magnitude() <
      base::Seconds(1));
  EXPECT_TRUE(cookie->IsCanonical());

  // Expires in the far future using CreateUnsafeCookieForTesting.
  cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "1", url.host(), url.path(), creation_time, base::Time::Max(),
      base::Time(), base::Time(), true, false, CookieSameSite::UNSPECIFIED,
      COOKIE_PRIORITY_HIGH, std::nullopt /* cookie_partition_key */,
      CookieSourceScheme::kSecure, 443);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time::Max(), cookie->ExpiryDate());
  EXPECT_EQ(base::Time(), cookie->LastUpdateDate());
  EXPECT_FALSE(cookie->IsCanonical());

  // Expires in the far future using FromStorage.
  cookie = CanonicalCookie::FromStorage(
      "A", "B", "www.foo.com", "/bar", creation_time, base::Time::Max(),
      base::Time(), base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, CookieSourceScheme::kSecure, 443,
      CookieSourceType::kUnknown);
  EXPECT_TRUE(cookie.get());
  EXPECT_TRUE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time::Max(), cookie->ExpiryDate());
  EXPECT_EQ(base::Time(), cookie->LastUpdateDate());
  EXPECT_FALSE(cookie->IsCanonical());
}

TEST(CanonicalCookieTest, EmptyExpiry) {
  GURL url("http://www7.ipdl.inpit.go.jp/Tokujitu/tjkta.ipdl?N0000=108");
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;

  std::string cookie_line =
      "ACSTM=20130308043820420042; path=/; domain=ipdl.inpit.go.jp; Expires=";
  std::unique_ptr<CanonicalCookie> cookie(CanonicalCookie::CreateForTesting(
      url, cookie_line, creation_time, server_time));
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time(), cookie->ExpiryDate());

  // With a stale server time
  server_time = creation_time - base::Hours(1);
  cookie = CanonicalCookie::CreateForTesting(url, cookie_line, creation_time,
                                             server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time(), cookie->ExpiryDate());

  // With a future server time
  server_time = creation_time + base::Hours(1);
  cookie = CanonicalCookie::CreateForTesting(url, cookie_line, creation_time,
                                             server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time(), cookie->ExpiryDate());
}

TEST(CanonicalCookieTest, CreateWithLastUpdate) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now() - base::Days(1);
  base::Time last_update_time = base::Time::Now() - base::Hours(1);
  std::optional<base::Time> server_time = std::nullopt;

  // Creating a cookie sets the last update date as now.
  std::unique_ptr<CanonicalCookie> cookie =
      CanonicalCookie::CreateForTesting(url, "A=1", creation_time, server_time,
                                        /*cookie_partition_key=*/std::nullopt);
  ASSERT_TRUE(cookie.get());
  EXPECT_TRUE((base::Time::Now() - cookie->LastUpdateDate()).magnitude() <
              base::Seconds(1));

  // Creating a sanitized cookie sets the last update date as now.
  cookie = CanonicalCookie::CreateSanitizedCookie(
      url, "A", "1", url.host(), url.path(), creation_time, base::Time(),
      creation_time, /*secure=*/true,
      /*http_only=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, /*status=*/nullptr);
  ASSERT_TRUE(cookie.get());
  EXPECT_TRUE((base::Time::Now() - cookie->LastUpdateDate()).magnitude() <
              base::Seconds(1));

  // Creating an unsafe cookie allows us to set the last update date.
  cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "1", url.host(), url.path(), creation_time, base::Time(),
      base::Time(), last_update_time, /*secure=*/true,
      /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, CookieSourceScheme::kSecure,
      /*source_port=*/443);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(last_update_time, cookie->LastUpdateDate());

  // Loading a cookie from storage allows us to set the last update date.
  cookie = CanonicalCookie::FromStorage(
      "A", "1", url.host(), url.path(), creation_time, base::Time(),
      base::Time(), last_update_time, /*secure=*/true,
      /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, CookieSourceScheme::kSecure,
      /*source_port=*/443, CookieSourceType::kUnknown);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(last_update_time, cookie->LastUpdateDate());
}

TEST(CanonicalCookieTest, IsEquivalent) {
  GURL url("https://www.example.com/");
  std::string cookie_name = "A";
  std::string cookie_value = "2EDA-EF";
  std::string cookie_domain = ".www.example.com";
  std::string cookie_path = "/path";
  base::Time creation_time = base::Time::Now();
  base::Time expiration_time = creation_time + base::Days(2);
  base::Time update_time = creation_time + base::Days(1);
  bool secure = false;
  bool httponly = false;
  CookieSameSite same_site = CookieSameSite::NO_RESTRICTION;

  // Test that a cookie is equivalent to itself.
  auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), update_time, secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_TRUE(cookie->IsEquivalent(*cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Test that two identical cookies are equivalent.
  auto other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), update_time, secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Tests that use different variations of attribute values that
  // DON'T affect cookie equivalence.
  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, "2", cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), update_time, secure, httponly, same_site,
      COOKIE_PRIORITY_HIGH);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  base::Time other_creation_time = creation_time + base::Minutes(2);
  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, "2", cookie_domain, cookie_path, other_creation_time,
      expiration_time, base::Time(), update_time, secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_name, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), update_time, true, httponly, same_site,
      COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_name, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), update_time, secure, true, same_site,
      COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_name, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), update_time, secure, httponly,
      CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Test the effect of a differing last_update_time on equivalency.
  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_name, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));
  EXPECT_FALSE(cookie->HasEquivalentDataMembers(*other_cookie));

  // Cookies whose names mismatch are not equivalent.
  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      "B", cookie_value, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), update_time, secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_FALSE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_FALSE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // A domain cookie at 'www.example.com' is not equivalent to a host cookie
  // at the same domain. These are, however, equivalent according to the laxer
  // rules of 'IsEquivalentForSecureCookieMatching'.
  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, "www.example.com", cookie_path, creation_time,
      expiration_time, base::Time(), update_time, secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_TRUE(cookie->IsDomainCookie());
  EXPECT_FALSE(other_cookie->IsDomainCookie());
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Likewise, a cookie on 'example.com' is not equivalent to a cookie on
  // 'www.example.com', but they are equivalent for secure cookie matching.
  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, ".example.com", cookie_path, creation_time,
      expiration_time, base::Time(), update_time, secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
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
      expiration_time, base::Time(), update_time, secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_FALSE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_FALSE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, cookie_domain, cookie_path + "/subpath",
      creation_time, expiration_time, base::Time(), update_time, secure,
      httponly, same_site, COOKIE_PRIORITY_MEDIUM);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  // The path comparison is asymmetric
  EXPECT_FALSE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, cookie_domain, "/", creation_time,
      expiration_time, base::Time(), update_time, secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_FALSE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Partitioned cookies are not equivalent to unpartitioned cookies.
  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), update_time, secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM,
      CookiePartitionKey::FromURLForTesting(GURL("https://foo.com")));
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_FALSE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));

  // Partitioned cookies are equal if they have the same partition key.
  auto paritioned_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), update_time, secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM,
      CookiePartitionKey::FromURLForTesting(GURL("https://foo.com")));
  EXPECT_TRUE(paritioned_cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(
      paritioned_cookie->IsEquivalentForSecureCookieMatching(*other_cookie));

  // Partitioned cookies with different partition keys are not equal
  other_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, cookie_value, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), update_time, secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM,
      CookiePartitionKey::FromURLForTesting(GURL("https://bar.com")));
  EXPECT_FALSE(paritioned_cookie->IsEquivalent(*other_cookie));
  EXPECT_FALSE(
      paritioned_cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
}

TEST(CanonicalCookieTest, IsEquivalentForSecureCookieMatching) {
  struct {
    struct {
      const char* name;
      const char* domain;
      const char* path;
      std::optional<CookiePartitionKey> cookie_partition_key = std::nullopt;
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
      // Partitioned cookies are not equivalent to unpartitioned cookies.
      {{"A", ".a.foo.com", "/"},
       {"A", ".a.foo.com", "/",
        CookiePartitionKey::FromURLForTesting(GURL("https://bar.com"))},
       false,
       true},
      // Partitioned cookies are equivalent if they have the same partition key.
      {{"A", "a.foo.com", "/",
        CookiePartitionKey::FromURLForTesting(GURL("https://bar.com"))},
       {"A", "a.foo.com", "/",
        CookiePartitionKey::FromURLForTesting(GURL("https://bar.com"))},
       true,
       true},
      // Partitioned cookies are *not* equivalent if they have the different
      // partition keys.
      {{"A", "a.foo.com", "/",
        CookiePartitionKey::FromURLForTesting(GURL("https://bar.com"))},
       {"A", "a.foo.com", "/",
        CookiePartitionKey::FromURLForTesting(GURL("https://baz.com"))},
       false,
       true},
  };

  for (auto test : kTests) {
    auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
        test.cookie.name, "value1", test.cookie.domain, test.cookie.path,
        base::Time(), base::Time(), base::Time(), base::Time(),
        false /* secure */, false /* httponly */, CookieSameSite::LAX_MODE,
        COOKIE_PRIORITY_MEDIUM, test.cookie.cookie_partition_key);
    auto secure_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
        test.secure_cookie.name, "value2", test.secure_cookie.domain,
        test.secure_cookie.path, base::Time(), base::Time(), base::Time(),
        base::Time(), true /* secure */, false /* httponly */,
        CookieSameSite::LAX_MODE, COOKIE_PRIORITY_MEDIUM,
        test.secure_cookie.cookie_partition_key);

    EXPECT_EQ(test.equivalent,
              cookie->IsEquivalentForSecureCookieMatching(*secure_cookie));
    EXPECT_EQ(test.equivalent == test.is_symmetric,
              secure_cookie->IsEquivalentForSecureCookieMatching(*cookie));
  }
}

TEST(CanonicalCookieTest, IsEquivalentForOriginBoundCookies) {
  auto create_cookie = [](const char* domain_field,
                          CookieSourceScheme source_scheme, int source_port) {
    const char* cookie_name = "A";
    const char* cookie_value = "2EDA-EF";
    const char* cookie_path = "/";
    const base::Time creation_time = base::Time::Now();
    const base::Time expiration_time = creation_time + base::Days(2);
    const base::Time update_time = creation_time + base::Days(1);
    const bool secure = false;
    const bool httponly = false;
    const CookieSameSite same_site = CookieSameSite::NO_RESTRICTION;
    const std::optional<CookiePartitionKey> partition_key = std::nullopt;

    return CanonicalCookie::CreateUnsafeCookieForTesting(
        cookie_name, cookie_value, domain_field, cookie_path, creation_time,
        expiration_time, base::Time(), update_time, secure, httponly, same_site,
        COOKIE_PRIORITY_MEDIUM, partition_key, source_scheme, source_port);
  };

  const char* domain = ".www.example.com";
  const char* host_only_domain = "www.example.com";
  const CookieSourceScheme http_scheme = CookieSourceScheme::kNonSecure;
  const int port_80 = 80;

  auto domain_cookie = create_cookie(domain, http_scheme, port_80);

  auto host_cookie = create_cookie(host_only_domain, http_scheme, port_80);

  // Host cookies are never equivalent to domain cookies.
  ASSERT_FALSE(domain_cookie->IsEquivalent(*host_cookie));
  ASSERT_FALSE(host_cookie->IsEquivalent(*domain_cookie));

  // With neither binding enabled, difference in scheme and port have no effect
  // on equivalency.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({}, {features::kEnableSchemeBoundCookies,
                                       features::kEnablePortBoundCookies});

    // Different schemes are equivalent.
    auto other_cookie =
        create_cookie(domain, CookieSourceScheme::kSecure, port_80);
    EXPECT_TRUE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie = create_cookie(domain, CookieSourceScheme::kUnset, port_80);
    EXPECT_TRUE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie =
        create_cookie(host_only_domain, CookieSourceScheme::kSecure, port_80);
    EXPECT_TRUE(host_cookie->IsEquivalent(*other_cookie));

    other_cookie =
        create_cookie(host_only_domain, CookieSourceScheme::kUnset, port_80);
    EXPECT_TRUE(host_cookie->IsEquivalent(*other_cookie));

    // Different ports are equivalent.
    other_cookie = create_cookie(domain, http_scheme, -1);
    EXPECT_TRUE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie = create_cookie(domain, http_scheme, 123);
    EXPECT_TRUE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie = create_cookie(host_only_domain, http_scheme, -1);
    EXPECT_TRUE(host_cookie->IsEquivalent(*other_cookie));

    other_cookie = create_cookie(host_only_domain, http_scheme, 123);
    EXPECT_TRUE(host_cookie->IsEquivalent(*other_cookie));

    // Different scheme and ports are equivalent.
    other_cookie = create_cookie(domain, CookieSourceScheme::kSecure, 123);
    EXPECT_TRUE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie =
        create_cookie(host_only_domain, CookieSourceScheme::kSecure, 123);
    EXPECT_TRUE(host_cookie->IsEquivalent(*other_cookie));
  }

  // With scheme binding enabled, differences in scheme means cookies are not
  // equivalent.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kEnableSchemeBoundCookies},
                                  {features::kEnablePortBoundCookies});

    // Different schemes are not equivalent.
    auto other_cookie =
        create_cookie(domain, CookieSourceScheme::kSecure, port_80);
    EXPECT_FALSE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie = create_cookie(domain, CookieSourceScheme::kUnset, port_80);
    EXPECT_FALSE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie =
        create_cookie(host_only_domain, CookieSourceScheme::kSecure, port_80);
    EXPECT_FALSE(host_cookie->IsEquivalent(*other_cookie));

    other_cookie =
        create_cookie(host_only_domain, CookieSourceScheme::kUnset, port_80);
    EXPECT_FALSE(host_cookie->IsEquivalent(*other_cookie));

    // Different ports are equivalent.
    other_cookie = create_cookie(domain, http_scheme, -1);
    EXPECT_TRUE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie = create_cookie(domain, http_scheme, 123);
    EXPECT_TRUE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie = create_cookie(host_only_domain, http_scheme, -1);
    EXPECT_TRUE(host_cookie->IsEquivalent(*other_cookie));

    other_cookie = create_cookie(host_only_domain, http_scheme, 123);
    EXPECT_TRUE(host_cookie->IsEquivalent(*other_cookie));

    // Different scheme and ports are not equivalent.
    other_cookie = create_cookie(domain, CookieSourceScheme::kSecure, 123);
    EXPECT_FALSE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie =
        create_cookie(host_only_domain, CookieSourceScheme::kSecure, 123);
    EXPECT_FALSE(host_cookie->IsEquivalent(*other_cookie));
  }
  // With port binding enabled, domain cookies with the different ports are
  // equivalent. Host cookies are not equivalent.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kEnablePortBoundCookies},
                                  {features::kEnableSchemeBoundCookies});

    // Different schemes are equivalent.
    auto other_cookie =
        create_cookie(domain, CookieSourceScheme::kSecure, port_80);
    EXPECT_TRUE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie = create_cookie(domain, CookieSourceScheme::kUnset, port_80);
    EXPECT_TRUE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie =
        create_cookie(host_only_domain, CookieSourceScheme::kSecure, port_80);
    EXPECT_TRUE(host_cookie->IsEquivalent(*other_cookie));

    other_cookie =
        create_cookie(host_only_domain, CookieSourceScheme::kUnset, port_80);
    EXPECT_TRUE(host_cookie->IsEquivalent(*other_cookie));

    // Different ports are equivalent for domain cookies.
    other_cookie = create_cookie(domain, http_scheme, -1);
    EXPECT_TRUE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie = create_cookie(domain, http_scheme, 123);
    EXPECT_TRUE(domain_cookie->IsEquivalent(*other_cookie));

    // But not so for host cookies.
    other_cookie = create_cookie(host_only_domain, http_scheme, -1);
    EXPECT_FALSE(host_cookie->IsEquivalent(*other_cookie));

    other_cookie = create_cookie(host_only_domain, http_scheme, 123);
    EXPECT_FALSE(host_cookie->IsEquivalent(*other_cookie));

    // Different scheme and ports are equivalent for domain cookies.
    other_cookie = create_cookie(domain, CookieSourceScheme::kSecure, 123);
    EXPECT_TRUE(domain_cookie->IsEquivalent(*other_cookie));

    // But not so for host cookies.
    other_cookie =
        create_cookie(host_only_domain, CookieSourceScheme::kSecure, 123);
    EXPECT_FALSE(host_cookie->IsEquivalent(*other_cookie));
  }

  // When both scheme and port binding are enabled, different schemes are always
  // not equivalent while different ports depend on whether the cookie is host
  // or domain.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kEnablePortBoundCookies,
                                   features::kEnableSchemeBoundCookies},
                                  {});

    // Different schemes are not equivalent.
    auto other_cookie =
        create_cookie(domain, CookieSourceScheme::kSecure, port_80);
    EXPECT_FALSE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie = create_cookie(domain, CookieSourceScheme::kSecure, port_80);
    EXPECT_FALSE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie =
        create_cookie(host_only_domain, CookieSourceScheme::kSecure, port_80);
    EXPECT_FALSE(host_cookie->IsEquivalent(*other_cookie));

    other_cookie =
        create_cookie(host_only_domain, CookieSourceScheme::kUnset, port_80);
    EXPECT_FALSE(host_cookie->IsEquivalent(*other_cookie));

    // Different ports are equivalent for domain cookies.
    other_cookie = create_cookie(domain, http_scheme, -1);
    EXPECT_TRUE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie = create_cookie(domain, http_scheme, 123);
    EXPECT_TRUE(domain_cookie->IsEquivalent(*other_cookie));

    // But not so for host cookies.
    other_cookie = create_cookie(host_only_domain, http_scheme, -1);
    EXPECT_FALSE(host_cookie->IsEquivalent(*other_cookie));

    other_cookie = create_cookie(host_only_domain, http_scheme, 123);
    EXPECT_FALSE(host_cookie->IsEquivalent(*other_cookie));

    // Different scheme and ports are not equivalent.
    other_cookie = create_cookie(domain, CookieSourceScheme::kSecure, 123);
    EXPECT_FALSE(domain_cookie->IsEquivalent(*other_cookie));

    other_cookie =
        create_cookie(host_only_domain, CookieSourceScheme::kSecure, 123);
    EXPECT_FALSE(host_cookie->IsEquivalent(*other_cookie));
  }
}

TEST(CanonicalCookieTest, IsDomainMatch) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;

  std::unique_ptr<CanonicalCookie> cookie(CanonicalCookie::CreateForTesting(
      url, "A=2", creation_time, server_time));
  EXPECT_TRUE(cookie->IsHostCookie());
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));

  cookie = CanonicalCookie::CreateForTesting(url, "A=2; Domain=www.example.com",
                                             creation_time, server_time);
  EXPECT_TRUE(cookie->IsDomainCookie());
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));

  cookie = CanonicalCookie::CreateForTesting(
      url, "A=2; Domain=.www.example.com", creation_time, server_time);
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));
}

TEST(CanonicalCookieTest, IsOnPath) {
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;

  std::unique_ptr<CanonicalCookie> cookie(CanonicalCookie::CreateForTesting(
      GURL("http://www.example.com"), "A=2", creation_time, server_time));
  EXPECT_TRUE(cookie->IsOnPath("/"));
  EXPECT_TRUE(cookie->IsOnPath("/test"));
  EXPECT_TRUE(cookie->IsOnPath("/test/bar.html"));

  // Test the empty string edge case.
  EXPECT_FALSE(cookie->IsOnPath(std::string()));

  cookie = CanonicalCookie::CreateForTesting(
      GURL("http://www.example.com/test/foo.html"), "A=2", creation_time,
      server_time);
  EXPECT_FALSE(cookie->IsOnPath("/"));
  EXPECT_TRUE(cookie->IsOnPath("/test"));
  EXPECT_TRUE(cookie->IsOnPath("/test/bar.html"));
  EXPECT_TRUE(cookie->IsOnPath("/test/sample/bar.html"));
}

TEST(CanonicalCookieTest, GetEffectiveSameSite) {
  struct {
    CookieSameSite same_site;
    CookieEffectiveSameSite expected_effective_same_site;
    // nullopt for following members indicates same effective SameSite result
    // for all possible values.
    std::optional<CookieAccessSemantics> access_semantics = std::nullopt;
    std::optional<bool> is_cookie_recent = std::nullopt;
  } kTestCases[] = {
      // Explicitly specified SameSite always has the same effective SameSite
      // regardless of the access semantics.
      {CookieSameSite::NO_RESTRICTION, CookieEffectiveSameSite::NO_RESTRICTION},
      {CookieSameSite::LAX_MODE, CookieEffectiveSameSite::LAX_MODE},
      {CookieSameSite::STRICT_MODE, CookieEffectiveSameSite::STRICT_MODE},
      {CookieSameSite::NO_RESTRICTION, CookieEffectiveSameSite::NO_RESTRICTION},

      // UNSPECIFIED always maps to NO_RESTRICTION if LEGACY access semantics.
      {CookieSameSite::UNSPECIFIED, CookieEffectiveSameSite::NO_RESTRICTION,
       CookieAccessSemantics::LEGACY},

      // UNSPECIFIED with non-LEGACY access semantics depends on whether cookie
      // is recently created.
      {CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE,
       CookieAccessSemantics::NONLEGACY, true},
      {CookieSameSite::UNSPECIFIED,
       CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE,
       CookieAccessSemantics::UNKNOWN, true},
      {CookieSameSite::UNSPECIFIED, CookieEffectiveSameSite::LAX_MODE,
       CookieAccessSemantics::NONLEGACY, false},
      {CookieSameSite::UNSPECIFIED, CookieEffectiveSameSite::LAX_MODE,
       CookieAccessSemantics::UNKNOWN, false},
  };

  for (const auto& test : kTestCases) {
    std::vector<std::unique_ptr<CanonicalCookie>> cookies;

    base::Time now = base::Time::Now();
    base::Time recent_creation_time = now - (kLaxAllowUnsafeMaxAge / 4);
    base::Time not_recent_creation_time = now - (kLaxAllowUnsafeMaxAge * 4);
    base::Time expiry_time = now + (kLaxAllowUnsafeMaxAge / 4);

    if (!test.is_cookie_recent.has_value() || *test.is_cookie_recent) {
      // Recent session cookie.
      cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "2", "example.test", "/", recent_creation_time, base::Time(),
          base::Time(), base::Time(), true /* secure */, false /* httponly */,
          test.same_site, COOKIE_PRIORITY_DEFAULT));
      // Recent persistent cookie.
      cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "2", "example.test", "/", recent_creation_time, expiry_time,
          base::Time(), base::Time(), true /* secure */, false /* httponly */,
          test.same_site, COOKIE_PRIORITY_DEFAULT));
    }
    if (!test.is_cookie_recent.has_value() || !(*test.is_cookie_recent)) {
      // Not-recent session cookie.
      cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "2", "example.test", "/", not_recent_creation_time, base::Time(),
          base::Time(), base::Time(), true /* secure */, false /* httponly */,
          test.same_site, COOKIE_PRIORITY_DEFAULT));
      // Not-recent persistent cookie.
      cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "2", "example.test", "/", not_recent_creation_time, expiry_time,
          base::Time(), base::Time(), true /* secure */, false /* httponly */,
          test.same_site, COOKIE_PRIORITY_DEFAULT));
    }

    std::vector<CookieAccessSemantics> access_semantics = {
        CookieAccessSemantics::UNKNOWN, CookieAccessSemantics::LEGACY,
        CookieAccessSemantics::NONLEGACY};
    if (test.access_semantics.has_value())
      access_semantics = {*test.access_semantics};

    for (const auto& cookie : cookies) {
      for (const auto semantics : access_semantics) {
        EXPECT_EQ(test.expected_effective_same_site,
                  cookie->GetEffectiveSameSiteForTesting(semantics));
      }
    }
  }
}

TEST(CanonicalCookieTest, IncludeForRequestURL) {
  GURL url("http://www.example.com");
  base::Time creation_time = base::Time::Now();
  CookieOptions options = CookieOptions::MakeAllInclusive();
  std::optional<base::Time> server_time = std::nullopt;

  std::unique_ptr<CanonicalCookie> cookie(CanonicalCookie::CreateForTesting(
      url, "A=2", creation_time, server_time));
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              url, options,
              CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                                 /*delegate_treats_url_as_trustworthy=*/false})
          .status.IsInclude());
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              GURL("http://www.example.com/foo/bar"), options,
              CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                                 /*delegate_treats_url_as_trustworthy=*/false})
          .status.IsInclude());
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              GURL("https://www.example.com/foo/bar"), options,
              CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                                 /*delegate_treats_url_as_trustworthy=*/false})
          .status.IsInclude());
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              GURL("https://sub.example.com"), options,
              CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                                 /*delegate_treats_url_as_trustworthy=*/false})
          .status.HasExactlyExclusionReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH}));
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              GURL("https://sub.www.example.com"), options,
              CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                                 /*delegate_treats_url_as_trustworthy=*/false})
          .status.HasExactlyExclusionReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH}));
  // Test that cookie with a cookie path that does not match the url path are
  // not included.
  cookie = CanonicalCookie::CreateForTesting(url, "A=2; Path=/foo/bar",
                                             creation_time, server_time);
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              url, options,
              CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                                 /*delegate_treats_url_as_trustworthy=*/false})
          .status.HasExactlyExclusionReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_NOT_ON_PATH}));
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              GURL("http://www.example.com/foo/bar/index.html"), options,
              CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                                 /*delegate_treats_url_as_trustworthy=*/false})
          .status.IsInclude());
  // Test that a secure cookie is not included for a non secure URL.
  GURL secure_url("https://www.example.com");
  cookie = CanonicalCookie::CreateForTesting(secure_url, "A=2; Secure",
                                             creation_time, server_time);
  EXPECT_TRUE(cookie->SecureAttribute());
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              secure_url, options,
              CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                                 /*delegate_treats_url_as_trustworthy=*/false})
          .status.IsInclude());
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              url, options,
              CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                                 /*delegate_treats_url_as_trustworthy=*/false})
          .status.HasExactlyExclusionReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));

  // Test that a delegate can make an exception, however, and ask for a
  // non-secure URL to be treated as trustworthy... with a warning.
  cookie = CanonicalCookie::CreateForTesting(url, "A=2; Secure", creation_time,
                                             server_time);
  ASSERT_TRUE(cookie);
  EXPECT_TRUE(cookie->SecureAttribute());
  CookieAccessResult result = cookie->IncludeForRequestURL(
      url, options,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/true});
  EXPECT_TRUE(result.status.IsInclude());
  EXPECT_TRUE(result.status.HasWarningReason(
      CookieInclusionStatus::WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC));

  // The same happens for localhost even w/o delegate intervention.
  GURL localhost_url("http://localhost/");
  cookie = CanonicalCookie::CreateForTesting(localhost_url, "A=2; Secure",
                                             creation_time, server_time);
  ASSERT_TRUE(cookie);
  EXPECT_TRUE(cookie->SecureAttribute());
  result = cookie->IncludeForRequestURL(
      localhost_url, options,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false});
  EXPECT_TRUE(result.status.IsInclude());
  EXPECT_TRUE(result.status.HasWarningReason(
      CookieInclusionStatus::WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC));

  // An unneeded exception doesn't add a warning, however.
  cookie = CanonicalCookie::CreateForTesting(secure_url, "A=2; Secure",
                                             creation_time, server_time);
  ASSERT_TRUE(cookie);
  EXPECT_TRUE(cookie->SecureAttribute());
  result = cookie->IncludeForRequestURL(
      secure_url, options,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/true});
  EXPECT_TRUE(result.status.IsInclude());
  EXPECT_FALSE(result.status.ShouldWarn());

  // Test that http only cookies are only included if the include httponly flag
  // is set on the cookie options.
  options.set_include_httponly();
  cookie = CanonicalCookie::CreateForTesting(url, "A=2; HttpOnly",
                                             creation_time, server_time);
  EXPECT_TRUE(cookie->IsHttpOnly());
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              url, options,
              CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                                 /*delegate_treats_url_as_trustworthy=*/false})
          .status.IsInclude());
  options.set_exclude_httponly();
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              url, options,
              CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                                 /*delegate_treats_url_as_trustworthy=*/false})
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
    CookieAccessSemantics access_semantics,
    std::vector<IncludeForRequestURLTestCase> test_cases) {
  GURL url("https://example.test");
  for (const auto& test : test_cases) {
    base::Time creation_time = base::Time::Now() - test.creation_time_delta;
    std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::CreateForTesting(
        url, test.cookie_line, creation_time, std::nullopt /* server_time */);
    EXPECT_EQ(test.expected_samesite, cookie->SameSite());

    CookieOptions request_options;
    request_options.set_same_site_cookie_context(
        test.request_options_samesite_context);

    EXPECT_THAT(
        cookie->IncludeForRequestURL(
            url, request_options,
            CookieAccessParams{access_semantics,
                               /*delegate_treats_url_as_trustworthy=*/false}),
        MatchesCookieAccessResult(test.expected_inclusion_status,
                                  test.expected_effective_samesite,
                                  access_semantics, true))
        << cookie->Name() << "=" << cookie->Value();
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

  // Test cases where the unspecified-SameSite cookie defaults to SameSite=None
  // due to LEGACY access semantics):
  std::vector<IncludeForRequestURLTestCase> default_none_test_cases = {
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

  // Test cases where the unspecified-SameSite cookie defaults to SameSite=Lax:
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
    VerifyIncludeForRequestURLTestCases(CookieAccessSemantics::UNKNOWN,
                                        common_test_cases);
    VerifyIncludeForRequestURLTestCases(CookieAccessSemantics::UNKNOWN,
                                        default_lax_test_cases);
    VerifyIncludeForRequestURLTestCases(CookieAccessSemantics::LEGACY,
                                        common_test_cases);
    VerifyIncludeForRequestURLTestCases(CookieAccessSemantics::LEGACY,
                                        default_none_test_cases);
    VerifyIncludeForRequestURLTestCases(CookieAccessSemantics::NONLEGACY,
                                        common_test_cases);
    VerifyIncludeForRequestURLTestCases(CookieAccessSemantics::NONLEGACY,
                                        default_lax_test_cases);
  };

  {
    // Schemeful Same-Site disabled.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kSchemefulSameSite);

    SchemefulIndependentCases();

    VerifyIncludeForRequestURLTestCases(CookieAccessSemantics::LEGACY,
                                        schemeful_disabled_test_cases);
    VerifyIncludeForRequestURLTestCases(CookieAccessSemantics::NONLEGACY,
                                        schemeful_disabled_test_cases);
    VerifyIncludeForRequestURLTestCases(CookieAccessSemantics::UNKNOWN,
                                        schemeful_disabled_test_cases);
  }
  {
    // Schemeful Same-Site enabled.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kSchemefulSameSite);

    SchemefulIndependentCases();

    // With LEGACY access the cases should act as if schemeful is disabled, even
    // when it's not.
    VerifyIncludeForRequestURLTestCases(CookieAccessSemantics::LEGACY,
                                        schemeful_disabled_test_cases);

    VerifyIncludeForRequestURLTestCases(CookieAccessSemantics::NONLEGACY,
                                        schemeful_enabled_test_cases);
    VerifyIncludeForRequestURLTestCases(CookieAccessSemantics::UNKNOWN,
                                        schemeful_enabled_test_cases);
  }
}

TEST(CanonicalCookieTest, TestFirstPartyPartitionedAndCrossSiteContext) {
  std::string histogram_name =
      "Cookie.FirstPartyPartitioned.HasCrossSiteAncestor";
  base::Time current_time = base::Time::Now();
  base::HistogramTester histogram_tester;
  GURL url("https://www.example.com");
  GURL url2("https://wwwnottheSame.com");
  CookieOptions options;

  auto make_cookie = [current_time](const auto& partition_key) {
    return CanonicalCookie::CreateUnsafeCookieForTesting(
        "A", "2", "www.example.com", "/test", current_time, base::Time(),
        base::Time(), base::Time(), true /*secure*/, true /*httponly*/,
        CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, partition_key);
  };

  auto no_partition_key_cookie = make_cookie(std::nullopt);
  auto partitioned_cookie =
      make_cookie(CookiePartitionKey::FromURLForTesting(GURL(url)));
  auto nonced_partition_key_cookie =
      make_cookie(CookiePartitionKey::FromURLForTesting(
          GURL(url), CookiePartitionKey::AncestorChainBit::kCrossSite,
          base::UnguessableToken::Create()));
  auto different_site_partition_key_cookie =
      make_cookie(CookiePartitionKey::FromURLForTesting(GURL(url2)));

  histogram_tester.ExpectBucketCount(histogram_name, true, 0);
  histogram_tester.ExpectBucketCount(histogram_name, false, 0);
  no_partition_key_cookie->IncludeForRequestURL(
      url, options,
      CookieAccessParams{CookieAccessSemantics::NONLEGACY, false});

  histogram_tester.ExpectBucketCount(histogram_name, true, 0);
  histogram_tester.ExpectBucketCount(histogram_name, false, 0);

  partitioned_cookie->IncludeForRequestURL(
      url, options,
      CookieAccessParams{CookieAccessSemantics::NONLEGACY, false});
  histogram_tester.ExpectBucketCount(histogram_name, true, 1);
  histogram_tester.ExpectBucketCount(histogram_name, false, 0);

  nonced_partition_key_cookie->IncludeForRequestURL(
      url, options,
      CookieAccessParams{CookieAccessSemantics::NONLEGACY, false});
  histogram_tester.ExpectBucketCount(histogram_name, true, 1);
  histogram_tester.ExpectBucketCount(histogram_name, false, 0);

  different_site_partition_key_cookie->IncludeForRequestURL(
      url, options,
      CookieAccessParams{CookieAccessSemantics::NONLEGACY, false});
  histogram_tester.ExpectBucketCount(histogram_name, true, 1);
  histogram_tester.ExpectBucketCount(histogram_name, false, 0);

  // Show that a cookie in a non-CROSS_SITE context registers as false.
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::
              SAME_SITE_LAX));

  partitioned_cookie->IncludeForRequestURL(
      url, options,
      CookieAccessParams{CookieAccessSemantics::NONLEGACY, false});
  histogram_tester.ExpectBucketCount(histogram_name, true, 1);
  histogram_tester.ExpectBucketCount(histogram_name, false, 1);
}
// Test that SameSite=None requires Secure.
TEST(CanonicalCookieTest, IncludeCookiesWithoutSameSiteMustBeSecure) {
  GURL url("https://www.example.com");
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  CookieOptions options;

  // Make a SameSite=None, *not* Secure cookie.
  std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::CreateForTesting(
      url, "A=2; SameSite=None", creation_time, server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->SecureAttribute());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie->SameSite());
  EXPECT_EQ(CookieEffectiveSameSite::NO_RESTRICTION,
            cookie->GetEffectiveSameSiteForTesting());

  // UKNOWN semantics results in modern behavior (requiring Secure).
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              url, options,
              CookieAccessParams{CookieAccessSemantics::UNKNOWN,
                                 /*delegate_treats_url_as_trustworthy=*/false})
          .status.HasExactlyExclusionReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE}));

  // LEGACY semantics does not require Secure for SameSite=None cookies.
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              url, options,
              CookieAccessParams{CookieAccessSemantics::LEGACY,
                                 /*delegate_treats_url_as_trustworthy=*/false})
          .status.IsInclude());

  // NONLEGACY semantics results in modern behavior (requiring Secure).
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              url, options,
              CookieAccessParams{CookieAccessSemantics::NONLEGACY,
                                 /*delegate_treats_url_as_trustworthy=*/false})
          .status.HasExactlyExclusionReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE}));
}

TEST(CanonicalCookieTest, IncludeForRequestURL_SameSiteNone_Metrics) {
  constexpr bool delegate_treats_url_as_trustworthy = false;
  const base::Time now = base::Time::Now();
  const auto make_cookie = [now](CookieSameSite same_site) {
    return CanonicalCookie::CreateUnsafeCookieForTesting(
        "A", "1", "www.example.com", "/test", now, base::Time(), base::Time(),
        base::Time(), true /* secure */, false /*httponly*/, same_site,
        COOKIE_PRIORITY_DEFAULT);
  };
  GURL url("https://www.example.com/test");

  const std::unique_ptr<CanonicalCookie> same_site_none_cookie =
      make_cookie(CookieSameSite::NO_RESTRICTION);
  const std::unique_ptr<CanonicalCookie> same_site_lax_cookie =
      make_cookie(CookieSameSite::LAX_MODE);
  const std::unique_ptr<CanonicalCookie> same_site_strict_cookie =
      make_cookie(CookieSameSite::STRICT_MODE);
  CookieOptions options;
  options.set_same_site_cookie_context(CookieOptions::SameSiteCookieContext(
      CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE));

  // Check that the most restrictive context is recognized and enforced.
  EXPECT_THAT(same_site_none_cookie->IncludeForRequestURL(
                  url, options,
                  CookieAccessParams(CookieAccessSemantics::LEGACY,
                                     delegate_treats_url_as_trustworthy)),
              MatchesCookieAccessResult(CookieInclusionStatus(), _, _, true));
  EXPECT_THAT(same_site_lax_cookie->IncludeForRequestURL(
                  url, options,
                  CookieAccessParams(CookieAccessSemantics::LEGACY,
                                     delegate_treats_url_as_trustworthy)),
              MatchesCookieAccessResult(Not(net::IsInclude()), _, _, true));
  EXPECT_THAT(same_site_strict_cookie->IncludeForRequestURL(
                  url, options,
                  CookieAccessParams(CookieAccessSemantics::LEGACY,
                                     delegate_treats_url_as_trustworthy)),
              MatchesCookieAccessResult(Not(net::IsInclude()), _, _, true));

  // Next: allow a SameSite=Lax cookie.
  options.set_same_site_cookie_context(CookieOptions::SameSiteCookieContext(
      CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX));
  EXPECT_THAT(same_site_none_cookie->IncludeForRequestURL(
                  url, options,
                  CookieAccessParams(CookieAccessSemantics::LEGACY,
                                     delegate_treats_url_as_trustworthy)),
              MatchesCookieAccessResult(CookieInclusionStatus(), _, _, true));
  EXPECT_THAT(same_site_lax_cookie->IncludeForRequestURL(
                  url, options,
                  CookieAccessParams(CookieAccessSemantics::LEGACY,
                                     delegate_treats_url_as_trustworthy)),
              MatchesCookieAccessResult(net::IsInclude(), _, _, true));
  EXPECT_THAT(same_site_strict_cookie->IncludeForRequestURL(
                  url, options,
                  CookieAccessParams(CookieAccessSemantics::LEGACY,
                                     delegate_treats_url_as_trustworthy)),
              MatchesCookieAccessResult(Not(net::IsInclude()), _, _, true));

  // Next: allow a SameSite=Strict cookie.
  options.set_same_site_cookie_context(CookieOptions::SameSiteCookieContext(
      CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT));
  EXPECT_THAT(same_site_none_cookie->IncludeForRequestURL(
                  url, options,
                  CookieAccessParams(CookieAccessSemantics::LEGACY,
                                     delegate_treats_url_as_trustworthy)),
              MatchesCookieAccessResult(CookieInclusionStatus(), _, _, true));
  EXPECT_THAT(same_site_strict_cookie->IncludeForRequestURL(
                  url, options,
                  CookieAccessParams(CookieAccessSemantics::LEGACY,
                                     delegate_treats_url_as_trustworthy)),
              MatchesCookieAccessResult(net::IsInclude(), _, _, true));
}

// Test that the CookieInclusionStatus warning for inclusion changed by
// cross-site redirect context downgrade is applied correctly.
TEST(CanonicalCookieTest, IncludeForRequestURL_RedirectDowngradeWarning) {
  using Context = CookieOptions::SameSiteCookieContext;
  using ContextType = Context::ContextType;

  Context::ContextMetadata strict_lax_downgrade_metadata,
      strict_cross_downgrade_metadata;
  strict_lax_downgrade_metadata.cross_site_redirect_downgrade =
      Context::ContextMetadata::ContextDowngradeType::kStrictToLax;
  strict_cross_downgrade_metadata.cross_site_redirect_downgrade =
      Context::ContextMetadata::ContextDowngradeType::kStrictToCross;

  // Because there are downgrades we need to set the HTTP method as well, since
  // some metrics code expects that. The actual method doesn't matter here.
  strict_lax_downgrade_metadata.http_method_bug_1221316 =
      Context::ContextMetadata::HttpMethod::kGet;
  strict_cross_downgrade_metadata.http_method_bug_1221316 =
      Context::ContextMetadata::HttpMethod::kGet;

  GURL url("https://www.example.test/test");
  GURL insecure_url("http://www.example.test/test");

  const struct {
    ContextType context_type;
    Context::ContextMetadata metadata;
    CookieSameSite samesite;
    bool expect_cross_site_redirect_warning;
  } kTestCases[] = {
      // Strict-to-lax downgrade.
      {ContextType::SAME_SITE_STRICT, strict_lax_downgrade_metadata,
       CookieSameSite::STRICT_MODE, true},
      {ContextType::SAME_SITE_LAX, strict_lax_downgrade_metadata,
       CookieSameSite::STRICT_MODE, true},
      {ContextType::SAME_SITE_STRICT, strict_lax_downgrade_metadata,
       CookieSameSite::LAX_MODE, false},
      {ContextType::SAME_SITE_LAX, strict_lax_downgrade_metadata,
       CookieSameSite::LAX_MODE, false},
      {ContextType::SAME_SITE_STRICT, strict_lax_downgrade_metadata,
       CookieSameSite::NO_RESTRICTION, false},
      {ContextType::SAME_SITE_LAX, strict_lax_downgrade_metadata,
       CookieSameSite::NO_RESTRICTION, false},

      // Strict-to-cross downgrade.
      {ContextType::SAME_SITE_STRICT, strict_cross_downgrade_metadata,
       CookieSameSite::STRICT_MODE, true},
      {ContextType::CROSS_SITE, strict_cross_downgrade_metadata,
       CookieSameSite::STRICT_MODE, true},
      {ContextType::SAME_SITE_STRICT, strict_cross_downgrade_metadata,
       CookieSameSite::LAX_MODE, true},
      {ContextType::CROSS_SITE, strict_cross_downgrade_metadata,
       CookieSameSite::LAX_MODE, true},
      {ContextType::SAME_SITE_STRICT, strict_cross_downgrade_metadata,
       CookieSameSite::NO_RESTRICTION, false},
      {ContextType::CROSS_SITE, strict_cross_downgrade_metadata,
       CookieSameSite::NO_RESTRICTION, false},
  };

  for (bool consider_redirects : {true, false}) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(
        features::kCookieSameSiteConsidersRedirectChain, consider_redirects);

    for (CookieAccessSemantics semantics :
         {CookieAccessSemantics::LEGACY, CookieAccessSemantics::NONLEGACY}) {
      // There are no downgrade warnings for undowngraded contexts.
      for (ContextType context_type :
           {ContextType::SAME_SITE_STRICT, ContextType::SAME_SITE_LAX,
            ContextType::SAME_SITE_LAX_METHOD_UNSAFE,
            ContextType::CROSS_SITE}) {
        for (CookieSameSite samesite :
             {CookieSameSite::UNSPECIFIED, CookieSameSite::NO_RESTRICTION,
              CookieSameSite::LAX_MODE, CookieSameSite::STRICT_MODE}) {
          std::unique_ptr<CanonicalCookie> cookie =
              CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "1", "www.example.test", "/test", base::Time::Now(),
                  base::Time(), base::Time(), base::Time(), /*secure=*/true,
                  /*httponly=*/false, samesite, COOKIE_PRIORITY_DEFAULT);

          CookieOptions options;
          options.set_same_site_cookie_context(Context(context_type));

          EXPECT_FALSE(
              cookie
                  ->IncludeForRequestURL(
                      url, options,
                      CookieAccessParams(
                          semantics,
                          /*delegate_treats_url_as_trustworthy=*/false))
                  .status.HasWarningReason(
                      CookieInclusionStatus::
                          WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION));
        }
      }

      for (const auto& test : kTestCases) {
        std::unique_ptr<CanonicalCookie> cookie =
            CanonicalCookie::CreateUnsafeCookieForTesting(
                "A", "1", "www.example.test", "/test", base::Time::Now(),
                base::Time(), base::Time(), base::Time(), /*secure=*/true,
                /*httponly=*/false, test.samesite, COOKIE_PRIORITY_DEFAULT);

        CookieOptions options;
        options.set_same_site_cookie_context(
            Context(test.context_type, test.context_type, test.metadata,
                    test.metadata));
        EXPECT_EQ(
            cookie
                ->IncludeForRequestURL(
                    url, options,
                    CookieAccessParams(
                        semantics,
                        /*delegate_treats_url_as_trustworthy=*/false))
                .status.HasWarningReason(
                    CookieInclusionStatus::
                        WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION),
            test.expect_cross_site_redirect_warning);

        // SameSite warnings not applied if other exclusion reasons apply (e.g.
        // non-https with Secure attribute).
        EXPECT_FALSE(
            cookie
                ->IncludeForRequestURL(
                    insecure_url, options,
                    CookieAccessParams(
                        semantics,
                        /*delegate_treats_url_as_trustworthy=*/false))
                .status.HasWarningReason(
                    CookieInclusionStatus::
                        WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION));
      }
    }
  }
}

// Test that the correct inclusion status is generated when a cookie's source
// scheme does(n't) match the url's.
TEST(CanonicalCookieTest, IncludeForRequestURL_SchemeBoundStatus) {
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  CookieOptions options;
  options.set_same_site_cookie_context(
      CookieOptions::SameSiteCookieContext::MakeInclusive());

  CookieAccessParams params(CookieAccessSemantics::UNKNOWN,
                            /*delegate_treats_url_as_trustworthy=*/false);
  CookieAccessParams trusted_params(
      CookieAccessSemantics::UNKNOWN,
      /*delegate_treats_url_as_trustworthy=*/true);

  GURL secure_url("https://www.example.test:123/");
  GURL insecure_url("http://www.example.test:123/");

  // Specify SameSite=Lax not because we care about SameSite in this test, but
  // rather to prevent warnings that SameSite isn't specified.
  auto secure_cookie = CanonicalCookie::CreateForTesting(
      secure_url, "secure=foobar; SameSite=Lax", creation_time, server_time);
  auto secure_attr_cookie = CanonicalCookie::CreateForTesting(
      secure_url, "secure=foobar; SameSite=Lax; Secure", creation_time,
      server_time);
  auto insecure_cookie = CanonicalCookie::CreateForTesting(
      insecure_url, "insecure=foobar; SameSite=Lax", creation_time,
      server_time);
  // Create a cookie with an unset scheme. This can happen if a cookie was
  // stored in the DB before we began recording source schemes.
  auto unset_cookie = CanonicalCookie::CreateForTesting(
      secure_url, "unset=foobar; SameSite=Lax", creation_time, server_time);
  unset_cookie->SetSourceScheme(CookieSourceScheme::kUnset);

  // When the feature is disabled we should have warnings.
  {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitAndDisableFeature(
        features::kEnableSchemeBoundCookies);

    EXPECT_FALSE(
        secure_cookie->IncludeForRequestURL(secure_url, options, params)
            .status.ShouldWarn());

    EXPECT_TRUE(
        secure_cookie->IncludeForRequestURL(insecure_url, options, params)
            .status.HasWarningReason(
                CookieInclusionStatus::WARN_SCHEME_MISMATCH));

    // If a cookie is already blocked due to the `Secure` attribute then we
    // don't bother warning.
    auto status =
        secure_attr_cookie->IncludeForRequestURL(insecure_url, options, params)
            .status;
    EXPECT_TRUE(
        status.HasExclusionReason(CookieInclusionStatus::EXCLUDE_SECURE_ONLY));
    EXPECT_FALSE(status.ShouldWarn());

    EXPECT_FALSE(
        insecure_cookie->IncludeForRequestURL(insecure_url, options, params)
            .status.ShouldWarn());

    EXPECT_TRUE(
        insecure_cookie->IncludeForRequestURL(secure_url, options, params)
            .status.HasWarningReason(
                CookieInclusionStatus::WARN_SCHEME_MISMATCH));

    // If a url is treated as trustworthy, then it's allowed to access cookies
    // with a secure source scheme.
    EXPECT_FALSE(
        secure_cookie
            ->IncludeForRequestURL(insecure_url, options, trusted_params)
            .status.ShouldWarn());

    // Cookies with an unset source scheme should match any url scheme.
    EXPECT_FALSE(unset_cookie->IncludeForRequestURL(secure_url, options, params)
                     .status.ShouldWarn());
    EXPECT_FALSE(
        unset_cookie->IncludeForRequestURL(insecure_url, options, params)
            .status.ShouldWarn());
  }
  // When the feature is enabled we should have exclusions.
  {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitAndEnableFeature(
        features::kEnableSchemeBoundCookies);

    EXPECT_TRUE(secure_cookie->IncludeForRequestURL(secure_url, options, params)
                    .status.IsInclude());

    EXPECT_TRUE(
        secure_cookie->IncludeForRequestURL(insecure_url, options, params)
            .status.HasExclusionReason(
                CookieInclusionStatus::EXCLUDE_SCHEME_MISMATCH));

    // If a cookie is already blocked due to the `Secure` attribute then we
    // don't bother with our exclusion reason.
    auto status =
        secure_attr_cookie->IncludeForRequestURL(insecure_url, options, params)
            .status;
    EXPECT_TRUE(
        status.HasExclusionReason(CookieInclusionStatus::EXCLUDE_SECURE_ONLY));
    EXPECT_FALSE(status.HasExclusionReason(
        CookieInclusionStatus::EXCLUDE_SCHEME_MISMATCH));

    EXPECT_TRUE(
        insecure_cookie->IncludeForRequestURL(insecure_url, options, params)
            .status.IsInclude());

    EXPECT_TRUE(
        insecure_cookie->IncludeForRequestURL(secure_url, options, params)
            .status.HasExclusionReason(
                CookieInclusionStatus::EXCLUDE_SCHEME_MISMATCH));

    // If a url is treated as trustworthy, then it's allowed to access cookies
    // with a secure source scheme. But we should have a warning indicating
    // this.
    status = secure_cookie
                 ->IncludeForRequestURL(insecure_url, options, trusted_params)
                 .status;
    EXPECT_TRUE(status.IsInclude());
    EXPECT_TRUE(status.HasWarningReason(
        CookieInclusionStatus::WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC));

    // Cookies with an unset source scheme should match any url scheme.
    EXPECT_TRUE(unset_cookie->IncludeForRequestURL(secure_url, options, params)
                    .status.IsInclude());
    EXPECT_TRUE(
        unset_cookie->IncludeForRequestURL(insecure_url, options, params)
            .status.IsInclude());
  }
}

// Test that the correct inclusion status is generated when a cookie's source
// port does(n't) match the url's.
TEST(CanonicalCookieTest, IncludeForRequestURL_PortBoundStatus) {
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  CookieOptions options;
  options.set_same_site_cookie_context(
      CookieOptions::SameSiteCookieContext::MakeInclusive());

  CookieAccessParams params(CookieAccessSemantics::UNKNOWN,
                            /*delegate_treats_url_as_trustworthy=*/false);

  GURL url1("https://www.example.test:443/");
  GURL url2("https://www.example.test:123/");

  // Specify SameSite=Lax not because we care about SameSite in this test, but
  // rather to prevent warnings that SameSite isn't specified.
  auto cookie1 = CanonicalCookie::CreateForTesting(
      url1, "cookie=1; SameSite=Lax", creation_time, server_time);
  auto cookie2 = CanonicalCookie::CreateForTesting(
      url2, "cookie=2; SameSite=Lax", creation_time, server_time);

  // Create a cookie with an unspecified port. This can happen if a cookie was
  // stored in the DB before we began recording source ports.
  auto unspecified_cookie = CanonicalCookie::CreateForTesting(
      url2, "cookie=unspecified; SameSite=Lax", creation_time, server_time);
  unspecified_cookie->SetSourcePort(url::PORT_UNSPECIFIED);

  // When the feature is disabled we should have warnings.
  {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitAndDisableFeature(features::kEnablePortBoundCookies);

    EXPECT_FALSE(cookie1->IncludeForRequestURL(url1, options, params)
                     .status.ShouldWarn());

    EXPECT_TRUE(cookie1->IncludeForRequestURL(url2, options, params)
                    .status.HasWarningReason(
                        CookieInclusionStatus::WARN_PORT_MISMATCH));

    // Cookies with an unspecified port should match any url port.
    EXPECT_FALSE(unspecified_cookie->IncludeForRequestURL(url1, options, params)
                     .status.ShouldWarn());
    EXPECT_FALSE(unspecified_cookie->IncludeForRequestURL(url2, options, params)
                     .status.ShouldWarn());
  }
  // When the feature is enabled we should have exclusions.
  {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitAndEnableFeature(features::kEnablePortBoundCookies);

    EXPECT_TRUE(cookie1->IncludeForRequestURL(url1, options, params)
                    .status.IsInclude());

    EXPECT_TRUE(cookie1->IncludeForRequestURL(url2, options, params)
                    .status.HasExclusionReason(
                        CookieInclusionStatus::EXCLUDE_PORT_MISMATCH));

    // Cookies with an unspecified port should match any url port.
    EXPECT_TRUE(unspecified_cookie->IncludeForRequestURL(url1, options, params)
                    .status.IsInclude());
    EXPECT_TRUE(unspecified_cookie->IncludeForRequestURL(url2, options, params)
                    .status.IsInclude());
  }
}

// Test that domain cookies match any request url port.
TEST(CanonicalCookieTest, IncludeForRequestURL_DomainCookiesPortMatch) {
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  CookieOptions options;
  options.set_same_site_cookie_context(
      CookieOptions::SameSiteCookieContext::MakeInclusive());

  CookieAccessParams params(CookieAccessSemantics::UNKNOWN,
                            /*delegate_treats_url_as_trustworthy=*/false);

  GURL url1("https://www.example.test:443/");
  GURL url2("https://www.example.test:123/");

  // Specify SameSite=Lax not because we care about SameSite in this test, but
  // rather to prevent warnings that SameSite isn't specified.
  auto host_cookie = CanonicalCookie::CreateForTesting(
      url1, "cookie=hostonly; SameSite=Lax", creation_time, server_time);

  auto domain_cookie = CanonicalCookie::CreateForTesting(
      url1, "cookie=domain; SameSite=Lax; Domain=example.test", creation_time,
      server_time);

  // When the feature is disabled we shouldn't get any port mismatch warnings
  // for domain cookies.
  {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitAndDisableFeature(features::kEnablePortBoundCookies);

    EXPECT_FALSE(host_cookie->IncludeForRequestURL(url1, options, params)
                     .status.ShouldWarn());

    EXPECT_FALSE(domain_cookie->IncludeForRequestURL(url1, options, params)
                     .status.ShouldWarn());

    EXPECT_TRUE(host_cookie->IncludeForRequestURL(url2, options, params)
                    .status.HasWarningReason(
                        CookieInclusionStatus::WARN_PORT_MISMATCH));

    EXPECT_FALSE(domain_cookie->IncludeForRequestURL(url2, options, params)
                     .status.ShouldWarn());
  }
  // When the feature is enabled domain cookies should match any url port.
  {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitAndEnableFeature(features::kEnablePortBoundCookies);

    EXPECT_TRUE(host_cookie->IncludeForRequestURL(url1, options, params)
                    .status.IsInclude());

    EXPECT_TRUE(domain_cookie->IncludeForRequestURL(url1, options, params)
                    .status.IsInclude());

    EXPECT_TRUE(host_cookie->IncludeForRequestURL(url2, options, params)
                    .status.HasExclusionReason(
                        CookieInclusionStatus::EXCLUDE_PORT_MISMATCH));

    EXPECT_TRUE(domain_cookie->IncludeForRequestURL(url2, options, params)
                    .status.IsInclude());
  }
}

TEST(CanonicalCookieTest, InsecureCookiesExpiryTimeLimit) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  base::Time future_date = creation_time + base::Days(1);
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kEnableSchemeBoundCookies,
         features::kTimeLimitedInsecureCookies},
        {});
    std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::CreateForTesting(
        url, "A=1; expires=" + HttpUtil::TimeFormatHTTP(future_date),
        creation_time);
    ASSERT_TRUE(cookie);
    // With the feature enabled, expiration time should be limited to 3 hours
    // after creation. Equality check needs to have a second margin due to
    // microsecond rounding causing breakage.
    EXPECT_TRUE(((creation_time + base::Hours(3)) - cookie->ExpiryDate())
                    .FloorToMultiple(base::Seconds(1))
                    .is_zero());
  }
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kEnableSchemeBoundCookies},
        {features::kTimeLimitedInsecureCookies});
    std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::CreateForTesting(
        url, "A=1; expires=" + HttpUtil::TimeFormatHTTP(future_date),
        creation_time);
    ASSERT_TRUE(cookie);
    // With the feature disabled, expiration time should not be limited.
    // Equality check needs to have a second margin due to microsecond rounding
    // causing breakage.
    EXPECT_TRUE((future_date - cookie->ExpiryDate())
                    .FloorToMultiple(base::Seconds(1))
                    .is_zero());
  }
}

TEST(CanonicalCookieTest, MultipleExclusionReasons) {
  GURL url("http://www.not-secure.com/foo");
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  CookieOptions options;
  options.set_exclude_httponly();
  options.set_same_site_cookie_context(CookieOptions::SameSiteCookieContext(
      CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE));

  // Test IncludeForRequestURL()
  // Note: This is a cookie that should never exist normally, because Create()
  // would weed it out.
  auto cookie1 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "name", "value", "other-domain.com", "/bar", creation_time, base::Time(),
      base::Time(), base::Time(), true /* secure */, true /* httponly */,
      CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_DEFAULT);
  EXPECT_THAT(
      cookie1->IncludeForRequestURL(
          url, options,
          CookieAccessParams{CookieAccessSemantics::UNKNOWN,
                             /*delegate_treats_url_as_trustworthy=*/false}),
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
      creation_time, server_time, /*cookie_partition_key=*/std::nullopt,
      CookieSourceType::kUnknown, &create_status);
  ASSERT_FALSE(cookie2);
  EXPECT_TRUE(create_status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX,
       CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));

  // Test IsSetPermittedInContext()
  auto cookie3 = CanonicalCookie::CreateForTesting(
      url, "name=value;HttpOnly;SameSite=Lax", creation_time, server_time);
  ASSERT_TRUE(cookie3);
  EXPECT_THAT(
      cookie3->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
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
  std::optional<base::Time> server_time = std::nullopt;
  std::unique_ptr<CanonicalCookie> cookie(CanonicalCookie::CreateForTesting(
      url, "a=b", creation_time, server_time));
  std::unique_ptr<CanonicalCookie> cookie_different_path(
      CanonicalCookie::CreateForTesting(url, "a=b; path=/foo", creation_time,
                                        server_time));
  std::unique_ptr<CanonicalCookie> cookie_different_value(
      CanonicalCookie::CreateForTesting(url, "a=c", creation_time,
                                        server_time));

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
  std::optional<base::Time> server_time = std::nullopt;
  CookieInclusionStatus status;

  // A __Secure- cookie must be Secure.
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Secure-A=B", creation_time,
                                       server_time,
                                       /*cookie_partition_key=*/std::nullopt,
                                       CookieSourceType::kUnknown, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Secure-A=B; httponly",
                                       creation_time, server_time,
                                       /*cookie_partition_key=*/std::nullopt,
                                       CookieSourceType::kUnknown, &status));
  // (EXCLUDE_HTTP_ONLY would be fine, too)
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // Prefixes are case insensitive.
  EXPECT_FALSE(CanonicalCookie::CreateForTesting(https_url, "__secure-A=C;",
                                                 creation_time, server_time));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::CreateForTesting(https_url, "__SECURE-A=C;",
                                                 creation_time, server_time));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::CreateForTesting(https_url, "__SeCuRe-A=C;",
                                                 creation_time, server_time));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // A typoed prefix does not have to be Secure.
  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "__SecureA=B; Secure", creation_time, server_time));
  EXPECT_TRUE(CanonicalCookie::CreateForTesting(https_url, "__SecureA=C;",
                                                creation_time, server_time));
  EXPECT_TRUE(CanonicalCookie::CreateForTesting(https_url, "_Secure-A=C;",
                                                creation_time, server_time));
  EXPECT_TRUE(CanonicalCookie::CreateForTesting(https_url, "Secure-A=C;",
                                                creation_time, server_time));

  // A __Secure- cookie can't be set on a non-secure origin.
  EXPECT_FALSE(CanonicalCookie::Create(http_url, "__Secure-A=B; Secure",
                                       creation_time, server_time,
                                       /*cookie_partition_key=*/std::nullopt,
                                       CookieSourceType::kUnknown, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // Hidden __Secure- prefixes should be rejected.
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "=__Secure-A=B; Secure",
                                       creation_time, server_time,
                                       /*cookie_partition_key=*/std::nullopt,
                                       CookieSourceType::kUnknown, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "=__Secure-A; Secure",
                                       creation_time, server_time,
                                       /*cookie_partition_key=*/std::nullopt,
                                       CookieSourceType::kUnknown, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // While tricky, this isn't considered hidden and is fine.
  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "A=__Secure-A=B; Secure", creation_time, server_time));
}

TEST(CanonicalCookieTest, HostCookiePrefix) {
  GURL https_url("https://www.example.test");
  GURL http_url("http://www.example.test");
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  std::string domain = https_url.host();
  CookieInclusionStatus status;

  // A __Host- cookie must be Secure.
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Host-A=B;", creation_time,
                                       server_time,
                                       /*cookie_partition_key=*/std::nullopt,
                                       CookieSourceType::kUnknown, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "__Host-A=B; Domain=" + domain + "; Path=/;", creation_time,
      server_time, /*cookie_partition_key=*/std::nullopt,
      CookieSourceType::kUnknown, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "__Host-A=B; Path=/; Secure;", creation_time, server_time));

  // A __Host- cookie must be set from a secure scheme.
  EXPECT_FALSE(CanonicalCookie::Create(
      http_url, "__Host-A=B; Domain=" + domain + "; Path=/; Secure;",
      creation_time, server_time, /*cookie_partition_key=*/std::nullopt,
      CookieSourceType::kUnknown, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "__Host-A=B; Path=/; Secure;", creation_time, server_time));

  // A __Host- cookie can't have a Domain.
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "__Host-A=B; Domain=" + domain + "; Path=/; Secure;",
      creation_time, server_time, /*cookie_partition_key=*/std::nullopt,
      CookieSourceType::kUnknown, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "__Host-A=B; Domain=" + domain + "; Secure;", creation_time,
      server_time, /*cookie_partition_key=*/std::nullopt,
      CookieSourceType::kUnknown, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // A __Host- cookie may have a domain if it's an IP address that matches the
  // URL.
  EXPECT_TRUE(CanonicalCookie::Create(
      GURL("https://127.0.0.1"),
      "__Host-A=B; Domain=127.0.0.1; Path=/; Secure;", creation_time,
      server_time, /*cookie_partition_key=*/std::nullopt,
      CookieSourceType::kUnknown, &status));
  // A __Host- cookie with an IP address domain does not need the domain
  // attribute specified explicitly (just like a normal domain).
  EXPECT_TRUE(CanonicalCookie::Create(
      GURL("https://127.0.0.1"), "__Host-A=B; Domain=; Path=/; Secure;",
      creation_time, server_time, /*cookie_partition_key=*/std::nullopt,
      CookieSourceType::kUnknown, &status));

  // A __Host- cookie must have a Path of "/".
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "__Host-A=B; Path=/foo; Secure;", creation_time, server_time,
      /*cookie_partition_key=*/std::nullopt, CookieSourceType::kUnknown,
      &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Host-A=B; Secure;",
                                       creation_time, server_time,
                                       /*cookie_partition_key=*/std::nullopt,
                                       CookieSourceType::kUnknown, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "__Host-A=B; Secure; Path=/;", creation_time, server_time));

  // Prefixes are case insensitive.
  EXPECT_FALSE(CanonicalCookie::Create(
      http_url, "__host-A=B; Domain=" + domain + "; Path=/;", creation_time,
      server_time, /*cookie_partition_key=*/std::nullopt,
      CookieSourceType::kUnknown, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  EXPECT_FALSE(CanonicalCookie::Create(
      http_url, "__HOST-A=B; Domain=" + domain + "; Path=/;", creation_time,
      server_time, /*cookie_partition_key=*/std::nullopt,
      CookieSourceType::kUnknown, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  EXPECT_FALSE(CanonicalCookie::Create(
      http_url, "__HoSt-A=B; Domain=" + domain + "; Path=/;", creation_time,
      server_time, /*cookie_partition_key=*/std::nullopt,
      CookieSourceType::kUnknown, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // Rules don't apply for a typoed prefix.
  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "__HostA=B; Domain=" + domain + "; Secure;", creation_time,
      server_time));

  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "_Host-A=B; Domain=" + domain + "; Secure;", creation_time,
      server_time));

  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "Host-A=B; Domain=" + domain + "; Secure;", creation_time,
      server_time));

  // Hidden __Host- prefixes should be rejected.
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "=__Host-A=B; Path=/; Secure;", creation_time, server_time,
      /*cookie_partition_key=*/std::nullopt, CookieSourceType::kUnknown,
      &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "=__Host-A; Path=/; Secure;",
                                       creation_time, server_time,
                                       /*cookie_partition_key=*/std::nullopt,
                                       CookieSourceType::kUnknown, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // While tricky, this isn't considered hidden and is fine.
  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "A=__Host-A=B; Path=/; Secure;", creation_time, server_time));
}

TEST(CanonicalCookieTest, CanCreateSecureCookiesFromAnyScheme) {
  GURL http_url("http://www.example.com");
  GURL https_url("https://www.example.com");
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;

  std::unique_ptr<CanonicalCookie> http_cookie_no_secure(
      CanonicalCookie::CreateForTesting(http_url, "a=b", creation_time,
                                        server_time));
  std::unique_ptr<CanonicalCookie> http_cookie_secure(
      CanonicalCookie::CreateForTesting(http_url, "a=b; Secure", creation_time,
                                        server_time));
  std::unique_ptr<CanonicalCookie> https_cookie_no_secure(
      CanonicalCookie::CreateForTesting(https_url, "a=b", creation_time,
                                        server_time));
  std::unique_ptr<CanonicalCookie> https_cookie_secure(
      CanonicalCookie::CreateForTesting(https_url, "a=b; Secure", creation_time,
                                        server_time));

  EXPECT_TRUE(http_cookie_no_secure.get());
  EXPECT_TRUE(http_cookie_secure.get());
  EXPECT_TRUE(https_cookie_no_secure.get());
  EXPECT_TRUE(https_cookie_secure.get());
}

TEST(CanonicalCookieTest, IsCanonical) {
  // Base correct template.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "x.y", "/path", base::Time(), base::Time(),
                  base::Time(), base::Time(), false, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // Newline in name.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A\n", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Carriage return in name.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A\r", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Null character in name.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   std::string("A\0Z", 3), "B", "x.y", "/path", base::Time(),
                   base::Time(), base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Name begins with whitespace.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   " A", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Name ends with whitespace.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A ", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Empty name.  (Note this is against the spec but compatible with other
  // browsers.)
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "", "B", "x.y", "/path", base::Time(), base::Time(),
                  base::Time(), base::Time(), false, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // Space in name
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A C", "B", "x.y", "/path", base::Time(), base::Time(),
                  base::Time(), base::Time(), false, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // Extra space suffixing name.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A ", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // '=' character in name.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A=", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Separator in name.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A;", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // '=' character in value.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B=", "x.y", "/path", base::Time(), base::Time(),
                  base::Time(), base::Time(), false, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // Separator in value.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B;", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Separator in domain.
  //
  // TODO(crbug.com/40256677): The character ';' is permitted in the URL
  // host. That makes IsCanonical() return true here. However, previously,
  // IsCanonical() used to false because ';' was a forbidden character. We need
  // to verify whether this change is acceptable or not.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", ";x.y", "/path", base::Time(), base::Time(),
                  base::Time(), base::Time(), false, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // Garbage in domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "@:&", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Space in domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "x.y ", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Empty domain.  (This is against cookie spec, but needed for Chrome's
  // out-of-spec use of cookies for extensions; see http://crbug.com/730633.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "", "/path", base::Time(), base::Time(),
                  base::Time(), base::Time(), false, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // Path does not start with a "/".
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "x.y", "path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Empty path.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "x.y", "", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // "localhost" as domain.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "localhost", "/path", base::Time(), base::Time(),
                  base::Time(), base::Time(), false, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // non-ASCII domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "\xC3\xA9xample.com", "/path", base::Time(),
                   base::Time(), base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // punycode domain.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "xn--xample-9ua.com", "/path", base::Time(),
                  base::Time(), base::Time(), base::Time(), false, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // Localhost IPv4 address as domain.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "127.0.0.1", "/path", base::Time(), base::Time(),
                  base::Time(), base::Time(), false, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // Simple IPv4 address as domain.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "1.2.3.4", "/path", base::Time(), base::Time(),
                  base::Time(), base::Time(), false, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // period-prefixed IPv4 address as domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", ".1.3.2.4", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // period-prefixed truncated IPv4 address as domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", ".3.2.4", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), true, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // truncated IPv4 address as domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "3.2.4", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), true, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Non-canonical IPv4 address as domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "01.2.03.4", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Non-canonical IPv4 address as domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "16843009", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Non-canonical IPv4 address as domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "0x1010101", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Null IPv6 address as domain.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "[::]", "/path", base::Time(), base::Time(),
                  base::Time(), base::Time(), false, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // Localhost IPv6 address as domain.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "[::1]", "/path", base::Time(), base::Time(),
                  base::Time(), base::Time(), false, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // Fully speced IPv6 address as domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "[2001:0DB8:AC10:FE01:0000:0000:0000:0000]",
                   "/path", base::Time(), base::Time(), base::Time(),
                   base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Zero abbreviated IPv6 address as domain.  Not canonical because of leading
  // zeros & uppercase hex letters.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "[2001:0DB8:AC10:FE01::]", "/path", base::Time(),
                   base::Time(), base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Zero prefixes removed IPv6 address as domain.  Not canoncial because of
  // uppercase hex letters.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "[2001:DB8:AC10:FE01::]", "/path", base::Time(),
                   base::Time(), base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Lowercased hex IPv6 address as domain.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "[2001:db8:ac10:fe01::]", "/path", base::Time(),
                  base::Time(), base::Time(), base::Time(), false, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // Lowercased hex IPv6 address as domain for domain cookie.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", ".[2001:db8:ac10:fe01::]", "/path", base::Time(),
                   base::Time(), base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Incomplete lowercased hex IPv6 address as domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "[2001:db8:ac10:fe01:]", "/path", base::Time(),
                   base::Time(), base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Missing square brackets in IPv6 address as domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "2606:2800:220:1:248:1893:25c8:1946", "/path",
                   base::Time(), base::Time(), base::Time(), base::Time(),
                   false, false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Properly formatted host cookie.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "__Host-A", "B", "x.y", "/", base::Time(), base::Time(),
                  base::Time(), base::Time(), true, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // Insecure host cookie.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "__Host-A", "B", "x.y", "/", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Host cookie with non-null path.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "__Host-A", "B", "x.y", "/path", base::Time(), base::Time(),
                   base::Time(), base::Time(), true, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Host cookie with empty domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "__Host-A", "B", "", "/", base::Time(), base::Time(),
                   base::Time(), base::Time(), true, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Host cookie with period prefixed domain.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "__Host-A", "B", ".x.y", "/", base::Time(), base::Time(),
                   base::Time(), base::Time(), true, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Properly formatted secure cookie.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "__Secure-A", "B", "x.y", "/", base::Time(), base::Time(),
                  base::Time(), base::Time(), true, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  // Insecure secure cookie.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "__Secure-A", "B", "x.y", "/", base::Time(), base::Time(),
                   base::Time(), base::Time(), false, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  // Partitioned attribute used correctly (__Host- prefix).
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "__Host-A", "B", "x.y", "/", base::Time(), base::Time(),
                  base::Time(), base::Time(), /*secure=*/true,
                  /*httponly=*/false, CookieSameSite::UNSPECIFIED,
                  COOKIE_PRIORITY_LOW,
                  CookiePartitionKey::FromURLForTesting(
                      GURL("https://toplevelsite.com")))
                  ->IsCanonical());

  // Partitioned attribute with no __Host- prefix is still valid if it has
  // Secure, Path=/, and no Domain.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "x.y", "/", base::Time(), base::Time(),
                  base::Time(), base::Time(), /*secure=*/true,
                  /*httponly=*/false, CookieSameSite::UNSPECIFIED,
                  COOKIE_PRIORITY_LOW,
                  CookiePartitionKey::FromURLForTesting(
                      GURL("https://toplevelsite.com")))
                  ->IsCanonical());

  // Partitioned attribute invalid, not Secure.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "x.y", "/", base::Time(), base::Time(),
                   base::Time(), base::Time(), /*secure=*/false,
                   /*httponly=*/false, CookieSameSite::UNSPECIFIED,
                   COOKIE_PRIORITY_LOW,
                   CookiePartitionKey::FromURLForTesting(
                       GURL("https://toplevelsite.com")))
                   ->IsCanonical());

  // Partitioned attribute is valid when Path != "/".
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "x.y", "/foo/bar", base::Time(), base::Time(),
                  base::Time(), base::Time(), /*secure=*/true,
                  /*httponly=*/false, CookieSameSite::UNSPECIFIED,
                  COOKIE_PRIORITY_LOW,
                  CookiePartitionKey::FromURLForTesting(
                      GURL("https://toplevelsite.com")))
                  ->IsCanonical());

  // Partitioned attribute is valid when Domain attribute also included.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", ".x.y", "/", base::Time(), base::Time(),
                  base::Time(), base::Time(), /*secure=*/true,
                  /*httponly=*/false, CookieSameSite::UNSPECIFIED,
                  COOKIE_PRIORITY_LOW,
                  CookiePartitionKey::FromURLForTesting(
                      GURL("https://toplevelsite.com")))
                  ->IsCanonical());

  // Hidden cookie prefixes.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "", "__Secure-a=b", "x.y", "/", base::Time(), base::Time(),
                   base::Time(), base::Time(), true, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "", "__Secure-a", "x.y", "/", base::Time(), base::Time(),
                   base::Time(), base::Time(), true, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "", "__Host-a=b", "x.y", "/", base::Time(), base::Time(),
                   base::Time(), base::Time(), true, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "", "__Host-a", "x.y", "/", base::Time(), base::Time(),
                   base::Time(), base::Time(), true, false,
                   CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   ->IsCanonical());

  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "a", "__Secure-a=b", "x.y", "/", base::Time(), base::Time(),
                  base::Time(), base::Time(), true, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());

  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "a", "__Host-a=b", "x.y", "/", base::Time(), base::Time(),
                  base::Time(), base::Time(), true, false,
                  CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                  ->IsCanonical());
}

TEST(CanonicalCookieTest, TestSetCreationDate) {
  auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(),
      base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cookie->CreationDate().is_null());

  base::Time now(base::Time::Now());
  cookie->SetCreationDate(now);
  EXPECT_EQ(now, cookie->CreationDate());
}

TEST(CanonicalCookieTest, TestPrefixHistograms) {
  base::HistogramTester histograms;
  const char kCookiePrefixHistogram[] = "Cookie.CookiePrefix";
  GURL https_url("https://www.example.test");
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;

  EXPECT_FALSE(CanonicalCookie::CreateForTesting(https_url, "__Host-A=B;",
                                                 creation_time, server_time));

  histograms.ExpectBucketCount(kCookiePrefixHistogram, COOKIE_PREFIX_HOST, 1);

  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "__Host-A=B; Path=/; Secure", creation_time, server_time));
  histograms.ExpectBucketCount(kCookiePrefixHistogram, COOKIE_PREFIX_HOST, 2);
  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "__HostA=B; Path=/; Secure", creation_time, server_time));
  histograms.ExpectBucketCount(kCookiePrefixHistogram, COOKIE_PREFIX_HOST, 2);

  EXPECT_FALSE(CanonicalCookie::CreateForTesting(https_url, "__Secure-A=B;",
                                                 creation_time, server_time));

  histograms.ExpectBucketCount(kCookiePrefixHistogram, COOKIE_PREFIX_SECURE, 1);
  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "__Secure-A=B; Path=/; Secure", creation_time, server_time));
  histograms.ExpectBucketCount(kCookiePrefixHistogram, COOKIE_PREFIX_SECURE, 2);
  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "__SecureA=B; Path=/; Secure", creation_time, server_time));
  histograms.ExpectBucketCount(kCookiePrefixHistogram, COOKIE_PREFIX_SECURE, 2);

  // Prefix case variants will also increment the histogram.
  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "__SECURE-A=B; Path=/; Secure", creation_time, server_time));
  histograms.ExpectBucketCount(kCookiePrefixHistogram, COOKIE_PREFIX_SECURE, 3);

  EXPECT_TRUE(CanonicalCookie::CreateForTesting(
      https_url, "__HOST-A=B; Path=/; Secure", creation_time, server_time));
  histograms.ExpectBucketCount(kCookiePrefixHistogram, COOKIE_PREFIX_HOST, 3);
}

TEST(CanonicalCookieTest, TestHasNonASCIIHistograms) {
  base::HistogramTester histograms;
  const char kCookieNonASCIINameHistogram[] = "Cookie.HasNonASCII.Name";
  const char kCookieNonASCIIValueHistogram[] = "Cookie.HasNonASCII.Value";
  const GURL test_url("https://www.example.test");
  int expected_name_true = 0;
  int expected_name_false = 0;
  int expected_value_true = 0;
  int expected_value_false = 0;

  auto create_for_test = [&](const std::string& name,
                             const std::string& value) {
    return CanonicalCookie::CreateForTesting(
        test_url, name + "=" + value, /*creation_time=*/base::Time::Now());
  };

  auto check_histograms = [&]() {
    histograms.ExpectBucketCount(kCookieNonASCIINameHistogram, true,
                                 expected_name_true);
    histograms.ExpectBucketCount(kCookieNonASCIINameHistogram, false,
                                 expected_name_false);
    histograms.ExpectBucketCount(kCookieNonASCIIValueHistogram, true,
                                 expected_value_true);
    histograms.ExpectBucketCount(kCookieNonASCIIValueHistogram, false,
                                 expected_value_false);
  };

  EXPECT_TRUE(create_for_test("foo", "bar"));
  expected_name_false++;
  expected_value_false++;
  check_histograms();

  EXPECT_TRUE(create_for_test("Uni\xf0\x9f\x8d\xaa", "bar"));
  expected_name_true++;
  expected_value_false++;
  check_histograms();

  EXPECT_TRUE(create_for_test("foo", "Uni\xf0\x9f\x8d\xaa"));
  expected_name_false++;
  expected_value_true++;
  check_histograms();

  EXPECT_TRUE(create_for_test("Uni\xf0\x9f\x8d\xaa", "Uni\xf0\x9f\x8d\xaa"));
  expected_name_true++;
  expected_value_true++;
  check_histograms();
}

TEST(CanonicalCookieTest, BuildCookieLine) {
  std::vector<std::unique_ptr<CanonicalCookie>> cookies;
  GURL url("https://example.com/");
  base::Time now = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  MatchCookieLineToVector("", cookies);

  cookies.push_back(
      CanonicalCookie::CreateForTesting(url, "A=B", now, server_time));
  MatchCookieLineToVector("A=B", cookies);
  // Nameless cookies are sent back without a prefixed '='.
  cookies.push_back(
      CanonicalCookie::CreateForTesting(url, "C", now, server_time));
  MatchCookieLineToVector("A=B; C", cookies);
  // Cookies separated by ';'.
  cookies.push_back(
      CanonicalCookie::CreateForTesting(url, "D=E", now, server_time));
  MatchCookieLineToVector("A=B; C; D=E", cookies);
  // BuildCookieLine doesn't reorder the list, it relies on the caller to do so.
  cookies.push_back(CanonicalCookie::CreateForTesting(
      url, "F=G", now - base::Seconds(1), server_time));
  MatchCookieLineToVector("A=B; C; D=E; F=G", cookies);
  // BuildCookieLine doesn't deduplicate.
  cookies.push_back(CanonicalCookie::CreateForTesting(
      url, "D=E", now - base::Seconds(2), server_time));
  MatchCookieLineToVector("A=B; C; D=E; F=G; D=E", cookies);
  // BuildCookieLine should match the spec in the case of an empty name with a
  // value containing an equal sign (even if it currently produces "invalid"
  // cookie lines).
  cookies.push_back(
      CanonicalCookie::CreateForTesting(url, "=H=I", now, server_time));
  MatchCookieLineToVector("A=B; C; D=E; F=G; D=E; H=I", cookies);
}

TEST(CanonicalCookieTest, BuildCookieAttributesLine) {
  std::unique_ptr<CanonicalCookie> cookie;
  GURL url("https://example.com/");
  base::Time now = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;

  cookie = CanonicalCookie::CreateForTesting(url, "A=B", now, server_time);
  EXPECT_EQ("A=B; domain=example.com; path=/",
            CanonicalCookie::BuildCookieAttributesLine(*cookie));
  // Nameless cookies are sent back without a prefixed '='.
  cookie = CanonicalCookie::CreateForTesting(url, "C", now, server_time);
  EXPECT_EQ("C; domain=example.com; path=/",
            CanonicalCookie::BuildCookieAttributesLine(*cookie));
  // BuildCookieAttributesLine should match the spec in the case of an empty
  // name with a value containing an equal sign (even if it currently produces
  // "invalid" cookie lines).
  cookie = CanonicalCookie::CreateForTesting(url, "=H=I", now, server_time);
  EXPECT_EQ("H=I; domain=example.com; path=/",
            CanonicalCookie::BuildCookieAttributesLine(*cookie));
  // BuildCookieAttributesLine should include all attributes.
  cookie = CanonicalCookie::CreateForTesting(
      url,
      "A=B; domain=.example.com; path=/; secure; "
      "httponly; partitioned; samesite=lax",
      now, server_time, CookiePartitionKey::FromURLForTesting(url));
  EXPECT_EQ(
      "A=B; domain=.example.com; path=/; secure; httponly; partitioned; "
      "samesite=lax",
      CanonicalCookie::BuildCookieAttributesLine(*cookie));
}

// Confirm that input arguments are reflected in the output cookie.
TEST(CanonicalCookieTest, CreateSanitizedCookie_Inputs) {
  base::Time two_hours_ago = base::Time::Now() - base::Hours(2);
  base::Time one_hour_ago = base::Time::Now() - base::Hours(1);
  base::Time one_hour_from_now = base::Time::Now() + base::Hours(1);
  CookieInclusionStatus status;
  std::unique_ptr<CanonicalCookie> cc;

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(cc);
  EXPECT_EQ("A", cc->Name());
  EXPECT_EQ("B", cc->Value());
  EXPECT_EQ("www.foo.com", cc->Domain());
  EXPECT_EQ("/foo", cc->Path());
  EXPECT_EQ(base::Time(), cc->CreationDate());
  EXPECT_EQ(base::Time(), cc->LastAccessDate());
  EXPECT_EQ(base::Time(), cc->ExpiryDate());
  EXPECT_FALSE(cc->SecureAttribute());
  EXPECT_FALSE(cc->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cc->SameSite());
  EXPECT_EQ(COOKIE_PRIORITY_MEDIUM, cc->Priority());
  EXPECT_FALSE(cc->IsPartitioned());
  EXPECT_FALSE(cc->IsDomainCookie());
  EXPECT_TRUE(status.IsInclude());

  // Creation date
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      two_hours_ago, base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(cc);
  EXPECT_EQ(two_hours_ago, cc->CreationDate());
  EXPECT_TRUE(status.IsInclude());

  // Last access date
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      two_hours_ago, base::Time(), one_hour_ago, false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(cc);
  EXPECT_EQ(one_hour_ago, cc->LastAccessDate());
  EXPECT_TRUE(status.IsInclude());

  // Expiry
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(cc);
  EXPECT_EQ(one_hour_from_now, cc->ExpiryDate());
  EXPECT_TRUE(status.IsInclude());

  // Secure
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), true /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(cc->SecureAttribute());
  EXPECT_TRUE(status.IsInclude());

  // Httponly
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      true /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(cc->IsHttpOnly());
  EXPECT_TRUE(status.IsInclude());

  // Same site
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(cc);
  EXPECT_EQ(CookieSameSite::LAX_MODE, cc->SameSite());
  EXPECT_TRUE(status.IsInclude());

  // Priority
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW,
      std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(cc);
  EXPECT_EQ(COOKIE_PRIORITY_LOW, cc->Priority());
  EXPECT_TRUE(status.IsInclude());

  // Domain cookie
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", "www.foo.com", "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
  EXPECT_TRUE(status.IsInclude());

  // Partitioned
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/",
      base::Time(), base::Time(), base::Time(), true /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW,
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com")),
      &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(cc->IsPartitioned());
  EXPECT_TRUE(status.IsInclude());
}

// Make sure sanitization and blocking of cookies works correctly.
TEST(CanonicalCookieTest, CreateSanitizedCookie_Logic) {
  base::Time two_hours_ago = base::Time::Now() - base::Hours(2);
  base::Time one_hour_ago = base::Time::Now() - base::Hours(1);
  base::Time one_hour_from_now = base::Time::Now() + base::Hours(1);
  CookieInclusionStatus status;

  // Simple path and domain variations.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", std::string(), "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/bar"), "C", "D", "www.foo.com", "/",
      two_hours_ago, base::Time(), one_hour_ago, false /*secure*/,
      true /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "E", "F", std::string(), std::string(),
      base::Time(), base::Time(), base::Time(), true /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());

  // Test the file:// protocol.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("file:///"), "A", "B", std::string(), "/foo", one_hour_ago,
      one_hour_from_now, base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("file:///home/user/foo.txt"), "A", "B", std::string(), "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("file:///home/user/foo.txt"), "A", "B", "home", "/foo", one_hour_ago,
      one_hour_from_now, base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));

  // Test that malformed attributes fail to set the cookie.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), " A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), /*secure=*/false,
      /*http_only=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER}));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A;", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), /*secure=*/false,
      /*http_only=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER}));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A=", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), /*secure=*/false,
      /*http_only=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER}));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A\x07", "B", std::string(), "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), /*secure=*/false,
      /*http_only=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER}));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", " B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), /*secure=*/false,
      /*http_only=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER}));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "\x0fZ", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), /*secure=*/false,
      /*http_only=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER}));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", "www.foo.com ", "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", "foo.ozzzzzzle", "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", std::string(), "foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE}));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo ",
      base::Time(), base::Time(), base::Time(), /*secure=*/false,
      /*http_only=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER}));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", "%2Efoo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://domaintest.%E3%81%BF%E3%82%93%E3%81%AA"), "A", "B",
      "domaintest.%E3%81%BF%E3%82%93%E3%81%AA", "/foo", base::Time(),
      base::Time(), base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));

  std::unique_ptr<CanonicalCookie> cc;

  // Confirm that setting domain cookies with or without leading periods,
  // or on domains different from the URL's, functions correctly.
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", "www.foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  ASSERT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
  EXPECT_EQ(".www.foo.com", cc->Domain());
  EXPECT_TRUE(status.IsInclude());

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", ".www.foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  ASSERT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
  EXPECT_EQ(".www.foo.com", cc->Domain());
  EXPECT_TRUE(status.IsInclude());

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", ".foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  ASSERT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
  EXPECT_EQ(".foo.com", cc->Domain());
  EXPECT_TRUE(status.IsInclude());

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", ".www2.www.foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_FALSE(cc);
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));

  // Secure/URL Scheme mismatch.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo", base::Time(),
      base::Time(), base::Time(), /*secure=*/true,
      /*http_only=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status));
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(status.HasExactlyWarningReasonsForTesting(
      {CookieInclusionStatus::WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME}));

  // Null creation date/non-null last access date conflict.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo", base::Time(),
      base::Time(), base::Time::Now(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE}));

  // Domain doesn't match URL
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", "www.bar.com", "/", base::Time(),
      base::Time(), base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));

  // Path with unusual characters escaped.
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo\x7F",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  ASSERT_TRUE(cc);
  EXPECT_EQ("/foo%7F", cc->Path());
  EXPECT_TRUE(status.IsInclude());

  // Ensure that all characters get escaped the same on all platforms. This is
  // also useful for visualizing which characters will actually be escaped.
  std::stringstream ss;
  ss << "/";
  for (uint8_t character = 0; character < 0xFF; character++) {
    // Skip any "terminating characters" that CreateSanitizedCookie does not
    // allow to be in `path`.
    if (character == '\0' || character == '\n' || character == '\r' ||
        character == ';') {
      continue;
    }
    ss << character;
  }
  ss << "\xFF";
  std::string initial(ss.str());
  std::string expected =
      "/%01%02%03%04%05%06%07%08%09%0B%0C%0E%0F%10%11%12%13%14%15%16%17%18%19%"
      "1A%1B%1C%1D%1E%1F%20!%22%23$%&'()*+,-./"
      "0123456789:%3C=%3E%3F@ABCDEFGHIJKLMNOPQRSTUVWXYZ[/"
      "]%5E_%60abcdefghijklmnopqrstuvwxyz%7B%7C%7D~%7F%80%81%82%83%84%85%86%87%"
      "88%89%8A%8B%8C%8D%8E%8F%90%91%92%93%94%95%96%97%98%99%9A%9B%9C%9D%9E%9F%"
      "A0%A1%A2%A3%A4%A5%A6%A7%A8%A9%AA%AB%AC%AD%AE%AF%B0%B1%B2%B3%B4%B5%B6%B7%"
      "B8%B9%BA%BB%BC%BD%BE%BF%C0%C1%C2%C3%C4%C5%C6%C7%C8%C9%CA%CB%CC%CD%CE%CF%"
      "D0%D1%D2%D3%D4%D5%D6%D7%D8%D9%DA%DB%DC%DD%DE%DF%E0%E1%E2%E3%E4%E5%E6%E7%"
      "E8%E9%EA%EB%EC%ED%EE%EF%F0%F1%F2%F3%F4%F5%F6%F7%F8%F9%FA%FB%FC%FD%FE%FF";
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), initial,
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  ASSERT_TRUE(cc);
  EXPECT_EQ(expected, cc->Path());
  EXPECT_TRUE(status.IsInclude());

  // Empty name and value.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "", "", std::string(), "/", base::Time(),
      base::Time(), base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_NO_COOKIE_CONTENT}));

  // Check that value can contain an equal sign, even when no name is present.
  // Note that in newer drafts of RFC6265bis, it is specified that a cookie with
  // an empty name and a value containing an equal sign should result in a
  // corresponding cookie line that omits the preceding equal sign. This means
  // that the cookie line won't be deserialized into the original cookie in this
  // case. For now, we'll test for compliance with the spec here, but we aim to
  // collect metrics and hopefully fix this in the spec (and then in
  // CanonicalCookie) at some point.
  // For reference, see: https://github.com/httpwg/http-extensions/pull/1592
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "", "ambiguous=value", std::string(),
      std::string(), base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(cc);
  std::vector<std::unique_ptr<CanonicalCookie>> cookies;
  cookies.push_back(std::move(cc));
  MatchCookieLineToVector("ambiguous=value", cookies);

  // Check that name can't contain an equal sign ("ambiguous=name=value" should
  // correctly be parsed as name: "ambiguous" and value "name=value", so
  // allowing this case would result in cookies that can't serialize correctly).
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "ambiguous=name", "value", std::string(),
      std::string(), base::Time(), base::Time(), base::Time(), /*secure=*/false,
      /*http_only=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER}));

  // A __Secure- cookie must be Secure.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Secure-A", "B", ".www.foo.com", "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Secure-A", "B", ".www.foo.com", "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, false, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // A __Host- cookie must be Secure.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, false, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // A __Host- cookie must have path "/".
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/foo",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // A __Host- cookie must not specify a domain.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", ".www.foo.com", "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // Without __Host- prefix, this is a valid host cookie because it does not
  // specify a domain.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());

  // Without __Host- prefix, this is a valid domain (not host) cookie.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", ".www.foo.com", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());

  // The __Host- prefix should not prevent otherwise-valid host cookies from
  // being accepted.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://127.0.0.1"), "A", "B", std::string(), "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://127.0.0.1"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());

  // Host cookies should not specify domain unless it is an IP address that
  // matches the URL.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://127.0.0.1"), "A", "B", "127.0.0.1", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://127.0.0.1"), "__Host-A", "B", "127.0.0.1", "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());

  // Cookies with hidden prefixes should be rejected.

  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "", "__Host-A=B", "", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "", "__Host-A", "", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "", "__Secure-A=B", "", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "", "__Secure-A", "", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // While tricky, this aren't considered hidden prefixes and should succeed.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "__Host-A=B", "", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());

  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "__Secure-A=B", "", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());

  // Partitioned attribute requires __Host-.
  status = CookieInclusionStatus();
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true /*secure*/, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::optional<CookiePartitionKey>(CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite.com"))),
      &status));
  EXPECT_TRUE(status.IsInclude());
  // No __Host- prefix is still valid if the cookie still has Secure, Path=/,
  // and no Domain.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true /*secure*/, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::optional<CookiePartitionKey>(CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite.com"))),
      &status));
  EXPECT_TRUE(status.IsInclude());
  status = CookieInclusionStatus();
  // Invalid: Not Secure.
  status = CookieInclusionStatus();
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, /*secure=*/false, /*http_only=*/false,
      CookieSameSite::LAX_MODE, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::optional<CookiePartitionKey>(CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite.com"))),
      &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_PARTITIONED}));
  // Invalid: invalid Path.
  status = CookieInclusionStatus();
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foobar",
      two_hours_ago, one_hour_from_now, one_hour_ago, /*secure=*/true,
      /*http_only=*/false, CookieSameSite::NO_RESTRICTION,
      CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::optional<CookiePartitionKey>(CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite.com"))),
      &status));
  EXPECT_TRUE(status.IsInclude());
  // Domain attribute present is still valid.
  status = CookieInclusionStatus();
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", ".foo.com", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, /*secure=*/true, /*http_only=*/false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      std::optional<CookiePartitionKey>(CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite.com"))),
      &status));
  EXPECT_TRUE(status.IsInclude());

  status = CookieInclusionStatus();

  // Check that CreateSanitizedCookie can gracefully fail on inputs that would
  // crash cookie_util::GetCookieDomainWithString due to failing
  // DCHECKs. Specifically, GetCookieDomainWithString requires that if the
  // domain is empty or the URL's host matches the domain, then the URL's host
  // must pass DomainIsHostOnly; it must not begin with a period.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://..."), "A", "B", "...", "/", base::Time(), base::Time(),
      base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://."), "A", "B", std::string(), "/", base::Time(),
      base::Time(), base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://.chromium.org"), "A", "B", ".chromium.org", "/",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));

  // Check that a file URL with an IPv6 host, and matching IPv6 domain, are
  // valid.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("file://[A::]"), "A", "B", "[A::]", "", base::Time(), base::Time(),
      base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());

  // On Windows, URLs beginning with two backslashes are considered file
  // URLs. On other platforms, they are invalid.
  auto double_backslash_ipv6_cookie = CanonicalCookie::CreateSanitizedCookie(
      GURL("\\\\[A::]"), "A", "B", "[A::]", "", base::Time(), base::Time(),
      base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status);
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(double_backslash_ipv6_cookie);
  EXPECT_TRUE(double_backslash_ipv6_cookie->IsCanonical());
  EXPECT_TRUE(status.IsInclude());
#else
  EXPECT_FALSE(double_backslash_ipv6_cookie);
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));
#endif

  // Confirm multiple error types can be set.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL(""), "", "", "", "", base::Time(), base::Time(), base::Time::Now(),
      true /*secure*/, true /*httponly*/, CookieSameSite::STRICT_MODE,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_NO_COOKIE_CONTENT,
       CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE,
       CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));

  // Check that RFC6265bis name + value string length limits are enforced.
  std::string max_name(ParsedCookie::kMaxCookieNamePlusValueSize, 'a');
  std::string max_value(ParsedCookie::kMaxCookieNamePlusValueSize, 'b');
  std::string almost_max_name = max_name.substr(1, std::string::npos);
  std::string almost_max_value = max_value.substr(1, std::string::npos);

  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), max_name, "", std::string(), "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "", max_value, std::string(), "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), almost_max_name, "b", std::string(),
      "/foo", one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "a", almost_max_value, std::string(),
      "/foo", one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status));
  EXPECT_TRUE(status.IsInclude());

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), max_name, "X", std::string(), "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_FALSE(cc);
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE}));

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "X", max_value, std::string(), "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_FALSE(cc);
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE}));

  // Check that the RFC6265bis attribute value size limits apply to the Path
  // attribute value.
  std::string almost_max_path(ParsedCookie::kMaxCookieAttributeValueSize - 1,
                              'c');
  std::string max_path = "/" + almost_max_path;
  std::string too_long_path = "/X" + almost_max_path;

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com" + max_path), "name", "value", std::string(),
      max_path, one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(cc);
  EXPECT_EQ(max_path, cc->Path());
  EXPECT_TRUE(status.IsInclude());

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/path-attr-from-url/"), "name", "value",
      std::string(), too_long_path, one_hour_ago, one_hour_from_now,
      base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status);
  EXPECT_FALSE(cc);
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE}));

  // Check that length limits on the Path attribute value are not enforced
  // in the case where no Path attribute is specified and the path value is
  // implicitly set from the URL.
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com" + too_long_path + "/"), "name", "value",
      std::string(), std::string(), one_hour_ago, one_hour_from_now,
      base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(cc);
  EXPECT_EQ(too_long_path, cc->Path());
  EXPECT_TRUE(status.IsInclude());

  // The Path attribute value gets URL-encoded, so ensure that the size
  // limit is enforced after this (to avoid setting cookies where the Path
  // attribute value would otherwise exceed the lengths specified in the
  // RFC).
  std::string expanding_path(ParsedCookie::kMaxCookieAttributeValueSize / 2,
                             '#');
  expanding_path = "/" + expanding_path;

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/path-attr-from-url/"), "name", "value",
      std::string(), expanding_path, one_hour_ago, one_hour_from_now,
      base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, &status);
  EXPECT_FALSE(cc);
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE}));

  // Check that the RFC6265bis attribute value size limits apply to the Domain
  // attribute value.
  std::string max_domain(ParsedCookie::kMaxCookieAttributeValueSize, 'd');
  max_domain.replace(ParsedCookie::kMaxCookieAttributeValueSize - 4, 4, ".com");
  std::string too_long_domain = "x" + max_domain;

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://" + max_domain + "/"), "name", "value", max_domain, "/",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(cc);
  EXPECT_EQ(max_domain, cc->DomainWithoutDot());
  EXPECT_TRUE(status.IsInclude());
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.domain-from-url.com/"), "name", "value", too_long_domain,
      "/", one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_FALSE(cc);
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE}));
  // Check that length limits on the Domain attribute value are not enforced
  // in the case where no Domain attribute is specified and the domain value
  // is implicitly set from the URL.
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://" + too_long_domain + "/"), "name", "value", std::string(),
      "/", one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(cc);
  EXPECT_EQ(too_long_domain, cc->DomainWithoutDot());
  EXPECT_TRUE(status.IsInclude());
}

// Regression test for https://crbug.com/362535230.
TEST(CanonicalCookieTest, CreateSanitizedCookie_NoncanonicalDomain) {
  CookieInclusionStatus status;

  std::unique_ptr<CanonicalCookie> cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("foo://LOCALhost"), "name", "value", /*domain=*/"", /*path=*/"",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT, std::nullopt /*partition_key*/, &status);
  EXPECT_TRUE(status.IsInclude());
  ASSERT_TRUE(cc);
  EXPECT_TRUE(cc->IsCanonical());
  EXPECT_EQ(cc->Domain(), "localhost");
}

// Make sure that the source scheme and port are set correctly for cookies that
// are marked as "Secure".
TEST(CanonicalCookieTest, Create_SourceSchemePort) {
  GURL secure_url("https://example.com");
  GURL insecure_url("http://example.com");
  GURL insecure_url_custom_port("http://example.com:123");

  CookieInclusionStatus status;

  std::unique_ptr<CanonicalCookie> cc;

  // A secure url doesn't need "Secure" to have a source scheme of secure
  cc = CanonicalCookie::Create(secure_url, "a=b; SameSite=Lax",
                               base::Time::Now(), std::nullopt, std::nullopt,

                               CookieSourceType::kUnknown, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_FALSE(status.ShouldWarn());
  EXPECT_FALSE(cc->SecureAttribute());
  EXPECT_EQ(cc->SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cc->SourcePort(), 443);

  // But having "Secure" shouldn't change anything
  cc = CanonicalCookie::Create(secure_url, "a=b; SameSite=Lax; Secure",
                               base::Time::Now(), std::nullopt, std::nullopt,
                               CookieSourceType::kUnknown, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_FALSE(status.ShouldWarn());
  EXPECT_TRUE(cc->SecureAttribute());
  EXPECT_EQ(cc->SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cc->SourcePort(), 443);

  // An insecure url without "Secure" should get a non-secure source scheme and
  // a default port.
  cc = CanonicalCookie::Create(insecure_url, "a=b; SameSite=Lax",
                               base::Time::Now(), std::nullopt, std::nullopt,
                               CookieSourceType::kUnknown, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_FALSE(status.ShouldWarn());
  EXPECT_FALSE(cc->SecureAttribute());
  EXPECT_EQ(cc->SourceScheme(), CookieSourceScheme::kNonSecure);
  EXPECT_EQ(cc->SourcePort(), 80);

  // An insecure url with "Secure" should get a secure source scheme and
  // modified port. It should also get a warning that a secure source scheme was
  // tentatively allowed.
  cc = CanonicalCookie::Create(insecure_url, "a=b; SameSite=Lax; Secure",
                               base::Time::Now(), std::nullopt, std::nullopt,
                               CookieSourceType::kUnknown, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(status.HasExactlyWarningReasonsForTesting(
      {CookieInclusionStatus::WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME}));
  EXPECT_TRUE(cc->SecureAttribute());
  EXPECT_EQ(cc->SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cc->SourcePort(), 443);

  // An insecure url with a non-default port without "Secure" should get a
  // non-secure source scheme and keep its port.
  cc = CanonicalCookie::Create(insecure_url_custom_port, "a=b; SameSite=Lax",
                               base::Time::Now(), std::nullopt, std::nullopt,
                               CookieSourceType::kUnknown, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_FALSE(status.ShouldWarn());
  EXPECT_FALSE(cc->SecureAttribute());
  EXPECT_EQ(cc->SourceScheme(), CookieSourceScheme::kNonSecure);
  EXPECT_EQ(cc->SourcePort(), 123);

  // An insecure url with a non-default port with "Secure" should get a secure
  // source scheme and keep its port. It should also get a warning that a secure
  // source scheme was tentatively allowed.
  cc = CanonicalCookie::Create(
      insecure_url_custom_port, "a=b; SameSite=Lax; Secure", base::Time::Now(),
      std::nullopt, std::nullopt, CookieSourceType::kUnknown, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(status.HasExactlyWarningReasonsForTesting(
      {CookieInclusionStatus::WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME}));
  EXPECT_TRUE(cc->SecureAttribute());
  EXPECT_EQ(cc->SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cc->SourcePort(), 123);
}

// Make sure that the source scheme and port are set correctly for cookies that
// are marked as "Secure".
TEST(CanonicalCookieTest, CreateSanitizedCookie_SourceSchemePort) {
  GURL secure_url("https://example.com");
  GURL insecure_url("http://example.com");
  GURL insecure_url_custom_port("http://example.com:123");

  CookieInclusionStatus status;

  std::unique_ptr<CanonicalCookie> cc;

  // A secure url doesn't need "Secure" to have a source scheme of secure
  cc = CanonicalCookie::CreateSanitizedCookie(
      secure_url, "a", "b", "example.com", "", base::Time(), base::Time(),
      base::Time(), /*secure=*/false, /*http_only=*/false,
      CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_FALSE(status.ShouldWarn());
  EXPECT_FALSE(cc->SecureAttribute());
  EXPECT_EQ(cc->SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cc->SourcePort(), 443);

  // But having "Secure" shouldn't change anything
  cc = CanonicalCookie::CreateSanitizedCookie(
      secure_url, "a", "b", "example.com", "", base::Time(), base::Time(),
      base::Time(), /*secure=*/true, /*http_only=*/false,
      CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_FALSE(status.ShouldWarn());
  EXPECT_TRUE(cc->SecureAttribute());
  EXPECT_EQ(cc->SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cc->SourcePort(), 443);

  // An insecure url without "Secure" should get a non-secure source scheme and
  // a default port.
  cc = CanonicalCookie::CreateSanitizedCookie(
      insecure_url, "a", "b", "example.com", "", base::Time(), base::Time(),
      base::Time(), /*secure=*/false, /*http_only=*/false,
      CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_FALSE(status.ShouldWarn());
  EXPECT_FALSE(cc->SecureAttribute());
  EXPECT_EQ(cc->SourceScheme(), CookieSourceScheme::kNonSecure);
  EXPECT_EQ(cc->SourcePort(), 80);

  // An insecure url with "Secure" should get a secure source scheme and
  // modified port. It should also get a warning that a secure source scheme was
  // tentatively allowed.
  cc = CanonicalCookie::CreateSanitizedCookie(
      insecure_url, "a", "b", "example.com", "", base::Time(), base::Time(),
      base::Time(), /*secure=*/true, /*http_only=*/false,
      CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(status.HasExactlyWarningReasonsForTesting(
      {CookieInclusionStatus::WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME}));
  EXPECT_TRUE(cc->SecureAttribute());
  EXPECT_EQ(cc->SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cc->SourcePort(), 443);

  // An insecure url with a non-default port without "Secure" should get a
  // non-secure source scheme and keep its port.
  cc = CanonicalCookie::CreateSanitizedCookie(
      insecure_url_custom_port, "a", "b", "example.com", "", base::Time(),
      base::Time(), base::Time(), /*secure=*/false, /*http_only=*/false,
      CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_FALSE(status.ShouldWarn());
  EXPECT_FALSE(cc->SecureAttribute());
  EXPECT_EQ(cc->SourceScheme(), CookieSourceScheme::kNonSecure);
  EXPECT_EQ(cc->SourcePort(), 123);

  // An insecure url with a non-default port with "Secure" should get a secure
  // source scheme and keep its port. It should also get a warning that a secure
  // source scheme was tentatively allowed.
  cc = CanonicalCookie::CreateSanitizedCookie(
      insecure_url_custom_port, "a", "b", "example.com", "", base::Time(),
      base::Time(), base::Time(), /*secure=*/true, /*http_only=*/false,
      CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, &status);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(status.IsInclude());
  EXPECT_TRUE(status.HasExactlyWarningReasonsForTesting(
      {CookieInclusionStatus::WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME}));
  EXPECT_TRUE(cc->SecureAttribute());
  EXPECT_EQ(cc->SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cc->SourcePort(), 123);
}

TEST(CanonicalCookieTest, FromStorage) {
  base::Time two_hours_ago = base::Time::Now() - base::Hours(2);
  base::Time one_hour_ago = base::Time::Now() - base::Hours(1);
  base::Time one_hour_from_now = base::Time::Now() + base::Hours(1);

  std::unique_ptr<CanonicalCookie> cc = CanonicalCookie::FromStorage(
      "A", "B", "www.foo.com", "/bar", two_hours_ago, one_hour_from_now,
      one_hour_ago, one_hour_ago, false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, CookieSourceScheme::kSecure, 87,
      CookieSourceType::kUnknown);
  EXPECT_TRUE(cc);
  EXPECT_EQ("A", cc->Name());
  EXPECT_EQ("B", cc->Value());
  EXPECT_EQ("www.foo.com", cc->Domain());
  EXPECT_EQ("/bar", cc->Path());
  EXPECT_EQ(two_hours_ago, cc->CreationDate());
  EXPECT_EQ(one_hour_ago, cc->LastAccessDate());
  EXPECT_EQ(one_hour_from_now, cc->ExpiryDate());
  EXPECT_EQ(one_hour_ago, cc->LastUpdateDate());
  EXPECT_FALSE(cc->SecureAttribute());
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
      one_hour_ago, one_hour_ago, false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, CookieSourceScheme::kSecure, 80,
      CookieSourceType::kUnknown));

  // If the port information gets corrupted out of the valid range
  // FromStorage() should result in a PORT_INVALID.
  std::unique_ptr<CanonicalCookie> cc2 = CanonicalCookie::FromStorage(
      "A", "B", "www.foo.com", "/bar", two_hours_ago, one_hour_from_now,
      one_hour_ago, one_hour_ago, false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, CookieSourceScheme::kSecure, 80000,
      CookieSourceType::kUnknown);

  EXPECT_EQ(cc2->SourcePort(), url::PORT_INVALID);

  // Test port edge cases: unspecified.
  std::unique_ptr<CanonicalCookie> cc3 = CanonicalCookie::FromStorage(
      "A", "B", "www.foo.com", "/bar", two_hours_ago, one_hour_from_now,
      one_hour_ago, one_hour_ago, false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, CookieSourceScheme::kSecure,
      url::PORT_UNSPECIFIED, CookieSourceType::kUnknown);
  EXPECT_EQ(cc3->SourcePort(), url::PORT_UNSPECIFIED);

  // Test port edge cases: invalid.
  std::unique_ptr<CanonicalCookie> cc4 = CanonicalCookie::FromStorage(
      "A", "B", "www.foo.com", "/bar", two_hours_ago, one_hour_from_now,
      one_hour_ago, one_hour_ago, false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, CookieSourceScheme::kSecure,
      url::PORT_INVALID, CookieSourceType::kUnknown);
  EXPECT_EQ(cc4->SourcePort(), url::PORT_INVALID);
}

TEST(CanonicalCookieTest, IsSetPermittedInContext) {
  GURL url("https://www.example.com/test");
  GURL insecure_url("http://www.example.com/test");
  base::Time current_time = base::Time::Now();

  auto cookie_scriptable = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), base::Time(), true /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT);
  auto cookie_httponly = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), base::Time(), true /*secure*/, true /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT);

  CookieOptions context_script;
  CookieOptions context_network;
  context_network.set_include_httponly();

  EXPECT_THAT(
      cookie_scriptable->IsSetPermittedInContext(
          GURL("file://foo/bar.txt"), context_network,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(
          CookieInclusionStatus::MakeFromReasonsForTesting({
              CookieInclusionStatus::EXCLUDE_NONCOOKIEABLE_SCHEME,
              CookieInclusionStatus::EXCLUDE_SECURE_ONLY,
              CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH,
          }),
          _, _, false));

  EXPECT_THAT(
      cookie_scriptable->IsSetPermittedInContext(
          insecure_url, context_network,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(
          CookieInclusionStatus::MakeFromReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_SECURE_ONLY}),
          _, _, false));
  EXPECT_THAT(
      cookie_scriptable->IsSetPermittedInContext(
          url, context_network,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(IsInclude(), _, _, true));
  EXPECT_THAT(
      cookie_scriptable->IsSetPermittedInContext(
          url, context_script,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(IsInclude(), _, _, true));

  EXPECT_THAT(
      cookie_httponly->IsSetPermittedInContext(
          url, context_network,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(IsInclude(), _, _, true));
  EXPECT_THAT(
      cookie_httponly->IsSetPermittedInContext(
          url, context_script,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(
          CookieInclusionStatus::MakeFromReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_HTTP_ONLY}),
          _, _, true));

  EXPECT_THAT(
      cookie_scriptable->IsSetPermittedInContext(
          GURL("https://www.badexample.com/test"), context_script,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(
          CookieInclusionStatus::MakeFromReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH}),
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
            base::Time(), base::Time(), true /*secure*/, false /*httponly*/,
            CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT);

    EXPECT_THAT(
        cookie_same_site_unrestricted->IsSetPermittedInContext(
            url, context_cross_site,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */
                               ),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unrestricted->IsSetPermittedInContext(
            url, context_same_site_lax,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */
                               ),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_unrestricted->IsSetPermittedInContext(
            url, context_same_site_strict,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */
                               ),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));

    {
      // Schemeful Same-Site disabled.
      base::test::ScopedFeatureList feature_list;
      feature_list.InitAndDisableFeature(features::kSchemefulSameSite);

      EXPECT_THAT(
          cookie_same_site_unrestricted->IsSetPermittedInContext(
              url, context_same_site_strict_to_lax,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasSchemefulDowngradeWarning())), _, _,
              true));
      EXPECT_THAT(
          cookie_same_site_unrestricted->IsSetPermittedInContext(
              url, context_same_site_strict_to_cross,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasSchemefulDowngradeWarning())), _, _,
              true));
      EXPECT_THAT(
          cookie_same_site_unrestricted->IsSetPermittedInContext(
              url, context_same_site_lax_to_cross,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasSchemefulDowngradeWarning())), _, _,
              true));
    }
    {
      // Schemeful Same-Site enabled.
      base::test::ScopedFeatureList feature_list;
      feature_list.InitAndEnableFeature(features::kSchemefulSameSite);

      EXPECT_THAT(
          cookie_same_site_unrestricted->IsSetPermittedInContext(
              url, context_same_site_strict_to_lax,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasSchemefulDowngradeWarning())), _, _,
              true));
      EXPECT_THAT(
          cookie_same_site_unrestricted->IsSetPermittedInContext(
              url, context_same_site_strict_to_cross,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasSchemefulDowngradeWarning())), _, _,
              true));
      EXPECT_THAT(
          cookie_same_site_unrestricted->IsSetPermittedInContext(
              url, context_same_site_lax_to_cross,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasSchemefulDowngradeWarning())), _, _,
              true));
    }
  }

  {
    auto cookie_same_site_lax = CanonicalCookie::CreateUnsafeCookieForTesting(
        "A", "2", "www.example.com", "/test", current_time, base::Time(),
        base::Time(), base::Time(), true /*secure*/, false /*httponly*/,
        CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT);

    EXPECT_THAT(
        cookie_same_site_lax->IsSetPermittedInContext(
            url, context_cross_site,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */
                               ),
            kCookieableSchemes),
        MatchesCookieAccessResult(
            CookieInclusionStatus::MakeFromReasonsForTesting(
                {CookieInclusionStatus::EXCLUDE_SAMESITE_LAX}),
            _, _, true));
    EXPECT_THAT(
        cookie_same_site_lax->IsSetPermittedInContext(
            url, context_same_site_lax,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */
                               ),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_lax->IsSetPermittedInContext(
            url, context_same_site_strict,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */
                               ),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));

    {
      // Schemeful Same-Site disabled.
      base::test::ScopedFeatureList feature_list;
      feature_list.InitAndDisableFeature(features::kSchemefulSameSite);

      EXPECT_THAT(
          cookie_same_site_lax->IsSetPermittedInContext(
              url, context_same_site_strict_to_lax,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasSchemefulDowngradeWarning())), _, _,
              true));
      EXPECT_THAT(
          cookie_same_site_lax->IsSetPermittedInContext(
              url, context_same_site_strict_to_cross,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(),
                    HasWarningReason(
                        CookieInclusionStatus::
                            WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE)),
              _, _, true));
      EXPECT_THAT(
          cookie_same_site_lax->IsSetPermittedInContext(
              url, context_same_site_lax_to_cross,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(
                  IsInclude(),
                  HasWarningReason(CookieInclusionStatus::
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
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasSchemefulDowngradeWarning())), _, _,
              true));
      EXPECT_THAT(
          cookie_same_site_lax->IsSetPermittedInContext(
              url, context_same_site_strict_to_cross,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
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
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
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
            base::Time(), base::Time(), true /*secure*/, false /*httponly*/,
            CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_DEFAULT);

    // TODO(morlovich): Do compatibility testing on whether set of strict in lax
    // context really should be accepted.
    EXPECT_THAT(
        cookie_same_site_strict->IsSetPermittedInContext(
            url, context_cross_site,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */
                               ),
            kCookieableSchemes),
        MatchesCookieAccessResult(
            CookieInclusionStatus::MakeFromReasonsForTesting(
                {CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT}),
            _, _, true));
    EXPECT_THAT(
        cookie_same_site_strict->IsSetPermittedInContext(
            url, context_same_site_lax,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */
                               ),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));
    EXPECT_THAT(
        cookie_same_site_strict->IsSetPermittedInContext(
            url, context_same_site_strict,
            CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                               false /* delegate_treats_url_as_trustworthy */
                               ),
            kCookieableSchemes),
        MatchesCookieAccessResult(IsInclude(), _, _, true));

    {
      // Schemeful Same-Site disabled.
      base::test::ScopedFeatureList feature_list;
      feature_list.InitAndDisableFeature(features::kSchemefulSameSite);

      EXPECT_THAT(
          cookie_same_site_strict->IsSetPermittedInContext(
              url, context_same_site_strict_to_lax,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasSchemefulDowngradeWarning())), _, _,
              true));
      EXPECT_THAT(
          cookie_same_site_strict->IsSetPermittedInContext(
              url, context_same_site_strict_to_cross,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(),
                    HasWarningReason(
                        CookieInclusionStatus::
                            WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE)),
              _, _, true));
      EXPECT_THAT(
          cookie_same_site_strict->IsSetPermittedInContext(
              url, context_same_site_lax_to_cross,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
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
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(
              AllOf(IsInclude(), Not(HasSchemefulDowngradeWarning())), _, _,
              true));
      EXPECT_THAT(
          cookie_same_site_strict->IsSetPermittedInContext(
              url, context_same_site_strict_to_cross,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
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
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
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

      EXPECT_THAT(
          cookie_same_site_strict->IsSetPermittedInContext(
              url, context_same_site_strict_to_cross,
              CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(Not(IsInclude()), _, _, true));
      EXPECT_THAT(
          cookie_same_site_strict->IsSetPermittedInContext(
              url, context_same_site_strict_to_cross,
              CookieAccessParams(CookieAccessSemantics::NONLEGACY,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(Not(IsInclude()), _, _, true));
      // LEGACY semantics should allow cookies which Schemeful Same-Site would
      // normally block.
      EXPECT_THAT(
          cookie_same_site_strict->IsSetPermittedInContext(
              url, context_same_site_strict_to_cross,
              CookieAccessParams(CookieAccessSemantics::LEGACY,
                                 false /* delegate_treats_url_as_trustworthy */
                                 ),
              kCookieableSchemes),
          MatchesCookieAccessResult(IsInclude(), _, _, true));
    }
  }

  // Behavior of UNSPECIFIED depends on CookieAccessSemantics.
  auto cookie_same_site_unspecified =
      CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "2", "www.example.com", "/test", current_time, base::Time(),
          base::Time(), base::Time(), true /*secure*/, false /*httponly*/,
          CookieSameSite::UNSPECIFIED, COOKIE_PRIORITY_DEFAULT);

  EXPECT_THAT(
      cookie_same_site_unspecified->IsSetPermittedInContext(
          url, context_cross_site,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
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
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(IsInclude(), _, _, true));
  EXPECT_THAT(
      cookie_same_site_unspecified->IsSetPermittedInContext(
          url, context_same_site_strict,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(IsInclude(), _, _, true));
  EXPECT_THAT(
      cookie_same_site_unspecified->IsSetPermittedInContext(
          url, context_cross_site,
          CookieAccessParams(CookieAccessSemantics::LEGACY,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(IsInclude(), _, _, true));
  EXPECT_THAT(
      cookie_same_site_unspecified->IsSetPermittedInContext(
          url, context_same_site_lax,
          CookieAccessParams(CookieAccessSemantics::LEGACY,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(IsInclude(), _, _, true));
  EXPECT_THAT(
      cookie_same_site_unspecified->IsSetPermittedInContext(
          url, context_same_site_strict,
          CookieAccessParams(CookieAccessSemantics::LEGACY,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(IsInclude(), _, _, true));
  EXPECT_THAT(
      cookie_same_site_unspecified->IsSetPermittedInContext(
          url, context_cross_site,
          CookieAccessParams(CookieAccessSemantics::NONLEGACY,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
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
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(IsInclude(), _, _, true));
  EXPECT_THAT(
      cookie_same_site_unspecified->IsSetPermittedInContext(
          url, context_same_site_strict,
          CookieAccessParams(CookieAccessSemantics::NONLEGACY,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(IsInclude(), _, _, true));

  // Test IsSetPermittedInContext successfully chains warnings by passing
  // in a CookieAccessResult and expecting the result to have a
  // WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE
  CookieInclusionStatus status;
  std::string long_path(ParsedCookie::kMaxCookieAttributeValueSize, 'a');

  std::unique_ptr<CanonicalCookie> cookie_with_long_path =
      CanonicalCookie::Create(url, "A=B; Path=/" + long_path, current_time,
                              std::nullopt, std::nullopt,
                              CookieSourceType::kUnknown, &status);
  CookieAccessResult cookie_access_result(status);
  CookieOptions cookie_with_long_path_options;
  EXPECT_THAT(
      cookie_with_long_path->IsSetPermittedInContext(
          url, cookie_with_long_path_options,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes, cookie_access_result),
      MatchesCookieAccessResult(
          HasWarningReason(
              CookieInclusionStatus::WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE),
          _, _, _));
}

TEST(CanonicalCookieTest, IsSetPermittedEffectiveSameSite) {
  GURL url("http://www.example.com/test");
  base::Time current_time = base::Time::Now();
  CookieOptions options;

  // Test IsSetPermitted CookieEffectiveSameSite for
  // CanonicalCookie with CookieSameSite::NO_RESTRICTION.
  auto cookie_no_restriction = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), base::Time(), true /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT);

  EXPECT_THAT(
      cookie_no_restriction->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(_, CookieEffectiveSameSite::NO_RESTRICTION, _,
                                false));

  // Test IsSetPermitted CookieEffectiveSameSite for
  // CanonicalCookie with CookieSameSite::LAX_MODE.
  auto cookie_lax = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), base::Time(), true /*secure*/, false /*httponly*/,
      CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT);

  EXPECT_THAT(
      cookie_lax->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(_, CookieEffectiveSameSite::LAX_MODE, _,
                                false));

  // Test IsSetPermitted CookieEffectiveSameSite for
  // CanonicalCookie with CookieSameSite::STRICT_MODE.
  auto cookie_strict = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), base::Time(), true /*secure*/, false /*httponly*/,
      CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_DEFAULT);

  EXPECT_THAT(
      cookie_strict->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(_, CookieEffectiveSameSite::STRICT_MODE, _,
                                false));

  // Test IsSetPermitted CookieEffectiveSameSite for
  // CanonicalCookie with CookieSameSite::UNSPECIFIED.
  base::Time creation_time = base::Time::Now() - (kLaxAllowUnsafeMaxAge * 4);
  auto cookie_old_unspecified = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", creation_time, base::Time(),
      base::Time(), base::Time(), true /*secure*/, false /*httponly*/,
      CookieSameSite::UNSPECIFIED, COOKIE_PRIORITY_DEFAULT);
  auto cookie_unspecified = CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), base::Time(), true /*secure*/, false /*httponly*/,
      CookieSameSite::UNSPECIFIED, COOKIE_PRIORITY_DEFAULT);

  EXPECT_THAT(
      cookie_old_unspecified->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(_, CookieEffectiveSameSite::LAX_MODE, _,
                                false));

  EXPECT_THAT(
      cookie_unspecified->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::UNKNOWN,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(
          _, CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE, _, false));

  EXPECT_THAT(
      cookie_unspecified->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::NONLEGACY,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
          kCookieableSchemes),
      MatchesCookieAccessResult(
          _, CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE, _, false));

  EXPECT_THAT(
      cookie_unspecified->IsSetPermittedInContext(
          url, options,
          CookieAccessParams(CookieAccessSemantics::LEGACY,
                             false /* delegate_treats_url_as_trustworthy */
                             ),
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
         }) {
      auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "2", "www.example.com", "/test", current_time, base::Time(),
          base::Time(), base::Time(), secure, false /*httponly*/, same_site,
          COOKIE_PRIORITY_DEFAULT);

      for (bool delegate_treats_url_as_trustworthy : {false, true}) {
        for (CookieAccessSemantics access_semantics : {
                 CookieAccessSemantics::UNKNOWN,
                 CookieAccessSemantics::LEGACY,
                 CookieAccessSemantics::NONLEGACY,
             }) {
          EXPECT_THAT(
              cookie->IsSetPermittedInContext(
                  url, options,
                  CookieAccessParams(access_semantics,
                                     delegate_treats_url_as_trustworthy),
                  kCookieableSchemes),
              MatchesCookieAccessResult(_, _, _, true));
          EXPECT_THAT(
              cookie->IsSetPermittedInContext(
                  insecure_url, options,
                  CookieAccessParams(access_semantics,
                                     delegate_treats_url_as_trustworthy),
                  kCookieableSchemes),
              MatchesCookieAccessResult(_, _, _,
                                        delegate_treats_url_as_trustworthy));
          EXPECT_THAT(
              cookie->IsSetPermittedInContext(
                  localhost_url, options,
                  CookieAccessParams(access_semantics,
                                     delegate_treats_url_as_trustworthy),
                  kCookieableSchemes),
              MatchesCookieAccessResult(_, _, _, true));
        }
      }
    }
  }
}

TEST(CanonicalCookieTest, IsSetPermitted_SameSiteNone_Metrics) {
  constexpr bool delegate_treats_url_as_trustworthy = false;
  const base::Time now = base::Time::Now();
  const auto make_cookie = [now](CookieSameSite same_site) {
    return CanonicalCookie::CreateUnsafeCookieForTesting(
        "A", "1", "www.example.com", "/test", now, base::Time(), base::Time(),
        base::Time(), true /* secure */, false /*httponly*/, same_site,
        COOKIE_PRIORITY_DEFAULT);
  };
  GURL url("https://www.example.com/test");

  const std::unique_ptr<CanonicalCookie> same_site_none_cookie =
      make_cookie(CookieSameSite::NO_RESTRICTION);
  const std::unique_ptr<CanonicalCookie> same_site_lax_cookie =
      make_cookie(CookieSameSite::LAX_MODE);
  const std::unique_ptr<CanonicalCookie> same_site_strict_cookie =
      make_cookie(CookieSameSite::STRICT_MODE);
  CookieOptions options;

  options.set_same_site_cookie_context(CookieOptions::SameSiteCookieContext(
      CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE));
  EXPECT_THAT(same_site_none_cookie->IsSetPermittedInContext(
                  url, options,
                  CookieAccessParams(CookieAccessSemantics::LEGACY,
                                     delegate_treats_url_as_trustworthy),
                  kCookieableSchemes),
              MatchesCookieAccessResult(CookieInclusionStatus(), _, _, true));
  EXPECT_THAT(same_site_lax_cookie->IsSetPermittedInContext(
                  url, options,
                  CookieAccessParams(CookieAccessSemantics::LEGACY,
                                     delegate_treats_url_as_trustworthy),
                  kCookieableSchemes),
              MatchesCookieAccessResult(Not(net::IsInclude()), _, _, true));
  EXPECT_THAT(same_site_strict_cookie->IsSetPermittedInContext(
                  url, options,
                  CookieAccessParams(CookieAccessSemantics::LEGACY,
                                     delegate_treats_url_as_trustworthy),
                  kCookieableSchemes),
              MatchesCookieAccessResult(Not(net::IsInclude()), _, _, true));

  // Next: allow a SameSite=Lax or SameSite=Strict cookie.
  options.set_same_site_cookie_context(CookieOptions::SameSiteCookieContext(
      CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX));
  EXPECT_THAT(same_site_none_cookie->IsSetPermittedInContext(
                  url, options,
                  CookieAccessParams(CookieAccessSemantics::LEGACY,
                                     delegate_treats_url_as_trustworthy),
                  kCookieableSchemes),
              MatchesCookieAccessResult(CookieInclusionStatus(), _, _, true));
  EXPECT_THAT(same_site_lax_cookie->IsSetPermittedInContext(
                  url, options,
                  CookieAccessParams(CookieAccessSemantics::LEGACY,
                                     delegate_treats_url_as_trustworthy),
                  kCookieableSchemes),
              MatchesCookieAccessResult(net::IsInclude(), _, _, true));
  EXPECT_THAT(same_site_strict_cookie->IsSetPermittedInContext(
                  url, options,
                  CookieAccessParams(CookieAccessSemantics::LEGACY,
                                     delegate_treats_url_as_trustworthy),
                  kCookieableSchemes),
              MatchesCookieAccessResult(net::IsInclude(), _, _, true));
}

// Test that the CookieInclusionStatus warning for inclusion changed by
// cross-site redirect context downgrade is applied correctly.
TEST(CanonicalCookieTest, IsSetPermittedInContext_RedirectDowngradeWarning) {
  using Context = CookieOptions::SameSiteCookieContext;
  using ContextType = Context::ContextType;

  GURL url("https://www.example.test/test");
  GURL insecure_url("http://www.example.test/test");

  // Test cases to be used with a lax-to-cross context downgrade.
  const struct {
    ContextType context_type;
    CookieSameSite samesite;
    bool expect_cross_site_redirect_warning;
  } kTestCases[] = {
      {ContextType::SAME_SITE_LAX, CookieSameSite::STRICT_MODE, true},
      {ContextType::CROSS_SITE, CookieSameSite::STRICT_MODE, true},
      {ContextType::SAME_SITE_LAX, CookieSameSite::LAX_MODE, true},
      {ContextType::CROSS_SITE, CookieSameSite::LAX_MODE, true},
      {ContextType::SAME_SITE_LAX, CookieSameSite::NO_RESTRICTION, false},
      {ContextType::CROSS_SITE, CookieSameSite::NO_RESTRICTION, false},
  };

  for (bool consider_redirects : {true, false}) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(
        features::kCookieSameSiteConsidersRedirectChain, consider_redirects);

    for (CookieAccessSemantics semantics :
         {CookieAccessSemantics::LEGACY, CookieAccessSemantics::NONLEGACY}) {
      // There are no downgrade warnings for undowngraded contexts.
      for (ContextType context_type : {ContextType::SAME_SITE_LAX,
                                       ContextType::SAME_SITE_LAX_METHOD_UNSAFE,
                                       ContextType::CROSS_SITE}) {
        for (CookieSameSite samesite :
             {CookieSameSite::UNSPECIFIED, CookieSameSite::NO_RESTRICTION,
              CookieSameSite::LAX_MODE, CookieSameSite::STRICT_MODE}) {
          std::unique_ptr<CanonicalCookie> cookie =
              CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "1", "www.example.test", "/test", base::Time::Now(),
                  base::Time(), base::Time(), base::Time(), /*secure=*/true,
                  /*httponly=*/false, samesite, COOKIE_PRIORITY_DEFAULT);

          CookieOptions options;
          options.set_same_site_cookie_context(Context(context_type));

          EXPECT_FALSE(
              cookie
                  ->IsSetPermittedInContext(
                      url, options,
                      CookieAccessParams(
                          semantics,
                          /*delegate_treats_url_as_trustworthy=*/false),
                      kCookieableSchemes)
                  .status.HasWarningReason(
                      CookieInclusionStatus::
                          WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION));
        }
      }

      for (const auto& test : kTestCases) {
        std::unique_ptr<CanonicalCookie> cookie =
            CanonicalCookie::CreateUnsafeCookieForTesting(
                "A", "1", "www.example.test", "/test", base::Time::Now(),
                base::Time(), base::Time(), base::Time(), /*secure=*/true,
                /*httponly=*/false, test.samesite, COOKIE_PRIORITY_DEFAULT);

        Context::ContextMetadata lax_cross_downgrade_metadata;
        lax_cross_downgrade_metadata.cross_site_redirect_downgrade =
            Context::ContextMetadata::ContextDowngradeType::kLaxToCross;
        CookieOptions options;
        options.set_same_site_cookie_context(Context(
            test.context_type, test.context_type, lax_cross_downgrade_metadata,
            lax_cross_downgrade_metadata));

        EXPECT_EQ(
            cookie
                ->IsSetPermittedInContext(
                    url, options,
                    CookieAccessParams(
                        semantics,
                        /*delegate_treats_url_as_trustworthy=*/false),
                    kCookieableSchemes)
                .status.HasWarningReason(
                    CookieInclusionStatus::
                        WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION),
            test.expect_cross_site_redirect_warning);

        // SameSite warnings not applied if other exclusion reasons apply (e.g.
        // non-https with Secure attribute).
        EXPECT_FALSE(
            cookie
                ->IsSetPermittedInContext(
                    insecure_url, options,
                    CookieAccessParams(
                        semantics,
                        /*delegate_treats_url_as_trustworthy=*/false),
                    kCookieableSchemes)
                .status.HasWarningReason(
                    CookieInclusionStatus::
                        WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION));
      }
    }
  }
}

TEST(CanonicalCookieTest, TestIsCanonicalWithInvalidSizeHistograms) {
  base::HistogramTester histograms;
  const char kFromStorageWithValidLengthHistogram[] =
      "Cookie.FromStorageWithValidLength";
  const base::HistogramBase::Sample kInValid = 0;
  const base::HistogramBase::Sample kValid = 1;

  base::Time two_hours_ago = base::Time::Now() - base::Hours(2);
  base::Time one_hour_ago = base::Time::Now() - base::Hours(1);
  base::Time one_hour_from_now = base::Time::Now() + base::Hours(1);

  // Test a cookie that is canonical and valid size
  EXPECT_TRUE(CanonicalCookie::FromStorage(
      "A", "B", "www.foo.com", "/bar", two_hours_ago, one_hour_from_now,
      one_hour_ago, one_hour_ago, false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, CookieSourceScheme::kSecure, 87,
      CookieSourceType::kUnknown));

  histograms.ExpectBucketCount(kFromStorageWithValidLengthHistogram, kInValid,
                               0);
  histograms.ExpectBucketCount(kFromStorageWithValidLengthHistogram, kValid, 1);

  // Test loading a couple of cookies which are canonical but with an invalid
  // size
  const std::string kCookieBig(4096, 'a');
  EXPECT_TRUE(CanonicalCookie::FromStorage(
      kCookieBig, "B", "www.foo.com", "/bar", two_hours_ago, one_hour_from_now,
      one_hour_ago, one_hour_ago, false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, CookieSourceScheme::kSecure, 87,
      CookieSourceType::kUnknown));
  EXPECT_TRUE(CanonicalCookie::FromStorage(
      "A", kCookieBig, "www.foo.com", "/bar", two_hours_ago, one_hour_from_now,
      one_hour_ago, one_hour_ago, false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
      std::nullopt /*partition_key*/, CookieSourceScheme::kSecure, 87,
      CookieSourceType::kUnknown));

  histograms.ExpectBucketCount(kFromStorageWithValidLengthHistogram, kInValid,
                               2);
  histograms.ExpectBucketCount(kFromStorageWithValidLengthHistogram, kValid, 1);
}

TEST(CanonicalCookieTest, TestGetAndAdjustPortForTrustworthyUrls) {
  // GetAndAdjustPortForTrustworthyUrls assumes that http/ws schemes have a port
  // of 80 and https/wss schemes have a port of 443 by default. While extremely
  // unlikely to change, we may as well confirm that before we continue.
  std::string_view http_scheme(url::kHttpScheme);
  std::string_view https_scheme(url::kHttpsScheme);
  std::string_view ws_scheme(url::kWsScheme);
  std::string_view wss_scheme(url::kWssScheme);

  EXPECT_EQ(url::DefaultPortForScheme(http_scheme), 80);
  EXPECT_EQ(url::DefaultPortForScheme(ws_scheme), 80);
  EXPECT_EQ(url::DefaultPortForScheme(https_scheme), 443);
  EXPECT_EQ(url::DefaultPortForScheme(wss_scheme), 443);

  const GURL secure_http = GURL("https://example.com");
  const GURL secure_http_custom_port = GURL("https://example.com:123");
  const GURL secure_ws = GURL("wss://example.com");
  const GURL secure_ws_custom_port = GURL("wss://example.com:123");

  // Secure schemes shouldn't return a different port.
  EXPECT_EQ(
      CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(secure_http, true),
      443);
  EXPECT_EQ(
      CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(secure_http, false),
      443);
  EXPECT_EQ(
      CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(secure_ws, true),
      443);
  EXPECT_EQ(
      CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(secure_ws, false),
      443);
  EXPECT_EQ(CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(
                secure_http_custom_port, true),
            123);
  EXPECT_EQ(CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(
                secure_http_custom_port, false),
            123);
  EXPECT_EQ(CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(
                secure_ws_custom_port, true),
            123);
  EXPECT_EQ(CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(
                secure_ws_custom_port, false),
            123);

  const GURL insecure_http = GURL("http://example.com");
  const GURL insecure_ws = GURL("ws://example.com");

  // Insecure schemes with their default port should return 443 only when
  // trustworthy.
  EXPECT_EQ(
      CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(insecure_http, false),
      80);
  EXPECT_EQ(
      CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(insecure_ws, false),
      80);
  EXPECT_EQ(
      CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(insecure_http, true),
      443);
  EXPECT_EQ(
      CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(insecure_ws, true),
      443);

  const GURL insecure_http_custom_port = GURL("http://example.com:123");
  const GURL insecure_ws_custom_port = GURL("ws://example.com:123");

  // Insecure schemes with a non-default port should never return a different
  // port.
  EXPECT_EQ(CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(
                insecure_http_custom_port, false),
            123);
  EXPECT_EQ(CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(
                insecure_ws_custom_port, false),
            123);
  EXPECT_EQ(CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(
                insecure_http_custom_port, true),
            123);
  EXPECT_EQ(CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(
                insecure_ws_custom_port, true),
            123);

  // File schemes don't have a port component.
  const GURL insecure_file = GURL("file://example.com");
  EXPECT_EQ(
      CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(insecure_file, false),
      url::PORT_UNSPECIFIED);
  EXPECT_EQ(
      CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(insecure_file, true),
      url::PORT_UNSPECIFIED);
}

TEST(CanonicalCookieTest, TestHasHiddenPrefixName) {
  const struct {
    const char* value;
    bool result;
  } kTestCases[] = {
      {"", false},
      {"  ", false},
      {"foobar=", false},
      {"foo=bar", false},
      {" \t ", false},
      {"\t", false},
      {"__Secure=-", false},
      {"__Secure=-abc", false},
      {"__Secur=e-abc", false},
      {"__Secureabc", false},
      {"__Host=-", false},
      {"__Host=-abc", false},
      {"__Hos=t-abc", false},
      {"_Host", false},
      {"a__Host-abc=123", false},
      {"a__Secure-abc=123", false},
      {"__Secure-abc", true},
      {"__Host-abc", true},
      {"   __Secure-abc", true},
      {"\t__Host-", true},
      {"__Host-=", true},
      {"__Host-=123", true},
      {"__host-=123", true},
      {"__HOST-=123", true},
      {"__HoSt-=123", true},
      {"__Host-abc=", true},
      {"__Host-abc=123", true},
      {" __Host-abc=123", true},
      {"    __Host-abc=", true},
      {"\t\t\t\t\t__Host-abc=123", true},
      {"\t __Host-abc=", true},
      {"__Secure-=", true},
      {"__Secure-=123", true},
      {"__secure-=123", true},
      {"__SECURE-=123", true},
      {"__SeCuRe-=123", true},
      {"__Secure-abc=", true},
      {"__Secure-abc=123", true},
      {" __Secure-abc=123", true},
      {"    __Secure-abc=", true},
      {"\t\t\t\t\t__Secure-abc=123", true},
      {"\t __Secure-abc=", true},
      {"__Secure-abc=123=d=4=fg=", true},
  };

  for (auto test_case : kTestCases) {
    EXPECT_EQ(CanonicalCookie::HasHiddenPrefixName(test_case.value),
              test_case.result)
        << test_case.value << " failed check";
  }
}

TEST(CanonicalCookieTest, TestDoubleUnderscorePrefixHistogram) {
  base::HistogramTester histograms;
  const char kDoubleUnderscorePrefixHistogram[] =
      "Cookie.DoubleUnderscorePrefixedName";

  CanonicalCookie::CreateForTesting(GURL("https://www.example.com/"),
                                    "__Secure-abc=123; Secure",
                                    base::Time::Now() /* Creation time */);

  CanonicalCookie::CreateForTesting(GURL("https://www.example.com/"),
                                    "__Host-abc=123; Secure; Path=/",
                                    base::Time::Now() /* Creation time */);

  // Cookie prefixes shouldn't count.
  histograms.ExpectTotalCount(kDoubleUnderscorePrefixHistogram, 2);
  histograms.ExpectBucketCount(kDoubleUnderscorePrefixHistogram, false, 2);

  CanonicalCookie::CreateForTesting(GURL("https://www.example.com/"),
                                    "f__oo=bar",
                                    base::Time::Now() /* Creation time */);

  CanonicalCookie::CreateForTesting(GURL("https://www.example.com/"),
                                    "foo=__bar",
                                    base::Time::Now() /* Creation time */);

  CanonicalCookie::CreateForTesting(GURL("https://www.example.com/"),
                                    "_foo=bar",
                                    base::Time::Now() /* Creation time */);

  CanonicalCookie::CreateForTesting(GURL("https://www.example.com/"),
                                    "_f_oo=bar",
                                    base::Time::Now() /* Creation time */);

  // These should be counted.
  CanonicalCookie::CreateForTesting(GURL("https://www.example.com/"),
                                    "__foo=bar",
                                    base::Time::Now() /* Creation time */);

  CanonicalCookie::CreateForTesting(GURL("https://www.example.com/"),
                                    "___foo=bar",
                                    base::Time::Now() /* Creation time */);

  histograms.ExpectTotalCount(kDoubleUnderscorePrefixHistogram, 8);
  histograms.ExpectBucketCount(kDoubleUnderscorePrefixHistogram, false, 6);
  histograms.ExpectBucketCount(kDoubleUnderscorePrefixHistogram, true, 2);
}

TEST(CanonicalCookieTest, IsThirdPartyPartitioned) {
  // Partitioned cookie in 3p context.
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "x.y", "/foo/bar", base::Time(), base::Time(),
                  base::Time(), base::Time(), /*secure=*/true,
                  /*httponly=*/false, CookieSameSite::UNSPECIFIED,
                  COOKIE_PRIORITY_LOW,
                  CookiePartitionKey::FromURLForTesting(
                      GURL("https://toplevelsite.com")))
                  ->IsThirdPartyPartitioned());

  // Partitioned cookie in 1p context.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "x.y", "/foo/bar", base::Time(), base::Time(),
                   base::Time(), base::Time(), /*secure=*/true,
                   /*httponly=*/false, CookieSameSite::UNSPECIFIED,
                   COOKIE_PRIORITY_LOW,
                   CookiePartitionKey::FromURLForTesting(GURL("https://x.y")))
                   ->IsThirdPartyPartitioned());

  // Nonced-partitioned cookie should always be 3p context.
  auto partition_key_with_nonce =
      std::make_optional(CookiePartitionKey::FromURLForTesting(
          GURL("https://x.y"), CookiePartitionKey::AncestorChainBit::kCrossSite,
          base::UnguessableToken::Create()));
  EXPECT_TRUE(CanonicalCookie::CreateUnsafeCookieForTesting(
                  "A", "B", "x.y", "/foo/bar", base::Time(), base::Time(),
                  base::Time(), base::Time(), /*secure=*/true,
                  /*httponly=*/false, CookieSameSite::UNSPECIFIED,
                  COOKIE_PRIORITY_LOW, partition_key_with_nonce)
                  ->IsThirdPartyPartitioned());

  // Unpartitioned cookie.
  EXPECT_FALSE(CanonicalCookie::CreateUnsafeCookieForTesting(
                   "A", "B", "x.y", "/foo/bar", base::Time(), base::Time(),
                   base::Time(), base::Time(), /*secure=*/false,
                   /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
                   COOKIE_PRIORITY_LOW)
                   ->IsThirdPartyPartitioned());
}

// Tests that IsSecure returns true if a cookie's secure attribute is true
// OR if its source_scheme is kSecure when scheme binding is enabled.
TEST(CanonicalCookieTest, IsSecure) {
  auto create_cookie = [](bool secure_attribute,
                          CookieSourceScheme source_scheme) {
    return CanonicalCookie::CreateUnsafeCookieForTesting(
        "A", "B", "example.com", "/", base::Time(), base::Time(), base::Time(),
        base::Time(), secure_attribute, /*httponly=*/false,
        CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW,
        /*partition_key=*/std::nullopt, source_scheme, /*source_port=*/1234);
  };

  auto insecure_attr_unset_scheme =
      create_cookie(/*secure_attribute=*/false, CookieSourceScheme::kUnset);
  auto insecure_attr_insecure_scheme =
      create_cookie(/*secure_attribute=*/false, CookieSourceScheme::kNonSecure);
  auto insecure_attr_secure_scheme =
      create_cookie(/*secure_attribute=*/false, CookieSourceScheme::kSecure);

  auto secure_attr_unset_scheme =
      create_cookie(/*secure_attribute=*/true, CookieSourceScheme::kUnset);
  auto secure_attr_insecure_scheme =
      create_cookie(/*secure_attribute=*/true, CookieSourceScheme::kNonSecure);
  auto secure_attr_secure_scheme =
      create_cookie(/*secure_attribute=*/true, CookieSourceScheme::kSecure);

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kEnableSchemeBoundCookies);

    // When scheme binding is disabled only the secure attribute causes a return
    // value of true.

    EXPECT_FALSE(insecure_attr_unset_scheme->IsSecure());
    EXPECT_FALSE(insecure_attr_insecure_scheme->IsSecure());
    EXPECT_FALSE(insecure_attr_secure_scheme->IsSecure());

    EXPECT_TRUE(secure_attr_unset_scheme->IsSecure());
    EXPECT_TRUE(secure_attr_insecure_scheme->IsSecure());
    EXPECT_TRUE(secure_attr_secure_scheme->IsSecure());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kEnableSchemeBoundCookies);

    // When scheme binding is enabled a kSecure scheme also causes a returns
    // value of true.

    EXPECT_FALSE(insecure_attr_unset_scheme->IsSecure());
    EXPECT_FALSE(insecure_attr_insecure_scheme->IsSecure());
    EXPECT_TRUE(insecure_attr_secure_scheme->IsSecure());

    EXPECT_TRUE(secure_attr_unset_scheme->IsSecure());
    EXPECT_TRUE(secure_attr_insecure_scheme->IsSecure());
    EXPECT_TRUE(secure_attr_secure_scheme->IsSecure());
  }
}

}  // namespace net
