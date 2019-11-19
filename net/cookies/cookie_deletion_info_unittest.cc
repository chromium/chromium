// Copyright 2018 The Chromium Authors. All rights reserved.
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

  const base::Time kTestStart = base::Time::FromDoubleT(1000);
  const base::Time kTestEnd = base::Time::FromDoubleT(10000);

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
  range.SetStart(base::Time::FromDoubleT(kTestMinEpoch));
  EXPECT_FALSE(range.Contains(base::Time::Min()));
  EXPECT_FALSE(range.Contains(base::Time::FromDoubleT(kTestMinEpoch - 1)));
  EXPECT_TRUE(range.Contains(base::Time::FromDoubleT(kTestMinEpoch)));
  EXPECT_TRUE(range.Contains(base::Time::FromDoubleT(kTestMinEpoch + 1)));
  EXPECT_TRUE(range.Contains(base::Time::Max()));

  // With an end, but no start.
  const double kTestMaxEpoch = 10000000;
  range = TimeRange();
  range.SetEnd(base::Time::FromDoubleT(kTestMaxEpoch));
  EXPECT_TRUE(range.Contains(base::Time::Min()));
  EXPECT_TRUE(range.Contains(base::Time::FromDoubleT(kTestMaxEpoch - 1)));
  EXPECT_FALSE(range.Contains(base::Time::FromDoubleT(kTestMaxEpoch)));
  EXPECT_FALSE(range.Contains(base::Time::FromDoubleT(kTestMaxEpoch + 1)));
  EXPECT_FALSE(range.Contains(base::Time::Max()));

  // With both a start and an end.
  range.SetStart(base::Time::FromDoubleT(kTestMinEpoch));
  EXPECT_FALSE(range.Contains(base::Time::Min()));
  EXPECT_FALSE(range.Contains(base::Time::FromDoubleT(kTestMinEpoch - 1)));
  EXPECT_TRUE(range.Contains(base::Time::FromDoubleT(kTestMinEpoch)));
  EXPECT_TRUE(range.Contains(base::Time::FromDoubleT(kTestMinEpoch + 1)));
  EXPECT_TRUE(range.Contains(base::Time::FromDoubleT(kTestMaxEpoch - 1)));
  EXPECT_FALSE(range.Contains(base::Time::FromDoubleT(kTestMaxEpoch)));
  EXPECT_FALSE(range.Contains(base::Time::FromDoubleT(kTestMaxEpoch + 1)));
  EXPECT_FALSE(range.Contains(base::Time::Max()));

  // And where start==end.
  range = TimeRange(base::Time::FromDoubleT(kTestMinEpoch),
                    base::Time::FromDoubleT(kTestMinEpoch));
  EXPECT_FALSE(range.Contains(base::Time::Min()));
  EXPECT_FALSE(range.Contains(base::Time::FromDoubleT(kTestMinEpoch - 1)));
  EXPECT_TRUE(range.Contains(base::Time::FromDoubleT(kTestMinEpoch)));
  EXPECT_FALSE(range.Contains(base::Time::FromDoubleT(kTestMinEpoch + 1)));
}

TEST(CookieDeletionInfoTest, CookieDeletionInfoMatchSessionControl) {
  CanonicalCookie persistent_cookie("persistent-cookie", "persistent-value",
                                    "persistent-domain", "persistent-path",
                                    /*creation=*/base::Time::Now(),
                                    /*expiration=*/base::Time::Max(),
                                    /*last_access=*/base::Time::Now(),
                                    /*secure=*/true,
                                    /*httponly=*/false,
                                    CookieSameSite::NO_RESTRICTION,
                                    CookiePriority::COOKIE_PRIORITY_DEFAULT);

  CanonicalCookie session_cookie(
      "session-cookie", "session-value", "session-domain", "session-path",
      /*creation=*/base::Time::Now(),
      /*expiration=*/base::Time(),
      /*last_access=*/base::Time::Now(),
      /*secure=*/true,
      /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      CookiePriority::COOKIE_PRIORITY_DEFAULT);

  CookieDeletionInfo delete_info;
  EXPECT_TRUE(delete_info.Matches(persistent_cookie));
  EXPECT_TRUE(delete_info.Matches(session_cookie));

  delete_info.session_control =
      CookieDeletionInfo::SessionControl::PERSISTENT_COOKIES;
  EXPECT_TRUE(delete_info.Matches(persistent_cookie));
  EXPECT_FALSE(delete_info.Matches(session_cookie));

  delete_info.session_control =
      CookieDeletionInfo::SessionControl::SESSION_COOKIES;
  EXPECT_FALSE(delete_info.Matches(persistent_cookie));
  EXPECT_TRUE(delete_info.Matches(session_cookie));
}

