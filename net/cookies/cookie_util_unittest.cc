// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/strings/string_split.h"
#include "base/test/bind_test_util.h"
#include "net/cookies/cookie_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

struct RequestCookieParsingTest {
  std::string str;
  base::StringPairs parsed;
};

void CheckParse(const std::string& str,
                const base::StringPairs& parsed_expected) {
  cookie_util::ParsedRequestCookies parsed;
  cookie_util::ParseRequestCookieLine(str, &parsed);
  EXPECT_EQ(parsed_expected, parsed);
}

void CheckSerialize(const base::StringPairs& parsed,
                    const std::string& str_expected) {
  EXPECT_EQ(str_expected, cookie_util::SerializeRequestCookieLine(parsed));
}

TEST(CookieUtilTest, TestDomainIsHostOnly) {
  const struct {
    const char* str;
    const bool is_host_only;
  } tests[] = {{"", true}, {"www.foo.com", true}, {".foo.com", false}};

  for (const auto& test : tests) {
    EXPECT_EQ(test.is_host_only, cookie_util::DomainIsHostOnly(test.str));
  }
}

TEST(CookieUtilTest, TestCookieDateParsing) {
  const struct {
    const char* str;
    const bool valid;
    const time_t epoch;
  } tests[] = {
      {"Sat, 15-Apr-17 21:01:22 GMT", true, 1492290082},
      {"Thu, 19-Apr-2007 16:00:00 GMT", true, 1176998400},
      {"Wed, 25 Apr 2007 21:02:13 GMT", true, 1177534933},
      {"Thu, 19/Apr\\2007 16:00:00 GMT", true, 1176998400},
      {"Fri, 1 Jan 2010 01:01:50 GMT", true, 1262307710},
      {"Wednesday, 1-Jan-2003 00:00:00 GMT", true, 1041379200},
      {", 1-Jan-2003 00:00:00 GMT", true, 1041379200},
      {" 1-Jan-2003 00:00:00 GMT", true, 1041379200},
      {"1-Jan-2003 00:00:00 GMT", true, 1041379200},
      {"Wed,18-Apr-07 22:50:12 GMT", true, 1176936612},
      {"WillyWonka  , 18-Apr-07 22:50:12 GMT", true, 1176936612},
      {"WillyWonka  , 18-Apr-07 22:50:12", true, 1176936612},
      {"WillyWonka  ,  18-apr-07   22:50:12", true, 1176936612},
      {"Mon, 18-Apr-1977 22:50:13 GMT", true, 230251813},
      {"Mon, 18-Apr-77 22:50:13 GMT", true, 230251813},
      // If the cookie came in with the expiration quoted (which in terms of
      // the RFC you shouldn't do), we will get string quoted.  Bug 1261605.
      {"\"Sat, 15-Apr-17\\\"21:01:22\\\"GMT\"", true, 1492290082},
      // Test with full month names and partial names.
      {"Partyday, 18- April-07 22:50:12", true, 1176936612},
      {"Partyday, 18 - Apri-07 22:50:12", true, 1176936612},
      {"Wednes, 1-Januar-2003 00:00:00 GMT", true, 1041379200},
      // Test that we always take GMT even with other time zones or bogus
      // values.  The RFC says everything should be GMT, and in the worst case
      // we are 24 hours off because of zone issues.
      {"Sat, 15-Apr-17 21:01:22", true, 1492290082},
      {"Sat, 15-Apr-17 21:01:22 GMT-2", true, 1492290082},
      {"Sat, 15-Apr-17 21:01:22 GMT BLAH", true, 1492290082},
      {"Sat, 15-Apr-17 21:01:22 GMT-0400", true, 1492290082},
      {"Sat, 15-Apr-17 21:01:22 GMT-0400 (EDT)", true, 1492290082},
      {"Sat, 15-Apr-17 21:01:22 DST", true, 1492290082},
      {"Sat, 15-Apr-17 21:01:22 -0400", true, 1492290082},
      {"Sat, 15-Apr-17 21:01:22 (hello there)", true, 1492290082},
      // Test that if we encounter multiple : fields, that we take the first
      // that correctly parses.
      {"Sat, 15-Apr-17 21:01:22 11:22:33", true, 1492290082},
      {"Sat, 15-Apr-17 ::00 21:01:22", true, 1492290082},
      {"Sat, 15-Apr-17 boink:z 21:01:22", true, 1492290082},
      // We take the first, which in this case is invalid.
      {"Sat, 15-Apr-17 91:22:33 21:01:22", false, 0},
      // amazon.com formats their cookie expiration like this.
      {"Thu Apr 18 22:50:12 2007 GMT", true, 1176936612},
      // Test that hh:mm:ss can occur anywhere.
      {"22:50:12 Thu Apr 18 2007 GMT", true, 1176936612},
      {"Thu 22:50:12 Apr 18 2007 GMT", true, 1176936612},
      {"Thu Apr 22:50:12 18 2007 GMT", true, 1176936612},
      {"Thu Apr 18 22:50:12 2007 GMT", true, 1176936612},
      {"Thu Apr 18 2007 22:50:12 GMT", true, 1176936612},
      {"Thu Apr 18 2007 GMT 22:50:12", true, 1176936612},
      // Test that the day and year can be anywhere if they are unambigious.
      {"Sat, 15-Apr-17 21:01:22 GMT", true, 1492290082},
      {"15-Sat, Apr-17 21:01:22 GMT", true, 1492290082},
      {"15-Sat, Apr 21:01:22 GMT 17", true, 1492290082},
      {"15-Sat, Apr 21:01:22 GMT 2017", true, 1492290082},
      {"15 Apr 21:01:22 2017", true, 1492290082},
      {"15 17 Apr 21:01:22", true, 1492290082},
      {"Apr 15 17 21:01:22", true, 1492290082},
      {"Apr 15 21:01:22 17", true, 1492290082},
      {"2017 April 15 21:01:22", true, 1492290082},
      {"15 April 2017 21:01:22", true, 1492290082},
      // Some invalid dates
      {"98 April 17 21:01:22", false, 0},
      {"Thu, 012-Aug-2008 20:49:07 GMT", false, 0},
      {"Thu, 12-Aug-9999999999 20:49:07 GMT", false, 0},
      {"Thu, 999999999999-Aug-2007 20:49:07 GMT", false, 0},
      {"Thu, 12-Aug-2007 20:61:99999999999 GMT", false, 0},
      {"IAintNoDateFool", false, 0},
      {"1600 April 33 21:01:22", false, 0},
      {"1970 April 33 21:01:22", false, 0},
      {"Thu, 33-Aug-31841 20:49:07 GMT", false, 0},
  };

  base::Time parsed_time;
  for (const auto& test : tests) {
    parsed_time = cookie_util::ParseCookieExpirationTime(test.str);
    if (!test.valid) {
      EXPECT_TRUE(parsed_time.is_null()) << test.str;
      continue;
    }
    EXPECT_TRUE(!parsed_time.is_null()) << test.str;
    EXPECT_EQ(test.epoch, parsed_time.ToTimeT()) << test.str;
  }
}

