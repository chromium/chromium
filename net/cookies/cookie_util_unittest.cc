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
  // |site_for_cookies| not matching the URL -> it's cross-site.
  EXPECT_EQ(
      CookieOptions::SameSiteCookieContext::CROSS_SITE,
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"), GURL("http://notexample.com"),
          base::nullopt /*initiator*/, false /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::CROSS_SITE,
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://example.com"), GURL("http://notexample.com"),
                url::Origin::Create(GURL("http://example.com")),
                false /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::CROSS_SITE,
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://a.com"), GURL("http://b.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /* attach_same_site_cookies */));

  // Same |site_for_cookies|, but not |initiator| -> it's same-site lax.
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /* attach_same_site_cookies */));

  // This isn't a full on origin check --- subdomains and different schema are
  // accepted.
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_LAX_CROSS_SCHEME_SECURE_URL,
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("https://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_LAX_CROSS_SCHEME_INSECURE_URL,
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://example.com"), GURL("https://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://sub.example.com"), GURL("http://sub2.example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /* attach_same_site_cookies */));

  EXPECT_EQ(
      CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://sub.example.com"), GURL("http://sub.example.com:8080"),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          false /* attach_same_site_cookies */));

  // nullopt |initiator| is trusted for purposes of strict, an opaque one isn't.
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://example.com")),
                false /* attach_same_site_cookies */));

  EXPECT_EQ(
      CookieOptions::SameSiteCookieContext::
          SAME_SITE_STRICT_CROSS_SCHEME_SECURE_URL,
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("https://example.com"), GURL("http://example.com"),
          base::nullopt /*initiator*/, false /* attach_same_site_cookies */));

  EXPECT_EQ(
      CookieOptions::SameSiteCookieContext::
          SAME_SITE_STRICT_CROSS_SCHEME_INSECURE_URL,
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"), GURL("https://example.com"),
          base::nullopt /*initiator*/, false /* attach_same_site_cookies */));

  EXPECT_EQ(
      CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"), GURL("http://example.com"),
          base::nullopt /*initiator*/, false /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://example.com"), GURL("http://example.com"),
                url::Origin(), false /* attach_same_site_cookies */));

  // |attach_same_site_cookies| causes (some variant of) SAME_SITE_STRICT to be
  // returned.
  EXPECT_EQ(
      CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://example.com"), GURL("http://notexample.com"),
          base::nullopt /*initiator*/, true /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://example.com"), GURL("http://notexample.com"),
                url::Origin::Create(GURL("http://example.com")),
                true /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://a.com"), GURL("http://b.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                true /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                true /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_STRICT_CROSS_SCHEME_SECURE_URL,
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("https://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                true /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_STRICT_CROSS_SCHEME_INSECURE_URL,
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://example.com"), GURL("https://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                true /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
            cookie_util::ComputeSameSiteContextForScriptGet(
                GURL("http://sub.example.com"), GURL("http://sub2.example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                true /* attach_same_site_cookies */));

  EXPECT_EQ(
      CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
      cookie_util::ComputeSameSiteContextForScriptGet(
          GURL("http://sub.example.com"), GURL("http://sub.example.com:8080"),
          url::Origin::Create(GURL("http://from-elsewhere.com")),
          true /* attach_same_site_cookies */));
}

TEST(CookieUtilTest, ComputeSameSiteContextForRequest) {
  EXPECT_EQ(
      CookieOptions::SameSiteCookieContext::CROSS_SITE,
      cookie_util::ComputeSameSiteContextForRequest(
          "GET", GURL("http://example.com"), GURL("http://notexample.com"),
          base::nullopt /*initiator*/, false /*attach_same_site_cookies*/));

  // |attach_same_site_cookies| = true bypasses all checks.
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("http://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                true /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", GURL("http://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                true /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("http://example.com"), GURL("http://question.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                true /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("http://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://example.com")),
                false /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", GURL("http://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://example.com")),
                false /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_STRICT_CROSS_SCHEME_SECURE_URL,
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("https://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://example.com")),
                false /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_STRICT_CROSS_SCHEME_SECURE_URL,
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", GURL("https://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://example.com")),
                false /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_STRICT_CROSS_SCHEME_INSECURE_URL,
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("http://example.com"), GURL("https://example.com"),
                url::Origin::Create(GURL("http://example.com")),
                false /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_STRICT_CROSS_SCHEME_INSECURE_URL,
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", GURL("http://example.com"), GURL("https://example.com"),
                url::Origin::Create(GURL("http://example.com")),
                false /*attach_same_site_cookies*/));

  // Normally, lax requests also require a safe method.
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("http://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
            cookie_util::ComputeSameSiteContextForRequest(
                "HEAD", GURL("http://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_LAX_CROSS_SCHEME_SECURE_URL,
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("https://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_LAX_CROSS_SCHEME_INSECURE_URL,
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", GURL("http://example.com"), GURL("https://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE,
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", GURL("http://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_SECURE_URL,
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", GURL("https://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_INSECURE_URL,
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", GURL("http://example.com"), GURL("https://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /*attach_same_site_cookies*/));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_SECURE_URL,
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", GURL("https://example.com"), GURL("http://example.com"),
                url::Origin::Create(GURL("http://from-elsewhere.com")),
                false /*attach_same_site_cookies*/));
}

TEST(CookieUtilTest, ComputeSameSiteContextForSet) {
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::CROSS_SITE,
            cookie_util::ComputeSameSiteContextForResponse(
                GURL("http://example.com"), GURL("http://notexample.com"),
                base::nullopt, false /* attach_same_site_cookies */));

  // Same as above except |attach_same_site_cookies| makes it return LAX.
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
            cookie_util::ComputeSameSiteContextForResponse(
                GURL("http://example.com"), GURL("http://notexample.com"),
                base::nullopt, true /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::CROSS_SITE,
            cookie_util::ComputeSameSiteContextForScriptSet(
                GURL("http://example.com"), GURL("http://notexample.com"),
                false /* attach_same_site_cookies */));

  // Same as above except |attach_same_site_cookies| makes it return LAX.
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
            cookie_util::ComputeSameSiteContextForScriptSet(
                GURL("http://example.com"), GURL("http://notexample.com"),
                true /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
            cookie_util::ComputeSameSiteContextForResponse(
                GURL("http://example.com/dir"), GURL("http://sub.example.com"),
                base::nullopt, false /* attach_same_site_cookies */));
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
            cookie_util::ComputeSameSiteContextForResponse(
                GURL("http://example.com/dir"), GURL("http://sub.example.com"),
                base::nullopt, true /* attach_same_site_cookies */));
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_LAX_CROSS_SCHEME_INSECURE_URL,
            cookie_util::ComputeSameSiteContextForResponse(
                GURL("http://example.com/dir"), GURL("https://sub.example.com"),
                base::nullopt, false /* attach_same_site_cookies */));
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_LAX_CROSS_SCHEME_INSECURE_URL,
            cookie_util::ComputeSameSiteContextForResponse(
                GURL("http://example.com/dir"), GURL("https://sub.example.com"),
                base::nullopt, true /* attach_same_site_cookies */));
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_LAX_CROSS_SCHEME_SECURE_URL,
            cookie_util::ComputeSameSiteContextForResponse(
                GURL("https://example.com/dir"), GURL("http://sub.example.com"),
                base::nullopt, false /* attach_same_site_cookies */));
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_LAX_CROSS_SCHEME_SECURE_URL,
            cookie_util::ComputeSameSiteContextForResponse(
                GURL("https://example.com/dir"), GURL("http://sub.example.com"),
                base::nullopt, true /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
            cookie_util::ComputeSameSiteContextForScriptSet(
                GURL("http://example.com/dir"), GURL("http://sub.example.com"),
                false /* attach_same_site_cookies */));
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_LAX_CROSS_SCHEME_INSECURE_URL,
            cookie_util::ComputeSameSiteContextForScriptSet(
                GURL("http://example.com/dir"), GURL("https://sub.example.com"),
                false /* attach_same_site_cookies */));
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_LAX_CROSS_SCHEME_SECURE_URL,
            cookie_util::ComputeSameSiteContextForScriptSet(
                GURL("https://example.com/dir"), GURL("http://sub.example.com"),
                false /* attach_same_site_cookies */));
}