TEST(CookieDeletionInfoTest, CookieDeletionInfoMatchHost) {
  CanonicalCookie domain_cookie("domain-cookie", "domain-cookie-value",
                                /*domain=*/".example.com", "/path",
                                /*creation=*/base::Time::Now(),
                                /*expiration=*/base::Time::Max(),
                                /*last_access=*/base::Time::Now(),
                                /*secure=*/true,
                                /*httponly=*/false,
                                CookieSameSite::NO_RESTRICTION,
                                CookiePriority::COOKIE_PRIORITY_DEFAULT);

  CanonicalCookie host_cookie("host-cookie", "host-cookie-value",
                              /*domain=*/"thehost.hosting.com", "/path",
                              /*creation=*/base::Time::Now(),
                              /*expiration=*/base::Time::Max(),
                              /*last_access=*/base::Time::Now(),
                              /*secure=*/true,
                              /*httponly=*/false,
                              CookieSameSite::NO_RESTRICTION,
                              CookiePriority::COOKIE_PRIORITY_DEFAULT);

  EXPECT_TRUE(domain_cookie.IsDomainCookie());
  EXPECT_TRUE(host_cookie.IsHostCookie());

  CookieDeletionInfo delete_info;
  EXPECT_TRUE(delete_info.Matches(domain_cookie));
  EXPECT_TRUE(delete_info.Matches(host_cookie));

  delete_info.host = "thehost.hosting.com";
  EXPECT_FALSE(delete_info.Matches(domain_cookie));
  EXPECT_TRUE(delete_info.Matches(host_cookie));

  delete_info.host = "otherhost.hosting.com";
  EXPECT_FALSE(delete_info.Matches(domain_cookie));
  EXPECT_FALSE(delete_info.Matches(host_cookie));

  delete_info.host = "thehost.otherhosting.com";
  EXPECT_FALSE(delete_info.Matches(domain_cookie));
  EXPECT_FALSE(delete_info.Matches(host_cookie));
}

TEST(CookieDeletionInfoTest, CookieDeletionInfoMatchName) {
  CanonicalCookie cookie1("cookie1-name", "cookie1-value",
                          /*domain=*/".example.com", "/path",
                          /*creation=*/base::Time::Now(),
                          /*expiration=*/base::Time::Max(),
                          /*last_access=*/base::Time::Now(),
                          /*secure=*/true,
                          /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
                          CookiePriority::COOKIE_PRIORITY_DEFAULT);
  CanonicalCookie cookie2("cookie2-name", "cookie2-value",
                          /*domain=*/".example.com", "/path",
                          /*creation=*/base::Time::Now(),
                          /*expiration=*/base::Time::Max(),
                          /*last_access=*/base::Time::Now(),
                          /*secure=*/true,
                          /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
                          CookiePriority::COOKIE_PRIORITY_DEFAULT);

  CookieDeletionInfo delete_info;
  delete_info.name = "cookie1-name";
  EXPECT_TRUE(delete_info.Matches(cookie1));
  EXPECT_FALSE(delete_info.Matches(cookie2));
}