// Tests parsing dates that are beyond 2038. 32-bit (non-Mac) POSIX systems are
// incapable of doing this, however the expectation is for cookie parsing to
// succeed anyway (and return the minimum value Time::FromUTCExploded() can
// parse on the current platform). Also checks a date outside the limit on
// Windows, which is year 30827.
TEST(CookieUtilTest, ParseCookieExpirationTimeBeyond2038) {
  const char* kTests[] = {
      "Thu, 12-Aug-31841 20:49:07 GMT", "2039 April 15 21:01:22",
      "2039 April 15 21:01:22", "2038 April 15 21:01:22",
  };

  for (auto* test : kTests) {
    base::Time parsed_time = cookie_util::ParseCookieExpirationTime(test);
    EXPECT_FALSE(parsed_time.is_null());

    // It should either have an exact value, or be base::Time::Max(). For
    // simplicity just check that it is greater than an arbitray date.
    base::Time almost_jan_2038 =
        base::Time::UnixEpoch() + base::TimeDelta::FromDays(365 * 68);
    EXPECT_LT(almost_jan_2038, parsed_time);
  }
}

// Tests parsing dates that are prior to (or around) 1970. Non-Mac POSIX systems
// are incapable of doing this, however the expectation is for cookie parsing to
// succeed anyway (and return a minimal base::Time).
TEST(CookieUtilTest, ParseCookieExpirationTimeBefore1970) {
  const char* kTests[] = {
      // Times around the Unix epoch.
      "1970 Jan 1 00:00:00", "1969 March 3 21:01:22",
      // Times around the Windows epoch.
      "1601 Jan 1 00:00:00", "1600 April 15 21:01:22",
      // Times around kExplodedMinYear on Mac.
      "1902 Jan 1 00:00:00", "1901 Jan 1 00:00:00",
  };

  for (auto* test : kTests) {
    base::Time parsed_time = cookie_util::ParseCookieExpirationTime(test);
    EXPECT_FALSE(parsed_time.is_null()) << test;

    // It should either have an exact value, or should be base::Time(1)
    // For simplicity just check that it is less than the unix epoch.
    EXPECT_LE(parsed_time, base::Time::UnixEpoch()) << test;
  }
}

TEST(CookieUtilTest, TestRequestCookieParsing) {
  std::vector<RequestCookieParsingTest> tests;

  // Simple case.
  tests.push_back(RequestCookieParsingTest());
  tests.back().str = "key=value";
  tests.back().parsed.push_back(std::make_pair(std::string("key"),
                                               std::string("value")));
  // Multiple key/value pairs.
  tests.push_back(RequestCookieParsingTest());
  tests.back().str = "key1=value1; key2=value2";
  tests.back().parsed.push_back(std::make_pair(std::string("key1"),
                                               std::string("value1")));
  tests.back().parsed.push_back(std::make_pair(std::string("key2"),
                                               std::string("value2")));
  // Empty value.
  tests.push_back(RequestCookieParsingTest());
  tests.back().str = "key=; otherkey=1234";
  tests.back().parsed.push_back(std::make_pair(std::string("key"),
                                               std::string()));
  tests.back().parsed.push_back(std::make_pair(std::string("otherkey"),
                                               std::string("1234")));
  // Special characters (including equals signs) in value.
  tests.push_back(RequestCookieParsingTest());
  tests.back().str = "key=; a2=s=(./&t=:&u=a#$; a3=+~";
  tests.back().parsed.push_back(std::make_pair(std::string("key"),
                                               std::string()));
  tests.back().parsed.push_back(std::make_pair(std::string("a2"),
                                               std::string("s=(./&t=:&u=a#$")));
  tests.back().parsed.push_back(std::make_pair(std::string("a3"),
                                               std::string("+~")));
  // Quoted value.
  tests.push_back(RequestCookieParsingTest());
  tests.back().str = "key=\"abcdef\"; otherkey=1234";
  tests.back().parsed.push_back(std::make_pair(std::string("key"),
                                               std::string("\"abcdef\"")));
  tests.back().parsed.push_back(std::make_pair(std::string("otherkey"),
                                               std::string("1234")));

  for (size_t i = 0; i < tests.size(); i++) {
    SCOPED_TRACE(testing::Message() << "Test " << i);
    CheckParse(tests[i].str, tests[i].parsed);
    CheckSerialize(tests[i].parsed, tests[i].str);
  }
}