TEST(CookieUtilTest, TestComputeSameSiteContextForSubresource) {
  // |site_for_cookies| not matching the URL -> it's cross-site.
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::CROSS_SITE,
            cookie_util::ComputeSameSiteContextForSubresource(
                GURL("http://example.com"), GURL("http://notexample.com"),
                false /* attach_same_site_cookies */));

  // Same as above except |attach_same_site_cookies| makes it return STRICT.
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
            cookie_util::ComputeSameSiteContextForSubresource(
                GURL("http://example.com"), GURL("http://notexample.com"),
                true /* attach_same_site_cookies */));

  // This isn't a full on origin check --- subdomains and different schema are
  // accepted.
  EXPECT_EQ(CookieOptions::SameSiteCookieContext::
                SAME_SITE_STRICT_CROSS_SCHEME_SECURE_URL,
            cookie_util::ComputeSameSiteContextForSubresource(
                GURL("https://example.com"), GURL("http://example.com"),
                false /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
            cookie_util::ComputeSameSiteContextForSubresource(
                GURL("http://sub.example.com"), GURL("http://sub2.example.com"),
                false /* attach_same_site_cookies */));

  EXPECT_EQ(
      CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
      cookie_util::ComputeSameSiteContextForSubresource(
          GURL("http://sub.example.com"), GURL("http://sub.example.com:8080"),
          false /* attach_same_site_cookies */));

  EXPECT_EQ(CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
            cookie_util::ComputeSameSiteContextForSubresource(
                GURL("http://example.com"), GURL("http://example.com"),
                false /* attach_same_site_cookies */));
}

TEST(CookieUtilTest, AdaptCookieInclusionStatusToBool) {
  bool result_out = true;
  base::OnceCallback<void(bool)> callback = base::BindLambdaForTesting(
      [&result_out](bool result) { result_out = result; });

  base::OnceCallback<void(CanonicalCookie::CookieInclusionStatus)>
      adapted_callback =
          cookie_util::AdaptCookieInclusionStatusToBool(std::move(callback));

  std::move(adapted_callback)
      .Run(CanonicalCookie::CookieInclusionStatus(
          CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR));

  EXPECT_FALSE(result_out);

  result_out = false;
  callback = base::BindLambdaForTesting(
      [&result_out](bool result) { result_out = result; });

  adapted_callback =
      cookie_util::AdaptCookieInclusionStatusToBool(std::move(callback));

  std::move(adapted_callback).Run(CanonicalCookie::CookieInclusionStatus());

  EXPECT_TRUE(result_out);
}

}  // namespace

}  // namespace net