TEST(CookieDeletionInfoTest, CookieDeletionInfoMatchValue) {
  CanonicalCookie cookie1("cookie1-name", "cookie1-value",
                          /*domain=*/".example.com", "/path",
                          /*creation=*/base::Time::Now(),
                          /*expiration=*/base::Time::Max(),
                          /*last_access=*/base::Time::Now(),
                          /*secure=*/true,
                          /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
                          CookiePriority::COOKIE_PRIORITY_DEFAULT);
  CanonicalCookie cookie2("cookie2-name", "cookie2-value",
                          /*domain=*/".example.com", "/path",
                          /*creation=*/base::Time::Now(),
                          /*expiration=*/base::Time::Max(),
                          /*last_access=*/base::Time::Now(),
                          /*secure=*/true,
                          /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
                          CookiePriority::COOKIE_PRIORITY_DEFAULT);

  CookieDeletionInfo delete_info;
  delete_info.value_for_testing = "cookie2-value";
  EXPECT_FALSE(delete_info.Matches(cookie1));
  EXPECT_TRUE(delete_info.Matches(cookie2));
}

TEST(CookieDeletionInfoTest, CookieDeletionInfoMatchUrl) {
  CanonicalCookie cookie("cookie-name", "cookie-value",
                         /*domain=*/"www.example.com", "/path",
                         /*creation=*/base::Time::Now(),
                         /*expiration=*/base::Time::Max(),
                         /*last_access=*/base::Time::Now(),
                         /*secure=*/true,
                         /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
                         CookiePriority::COOKIE_PRIORITY_DEFAULT);

  CookieDeletionInfo delete_info;
  delete_info.url = GURL("https://www.example.com/path");
  EXPECT_TRUE(delete_info.Matches(cookie));

  delete_info.url = GURL("https://www.example.com/another/path");
  EXPECT_FALSE(delete_info.Matches(cookie));
}

TEST(CookieDeletionInfoTest, CookieDeletionInfoDomainMatchesDomain) {
  CookieDeletionInfo delete_info;

  const double kTestMinEpoch = 1000;
  const double kTestMaxEpoch = 10000000;
  delete_info.creation_range.SetStart(base::Time::FromDoubleT(kTestMinEpoch));
  delete_info.creation_range.SetEnd(base::Time::FromDoubleT(kTestMaxEpoch));

  auto create_cookie = [kTestMinEpoch](std::string cookie_domain) {
    CanonicalCookie cookie(
        /*name=*/"test-cookie",
        /*value=*/"cookie-value", cookie_domain,
        /*path=*/"cookie/path",
        /*creation=*/base::Time::FromDoubleT(kTestMinEpoch + 1),
        /*expiration=*/base::Time::Max(),
        /*last_access=*/base::Time::FromDoubleT(kTestMinEpoch + 1),
        /*secure=*/true,
        /*httponly=*/false,
        /*same_site=*/CookieSameSite::NO_RESTRICTION,
        /*priority=*/CookiePriority::COOKIE_PRIORITY_DEFAULT);
    return cookie;
  };

  // by default empty domain list and default match action will match.
  EXPECT_TRUE(delete_info.Matches(create_cookie("example.com")));

  const char kExtensionHostname[] = "mgndgikekgjfcpckkfioiadnlibdjbkf";

  // Only using the inclusion list because this is only testing
  // DomainMatchesDomainSet and not CookieDeletionInfo::Matches.
  delete_info.domains_and_ips_to_delete =
      std::set<std::string>({"example.com", "another.com", "192.168.0.1"});
  EXPECT_TRUE(delete_info.Matches(create_cookie(".example.com")));
  EXPECT_TRUE(delete_info.Matches(create_cookie("example.com")));
  EXPECT_TRUE(delete_info.Matches(create_cookie(".another.com")));
  EXPECT_TRUE(delete_info.Matches(create_cookie("192.168.0.1")));
  EXPECT_FALSE(delete_info.Matches(create_cookie(".nomatch.com")));
  EXPECT_FALSE(delete_info.Matches(create_cookie("192.168.0.2")));
  EXPECT_FALSE(delete_info.Matches(create_cookie(kExtensionHostname)));
}