TEST(CookieUtilTest, CookieDomainAndPathToURL) {
  struct {
    std::string domain;
    std::string path;
    bool is_https;
    std::string expected_url;
  } kTests[]{
      {"a.com", "/", true, "https://a.com/"},
      {"a.com", "/", false, "http://a.com/"},
      {".a.com", "/", true, "https://a.com/"},
      {".a.com", "/", false, "http://a.com/"},
      {"b.a.com", "/", true, "https://b.a.com/"},
      {"b.a.com", "/", false, "http://b.a.com/"},
      {"a.com", "/example/path", true, "https://a.com/example/path"},
      {".a.com", "/example/path", false, "http://a.com/example/path"},
      {"b.a.com", "/example/path", true, "https://b.a.com/example/path"},
      {".b.a.com", "/example/path", false, "http://b.a.com/example/path"},
  };

  for (auto& test : kTests) {
    GURL url1 = cookie_util::CookieDomainAndPathToURL(test.domain, test.path,
                                                      test.is_https);
    GURL url2 = cookie_util::CookieDomainAndPathToURL(
        test.domain, test.path, std::string(test.is_https ? "https" : "http"));
    // Test both overloads for equality.
    EXPECT_EQ(url1, url2);
    EXPECT_EQ(url1, GURL(test.expected_url));
  }
}

TEST(CookieUtilTest, SimulatedCookieSource) {
  GURL secure_url("https://b.a.com");
  GURL insecure_url("http://b.a.com");

  struct {
    std::string cookie;
    std::string source_scheme;
    std::string expected_simulated_source;
  } kTests[]{
      {"cookie=foo", "http", "http://b.a.com/"},
      {"cookie=foo", "https", "https://b.a.com/"},
      {"cookie=foo", "wss", "wss://b.a.com/"},
      {"cookie=foo", "file", "file://b.a.com/"},
      {"cookie=foo; Domain=b.a.com", "https", "https://b.a.com/"},
      {"cookie=foo; Domain=a.com", "https", "https://a.com/"},
      {"cookie=foo; Domain=.b.a.com", "https", "https://b.a.com/"},
      {"cookie=foo; Domain=.a.com", "https", "https://a.com/"},
      {"cookie=foo; Path=/", "https", "https://b.a.com/"},
      {"cookie=foo; Path=/bar", "https", "https://b.a.com/bar"},
      {"cookie=foo; Domain=b.a.com; Path=/", "https", "https://b.a.com/"},
      {"cookie=foo; Domain=b.a.com; Path=/bar", "https", "https://b.a.com/bar"},
      {"cookie=foo; Domain=a.com; Path=/", "https", "https://a.com/"},
      {"cookie=foo; Domain=a.com; Path=/bar", "https", "https://a.com/bar"},
  };

  for (const auto& test : kTests) {
    std::vector<std::unique_ptr<CanonicalCookie>> cookies;
    // It shouldn't depend on the cookie's secureness or actual source scheme.
    cookies.push_back(CanonicalCookie::Create(
        insecure_url, test.cookie, base::Time::Now(), base::nullopt));
    cookies.push_back(CanonicalCookie::Create(
        secure_url, test.cookie, base::Time::Now(), base::nullopt));
    cookies.push_back(
        CanonicalCookie::Create(secure_url, test.cookie + "; Secure",
                                base::Time::Now(), base::nullopt));
    for (const auto& cookie : cookies) {
      GURL simulated_source =
          cookie_util::SimulatedCookieSource(*cookie, test.source_scheme);
      EXPECT_EQ(GURL(test.expected_simulated_source), simulated_source);
    }
  }
}

TEST(CookieUtilTest, TestGetEffectiveDomain) {
  // Note: registry_controlled_domains::GetDomainAndRegistry is tested in its
  // own unittests.
  EXPECT_EQ("example.com",
            cookie_util::GetEffectiveDomain("http", "www.example.com"));
  EXPECT_EQ("example.com",
            cookie_util::GetEffectiveDomain("https", "www.example.com"));
  EXPECT_EQ("example.com",
            cookie_util::GetEffectiveDomain("ws", "www.example.com"));
  EXPECT_EQ("example.com",
            cookie_util::GetEffectiveDomain("wss", "www.example.com"));
  EXPECT_EQ("www.example.com",
            cookie_util::GetEffectiveDomain("ftp", "www.example.com"));
}

