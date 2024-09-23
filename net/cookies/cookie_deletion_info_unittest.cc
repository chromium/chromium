// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_deletion_info.h"

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

using TimeRange = CookieDeletionInfo::TimeRange;

TEST(CookieDeletionInfoTest, TimeRangeValues) {
  TimeRange range;
  EXPECT_EQ(base::Time(), range.start());
  EXPECT_EQ(base::Time(), range.end());

  const base::Time kTestStart = base::Time::FromSecondsSinceUnixEpoch(1000);
  const base::Time kTestEnd = base::Time::FromSecondsSinceUnixEpoch(10000);

  EXPECT_EQ(kTestStart, TimeRange(kTestStart, base::Time()).start());
  EXPECT_EQ(base::Time(), TimeRange(kTestStart, base::Time()).end());

  EXPECT_EQ(kTestStart, TimeRange(kTestStart, kTestEnd).start());
  EXPECT_EQ(kTestEnd, TimeRange(kTestStart, kTestEnd).end());

  TimeRange range2;
  range2.SetStart(kTestStart);
  EXPECT_EQ(kTestStart, range2.start());
  EXPECT_EQ(base::Time(), range2.end());
  range2.SetEnd(kTestEnd);
  EXPECT_EQ(kTestStart, range2.start());
  EXPECT_EQ(kTestEnd, range2.end());
}

TEST(CookieDeletionInfoTest, TimeRangeContains) {
  // Default TimeRange matches all time values.
  TimeRange range;
  EXPECT_TRUE(range.Contains(base::Time::Now()));
  EXPECT_TRUE(range.Contains(base::Time::Max()));

  // With a start, but no end.
  const double kTestMinEpoch = 1000;
  range.SetStart(base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch));
  EXPECT_FALSE(range.Contains(base::Time::Min()));
  EXPECT_FALSE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch - 1)));
  EXPECT_TRUE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch)));
  EXPECT_TRUE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch + 1)));
  EXPECT_TRUE(range.Contains(base::Time::Max()));

  // With an end, but no start.
  const double kTestMaxEpoch = 10000000;
  range = TimeRange();
  range.SetEnd(base::Time::FromSecondsSinceUnixEpoch(kTestMaxEpoch));
  EXPECT_TRUE(range.Contains(base::Time::Min()));
  EXPECT_TRUE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMaxEpoch - 1)));
  EXPECT_FALSE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMaxEpoch)));
  EXPECT_FALSE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMaxEpoch + 1)));
  EXPECT_FALSE(range.Contains(base::Time::Max()));

  // With both a start and an end.
  range.SetStart(base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch));
  EXPECT_FALSE(range.Contains(base::Time::Min()));
  EXPECT_FALSE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch - 1)));
  EXPECT_TRUE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch)));
  EXPECT_TRUE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch + 1)));
  EXPECT_TRUE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMaxEpoch - 1)));
  EXPECT_FALSE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMaxEpoch)));
  EXPECT_FALSE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMaxEpoch + 1)));
  EXPECT_FALSE(range.Contains(base::Time::Max()));

  // And where start==end.
  range = TimeRange(base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch),
                    base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch));
  EXPECT_FALSE(range.Contains(base::Time::Min()));
  EXPECT_FALSE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch - 1)));
  EXPECT_TRUE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch)));
  EXPECT_FALSE(
      range.Contains(base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch + 1)));
}