TEST(CookieDeletionInfoTest, CookieDeletionInfoMatchesDomainList) {
  CookieDeletionInfo delete_info;

  auto create_cookie = [](std::string cookie_domain) {
    CanonicalCookie cookie(
        /*name=*/"test-cookie",
        /*value=*/"cookie-value", cookie_domain,
        /*path=*/"cookie/path",
        /*creation=*/base::Time::Now(),
        /*expiration=*/base::Time::Max(),
        /*last_access=*/base::Time::Now(),
        /*secure=*/false,
        /*httponly=*/false,
        /*same_site=*/CookieSameSite::NO_RESTRICTION,
        /*priority=*/CookiePriority::COOKIE_PRIORITY_DEFAULT);
    return cookie;
  };

  // With two empty lists (default) should match any domain.
  EXPECT_TRUE(delete_info.Matches(create_cookie("anything.com")));

  // With only an "to_delete" list.
  delete_info.domains_and_ips_to_delete =
      std::set<std::string>({"includea.com", "includeb.com"});
  EXPECT_TRUE(delete_info.Matches(create_cookie("includea.com")));
  EXPECT_TRUE(delete_info.Matches(create_cookie("includeb.com")));
  EXPECT_FALSE(delete_info.Matches(create_cookie("anything.com")));

  // With only an "to_ignore" list.
  delete_info.domains_and_ips_to_delete.clear();
  delete_info.domains_and_ips_to_ignore.insert("exclude.com");
  EXPECT_TRUE(delete_info.Matches(create_cookie("anything.com")));
  EXPECT_FALSE(delete_info.Matches(create_cookie("exclude.com")));

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
  delete_info.domains_and_ips_to_delete =
      std::set<std::string>({"left.com", "mid.com"});
  delete_info.domains_and_ips_to_ignore =
      std::set<std::string>({"mid.com", "right.com"});

  EXPECT_TRUE(delete_info.Matches(create_cookie("left.com")));
  EXPECT_FALSE(delete_info.Matches(create_cookie("mid.com")));
  EXPECT_FALSE(delete_info.Matches(create_cookie("right.com")));
  EXPECT_FALSE(delete_info.Matches(create_cookie("outside.com")));
}

// Test that Matches() works regardless of the cookie access semantics (because
// the IncludeForRequestURL call uses CookieOptions::MakeAllInclusive).
TEST(CookieDeletionInfoTest, MatchesWithCookieAccessSemantics) {
  // Cookie with unspecified SameSite.
  auto cookie =
      CanonicalCookie::Create(GURL("https://www.example.com"), "cookie=1",
                              base::Time::Now(), base::nullopt);

  {
    // With SameSite features off.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kSameSiteByDefaultCookies);

    CookieDeletionInfo delete_info;
    delete_info.url = GURL("https://www.example.com/path");
    EXPECT_TRUE(delete_info.Matches(*cookie));  // defaults to UNKNOWN
    EXPECT_TRUE(delete_info.Matches(*cookie, CookieAccessSemantics::UNKNOWN));
    EXPECT_TRUE(delete_info.Matches(*cookie, CookieAccessSemantics::LEGACY));
    EXPECT_TRUE(delete_info.Matches(*cookie, CookieAccessSemantics::NONLEGACY));
  }
  {
    // With SameSite features on.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kSameSiteByDefaultCookies);

    CookieDeletionInfo delete_info;
    delete_info.url = GURL("https://www.example.com/path");
    EXPECT_TRUE(delete_info.Matches(*cookie));  // defaults to UNKNOWN
    EXPECT_TRUE(delete_info.Matches(*cookie, CookieAccessSemantics::UNKNOWN));
    EXPECT_TRUE(delete_info.Matches(*cookie, CookieAccessSemantics::LEGACY));
    EXPECT_TRUE(delete_info.Matches(*cookie, CookieAccessSemantics::NONLEGACY));
  }
}

}  // namespace net