TEST(CookieUtilTest, TestIsDomainMatch) {
  EXPECT_TRUE(cookie_util::IsDomainMatch("example.com", "example.com"));
  EXPECT_FALSE(cookie_util::IsDomainMatch("www.example.com", "example.com"));

  EXPECT_TRUE(cookie_util::IsDomainMatch(".example.com", "example.com"));
  EXPECT_TRUE(cookie_util::IsDomainMatch(".example.com", "www.example.com"));
  EXPECT_FALSE(cookie_util::IsDomainMatch(".www.example.com", "example.com"));

  EXPECT_FALSE(cookie_util::IsDomainMatch("example.com", "example.de"));
  EXPECT_FALSE(cookie_util::IsDomainMatch(".example.com", "example.de"));
  EXPECT_FALSE(cookie_util::IsDomainMatch(".example.de", "example.de.vu"));
}

TEST(CookieUtilTest, TestComputeSameSiteContextForScriptGet) {
  using SameSiteCookieContext = CookieOptions::SameSiteCookieContext;
  // |site_for_cookies| not matching the URL -> it's cross-site.
  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://notexample.com")),
          base::nullopt /*initiator*/,
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("https://example.com"),
          SiteForCookies::FromUrl(GURL("http://notexample.com")),
          base::nullopt /*initiator*/,
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("https://notexample.com")),
          base::nullopt /*initiator*/,
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://notexample.com")),
          url::Origin::Create(GURL("http://example.com")),
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("https://example.com"),
          SiteForCookies::FromUrl(GURL("http://notexample.com")),
          url::Origin::Create(GURL("http://example.com")),
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("https://notexample.com")),
          url::Origin::Create(GURL("http://example.com")),
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://a.com"), SiteForCookies::FromUrl(GURL("http://b.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          false /* force_ignore_site_for_cookies */));

  // |site_for_cookies| not being schemefully_same -> it's cross-site.
  SiteForCookies insecure_not_schemefully_same =
      SiteForCookies::FromUrl(GURL("http://example.com"));
  insecure_not_schemefully_same.SetSchemefullySameForTesting(false);
  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://example.com"), insecure_not_schemefully_same,
                url::Origin::Create(GURL("http://example.com")),
                false /* force_ignore_site_for_cookies */));

  SiteForCookies secure_not_schemefully_same =
      SiteForCookies::FromUrl(GURL("https://example.com"));
  secure_not_schemefully_same.SetSchemefullySameForTesting(false);
  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("https://example.com"), secure_not_schemefully_same,
                url::Origin::Create(GURL("https://example.com")),
                false /* force_ignore_site_for_cookies */));

  // Same |site_for_cookies|, but not |initiator| -> it's same-site lax.
  EXPECT_EQ(
      CookieOptions::SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          false /* force_ignore_site_for_cookies */));

  // This isn't a full on origin check --- subdomains and different schema are
  // accepted. For SameSiteCookieContext::schemeful_context the scheme is
  // considered.
  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::SAME_SITE_LAX),
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("http://example.com")),
                url::Origin::Create(GURL("https://example.com")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::SAME_SITE_LAX),
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("https://example.com"),
                SiteForCookies::FromUrl(GURL("https://example.com")),
                url::Origin::Create(GURL("http://example.com")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX,
                            SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("https://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX,
                            SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("https://example.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://sub.example.com"),
          SiteForCookies::FromUrl(GURL("http://sub2.example.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://sub.example.com"),
          SiteForCookies::FromUrl(GURL("http://sub.example.com:8080")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          false /* force_ignore_site_for_cookies */));

  // wss/https and http/ws are considered the same for schemeful purposes.
  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("wss://example.com"),
                SiteForCookies::FromUrl(GURL("https://example.com")),
                url::Origin::Create(GURL("https://example.com")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("ws://example.com"),
                SiteForCookies::FromUrl(GURL("http://example.com")),
                url::Origin::Create(GURL("http://example.com")),
                false /* force_ignore_site_for_cookies */));

  // nullopt |initiator| is trusted for purposes of strict, an opaque one isn't.
  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("http://example.com")),
                url::Origin::Create(GURL("http://example.com")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("https://example.com"),
                SiteForCookies::FromUrl(GURL("http://example.com")),
                base::nullopt /*initiator*/,
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("https://example.com")),
                base::nullopt /*initiator*/,
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          base::nullopt /*initiator*/,
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")), url::Origin(),
          false /* force_ignore_site_for_cookies */));

  // |force_ignore_site_for_cookies| causes SAME_SITE_STRICT to be
  // returned.
  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://notexample.com")),
          base::nullopt /*initiator*/,
          true /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://notexample.com")),
          url::Origin::Create(GURL("http://example.com")),
          true /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://a.com"), SiteForCookies::FromUrl(GURL("http://b.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          true /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          true /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("https://example.com"),
                SiteForCookies::FromUrl(GURL("http://example.com")),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                true /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("https://example.com")),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                true /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://sub.example.com"),
          SiteForCookies::FromUrl(GURL("http://sub2.example.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          true /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://sub.example.com"),
          SiteForCookies::FromUrl(GURL("http://sub.example.com:8080")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          true /* force_ignore_site_for_cookies */));
}