TEST(CookieDeletionInfoTest, CookieDeletionInfoMatchSessionControl) {
  auto persistent_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      "persistent-cookie", "persistent-value", "persistent-domain",
      "persistent-path",
      /*creation=*/base::Time::Now(),
      /*expiration=*/base::Time::Max(),
      /*last_access=*/base::Time::Now(),
      /*last_update=*/base::Time::Now(),
      /*secure=*/true,
      /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      CookiePriority::COOKIE_PRIORITY_DEFAULT);

  auto session_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      "session-cookie", "session-value", "session-domain", "session-path",
      /*creation=*/base::Time::Now(),
      /*expiration=*/base::Time(),
      /*last_access=*/base::Time::Now(),
      /*last_update=*/base::Time::Now(),
      /*secure=*/true,
      /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      CookiePriority::COOKIE_PRIORITY_DEFAULT);

  CookieDeletionInfo delete_info;
  EXPECT_TRUE(delete_info.Matches(
      *persistent_cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_TRUE(delete_info.Matches(
      *session_cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));

  delete_info.session_control =
      CookieDeletionInfo::SessionControl::PERSISTENT_COOKIES;
  EXPECT_TRUE(delete_info.Matches(
      *persistent_cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_FALSE(delete_info.Matches(
      *session_cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));

  delete_info.session_control =
      CookieDeletionInfo::SessionControl::SESSION_COOKIES;
  EXPECT_FALSE(delete_info.Matches(
      *persistent_cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_TRUE(delete_info.Matches(
      *session_cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
}

TEST(CookieDeletionInfoTest, CookieDeletionInfoMatchHost) {
  auto domain_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      "domain-cookie", "domain-cookie-value",
      /*domain=*/".example.com", "/path",
      /*creation=*/base::Time::Now(),
      /*expiration=*/base::Time::Max(),
      /*last_access=*/base::Time::Now(),
      /*last_update=*/base::Time::Now(),
      /*secure=*/true,
      /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      CookiePriority::COOKIE_PRIORITY_DEFAULT);

  auto host_cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      "host-cookie", "host-cookie-value",
      /*domain=*/"thehost.hosting.com", "/path",
      /*creation=*/base::Time::Now(),
      /*expiration=*/base::Time::Max(),
      /*last_access=*/base::Time::Now(),
      /*last_update=*/base::Time::Now(),
      /*secure=*/true,
      /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      CookiePriority::COOKIE_PRIORITY_DEFAULT);

  EXPECT_TRUE(domain_cookie->IsDomainCookie());
  EXPECT_TRUE(host_cookie->IsHostCookie());

  CookieDeletionInfo delete_info;
  EXPECT_TRUE(delete_info.Matches(
      *domain_cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_TRUE(delete_info.Matches(
      *host_cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));

  delete_info.host = "thehost.hosting.com";
  EXPECT_FALSE(delete_info.Matches(
      *domain_cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_TRUE(delete_info.Matches(
      *host_cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));

  delete_info.host = "otherhost.hosting.com";
  EXPECT_FALSE(delete_info.Matches(
      *domain_cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_FALSE(delete_info.Matches(
      *host_cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));

  delete_info.host = "thehost.otherhosting.com";
  EXPECT_FALSE(delete_info.Matches(
      *domain_cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_FALSE(delete_info.Matches(
      *host_cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
}

TEST(CookieDeletionInfoTest, CookieDeletionInfoMatchName) {
  auto cookie1 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "cookie1-name", "cookie1-value",
      /*domain=*/".example.com", "/path",
      /*creation=*/base::Time::Now(),
      /*expiration=*/base::Time::Max(),
      /*last_access=*/base::Time::Now(),
      /*last_update=*/base::Time::Now(),
      /*secure=*/true,
      /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      CookiePriority::COOKIE_PRIORITY_DEFAULT);
  auto cookie2 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "cookie2-name", "cookie2-value",
      /*domain=*/".example.com", "/path",
      /*creation=*/base::Time::Now(),
      /*expiration=*/base::Time::Max(),
      /*last_access=*/base::Time::Now(),
      /*last_update=*/base::Time::Now(),
      /*secure=*/true,
      /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      CookiePriority::COOKIE_PRIORITY_DEFAULT);

  CookieDeletionInfo delete_info;
  delete_info.name = "cookie1-name";
  EXPECT_TRUE(delete_info.Matches(
      *cookie1,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_FALSE(delete_info.Matches(
      *cookie2,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
}

TEST(CookieDeletionInfoTest, CookieDeletionInfoMatchValue) {
  auto cookie1 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "cookie1-name", "cookie1-value",
      /*domain=*/".example.com", "/path",
      /*creation=*/base::Time::Now(),
      /*expiration=*/base::Time::Max(),
      /*last_access=*/base::Time::Now(),
      /*last_update=*/base::Time::Now(),
      /*secure=*/true,
      /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      CookiePriority::COOKIE_PRIORITY_DEFAULT);
  auto cookie2 = CanonicalCookie::CreateUnsafeCookieForTesting(
      "cookie2-name", "cookie2-value",
      /*domain=*/".example.com", "/path",
      /*creation=*/base::Time::Now(),
      /*expiration=*/base::Time::Max(),
      /*last_access=*/base::Time::Now(),
      /*last_update=*/base::Time::Now(),
      /*secure=*/true,
      /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      CookiePriority::COOKIE_PRIORITY_DEFAULT);

  CookieDeletionInfo delete_info;
  delete_info.value_for_testing = "cookie2-value";
  EXPECT_FALSE(delete_info.Matches(
      *cookie1,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_TRUE(delete_info.Matches(
      *cookie2,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
}

TEST(CookieDeletionInfoTest, CookieDeletionInfoMatchUrl) {
  auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      "cookie-name", "cookie-value",
      /*domain=*/"www.example.com", "/path",
      /*creation=*/base::Time::Now(),
      /*expiration=*/base::Time::Max(),
      /*last_access=*/base::Time::Now(),
      /*last_update=*/base::Time::Now(),
      /*secure=*/true,
      /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      CookiePriority::COOKIE_PRIORITY_DEFAULT);

  CookieDeletionInfo delete_info;
  delete_info.url = GURL("https://www.example.com/path");
  EXPECT_TRUE(delete_info.Matches(
      *cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));

  delete_info.url = GURL("https://www.example.com/another/path");
  EXPECT_FALSE(delete_info.Matches(
      *cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));

  delete_info.url = GURL("http://www.example.com/path");
  // Secure cookie on http:// URL -> no match.
  EXPECT_FALSE(delete_info.Matches(
      *cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));

  // Secure cookie on http:// URL, but delegate says treat is as trustworhy ->
  // match.
  EXPECT_TRUE(delete_info.Matches(
      *cookie,
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/true}));
}

TEST(CookieDeletionInfoTest, CookieDeletionInfoDomainMatchesDomain) {
  CookieDeletionInfo delete_info;

  const double kTestMinEpoch = 1000;
  const double kTestMaxEpoch = 10000000;
  delete_info.creation_range.SetStart(
      base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch));
  delete_info.creation_range.SetEnd(
      base::Time::FromSecondsSinceUnixEpoch(kTestMaxEpoch));

  auto create_cookie = [kTestMinEpoch](std::string cookie_domain) {
    return *CanonicalCookie::CreateUnsafeCookieForTesting(
        /*name=*/"test-cookie",
        /*value=*/"cookie-value", cookie_domain,
        /*path=*/"cookie/path",
        /*creation=*/base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch + 1),
        /*expiration=*/base::Time::Max(),
        /*last_access=*/
        base::Time::FromSecondsSinceUnixEpoch(kTestMinEpoch + 1),
        /*last_update=*/base::Time::Now(),
        /*secure=*/true,
        /*httponly=*/false,
        /*same_site=*/CookieSameSite::NO_RESTRICTION,
        /*priority=*/CookiePriority::COOKIE_PRIORITY_DEFAULT);
  };

  // by default empty domain list and default match action will match.
  EXPECT_TRUE(delete_info.Matches(
      create_cookie("example.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));

  const char kExtensionHostname[] = "mgndgikekgjfcpckkfioiadnlibdjbkf";

  // Only using the inclusion list because this is only testing
  // DomainMatchesDomainSet and not CookieDeletionInfo::Matches.
  delete_info.domains_and_ips_to_delete =
      std::set<std::string>({"example.com", "another.com", "192.168.0.1"});
  EXPECT_TRUE(delete_info.Matches(
      create_cookie(".example.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_TRUE(delete_info.Matches(
      create_cookie("example.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_TRUE(delete_info.Matches(
      create_cookie(".another.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_TRUE(delete_info.Matches(
      create_cookie("192.168.0.1"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_FALSE(delete_info.Matches(
      create_cookie(".nomatch.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_FALSE(delete_info.Matches(
      create_cookie("192.168.0.2"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_FALSE(delete_info.Matches(
      create_cookie(kExtensionHostname),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
}

TEST(CookieDeletionInfoTest, CookieDeletionInfoMatchesDomainList) {
  CookieDeletionInfo delete_info;

  auto create_cookie = [](std::string cookie_domain) {
    return *CanonicalCookie::CreateUnsafeCookieForTesting(
        /*name=*/"test-cookie",
        /*value=*/"cookie-value", cookie_domain,
        /*path=*/"cookie/path",
        /*creation=*/base::Time::Now(),
        /*expiration=*/base::Time::Max(),
        /*last_access=*/base::Time::Now(),
        /*last_update=*/base::Time::Now(),
        /*secure=*/false,
        /*httponly=*/false,
        /*same_site=*/CookieSameSite::NO_RESTRICTION,
        /*priority=*/CookiePriority::COOKIE_PRIORITY_DEFAULT);
  };

  // With two empty lists (default) should match any domain.
  EXPECT_TRUE(delete_info.Matches(
      create_cookie("anything.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));

  // With only an "to_delete" list.
  delete_info.domains_and_ips_to_delete = {"includea.com", "includeb.com"};
  EXPECT_TRUE(delete_info.Matches(
      create_cookie("includea.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_TRUE(delete_info.Matches(
      create_cookie("includeb.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_FALSE(delete_info.Matches(
      create_cookie("anything.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));

  // With only an "to_ignore" list.
  delete_info.domains_and_ips_to_delete.reset();
  delete_info.domains_and_ips_to_ignore = {"exclude.com"};
  EXPECT_TRUE(delete_info.Matches(
      create_cookie("anything.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_FALSE(delete_info.Matches(
      create_cookie("exclude.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));

  // Now with both lists populated.
  //
  // +----------------------+
  // | to_delete            |  outside.com
  // |                      |
  // |  left.com  +---------------------+
  // |            | mid.com | to_ignore |
  // |            |         |           |
  // +------------|---------+           |
  //              |           right.com |
  //              |                     |
  //              +---------------------+
  delete_info.domains_and_ips_to_delete = {"left.com", "mid.com"};
  delete_info.domains_and_ips_to_ignore = {"mid.com", "right.com"};

  EXPECT_TRUE(delete_info.Matches(
      create_cookie("left.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_FALSE(delete_info.Matches(
      create_cookie("mid.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_FALSE(delete_info.Matches(
      create_cookie("right.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_FALSE(delete_info.Matches(
      create_cookie("outside.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));

  // An empty list of deleted domains shouldn't delete anything.
  delete_info.domains_and_ips_to_delete = std::set<std::string>();
  delete_info.domains_and_ips_to_ignore.reset();
  EXPECT_FALSE(delete_info.Matches(
      create_cookie("outside.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));

  // An empty list of ignored domains should delete everything.
  delete_info.domains_and_ips_to_delete.reset();
  delete_info.domains_and_ips_to_ignore = std::set<std::string>();
  EXPECT_TRUE(delete_info.Matches(
      create_cookie("inside.com"),
      CookieAccessParams{net::CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
}

// Test that Matches() works regardless of the cookie access semantics (because
// the IncludeForRequestURL call uses CookieOptions::MakeAllInclusive).
TEST(CookieDeletionInfoTest, MatchesWithCookieAccessSemantics) {
  // Cookie with unspecified SameSite.
  auto cookie = CanonicalCookie::CreateForTesting(
      GURL("https://www.example.com"), "cookie=1", base::Time::Now(),
      /*server_time=*/std::nullopt,
      /*cookie_partition_key=*/std::nullopt);

  CookieDeletionInfo delete_info;
  delete_info.url = GURL("https://www.example.com/path");
  EXPECT_TRUE(delete_info.Matches(
      *cookie,
      CookieAccessParams{CookieAccessSemantics::UNKNOWN,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_TRUE(delete_info.Matches(
      *cookie,
      CookieAccessParams{CookieAccessSemantics::LEGACY,
                         /*delegate_treats_url_as_trustworthy=*/false}));
  EXPECT_TRUE(delete_info.Matches(
      *cookie,
      CookieAccessParams{CookieAccessSemantics::NONLEGACY,
                         /*delegate_treats_url_as_trustworthy=*/false}));
}

TEST(CookieDeletionInfoTest, MatchesCookiePartitionKeyCollection) {
  const CookiePartitionKey kPartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com"));
  const CookiePartitionKey kOtherPartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://www.bar.com"));
  const CookiePartitionKeyCollection kEmptyKeychain;
  const CookiePartitionKeyCollection kSingletonKeychain(kPartitionKey);
  const CookiePartitionKeyCollection kMultipleKeysKeychain(
      {kPartitionKey, kOtherPartitionKey});
  const CookiePartitionKeyCollection kAllKeysKeychain =
      CookiePartitionKeyCollection::ContainsAll();
  const std::optional<CookiePartitionKey> kPartitionKeyOpt =
      std::make_optional(kPartitionKey);
  const CookiePartitionKeyCollection kOtherKeySingletonKeychain(
      kOtherPartitionKey);

  struct TestCase {
    const std::string desc;
    const CookiePartitionKeyCollection filter_cookie_partition_key_collection;
    const std::optional<CookiePartitionKey> cookie_partition_key;
    bool expects_match;
  } test_cases[] = {
      // Unpartitioned cookie always matches
      {"Unpartitioned empty keychain", kEmptyKeychain, std::nullopt, true},
      {"Unpartitioned singleton keychain", kSingletonKeychain, std::nullopt,
       true},
      {"Unpartitioned multiple keys", kMultipleKeysKeychain, std::nullopt,
       true},
      {"Unpartitioned all keys", kAllKeysKeychain, std::nullopt, true},
      // Partitioned cookie only matches keychains which contain its partition
      // key.
      {"Partitioned empty keychain", kEmptyKeychain, kPartitionKeyOpt, false},
      {"Partitioned singleton keychain", kSingletonKeychain, kPartitionKeyOpt,
       true},
      {"Partitioned multiple keys", kMultipleKeysKeychain, kPartitionKeyOpt,
       true},
      {"Partitioned all keys", kAllKeysKeychain, kPartitionKeyOpt, true},
      {"Partitioned mismatched keys", kOtherKeySingletonKeychain,
       kPartitionKeyOpt, false},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.desc);
    auto cookie = CanonicalCookie::CreateForTesting(
        GURL("https://www.example.com"),
        "__Host-foo=bar; Secure; Path=/; Partitioned", base::Time::Now(),
        /*server_time=*/std::nullopt, test_case.cookie_partition_key);
    CookieDeletionInfo delete_info;
    delete_info.cookie_partition_key_collection =
        test_case.filter_cookie_partition_key_collection;
    EXPECT_EQ(test_case.expects_match,
              delete_info.Matches(
                  *cookie, CookieAccessParams{
                               net::CookieAccessSemantics::UNKNOWN,
                               /*delegate_treats_url_as_trustworthy=*/false}));
  }
}

TEST(CookieDeletionInfoTest, MatchesExcludeUnpartitionedCookies) {
  struct TestCase {
    const std::string desc;
    const std::optional<CookiePartitionKey> cookie_partition_key;
    bool partitioned_state_only;
    bool expects_match;
  } test_cases[] = {
      {"Unpartitioned cookie not excluded", std::nullopt, false, true},
      {"Unpartitioned cookie excluded", std::nullopt, true, false},
      {"Partitioned cookie when unpartitioned not excluded",
       CookiePartitionKey::FromURLForTesting(GURL("https://foo.com")), false,
       true},
      {"Partitioned cookie when unpartitioned excluded",
       CookiePartitionKey::FromURLForTesting(GURL("https://foo.com")), true,
       true},
      {"Nonced partitioned cookie when unpartitioned not excluded",
       CookiePartitionKey::FromURLForTesting(
           GURL("https://foo.com"),
           CookiePartitionKey::AncestorChainBit::kCrossSite,
           base::UnguessableToken::Create()),
       false, true},
      {"Nonced partitioned cookie when unpartitioned excluded",
       CookiePartitionKey::FromURLForTesting(
           GURL("https://foo.com"),
           CookiePartitionKey::AncestorChainBit::kCrossSite,
           base::UnguessableToken::Create()),
       true, true},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.desc);
    auto cookie = CanonicalCookie::CreateForTesting(
        GURL("https://www.example.com"),
        "__Host-foo=bar; Secure; Path=/; Partitioned", base::Time::Now(),
        /*server_time=*/std::nullopt, test_case.cookie_partition_key);
    CookieDeletionInfo delete_info;
    delete_info.partitioned_state_only = test_case.partitioned_state_only;
    EXPECT_EQ(test_case.expects_match,
              delete_info.Matches(
                  *cookie, CookieAccessParams{
                               net::CookieAccessSemantics::UNKNOWN,
                               /*delegate_treats_url_as_trustworthy=*/false}));
  }
}

}  // namespace net