TEST(CookieUtilTest, ComputeSameSiteContextForRequest) {
  using SameSiteCookieContext = CookieOptions::SameSiteCookieContext;
  EXPECT_EQ(SameSiteCookieContext(
                CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("http://notexample.com")),
                base::nullopt /*initiator*/,
                false /*force_ignore_site_for_cookies*/));
  EXPECT_EQ(SameSiteCookieContext(
                CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("https://example.com"),
                SiteForCookies::FromUrl(GURL("http://notexample.com")),
                base::nullopt /*initiator*/,
                false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(SameSiteCookieContext(
                CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("https://notexample.com")),
                base::nullopt /*initiator*/,
                false /*force_ignore_site_for_cookies*/));

  // |site_for_cookies| not being schemefully_same -> it's cross-site.
  SiteForCookies insecure_not_schemefully_same =
      SiteForCookies::FromUrl(GURL("http://example.com"));
  insecure_not_schemefully_same.SetSchemefullySameForTesting(false);
  EXPECT_EQ(
      SameSiteCookieContext(
          SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
          SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForRequest(
          "GET", GURL("http://example.com"), insecure_not_schemefully_same,
          url::Origin::Create(GURL("http://example.com")),
          false /* force_ignore_site_for_cookies */));

  SiteForCookies secure_not_schemefully_same =
      SiteForCookies::FromUrl(GURL("https://example.com"));
  secure_not_schemefully_same.SetSchemefullySameForTesting(false);
  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("https://example.com"), secure_not_schemefully_same,
                url::Origin::Create(GURL("https://example.com")),
                false /* force_ignore_site_for_cookies */));

  // |force_ignore_site_for_cookies| = true bypasses all checks.
  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
      cookie_util::ComputeSameSiteContextForRequest(
          "GET", GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          true /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
      cookie_util::ComputeSameSiteContextForRequest(
          "POST", GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          true /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
      cookie_util::ComputeSameSiteContextForRequest(
          "GET", GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://question.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          true /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
      cookie_util::ComputeSameSiteContextForRequest(
          "GET", GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          url::Origin::Create(GURL("http://example.com")),
          false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
      cookie_util::ComputeSameSiteContextForRequest(
          "POST", GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          url::Origin::Create(GURL("http://example.com")),
          false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("https://example.com"),
                SiteForCookies::FromUrl(GURL("http://example.com")),
                url::Origin::Create(GURL("http://example.com")),
                false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", GURL("https://example.com"),
                SiteForCookies::FromUrl(GURL("http://example.com")),
                url::Origin::Create(GURL("http://example.com")),
                false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("https://example.com")),
                url::Origin::Create(GURL("http://example.com")),
                false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("https://example.com")),
                url::Origin::Create(GURL("http://example.com")),
                false /*force_ignore_site_for_cookies*/));

  // Normally, lax requests also require a safe method.
  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::SAME_SITE_LAX),
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("https://example.com"),
                SiteForCookies::FromUrl(GURL("https://example.com")),
                url::Origin::Create(GURL("http://example.com")),
                false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::SAME_SITE_LAX),
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("http://example.com")),
                url::Origin::Create(GURL("https://example.com")),
                false /*force_ignore_site_for_cookies*/));

  // wss/https and http/ws are considered the same for schemeful purposes.
  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("wss://example.com"),
                SiteForCookies::FromUrl(GURL("https://example.com")),
                url::Origin::Create(GURL("https://example.com")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("ws://example.com"),
                SiteForCookies::FromUrl(GURL("http://example.com")),
                url::Origin::Create(GURL("http://example.com")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForRequest(
          "GET", GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForRequest(
          "HEAD", GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX,
                            SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForRequest(
          "GET", GURL("https://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX,
                            SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForRequest(
          "GET", GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("https://example.com")),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(
      SameSiteCookieContext(
          SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
          SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE),
      cookie_util::ComputeSameSiteContextForRequest(
          "POST", GURL("https://example.com"),
          SiteForCookies::FromUrl(GURL("https://example.com")),
          url::Origin::Create(GURL("http://example.com")),
          false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(
      SameSiteCookieContext(
          SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
          SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE),
      cookie_util::ComputeSameSiteContextForRequest(
          "POST", GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          url::Origin::Create(GURL("https://example.com")),
          false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(SameSiteCookieContext(CookieOptions::SameSiteCookieContext::
                                      ContextType::SAME_SITE_LAX_METHOD_UNSAFE),
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("http://example.com")),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE,
                SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", GURL("https://example.com"),
                SiteForCookies::FromUrl(GURL("http://example.com")),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /*force_ignore_site_for_cookies*/));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_LAX_METHOD_UNSAFE,
                SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("https://example.com")),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /*force_ignore_site_for_cookies*/));
}

TEST(CookieUtilTest, ComputeSameSiteContextForSet) {
  using SameSiteCookieContext = CookieOptions::SameSiteCookieContext;
  EXPECT_EQ(SameSiteCookieContext(
                CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForResponse(
                GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("http://notexample.com")),
                base::nullopt, false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForResponse(
                GURL("https://example.com"),
                SiteForCookies::FromUrl(GURL("http://notexample.com")),
                base::nullopt, false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForResponse(
                GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("https://notexample.com")),
                base::nullopt, false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX,
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForResponse(
          GURL("https://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")), base::nullopt,
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX,
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForResponse(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("https://example.com")), base::nullopt,
          false /* force_ignore_site_for_cookies */));

  // Same as above except |force_ignore_site_for_cookies| makes it return LAX.
  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForResponse(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://notexample.com")), base::nullopt,
          true /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForResponse(
          GURL("https://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")), base::nullopt,
          true /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForResponse(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("https://example.com")), base::nullopt,
          true /* force_ignore_site_for_cookies */));

  // |site_for_cookies| not being schemefully_same -> it's cross-site.
  SiteForCookies insecure_not_schemefully_same =
      SiteForCookies::FromUrl(GURL("http://example.com"));
  insecure_not_schemefully_same.SetSchemefullySameForTesting(false);
  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX,
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForResponse(
          GURL("http://example.com"), insecure_not_schemefully_same,
          base::nullopt, false /* force_ignore_site_for_cookies */));

  SiteForCookies secure_not_schemefully_same =
      SiteForCookies::FromUrl(GURL("https://example.com"));
  secure_not_schemefully_same.SetSchemefullySameForTesting(false);
  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX,
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForResponse(
          GURL("https://example.com"), secure_not_schemefully_same,
          base::nullopt, false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForScriptSet(
                GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("http://notexample.com")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForScriptSet(
                GURL("https://example.com"),
                SiteForCookies::FromUrl(GURL("http://notexample.com")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForScriptSet(
                GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("https://notexample.com")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX,
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptSet(
          GURL("https://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX,
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptSet(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("https://example.com")),
          false /* force_ignore_site_for_cookies */));

  // Same as above except |force_ignore_site_for_cookies| makes it return LAX.
  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForScriptSet(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://notexample.com")),
          true /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForScriptSet(
          GURL("https://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          true /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForScriptSet(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("https://example.com")),
          true /* force_ignore_site_for_cookies */));

  // |site_for_cookies| not being schemefully_same -> it's cross-site.
  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX,
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptSet(
          GURL("http://example.com"), insecure_not_schemefully_same,
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX,
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptSet(
          GURL("https://example.com"), secure_not_schemefully_same,
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForResponse(
          GURL("http://example.com/dir"),
          SiteForCookies::FromUrl(GURL("http://sub.example.com")),
          base::nullopt, false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForResponse(
          GURL("http://example.com/dir"),
          SiteForCookies::FromUrl(GURL("http://sub.example.com")),
          base::nullopt, true /* force_ignore_site_for_cookies */));

  // wss/https and http/ws are considered the same for schemeful purposes.
  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForResponse(
          GURL("ws://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")), base::nullopt,
          true /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForResponse(
          GURL("wss://example.com"),
          SiteForCookies::FromUrl(GURL("https://example.com")), base::nullopt,
          true /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_LAX,
                CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForResponse(
                GURL("http://example.com/dir"),
                SiteForCookies::FromUrl(GURL("https://sub.example.com")),
                base::nullopt, false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForResponse(
          GURL("http://example.com/dir"),
          SiteForCookies::FromUrl(GURL("https://sub.example.com")),
          base::nullopt, true /* force_ignore_site_for_cookies */));
  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_LAX,
                CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForResponse(
                GURL("https://example.com/dir"),
                SiteForCookies::FromUrl(GURL("http://sub.example.com")),
                base::nullopt, false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForResponse(
          GURL("https://example.com/dir"),
          SiteForCookies::FromUrl(GURL("http://sub.example.com")),
          base::nullopt, true /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForScriptSet(
          GURL("http://example.com/dir"),
          SiteForCookies::FromUrl(GURL("http://sub.example.com")),
          false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForScriptSet(
          GURL("ws://example.com"),
          SiteForCookies::FromUrl(GURL("http://example.com")),
          false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX),
      cookie_util::ComputeSameSiteContextForScriptSet(
          GURL("wss://example.com"),
          SiteForCookies::FromUrl(GURL("https://example.com")),
          false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX,
                            SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptSet(
          GURL("http://example.com/dir"),
          SiteForCookies::FromUrl(GURL("https://sub.example.com")),
          false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::SAME_SITE_LAX,
                            SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForScriptSet(
          GURL("https://example.com/dir"),
          SiteForCookies::FromUrl(GURL("http://sub.example.com")),
          false /* force_ignore_site_for_cookies */));
}

TEST(CookieUtilTest, TestComputeSameSiteContextForSubresource) {
  using SameSiteCookieContext = CookieOptions::SameSiteCookieContext;
  // |site_for_cookies| not matching the URL -> it's cross-site.
  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForSubresource(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("http://notexample.com")),
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForSubresource(
          GURL("https://example.com"),
          SiteForCookies::FromUrl(GURL("http://notexample.com")),
          false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(
      SameSiteCookieContext(SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForSubresource(
          GURL("http://example.com"),
          SiteForCookies::FromUrl(GURL("https://notexample.com")),
          false /* force_ignore_site_for_cookies */));

  // Same as above except |force_ignore_site_for_cookies| makes it return
  // STRICT.
  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
            cookie_util::ComputeSameSiteContextForSubresource(
                GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("http://notexample.com")),
                true /* force_ignore_site_for_cookies */));

  // |site_for_cookies| not being schemefully_same -> it's cross-site.
  SiteForCookies insecure_not_schemefully_same =
      SiteForCookies::FromUrl(GURL("http://example.com"));
  insecure_not_schemefully_same.SetSchemefullySameForTesting(false);
  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForSubresource(
          GURL("http://example.com"), insecure_not_schemefully_same,
          false /* force_ignore_site_for_cookies */));

  SiteForCookies secure_not_schemefully_same =
      SiteForCookies::FromUrl(GURL("https://example.com"));
  secure_not_schemefully_same.SetSchemefullySameForTesting(false);
  EXPECT_EQ(
      SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      cookie_util::ComputeSameSiteContextForSubresource(
          GURL("https://example.com"), secure_not_schemefully_same,
          false /* force_ignore_site_for_cookies */));

  // This isn't a full on origin check --- subdomains and different schema are
  // accepted. For SameSiteCookieContext::schemeful_context the scheme is
  // considered.
  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForSubresource(
                GURL("https://example.com"),
                SiteForCookies::FromUrl(GURL("http://example.com")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForSubresource(
                GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("https://example.com")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
                SameSiteCookieContext::ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForSubresource(
                GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("https://example.com")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
            cookie_util::ComputeSameSiteContextForSubresource(
                GURL("http://sub.example.com"),
                SiteForCookies::FromUrl(GURL("http://sub2.example.com")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
            cookie_util::ComputeSameSiteContextForSubresource(
                GURL("http://sub.example.com"),
                SiteForCookies::FromUrl(GURL("http://sub.example.com:8080")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
            cookie_util::ComputeSameSiteContextForSubresource(
                GURL("http://example.com"),
                SiteForCookies::FromUrl(GURL("http://example.com")),
                false /* force_ignore_site_for_cookies */));

  // wss/https and http/ws are considered the same for schemeful purposes.
  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
            cookie_util::ComputeSameSiteContextForSubresource(
                GURL("ws://example.com"),
                SiteForCookies::FromUrl(GURL("http://example.com")),
                false /* force_ignore_site_for_cookies */));

  EXPECT_EQ(SameSiteCookieContext(
                SameSiteCookieContext::ContextType::SAME_SITE_STRICT),
            cookie_util::ComputeSameSiteContextForSubresource(
                GURL("wss://example.com"),
                SiteForCookies::FromUrl(GURL("https://example.com")),
                false /* force_ignore_site_for_cookies */));
}

TEST(CookieUtilTest, AdaptCookieAccessResultToBool) {
  bool result_out = true;
  base::OnceCallback<void(bool)> callback = base::BindLambdaForTesting(
      [&result_out](bool result) { result_out = result; });

  base::OnceCallback<void(CookieAccessResult)> adapted_callback =
      cookie_util::AdaptCookieAccessResultToBool(std::move(callback));

  std::move(adapted_callback)
      .Run(CookieAccessResult(
          CookieInclusionStatus(CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR)));

  EXPECT_FALSE(result_out);

  result_out = false;
  callback = base::BindLambdaForTesting(
      [&result_out](bool result) { result_out = result; });

  adapted_callback =
      cookie_util::AdaptCookieAccessResultToBool(std::move(callback));

  std::move(adapted_callback).Run(CookieAccessResult());

  EXPECT_TRUE(result_out);
}

TEST(CookieUtilTest, IsSameSiteCompatPair) {
  ASSERT_EQ(3, cookie_util::kMinCompatPairNameLength)
      << "This test assumes that SameSite compatibility pairs have cookie name "
         "length at least 3.";
  GURL url("https://www.site.example/path");

  struct {
    const char* cookie_line_1;
    const char* cookie_line_2;
    bool expected_is_same_site_compat_pair;
  } kTestCases[] = {
      // Matching cases
      {"name=value; SameSite=None; Secure", "name_legacy=value", true},
      {"uid=value; SameSite=None; Secure", "uid_old=value", true},
      {"name=value; SameSite=None; Secure", "name2=value; Secure", true},
      {"name_samesite=value; SameSite=None; Secure", "name=value", true},
      {"__Secure-name=value; SameSite=None; Secure", "name=value", true},
      {"__Secure-3Pname=value; SameSite=None; Secure", "name=value", true},
      {"name=value; SameSite=None; Secure; HttpOnly", "name_legacy=value",
       true},
      {"name=value; SameSite=None; Secure; Domain=site.example",
       "name_legacy=value; Secure; Domain=site.example", true},
      // Fails because cookies are equivalent
      {"name=value; SameSite=None; Secure", "name=value", false},
      // Fails SameSite criterion
      {"name=value", "name_legacy=value", false},
      {"name=value; SameSite=None", "name_legacy=value", false},
      {"name=value; SameSite=None; Secure", "name_legacy=value; SameSite=None",
       false},
      {"name=value; SameSite=None; Secure",
       "name_legacy=value; SameSite=None; Secure", false},
      // Fails Domain criterion
      {"name=value; SameSite=None; Secure; Domain=site.example",
       "name_legacy=value", false},
      {"name=value; SameSite=None; Secure; Domain=www.site.example",
       "name_legacy=value", false},
      {"name=value; SameSite=None; Secure",
       "name_legacy=value; Domain=site.example", false},
      {"name=value; SameSite=None; Secure",
       "name_legacy=value; Domain=www.site.example", false},
      // Fails Path criterion
      {"name=value; SameSite=None; Secure; Path=/path", "name_legacy=value",
       false},
      {"name=value; SameSite=None; Secure; Path=/path",
       "name_legacy=value; Path=/", false},
      {"name=value; SameSite=None; Secure; Path=/",
       "name_legacy=value; Path=/path", false},
      {"name=value; SameSite=None; Secure", "name_legacy=value; Path=/path",
       false},
      // Fails value criterion
      {"name=value; SameSite=None; Secure", "name_legacy=foobar", false},
      {"name=value; SameSite=None; Secure", "name_legacy=value2", false},
      // Fails name length criterion
      {"id=value; SameSite=None; Secure", "id_legacy=value", false},
      {"id_samesite=value; SameSite=None; Secure", "id=value", false},
      {"value; SameSite=None; Secure", "legacy=value", false},
      // Fails suffix/prefix criterion
      {"name_samesite=value; SameSite=None; Secure", "name_legacy=value",
       false},
      {"name1=value; SameSite=None; Secure", "name2=value", false},
  };

  for (const auto& test_case : kTestCases) {
    auto cookie1 = CanonicalCookie::Create(url, test_case.cookie_line_1,
                                           base::Time::Now(), base::nullopt);
    auto cookie2 = CanonicalCookie::Create(url, test_case.cookie_line_2,
                                           base::Time::Now(), base::nullopt);

    ASSERT_TRUE(cookie1);
    ASSERT_TRUE(cookie2);
    EXPECT_EQ(test_case.expected_is_same_site_compat_pair,
              cookie_util::IsSameSiteCompatPair(
                  *cookie1, *cookie2, CookieOptions::MakeAllInclusive()));
    EXPECT_EQ(test_case.expected_is_same_site_compat_pair,
              cookie_util::IsSameSiteCompatPair(
                  *cookie2, *cookie1, CookieOptions::MakeAllInclusive()));
  }
}

TEST(CookieUtilTest, IsSameSiteCompatPair_HttpOnly) {
  GURL url("https://www.site.example/path");
  auto new_cookie =
      CanonicalCookie::Create(url, "name=value; SameSite=None; Secure",
                              base::Time::Now(), base::nullopt);
  auto legacy_cookie = CanonicalCookie::Create(
      url, "name_legacy=value", base::Time::Now(), base::nullopt);
  auto http_only_new_cookie = CanonicalCookie::Create(
      url, "name=value; SameSite=None; Secure; HttpOnly", base::Time::Now(),
      base::nullopt);
  auto http_only_legacy_cookie = CanonicalCookie::Create(
      url, "name_legacy=value; HttpOnly", base::Time::Now(), base::nullopt);
  ASSERT_TRUE(new_cookie);
  ASSERT_TRUE(legacy_cookie);
  ASSERT_TRUE(http_only_new_cookie);
  ASSERT_TRUE(http_only_legacy_cookie);

  // Allows HttpOnly access.
  CookieOptions inclusive_options = CookieOptions::MakeAllInclusive();
  // Disallows HttpOnly access.
  CookieOptions restrictive_options;
  // Allows SameSite but not HttpOnly access. (SameSite shouldn't matter.)
  CookieOptions same_site_options;
  same_site_options.set_same_site_cookie_context(
      CookieOptions::SameSiteCookieContext::MakeInclusive());

  EXPECT_TRUE(cookie_util::IsSameSiteCompatPair(*new_cookie, *legacy_cookie,
                                                inclusive_options));
  EXPECT_TRUE(cookie_util::IsSameSiteCompatPair(
      *http_only_new_cookie, *legacy_cookie, inclusive_options));
  EXPECT_TRUE(cookie_util::IsSameSiteCompatPair(
      *new_cookie, *http_only_legacy_cookie, inclusive_options));
  EXPECT_TRUE(cookie_util::IsSameSiteCompatPair(
      *http_only_new_cookie, *http_only_legacy_cookie, inclusive_options));

  EXPECT_TRUE(cookie_util::IsSameSiteCompatPair(*new_cookie, *legacy_cookie,
                                                restrictive_options));
  EXPECT_FALSE(cookie_util::IsSameSiteCompatPair(
      *http_only_new_cookie, *legacy_cookie, restrictive_options));
  EXPECT_FALSE(cookie_util::IsSameSiteCompatPair(
      *new_cookie, *http_only_legacy_cookie, restrictive_options));
  EXPECT_FALSE(cookie_util::IsSameSiteCompatPair(
      *http_only_new_cookie, *http_only_legacy_cookie, restrictive_options));

  EXPECT_TRUE(cookie_util::IsSameSiteCompatPair(*new_cookie, *legacy_cookie,
                                                same_site_options));
  EXPECT_FALSE(cookie_util::IsSameSiteCompatPair(
      *http_only_new_cookie, *legacy_cookie, same_site_options));
  EXPECT_FALSE(cookie_util::IsSameSiteCompatPair(
      *new_cookie, *http_only_legacy_cookie, same_site_options));
  EXPECT_FALSE(cookie_util::IsSameSiteCompatPair(
      *http_only_new_cookie, *http_only_legacy_cookie, same_site_options));
}

}  // namespace

}  // namespace net
