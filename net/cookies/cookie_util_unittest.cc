// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_util.h"
#include "net/first_party_sets/same_party_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace net {

namespace {

struct RequestCookieParsingTest {
  std::string str;
  base::StringPairs parsed;
  // Used for malformed cookies where the parsed-then-serialized string does not
  // match the original string.
  std::string serialized;
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
    const double epoch;
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
      // Test two-digit abbreviated year numbers.
      {"1-Jan-71 00:00:00 GMT" /* 1971 */, true, 31536000},
      {"1-Jan-70 00:00:00 GMT" /* 1970 */, true, 0},
      {"1-Jan-69 00:00:00 GMT" /* 2069 */, true, 3124224000},
      {"1-Jan-68 00:00:00 GMT" /* 2068 */, true, 3092601600},
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
    EXPECT_EQ(test.epoch, parsed_time.ToDoubleT()) << test.str;
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
      "2039 April 15 21:01:22",         "2038 April 15 21:01:22",
      "15 April 69 21:01:22",           "15 April 68, 21:01:22",
  };

  for (auto* test : kTests) {
    base::Time parsed_time = cookie_util::ParseCookieExpirationTime(test);
    EXPECT_FALSE(parsed_time.is_null());

    // It should either have an exact value, or be base::Time::Max(). For
    // simplicity just check that it is greater than an arbitray date.
    base::Time almost_jan_2038 = base::Time::UnixEpoch() + base::Days(365 * 68);
    EXPECT_LT(almost_jan_2038, parsed_time);
  }
}

// Tests parsing dates that are prior to (or around) 1970. Non-Mac POSIX systems
// are incapable of doing this, however the expectation is for cookie parsing to
// succeed anyway (and return a minimal base::Time).
TEST(CookieUtilTest, ParseCookieExpirationTimeBefore1970) {
  const char* kTests[] = {
      // Times around the Unix epoch.
      "1970 Jan 1 00:00:00",
      "1969 March 3 21:01:22",
      // Two digit year abbreviations.
      "1-Jan-70 00:00:00",
      "Jan 1, 70 00:00:00",
      // Times around the Windows epoch.
      "1601 Jan 1 00:00:00",
      "1600 April 15 21:01:22",
      // Times around kExplodedMinYear on Mac.
      "1902 Jan 1 00:00:00",
      "1901 Jan 1 00:00:00",
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
  tests.emplace_back();
  tests.back().str = "key=value";
  tests.back().parsed.push_back(std::make_pair(std::string("key"),
                                               std::string("value")));
  // Multiple key/value pairs.
  tests.emplace_back();
  tests.back().str = "key1=value1; key2=value2";
  tests.back().parsed.push_back(std::make_pair(std::string("key1"),
                                               std::string("value1")));
  tests.back().parsed.push_back(std::make_pair(std::string("key2"),
                                               std::string("value2")));
  // Empty value.
  tests.emplace_back();
  tests.back().str = "key=; otherkey=1234";
  tests.back().parsed.push_back(std::make_pair(std::string("key"),
                                               std::string()));
  tests.back().parsed.push_back(std::make_pair(std::string("otherkey"),
                                               std::string("1234")));
  // Special characters (including equals signs) in value.
  tests.emplace_back();
  tests.back().str = "key=; a2=s=(./&t=:&u=a#$; a3=+~";
  tests.back().parsed.push_back(std::make_pair(std::string("key"),
                                               std::string()));
  tests.back().parsed.push_back(std::make_pair(std::string("a2"),
                                               std::string("s=(./&t=:&u=a#$")));
  tests.back().parsed.push_back(std::make_pair(std::string("a3"),
                                               std::string("+~")));
  // Quoted value.
  tests.emplace_back();
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

TEST(CookieUtilTest, TestRequestCookieParsing_Malformed) {
  std::vector<RequestCookieParsingTest> tests;

  // Missing equal sign.
  tests.emplace_back();
  tests.back().str = "key";
  tests.back().parsed.emplace_back(
      std::make_pair(std::string("key"), std::string()));
  tests.back().serialized = "key=";

  // Quoted value with unclosed quote.
  tests.emplace_back();
  tests.back().str = "key=\"abcdef";

  // Quoted value with unclosed quote followed by regular value.
  tests.emplace_back();
  tests.back().str = "key=\"abcdef; otherkey=1234";

  // Quoted value with unclosed quote followed by another quoted value.
  tests.emplace_back();
  tests.back().str = "key=\"abcdef; otherkey=\"1234\"";
  tests.back().parsed.emplace_back(
      std::make_pair(std::string("key"), std::string("\"abcdef; otherkey=\"")));
  tests.back().parsed.emplace_back(
      std::make_pair(std::string("234\""), std::string()));
  tests.back().serialized = "key=\"abcdef; otherkey=\"; 234\"=";

  // Regular value followed by quoted value with unclosed quote.
  tests.emplace_back();
  tests.back().str = "key=abcdef; otherkey=\"1234";
  tests.back().parsed.emplace_back(
      std::make_pair(std::string("key"), std::string("abcdef")));
  tests.back().serialized = "key=abcdef";

  for (size_t i = 0; i < tests.size(); i++) {
    SCOPED_TRACE(testing::Message() << "Test " << i);
    CheckParse(tests[i].str, tests[i].parsed);
    CheckSerialize(tests[i].parsed, tests[i].serialized);
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
    cookies.push_back(
        CanonicalCookie::Create(insecure_url, test.cookie, base::Time::Now(),
                                absl::nullopt /* server_time */,
                                absl::nullopt /* cookie_partition_key */));
    cookies.push_back(
        CanonicalCookie::Create(secure_url, test.cookie, base::Time::Now(),
                                absl::nullopt /* server_time */,
                                absl::nullopt /* cookie_partition_key */));
    cookies.push_back(CanonicalCookie::Create(
        secure_url, test.cookie + "; Secure", base::Time::Now(),
        absl::nullopt /* server_time */,
        absl::nullopt /* cookie_partition_key */));
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

using ::testing::AllOf;
using SameSiteCookieContext = CookieOptions::SameSiteCookieContext;
using ContextType = CookieOptions::SameSiteCookieContext::ContextType;
using ContextRedirectTypeBug1221316 = CookieOptions::SameSiteCookieContext::
    ContextMetadata::ContextRedirectTypeBug1221316;
using HttpMethod =
    CookieOptions::SameSiteCookieContext::ContextMetadata::HttpMethod;

MATCHER_P2(ContextTypeIsWithSchemefulMode, context_type, schemeful, "") {
  return context_type == (schemeful ? arg.schemeful_context() : arg.context());
}

// Checks for the expected metadata related to context downgrades from
// cross-site redirects.
MATCHER_P5(CrossSiteRedirectMetadataCorrectWithSchemefulMode,
           method,
           context_type_without_chain,
           context_type_with_chain,
           redirect_type_with_chain,
           schemeful,
           "") {
  using ContextDowngradeType = CookieOptions::SameSiteCookieContext::
      ContextMetadata::ContextDowngradeType;

  const auto& metadata = schemeful ? arg.schemeful_metadata() : arg.metadata();

  if (metadata.redirect_type_bug_1221316 != redirect_type_with_chain)
    return false;

  // http_method_bug_1221316 is only set when there is a context downgrade.
  if (metadata.cross_site_redirect_downgrade !=
          ContextDowngradeType::kNoDowngrade &&
      metadata.http_method_bug_1221316 != method) {
    return false;
  }

  switch (metadata.cross_site_redirect_downgrade) {
    case ContextDowngradeType::kNoDowngrade:
      return context_type_without_chain == context_type_with_chain;
    case ContextDowngradeType::kStrictToLax:
      return context_type_without_chain == ContextType::SAME_SITE_STRICT &&
             (context_type_with_chain == ContextType::SAME_SITE_LAX ||
              context_type_with_chain ==
                  ContextType::SAME_SITE_LAX_METHOD_UNSAFE);
    case ContextDowngradeType::kStrictToCross:
      return context_type_without_chain == ContextType::SAME_SITE_STRICT &&
             context_type_with_chain == ContextType::CROSS_SITE;
    case ContextDowngradeType::kLaxToCross:
      return (context_type_without_chain == ContextType::SAME_SITE_LAX ||
              context_type_without_chain ==
                  ContextType::SAME_SITE_LAX_METHOD_UNSAFE) &&
             context_type_with_chain == ContextType::CROSS_SITE;
  }
}

std::string UrlChainToString(const std::vector<GURL>& url_chain) {
  std::string s;
  for (const GURL& url : url_chain) {
    base::StrAppend(&s, {" ", url.spec()});
  }
  return s;
}

// Tests for the various ComputeSameSiteContextFor*() functions. The first
// boolean test param is whether the results of the computations are evaluated
// schemefully. The second boolean param is whether SameSite considers redirect
// chains.
class CookieUtilComputeSameSiteContextTest
    : public ::testing::TestWithParam<std::tuple<bool, bool>> {
 public:
  CookieUtilComputeSameSiteContextTest() {
    if (DoesSameSiteConsiderRedirectChain()) {
      feature_list_.InitAndEnableFeature(
          features::kCookieSameSiteConsidersRedirectChain);
    } else {
      // No need to explicitly disable the redirect chain feature because it
      // is disabled by default.
      feature_list_.Init();
    }
  }
  ~CookieUtilComputeSameSiteContextTest() override = default;

  bool IsSchemeful() const { return std::get<0>(GetParam()); }

  bool DoesSameSiteConsiderRedirectChain() const {
    return std::get<1>(GetParam());
  }

  // Returns the proper gtest matcher to use for the schemeless/schemeful mode.
  auto ContextTypeIs(ContextType context_type) const {
    return ContextTypeIsWithSchemefulMode(context_type, IsSchemeful());
  }

  auto CrossSiteRedirectMetadataCorrect(
      HttpMethod method,
      ContextType context_type_without_chain,
      ContextType context_type_with_chain,
      ContextRedirectTypeBug1221316 redirect_type_with_chain) const {
    return CrossSiteRedirectMetadataCorrectWithSchemefulMode(
        method, context_type_without_chain, context_type_with_chain,
        redirect_type_with_chain, IsSchemeful());
  }

  // The following methods return the sets of URLs/SiteForCookies/initiators/URL
  // chains that are same-site or cross-site with respect to kSiteUrl.

  std::vector<GURL> GetAllUrls() const {
    return {kSiteUrl,
            kSiteUrlWithPath,
            kSecureSiteUrl,
            kCrossSiteUrl,
            kSecureCrossSiteUrl,
            kSubdomainUrl,
            kSecureSubdomainUrl,
            kWsUrl,
            kWssUrl};
  }

  std::vector<GURL> GetSameSiteUrls() const {
    // Same-site-same-scheme URLs are always same-site. (ws counts as
    // same-scheme with http.)
    std::vector<GURL> same_site_urls{kSiteUrl, kSiteUrlWithPath, kSubdomainUrl,
                                     kWsUrl};
    // If schemeless, the cross-scheme URLs are also same-site.
    if (!IsSchemeful()) {
      same_site_urls.push_back(kSecureSiteUrl);
      same_site_urls.push_back(kSecureSubdomainUrl);
      same_site_urls.push_back(kWssUrl);
    }
    return same_site_urls;
  }

  std::vector<GURL> GetCrossSiteUrls() const {
    std::vector<GURL> cross_site_urls;
    std::vector<GURL> same_site_urls = GetSameSiteUrls();
    for (const GURL& url : GetAllUrls()) {
      if (!base::Contains(same_site_urls, url))
        cross_site_urls.push_back(url);
    }
    return cross_site_urls;
  }

  std::vector<SiteForCookies> GetAllSitesForCookies() const {
    return {kNullSiteForCookies, kSiteForCookies, kSecureSiteForCookies,
            kCrossSiteForCookies, kSecureCrossSiteForCookies};
  }

  std::vector<SiteForCookies> GetSameSiteSitesForCookies() const {
    std::vector<SiteForCookies> same_site_sfc = {kSiteForCookies};
    // If schemeless, the cross-scheme SFC is also same-site.
    if (!IsSchemeful())
      same_site_sfc.push_back(kSecureSiteForCookies);
    return same_site_sfc;
  }

  std::vector<SiteForCookies> GetCrossSiteSitesForCookies() const {
    std::vector<SiteForCookies> cross_site_sfc;
    std::vector<SiteForCookies> same_site_sfc = GetSameSiteSitesForCookies();
    for (const SiteForCookies& sfc : GetAllSitesForCookies()) {
      if (!base::Contains(same_site_sfc, sfc.RepresentativeUrl(),
                          &SiteForCookies::RepresentativeUrl)) {
        cross_site_sfc.push_back(sfc);
      }
    }
    return cross_site_sfc;
  }

  std::vector<absl::optional<url::Origin>> GetAllInitiators() const {
    return {kBrowserInitiated,   kOpaqueInitiator,
            kSiteInitiator,      kSecureSiteInitiator,
            kCrossSiteInitiator, kSecureCrossSiteInitiator,
            kSubdomainInitiator, kSecureSubdomainInitiator,
            kUnrelatedInitiator};
  }

  std::vector<absl::optional<url::Origin>> GetSameSiteInitiators() const {
    std::vector<absl::optional<url::Origin>> same_site_initiators{
        kBrowserInitiated, kSiteInitiator, kSubdomainInitiator};
    // If schemeless, the cross-scheme origins are also same-site.
    if (!IsSchemeful()) {
      same_site_initiators.push_back(kSecureSiteInitiator);
      same_site_initiators.push_back(kSecureSubdomainInitiator);
    }
    return same_site_initiators;
  }

  std::vector<absl::optional<url::Origin>> GetCrossSiteInitiators() const {
    std::vector<absl::optional<url::Origin>> cross_site_initiators;
    std::vector<absl::optional<url::Origin>> same_site_initiators =
        GetSameSiteInitiators();
    for (const absl::optional<url::Origin>& initiator : GetAllInitiators()) {
      if (!base::Contains(same_site_initiators, initiator))
        cross_site_initiators.push_back(initiator);
    }
    return cross_site_initiators;
  }

  // Returns an assortment of redirect chains that end in `url` as the
  // current request URL, and are completely same-site. `url` is expected to be
  // same-site to kSiteUrl.
  std::vector<std::vector<GURL>> GetSameSiteUrlChains(const GURL& url) const {
    std::vector<std::vector<GURL>> same_site_url_chains;
    for (const GURL& same_site_url : GetSameSiteUrls()) {
      same_site_url_chains.push_back({same_site_url, url});
      for (const GURL& other_same_site_url : GetSameSiteUrls()) {
        same_site_url_chains.push_back(
            {other_same_site_url, same_site_url, url});
      }
    }
    return same_site_url_chains;
  }

  // Returns an assortment of redirect chains that end in `url` as the
  // current request URL, and are cross-site. `url` is expected to be same-site
  // to kSiteUrl.
  std::vector<std::vector<GURL>> GetCrossSiteUrlChains(const GURL& url) const {
    std::vector<std::vector<GURL>> cross_site_url_chains;
    for (const GURL& cross_site_url : GetCrossSiteUrls()) {
      cross_site_url_chains.push_back({cross_site_url, url});
      for (const GURL& same_site_url : GetSameSiteUrls()) {
        cross_site_url_chains.push_back({cross_site_url, same_site_url, url});
        cross_site_url_chains.push_back({same_site_url, cross_site_url, url});
      }
    }
    return cross_site_url_chains;
  }

  // Computes possible values of is_main_frame_navigation that are consistent
  // with the DCHECKs.
  bool CanBeMainFrameNavigation(const GURL& url,
                                const SiteForCookies& site_for_cookies) const {
    return (site_for_cookies.IsNull() ||
            site_for_cookies.IsFirstPartyWithSchemefulMode(url, true)) &&
           !url.SchemeIsWSOrWSS();
  }

  std::vector<bool> IsMainFrameNavigationPossibleValues(
      const GURL& url,
      const SiteForCookies& site_for_cookies) const {
    return CanBeMainFrameNavigation(url, site_for_cookies)
               ? std::vector<bool>{false, true}
               : std::vector<bool>{false};
  }

  // Request URL.
  const GURL kSiteUrl{"http://example.test/"};
  const GURL kSiteUrlWithPath{"http://example.test/path"};
  const GURL kSecureSiteUrl{"https://example.test/"};
  const GURL kCrossSiteUrl{"http://notexample.test/"};
  const GURL kSecureCrossSiteUrl{"https://notexample.test/"};
  const GURL kSubdomainUrl{"http://subdomain.example.test/"};
  const GURL kSecureSubdomainUrl{"https://subdomain.example.test/"};
  const GURL kWsUrl{"ws://example.test/"};
  const GURL kWssUrl{"wss://example.test/"};
  // Site for cookies.
  const SiteForCookies kNullSiteForCookies;
  const SiteForCookies kSiteForCookies = SiteForCookies::FromUrl(kSiteUrl);
  const SiteForCookies kSecureSiteForCookies =
      SiteForCookies::FromUrl(kSecureSiteUrl);
  const SiteForCookies kCrossSiteForCookies =
      SiteForCookies::FromUrl(kCrossSiteUrl);
  const SiteForCookies kSecureCrossSiteForCookies =
      SiteForCookies::FromUrl(kSecureCrossSiteUrl);
  // Initiator origin.
  const absl::optional<url::Origin> kBrowserInitiated = absl::nullopt;
  const absl::optional<url::Origin> kOpaqueInitiator =
      absl::make_optional(url::Origin());
  const absl::optional<url::Origin> kSiteInitiator =
      absl::make_optional(url::Origin::Create(kSiteUrl));
  const absl::optional<url::Origin> kSecureSiteInitiator =
      absl::make_optional(url::Origin::Create(kSecureSiteUrl));
  const absl::optional<url::Origin> kCrossSiteInitiator =
      absl::make_optional(url::Origin::Create(kCrossSiteUrl));
  const absl::optional<url::Origin> kSecureCrossSiteInitiator =
      absl::make_optional(url::Origin::Create(kSecureCrossSiteUrl));
  const absl::optional<url::Origin> kSubdomainInitiator =
      absl::make_optional(url::Origin::Create(kSubdomainUrl));
  const absl::optional<url::Origin> kSecureSubdomainInitiator =
      absl::make_optional(url::Origin::Create(kSecureSubdomainUrl));
  const absl::optional<url::Origin> kUnrelatedInitiator =
      absl::make_optional(url::Origin::Create(GURL("https://unrelated.test/")));

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(CookieUtilComputeSameSiteContextTest, UrlAndSiteForCookiesCrossSite) {
  // If the SiteForCookies and URL are cross-site, then the context is always
  // cross-site.
  for (const GURL& url : GetSameSiteUrls()) {
    for (const SiteForCookies& site_for_cookies :
         GetCrossSiteSitesForCookies()) {
      for (const absl::optional<url::Origin>& initiator : GetAllInitiators()) {
        for (const std::string& method : {"GET", "POST", "PUT", "HEAD"}) {
          EXPECT_THAT(cookie_util::ComputeSameSiteContextForScriptGet(
                          url, site_for_cookies, initiator,
                          false /* force_ignore_site_for_cookies */),
                      ContextTypeIs(ContextType::CROSS_SITE));
          EXPECT_THAT(cookie_util::ComputeSameSiteContextForScriptSet(
                          url, site_for_cookies,
                          false /* force_ignore_site_for_cookies */),
                      ContextTypeIs(ContextType::CROSS_SITE));
          for (bool is_main_frame_navigation :
               IsMainFrameNavigationPossibleValues(url, site_for_cookies)) {
            EXPECT_THAT(cookie_util::ComputeSameSiteContextForRequest(
                            method, {url}, site_for_cookies, initiator,
                            is_main_frame_navigation,
                            false /* force_ignore_site_for_cookies */),
                        ContextTypeIs(ContextType::CROSS_SITE));
            EXPECT_THAT(cookie_util::ComputeSameSiteContextForResponse(
                            {url}, site_for_cookies, initiator,
                            is_main_frame_navigation,
                            false /* force_ignore_site_for_cookies */),
                        ContextTypeIs(ContextType::CROSS_SITE));
            // If the current request URL is cross-site to the site-for-cookies,
            // the request context is always cross-site even if the URL chain
            // contains members that are same-site to the site-for-cookies.
            EXPECT_THAT(
                cookie_util::ComputeSameSiteContextForRequest(
                    method, {site_for_cookies.RepresentativeUrl(), url},
                    site_for_cookies, initiator, is_main_frame_navigation,
                    false /* force_ignore_site_for_cookies */),
                ContextTypeIs(ContextType::CROSS_SITE));
            EXPECT_THAT(
                cookie_util::ComputeSameSiteContextForResponse(
                    {site_for_cookies.RepresentativeUrl(), url},
                    site_for_cookies, initiator, is_main_frame_navigation,
                    false /* force_ignore_site_for_cookies */),
                ContextTypeIs(ContextType::CROSS_SITE));
          }
          EXPECT_THAT(cookie_util::ComputeSameSiteContextForSubresource(
                          url, site_for_cookies,
                          false /* force_ignore_site_for_cookies */),
                      ContextTypeIs(ContextType::CROSS_SITE));
        }
      }
    }
  }
}

TEST_P(CookieUtilComputeSameSiteContextTest, SiteForCookiesNotSchemefullySame) {
  // If the SiteForCookies is not schemefully_same, even if its value is
  // schemefully same-site, the schemeful context type will be cross-site.
  if (!IsSchemeful())
    return;

  std::vector<SiteForCookies> sites_for_cookies = GetAllSitesForCookies();
  for (SiteForCookies& sfc : sites_for_cookies) {
    sfc.SetSchemefullySameForTesting(false);
  }

  for (const GURL& url : GetSameSiteUrls()) {
    for (const SiteForCookies& site_for_cookies : sites_for_cookies) {
      for (const absl::optional<url::Origin>& initiator : GetAllInitiators()) {
        for (const std::string& method : {"GET", "POST", "PUT", "HEAD"}) {
          EXPECT_THAT(cookie_util::ComputeSameSiteContextForScriptGet(
                          url, site_for_cookies, initiator,
                          false /* force_ignore_site_for_cookies */),
                      ContextTypeIs(ContextType::CROSS_SITE));
          EXPECT_THAT(cookie_util::ComputeSameSiteContextForScriptSet(
                          url, site_for_cookies,
                          false /* force_ignore_site_for_cookies */),
                      ContextTypeIs(ContextType::CROSS_SITE));

          // If the site-for-cookies isn't schemefully_same, this cannot be a
          // main frame navigation.
          EXPECT_THAT(cookie_util::ComputeSameSiteContextForRequest(
                          method, {url}, site_for_cookies, initiator,
                          false /* is_main_frame_navigation */,
                          false /* force_ignore_site_for_cookies */),
                      ContextTypeIs(ContextType::CROSS_SITE));
          EXPECT_THAT(cookie_util::ComputeSameSiteContextForResponse(
                          {url}, site_for_cookies, initiator,
                          false /* is_main_frame_navigation */,
                          false /* force_ignore_site_for_cookies */),
                      ContextTypeIs(ContextType::CROSS_SITE));

          EXPECT_THAT(cookie_util::ComputeSameSiteContextForSubresource(
                          url, site_for_cookies,
                          false /* force_ignore_site_for_cookies */),
                      ContextTypeIs(ContextType::CROSS_SITE));
        }
      }
    }
  }
}

TEST_P(CookieUtilComputeSameSiteContextTest, ForScriptGet) {
  for (const GURL& url : GetSameSiteUrls()) {
    // Same-site site-for-cookies.
    // (Cross-site cases covered above in UrlAndSiteForCookiesCrossSite test.)
    for (const SiteForCookies& site_for_cookies :
         GetSameSiteSitesForCookies()) {
      // Cross-site initiator -> it's same-site lax.
      for (const absl::optional<url::Origin>& initiator :
           GetCrossSiteInitiators()) {
        EXPECT_THAT(cookie_util::ComputeSameSiteContextForScriptGet(
                        url, site_for_cookies, initiator,
                        false /* force_ignore_site_for_cookies */),
                    ContextTypeIs(ContextType::SAME_SITE_LAX));
      }

      // Same-site initiator -> it's same-site strict.
      for (const absl::optional<url::Origin>& initiator :
           GetSameSiteInitiators()) {
        EXPECT_THAT(cookie_util::ComputeSameSiteContextForScriptGet(
                        url, site_for_cookies, initiator,
                        false /* force_ignore_site_for_cookies */),
                    ContextTypeIs(ContextType::SAME_SITE_STRICT));
      }
    }
  }
}

TEST_P(CookieUtilComputeSameSiteContextTest, ForScriptGet_SchemefulDowngrade) {
  // Some test cases where the context is downgraded when computed schemefully.
  // (Should already be covered above, but just to be explicit.)
  EXPECT_EQ(SameSiteCookieContext(ContextType::SAME_SITE_STRICT,
                                  ContextType::SAME_SITE_LAX),
            cookie_util::ComputeSameSiteContextForScriptGet(
                kSiteUrl, kSiteForCookies, kSecureSiteInitiator,
                false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(SameSiteCookieContext(ContextType::SAME_SITE_STRICT,
                                  ContextType::SAME_SITE_LAX),
            cookie_util::ComputeSameSiteContextForScriptGet(
                kSecureSiteUrl, kSecureSiteForCookies, kSiteInitiator,
                false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(SameSiteCookieContext(ContextType::SAME_SITE_LAX,
                                  ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForScriptGet(
                kSecureSiteUrl, kSiteForCookies, kCrossSiteInitiator,
                false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(SameSiteCookieContext(ContextType::SAME_SITE_LAX,
                                  ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForScriptGet(
                kSiteUrl, kSecureSiteForCookies, kCrossSiteInitiator,
                false /* force_ignore_site_for_cookies */));
}

TEST_P(CookieUtilComputeSameSiteContextTest, ForScriptGet_WebSocketSchemes) {
  // wss/https and http/ws are considered the same for schemeful purposes.
  EXPECT_THAT(cookie_util::ComputeSameSiteContextForScriptGet(
                  kWssUrl, kSecureSiteForCookies, kSecureSiteInitiator,
                  false /* force_ignore_site_for_cookies */),
              ContextTypeIs(ContextType::SAME_SITE_STRICT));
  EXPECT_THAT(cookie_util::ComputeSameSiteContextForScriptGet(
                  kWssUrl, kSecureSiteForCookies, kSecureCrossSiteInitiator,
                  false /* force_ignore_site_for_cookies */),
              ContextTypeIs(ContextType::SAME_SITE_LAX));

  EXPECT_THAT(cookie_util::ComputeSameSiteContextForScriptGet(
                  kWsUrl, kSiteForCookies, kSiteInitiator,
                  false /* force_ignore_site_for_cookies */),
              ContextTypeIs(ContextType::SAME_SITE_STRICT));
  EXPECT_THAT(cookie_util::ComputeSameSiteContextForScriptGet(
                  kWsUrl, kSiteForCookies, kCrossSiteInitiator,
                  false /* force_ignore_site_for_cookies */),
              ContextTypeIs(ContextType::SAME_SITE_LAX));
}

// Test cases where the URL chain has 1 member (i.e. no redirects).
TEST_P(CookieUtilComputeSameSiteContextTest, ForRequest) {
  for (const GURL& url : GetSameSiteUrls()) {
    // Same-site site-for-cookies.
    // (Cross-site cases covered above in UrlAndSiteForCookiesCrossSite test.)
    for (const SiteForCookies& site_for_cookies :
         GetSameSiteSitesForCookies()) {
      // Same-Site initiator -> it's same-site strict.
      for (const absl::optional<url::Origin>& initiator :
           GetSameSiteInitiators()) {
        for (const std::string& method : {"GET", "POST", "PUT", "HEAD"}) {
          for (bool is_main_frame_navigation :
               IsMainFrameNavigationPossibleValues(url, site_for_cookies)) {
            EXPECT_THAT(cookie_util::ComputeSameSiteContextForRequest(
                            method, {url}, site_for_cookies, initiator,
                            is_main_frame_navigation,
                            false /* force_ignore_site_for_cookies */),
                        ContextTypeIs(ContextType::SAME_SITE_STRICT));
          }
        }
      }

      // Cross-Site initiator -> it's same-site lax iff the method is safe.
      for (const absl::optional<url::Origin>& initiator :
           GetCrossSiteInitiators()) {
        // For main frame navigations, the context is Lax (or Lax-unsafe).
        for (const std::string& method : {"GET", "HEAD"}) {
          if (!CanBeMainFrameNavigation(url, site_for_cookies))
            break;
          EXPECT_THAT(cookie_util::ComputeSameSiteContextForRequest(
                          method, {url}, site_for_cookies, initiator,
                          true /* is_main_frame_navigation */,
                          false /* force_ignore_site_for_cookies */),
                      ContextTypeIs(ContextType::SAME_SITE_LAX));
        }
        for (const std::string& method : {"POST", "PUT"}) {
          if (!CanBeMainFrameNavigation(url, site_for_cookies))
            break;
          EXPECT_THAT(cookie_util::ComputeSameSiteContextForRequest(
                          method, {url}, site_for_cookies, initiator,
                          true /* is_main_frame_navigation */,
                          false /* force_ignore_site_for_cookies */),
                      ContextTypeIs(ContextType::SAME_SITE_LAX_METHOD_UNSAFE));
        }

        // For non-main-frame-navigation requests, the context should be
        // cross-site.
        for (const std::string& method : {"GET", "POST", "PUT", "HEAD"}) {
          EXPECT_THAT(cookie_util::ComputeSameSiteContextForRequest(
                          method, {url}, site_for_cookies, initiator,
                          false /* is_main_frame_navigation */,
                          false /* force_ignore_site_for_cookies */),
                      ContextTypeIs(ContextType::CROSS_SITE));
        }
      }
    }
  }
}

TEST_P(CookieUtilComputeSameSiteContextTest, ForRequest_SchemefulDowngrade) {
  // Some test cases where the context is downgraded when computed schemefully.
  // (Should already be covered above, but just to be explicit.)

  // Cross-scheme URL and site-for-cookies with (schemelessly) same-site
  // initiator.
  // (The request cannot be a main frame navigation if the site-for-cookies is
  // not schemefully same-site).
  for (const std::string& method : {"GET", "POST"}) {
    EXPECT_EQ(SameSiteCookieContext(ContextType::SAME_SITE_STRICT,
                                    ContextType::CROSS_SITE),
              cookie_util::ComputeSameSiteContextForRequest(
                  method, {kSecureSiteUrl}, kSiteForCookies, kSiteInitiator,
                  false /* is_main_frame_navigation */,
                  false /* force_ignore_site_for_cookies */));
    EXPECT_EQ(SameSiteCookieContext(ContextType::SAME_SITE_STRICT,
                                    ContextType::CROSS_SITE),
              cookie_util::ComputeSameSiteContextForRequest(
                  method, {kSiteUrl}, kSecureSiteForCookies, kSiteInitiator,
                  false /* is_main_frame_navigation */,
                  false /* force_ignore_site_for_cookies */));
  }

  // Schemefully same-site URL and site-for-cookies with cross-scheme
  // initiator.
  for (bool is_main_frame_navigation : {false, true}) {
    ContextType lax_if_main_frame = is_main_frame_navigation
                                        ? ContextType::SAME_SITE_LAX
                                        : ContextType::CROSS_SITE;
    ContextType lax_unsafe_if_main_frame =
        is_main_frame_navigation ? ContextType::SAME_SITE_LAX_METHOD_UNSAFE
                                 : ContextType::CROSS_SITE;

    EXPECT_EQ(
        SameSiteCookieContext(ContextType::SAME_SITE_STRICT, lax_if_main_frame),
        cookie_util::ComputeSameSiteContextForRequest(
            "GET", {kSecureSiteUrl}, kSecureSiteForCookies, kSiteInitiator,
            is_main_frame_navigation,
            false /* force_ignore_site_for_cookies */));
    EXPECT_EQ(
        SameSiteCookieContext(ContextType::SAME_SITE_STRICT, lax_if_main_frame),
        cookie_util::ComputeSameSiteContextForRequest(
            "GET", {kSiteUrl}, kSiteForCookies, kSecureSiteInitiator,
            is_main_frame_navigation,
            false /* force_ignore_site_for_cookies */));
    EXPECT_EQ(SameSiteCookieContext(ContextType::SAME_SITE_STRICT,
                                    lax_unsafe_if_main_frame),
              cookie_util::ComputeSameSiteContextForRequest(
                  "POST", {kSecureSiteUrl}, kSecureSiteForCookies,
                  kSiteInitiator, is_main_frame_navigation,
                  false /* force_ignore_site_for_cookies */));
    EXPECT_EQ(SameSiteCookieContext(ContextType::SAME_SITE_STRICT,
                                    lax_unsafe_if_main_frame),
              cookie_util::ComputeSameSiteContextForRequest(
                  "POST", {kSiteUrl}, kSiteForCookies, kSecureSiteInitiator,
                  is_main_frame_navigation,
                  false /* force_ignore_site_for_cookies */));
  }

  // Cross-scheme URL and site-for-cookies with cross-site initiator.
  // (The request cannot be a main frame navigation if the site-for-cookies is
  // not schemefully same-site).
  EXPECT_EQ(SameSiteCookieContext(ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", {kSiteUrl}, kSecureSiteForCookies, kCrossSiteInitiator,
                false /* is_main_frame_navigation */,
                false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(SameSiteCookieContext(ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForRequest(
                "GET", {kSecureSiteUrl}, kSiteForCookies, kCrossSiteInitiator,
                false /* is_main_frame_navigation */,
                false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(SameSiteCookieContext(ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", {kSiteUrl}, kSecureSiteForCookies, kCrossSiteInitiator,
                false /* is_main_frame_navigation */,
                false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(SameSiteCookieContext(ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForRequest(
                "POST", {kSecureSiteUrl}, kSiteForCookies, kCrossSiteInitiator,
                false /* is_main_frame_navigation */,
                false /* force_ignore_site_for_cookies */));
}

TEST_P(CookieUtilComputeSameSiteContextTest, ForRequest_WebSocketSchemes) {
  // wss/https and http/ws are considered the same for schemeful purposes.
  // (ws/wss requests cannot be main frame navigations.)
  EXPECT_THAT(cookie_util::ComputeSameSiteContextForRequest(
                  "GET", {kWssUrl}, kSecureSiteForCookies, kSecureSiteInitiator,
                  false /* is_main_frame_navigation */,
                  false /* force_ignore_site_for_cookies */),
              ContextTypeIs(ContextType::SAME_SITE_STRICT));
  EXPECT_THAT(
      cookie_util::ComputeSameSiteContextForRequest(
          "GET", {kWssUrl}, kSecureSiteForCookies, kSecureCrossSiteInitiator,
          false /* is_main_frame_navigation */,
          false /* force_ignore_site_for_cookies */),
      ContextTypeIs(ContextType::CROSS_SITE));

  EXPECT_THAT(cookie_util::ComputeSameSiteContextForRequest(
                  "GET", {kWsUrl}, kSiteForCookies, kSiteInitiator,
                  false /* is_main_frame_navigation */,
                  false /* force_ignore_site_for_cookies */),
              ContextTypeIs(ContextType::SAME_SITE_STRICT));
  EXPECT_THAT(cookie_util::ComputeSameSiteContextForRequest(
                  "GET", {kWsUrl}, kSiteForCookies, kCrossSiteInitiator,
                  false /* is_main_frame_navigation */,
                  false /* force_ignore_site_for_cookies */),
              ContextTypeIs(ContextType::CROSS_SITE));
}

// Test cases where the URL chain contains multiple members, where the last
// member (current request URL) is same-site to kSiteUrl. (Everything is listed
// as same-site or cross-site relative to kSiteUrl.)
TEST_P(CookieUtilComputeSameSiteContextTest, ForRequest_Redirect) {
  struct {
    std::string method;
    bool url_chain_is_same_site;
    bool site_for_cookies_is_same_site;
    bool initiator_is_same_site;
    // These are the expected context types considering redirect chains:
    ContextType expected_context_type;  // for non-main-frame-nav requests.
    ContextType expected_context_type_for_main_frame_navigation;
    // These are the expected context types not considering redirect chains:
    ContextType expected_context_type_without_chain;
    ContextType expected_context_type_for_main_frame_navigation_without_chain;
    // The expected redirect type (only applicable for chains):
    ContextRedirectTypeBug1221316 expected_redirect_type_with_chain;
  } kTestCases[] = {
      // If the url chain is same-site, then the result is the same with or
      // without considering the redirect chain.
      {"GET", true, true, true, ContextType::SAME_SITE_STRICT,
       ContextType::SAME_SITE_STRICT, ContextType::SAME_SITE_STRICT,
       ContextType::SAME_SITE_STRICT,
       ContextRedirectTypeBug1221316::kAllSameSiteRedirect},
      {"GET", true, true, false, ContextType::CROSS_SITE,
       ContextType::SAME_SITE_LAX, ContextType::CROSS_SITE,
       ContextType::SAME_SITE_LAX,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      {"GET", true, false, true, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      {"GET", true, false, false, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      // If the url chain is cross-site, then the result will differ depending
      // on whether the redirect chain is considered, when the site-for-cookies
      // and initiator are both same-site.
      {"GET", false, true, true, ContextType::CROSS_SITE,
       ContextType::SAME_SITE_LAX, ContextType::SAME_SITE_STRICT,
       ContextType::SAME_SITE_STRICT,
       ContextRedirectTypeBug1221316::kPartialSameSiteRedirect},
      {"GET", false, true, false, ContextType::CROSS_SITE,
       ContextType::SAME_SITE_LAX, ContextType::CROSS_SITE,
       ContextType::SAME_SITE_LAX,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      {"GET", false, false, true, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      {"GET", false, false, false, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      // If the url chain is same-site, then the result is the same with or
      // without considering the redirect chain.
      {"POST", true, true, true, ContextType::SAME_SITE_STRICT,
       ContextType::SAME_SITE_STRICT, ContextType::SAME_SITE_STRICT,
       ContextType::SAME_SITE_STRICT,
       ContextRedirectTypeBug1221316::kAllSameSiteRedirect},
      {"POST", true, true, false, ContextType::CROSS_SITE,
       ContextType::SAME_SITE_LAX_METHOD_UNSAFE, ContextType::CROSS_SITE,
       ContextType::SAME_SITE_LAX_METHOD_UNSAFE,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      {"POST", true, false, true, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      {"POST", true, false, false, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      // If the url chain is cross-site, then the result will differ depending
      // on whether the redirect chain is considered, when the site-for-cookies
      // and initiator are both same-site.
      {"POST", false, true, true, ContextType::CROSS_SITE,
       ContextType::SAME_SITE_LAX_METHOD_UNSAFE, ContextType::SAME_SITE_STRICT,
       ContextType::SAME_SITE_STRICT,
       ContextRedirectTypeBug1221316::kPartialSameSiteRedirect},
      {"POST", false, true, false, ContextType::CROSS_SITE,
       ContextType::SAME_SITE_LAX_METHOD_UNSAFE, ContextType::CROSS_SITE,
       ContextType::SAME_SITE_LAX_METHOD_UNSAFE,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      {"POST", false, false, true, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      {"POST", false, false, false, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
  };

  for (const auto& test_case : kTestCases) {
    std::vector<std::vector<GURL>> url_chains =
        test_case.url_chain_is_same_site ? GetSameSiteUrlChains(kSiteUrl)
                                         : GetCrossSiteUrlChains(kSiteUrl);
    std::vector<SiteForCookies> sites_for_cookies =
        test_case.site_for_cookies_is_same_site ? GetSameSiteSitesForCookies()
                                                : GetCrossSiteSitesForCookies();
    std::vector<absl::optional<url::Origin>> initiators =
        test_case.initiator_is_same_site ? GetSameSiteInitiators()
                                         : GetCrossSiteInitiators();
    ContextType expected_context_type =
        DoesSameSiteConsiderRedirectChain()
            ? test_case.expected_context_type
            : test_case.expected_context_type_without_chain;
    ContextType expected_context_type_for_main_frame_navigation =
        DoesSameSiteConsiderRedirectChain()
            ? test_case.expected_context_type_for_main_frame_navigation
            : test_case
                  .expected_context_type_for_main_frame_navigation_without_chain;
    for (const std::vector<GURL>& url_chain : url_chains) {
      for (const SiteForCookies& site_for_cookies : sites_for_cookies) {
        for (const absl::optional<url::Origin>& initiator : initiators) {
          EXPECT_THAT(
              cookie_util::ComputeSameSiteContextForRequest(
                  test_case.method, url_chain, site_for_cookies, initiator,
                  false /* is_main_frame_navigation */,
                  false /* force_ignore_site_for_cookies */),
              AllOf(ContextTypeIs(expected_context_type),
                    CrossSiteRedirectMetadataCorrect(
                        cookie_util::HttpMethodStringToEnum(test_case.method),
                        test_case.expected_context_type_without_chain,
                        test_case.expected_context_type,
                        test_case.expected_redirect_type_with_chain)))
              << UrlChainToString(url_chain) << " "
              << site_for_cookies.ToDebugString() << " "
              << (initiator ? initiator->Serialize() : "nullopt");
          if (!CanBeMainFrameNavigation(url_chain.back(), site_for_cookies))
            continue;
          EXPECT_THAT(
              cookie_util::ComputeSameSiteContextForRequest(
                  test_case.method, url_chain, site_for_cookies, initiator,
                  true /* is_main_frame_navigation */,
                  false /* force_ignore_site_for_cookies */),
              AllOf(
                  ContextTypeIs(
                      expected_context_type_for_main_frame_navigation),
                  CrossSiteRedirectMetadataCorrect(
                      cookie_util::HttpMethodStringToEnum(test_case.method),
                      test_case
                          .expected_context_type_for_main_frame_navigation_without_chain,
                      test_case.expected_context_type_for_main_frame_navigation,
                      test_case.expected_redirect_type_with_chain)))
              << UrlChainToString(url_chain) << " "
              << site_for_cookies.ToDebugString() << " "
              << (initiator ? initiator->Serialize() : "nullopt");
        }
      }
    }
  }
}

TEST_P(CookieUtilComputeSameSiteContextTest, ForScriptSet) {
  for (const GURL& url : GetSameSiteUrls()) {
    for (const SiteForCookies& site_for_cookies :
         GetSameSiteSitesForCookies()) {
      // Same-site site-for-cookies -> it's same-site lax.
      // (Cross-site cases covered above in UrlAndSiteForCookiesCrossSite test.)
      EXPECT_THAT(
          cookie_util::ComputeSameSiteContextForScriptSet(
              url, site_for_cookies, false /* force_ignore_site_for_cookies */),
          ContextTypeIs(ContextType::SAME_SITE_LAX));
    }
  }
}

TEST_P(CookieUtilComputeSameSiteContextTest, ForScriptSet_SchemefulDowngrade) {
  // Some test cases where the context is downgraded when computed schemefully.
  // (Should already be covered above, but just to be explicit.)
  EXPECT_EQ(SameSiteCookieContext(ContextType::SAME_SITE_LAX,
                                  ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForScriptSet(
                kSiteUrl, kSecureSiteForCookies,
                false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(SameSiteCookieContext(ContextType::SAME_SITE_LAX,
                                  ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForScriptSet(
                kSecureSiteUrl, kSiteForCookies,
                false /* force_ignore_site_for_cookies */));
}

TEST_P(CookieUtilComputeSameSiteContextTest, ForScriptSet_WebSocketSchemes) {
  // wss/https and http/ws are considered the same for schemeful purposes.
  EXPECT_THAT(cookie_util::ComputeSameSiteContextForScriptSet(
                  kWssUrl, kSecureSiteForCookies,
                  false /* force_ignore_site_for_cookies */),
              ContextTypeIs(ContextType::SAME_SITE_LAX));
  EXPECT_THAT(
      cookie_util::ComputeSameSiteContextForScriptSet(
          kWsUrl, kSiteForCookies, false /* force_ignore_site_for_cookies */),
      ContextTypeIs(ContextType::SAME_SITE_LAX));
}

// Test cases where the URL chain has 1 member (i.e. no redirects).
TEST_P(CookieUtilComputeSameSiteContextTest, ForResponse) {
  for (const GURL& url : GetSameSiteUrls()) {
    // Same-site site-for-cookies.
    // (Cross-site cases covered above in UrlAndSiteForCookiesCrossSite test.)
    for (const SiteForCookies& site_for_cookies :
         GetSameSiteSitesForCookies()) {
      // For main frame navigations, setting all SameSite cookies is allowed
      // regardless of initiator.
      for (const absl::optional<url::Origin>& initiator : GetAllInitiators()) {
        if (!CanBeMainFrameNavigation(url, site_for_cookies))
          break;
        EXPECT_THAT(cookie_util::ComputeSameSiteContextForResponse(
                        {url}, site_for_cookies, initiator,
                        true /* is_main_frame_navigation */,
                        false /* force_ignore_site_for_cookies */),
                    ContextTypeIs(ContextType::SAME_SITE_LAX));
      }

      // For non-main-frame-navigation requests, the context should be lax iff
      // the initiator is same-site, and cross-site otherwise.
      for (const absl::optional<url::Origin>& initiator :
           GetSameSiteInitiators()) {
        EXPECT_THAT(cookie_util::ComputeSameSiteContextForResponse(
                        {url}, site_for_cookies, initiator,
                        false /* is_main_frame_navigation */,
                        false /* force_ignore_site_for_cookies */),
                    ContextTypeIs(ContextType::SAME_SITE_LAX));
      }
      for (const absl::optional<url::Origin>& initiator :
           GetCrossSiteInitiators()) {
        EXPECT_THAT(cookie_util::ComputeSameSiteContextForResponse(
                        {url}, site_for_cookies, initiator,
                        false /* is_main_frame_navigation */,
                        false /* force_ignore_site_for_cookies */),
                    ContextTypeIs(ContextType::CROSS_SITE));
      }
    }
  }
}

TEST_P(CookieUtilComputeSameSiteContextTest, ForResponse_SchemefulDowngrade) {
  // Some test cases where the context is downgraded when computed schemefully.
  // (Should already be covered above, but just to be explicit.)

  // URL and site-for-cookies are cross-scheme.
  // (If the URL and site-for-cookies are not schemefully same-site, this cannot
  // be a main frame navigation.)
  // With same-site initiator:
  EXPECT_EQ(SameSiteCookieContext(ContextType::SAME_SITE_LAX,
                                  ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForResponse(
                {kSiteUrl}, kSecureSiteForCookies, kSiteInitiator,
                false /* is_main_frame_navigation */,
                false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(SameSiteCookieContext(ContextType::SAME_SITE_LAX,
                                  ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForResponse(
                {kSecureSiteUrl}, kSiteForCookies, kSecureSiteInitiator,
                false /* is_main_frame_navigation */,
                false /* force_ignore_site_for_cookies */));
  // With cross-site initiator:
  EXPECT_EQ(SameSiteCookieContext(ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForResponse(
                {kSiteUrl}, kSecureSiteForCookies, kCrossSiteInitiator,
                false /* is_main_frame_navigation */,
                false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(SameSiteCookieContext(ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForResponse(
                {kSecureSiteUrl}, kSiteForCookies, kCrossSiteInitiator,
                false /* is_main_frame_navigation */,
                false /* force_ignore_site_for_cookies */));

  // Schemefully same-site URL and site-for-cookies with cross-scheme
  // initiator.
  for (bool is_main_frame_navigation : {false, true}) {
    ContextType lax_if_main_frame = is_main_frame_navigation
                                        ? ContextType::SAME_SITE_LAX
                                        : ContextType::CROSS_SITE;
    EXPECT_EQ(
        SameSiteCookieContext(ContextType::SAME_SITE_LAX, lax_if_main_frame),
        cookie_util::ComputeSameSiteContextForResponse(
            {kSiteUrl}, kSiteForCookies, kSecureSiteInitiator,
            is_main_frame_navigation,
            false /* force_ignore_site_for_cookies */));
    EXPECT_EQ(
        SameSiteCookieContext(ContextType::SAME_SITE_LAX, lax_if_main_frame),
        cookie_util::ComputeSameSiteContextForResponse(
            {kSecureSiteUrl}, kSecureSiteForCookies, kSiteInitiator,
            is_main_frame_navigation,
            false /* force_ignore_site_for_cookies */));
  }
}

TEST_P(CookieUtilComputeSameSiteContextTest, ForResponse_WebSocketSchemes) {
  // wss/https and http/ws are considered the same for schemeful purposes.
  // (ws/wss requests cannot be main frame navigations.)

  // Same-site initiators.
  for (const absl::optional<url::Origin>& initiator : GetSameSiteInitiators()) {
    EXPECT_THAT(cookie_util::ComputeSameSiteContextForResponse(
                    {kWsUrl}, kSiteForCookies, initiator,
                    false /* is_main_frame_navigation */,
                    false /* force_ignore_site_for_cookies */),
                ContextTypeIs(ContextType::SAME_SITE_LAX));
  }
  // Cross-site initiators.
  for (const absl::optional<url::Origin>& initiator :
       GetCrossSiteInitiators()) {
    EXPECT_THAT(cookie_util::ComputeSameSiteContextForResponse(
                    {kWsUrl}, kSiteForCookies, initiator,
                    false /* is_main_frame_navigation */,
                    false /* force_ignore_site_for_cookies */),
                ContextTypeIs(ContextType::CROSS_SITE));
  }
}

// Test cases where the URL chain contains multiple members, where the last
// member (current request URL) is same-site to kSiteUrl. (Everything is listed
// as same-site or cross-site relative to kSiteUrl.)
TEST_P(CookieUtilComputeSameSiteContextTest, ForResponse_Redirect) {
  struct {
    bool url_chain_is_same_site;
    bool site_for_cookies_is_same_site;
    bool initiator_is_same_site;
    // These are the expected context types considering redirect chains:
    ContextType expected_context_type;  // for non-main-frame-nav requests.
    ContextType expected_context_type_for_main_frame_navigation;
    // These are the expected context types not considering redirect chains:
    ContextType expected_context_type_without_chain;
    ContextType expected_context_type_for_main_frame_navigation_without_chain;
    // The expected redirect type (only applicable for chains):
    ContextRedirectTypeBug1221316 expected_redirect_type_with_chain;
  } kTestCases[] = {
      // If the url chain is same-site, then the result is the same with or
      // without considering the redirect chain.
      {true, true, true, ContextType::SAME_SITE_LAX, ContextType::SAME_SITE_LAX,
       ContextType::SAME_SITE_LAX, ContextType::SAME_SITE_LAX,
       ContextRedirectTypeBug1221316::kAllSameSiteRedirect},
      {true, true, false, ContextType::CROSS_SITE, ContextType::SAME_SITE_LAX,
       ContextType::CROSS_SITE, ContextType::SAME_SITE_LAX,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      {true, false, true, ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      {true, false, false, ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      // If the url chain is cross-site, then the result will differ depending
      // on whether the redirect chain is considered, when the site-for-cookies
      // and initiator are both same-site.
      {false, true, true, ContextType::CROSS_SITE, ContextType::SAME_SITE_LAX,
       ContextType::SAME_SITE_LAX, ContextType::SAME_SITE_LAX,
       ContextRedirectTypeBug1221316::kPartialSameSiteRedirect},
      {false, true, false, ContextType::CROSS_SITE, ContextType::SAME_SITE_LAX,
       ContextType::CROSS_SITE, ContextType::SAME_SITE_LAX,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      {false, false, true, ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
      {false, false, false, ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextType::CROSS_SITE, ContextType::CROSS_SITE,
       ContextRedirectTypeBug1221316::kCrossSiteRedirect},
  };
  for (const auto& test_case : kTestCases) {
    std::vector<std::vector<GURL>> url_chains =
        test_case.url_chain_is_same_site ? GetSameSiteUrlChains(kSiteUrl)
                                         : GetCrossSiteUrlChains(kSiteUrl);
    std::vector<SiteForCookies> sites_for_cookies =
        test_case.site_for_cookies_is_same_site ? GetSameSiteSitesForCookies()
                                                : GetCrossSiteSitesForCookies();
    std::vector<absl::optional<url::Origin>> initiators =
        test_case.initiator_is_same_site ? GetSameSiteInitiators()
                                         : GetCrossSiteInitiators();
    ContextType expected_context_type =
        DoesSameSiteConsiderRedirectChain()
            ? test_case.expected_context_type
            : test_case.expected_context_type_without_chain;
    ContextType expected_context_type_for_main_frame_navigation =
        DoesSameSiteConsiderRedirectChain()
            ? test_case.expected_context_type_for_main_frame_navigation
            : test_case
                  .expected_context_type_for_main_frame_navigation_without_chain;
    for (const std::vector<GURL>& url_chain : url_chains) {
      for (const SiteForCookies& site_for_cookies : sites_for_cookies) {
        for (const absl::optional<url::Origin>& initiator : initiators) {
          EXPECT_THAT(cookie_util::ComputeSameSiteContextForResponse(
                          url_chain, site_for_cookies, initiator,
                          false /* is_main_frame_navigation */,
                          false /* force_ignore_site_for_cookies */),
                      AllOf(ContextTypeIs(expected_context_type),
                            // The 'method' field is kept empty because it's
                            // only used to check http_method_bug_1221316 which
                            // is always empty for responses.
                            CrossSiteRedirectMetadataCorrect(
                                HttpMethod::kUnset,
                                test_case.expected_context_type_without_chain,
                                test_case.expected_context_type,
                                test_case.expected_redirect_type_with_chain)))
              << UrlChainToString(url_chain) << " "
              << site_for_cookies.ToDebugString() << " "
              << (initiator ? initiator->Serialize() : "nullopt");
          if (!CanBeMainFrameNavigation(url_chain.back(), site_for_cookies))
            continue;
          EXPECT_THAT(
              cookie_util::ComputeSameSiteContextForResponse(
                  url_chain, site_for_cookies, initiator,
                  true /* is_main_frame_navigation */,
                  false /* force_ignore_site_for_cookies */),
              AllOf(
                  ContextTypeIs(
                      expected_context_type_for_main_frame_navigation),
                  CrossSiteRedirectMetadataCorrect(
                      HttpMethod::kUnset,
                      test_case
                          .expected_context_type_for_main_frame_navigation_without_chain,
                      test_case.expected_context_type_for_main_frame_navigation,
                      test_case.expected_redirect_type_with_chain)))
              << UrlChainToString(url_chain) << " "
              << site_for_cookies.ToDebugString() << " "
              << (initiator ? initiator->Serialize() : "nullopt");
        }
      }
    }
  }
}

TEST_P(CookieUtilComputeSameSiteContextTest, ForSubresource) {
  for (const GURL& url : GetSameSiteUrls()) {
    // Same-site site-for-cookies.
    // (Cross-site cases covered above in UrlAndSiteForCookiesCrossSite test.)
    for (const SiteForCookies& site_for_cookies :
         GetSameSiteSitesForCookies()) {
      EXPECT_THAT(
          cookie_util::ComputeSameSiteContextForSubresource(
              url, site_for_cookies, false /* force_ignore_site_for_cookies */),
          ContextTypeIs(ContextType::SAME_SITE_STRICT));
    }
  }
}

TEST_P(CookieUtilComputeSameSiteContextTest,
       ForSubresource_SchemefulDowngrade) {
  // Some test cases where the context is downgraded when computed schemefully.
  // (Should already be covered above, but just to be explicit.)
  EXPECT_EQ(SameSiteCookieContext(ContextType::SAME_SITE_STRICT,
                                  ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForSubresource(
                kSiteUrl, kSecureSiteForCookies,
                false /* force_ignore_site_for_cookies */));
  EXPECT_EQ(SameSiteCookieContext(ContextType::SAME_SITE_STRICT,
                                  ContextType::CROSS_SITE),
            cookie_util::ComputeSameSiteContextForSubresource(
                kSecureSiteUrl, kSiteForCookies,
                false /* force_ignore_site_for_cookies */));
}

TEST_P(CookieUtilComputeSameSiteContextTest, ForSubresource_WebSocketSchemes) {
  // wss/https and http/ws are considered the same for schemeful purposes.
  EXPECT_THAT(cookie_util::ComputeSameSiteContextForSubresource(
                  kWssUrl, kSecureSiteForCookies,
                  false /* force_ignore_site_for_cookies */),
              ContextTypeIs(ContextType::SAME_SITE_STRICT));
  EXPECT_THAT(
      cookie_util::ComputeSameSiteContextForSubresource(
          kWsUrl, kSiteForCookies, false /* force_ignore_site_for_cookies */),
      ContextTypeIs(ContextType::SAME_SITE_STRICT));
}

TEST_P(CookieUtilComputeSameSiteContextTest, ForceIgnoreSiteForCookies) {
  // force_ignore_site_for_cookies overrides all checks and returns same-site
  // (STRICT for get or LAX for set).
  for (const GURL& url : GetAllUrls()) {
    for (const SiteForCookies& site_for_cookies : GetAllSitesForCookies()) {
      for (const absl::optional<url::Origin>& initiator : GetAllInitiators()) {
        for (const std::string& method : {"GET", "POST", "PUT", "HEAD"}) {
          EXPECT_THAT(cookie_util::ComputeSameSiteContextForScriptGet(
                          url, site_for_cookies, initiator,
                          true /* force_ignore_site_for_cookies */),
                      ContextTypeIs(ContextType::SAME_SITE_STRICT));
          EXPECT_THAT(cookie_util::ComputeSameSiteContextForScriptSet(
                          url, site_for_cookies,
                          true /* force_ignore_site_for_cookies */),
                      ContextTypeIs(ContextType::SAME_SITE_LAX));
          for (bool is_main_frame_navigation :
               IsMainFrameNavigationPossibleValues(url, site_for_cookies)) {
            EXPECT_THAT(cookie_util::ComputeSameSiteContextForRequest(
                            method, {url}, site_for_cookies, initiator,
                            is_main_frame_navigation,
                            true /* force_ignore_site_for_cookies */),
                        ContextTypeIs(ContextType::SAME_SITE_STRICT));
            EXPECT_THAT(cookie_util::ComputeSameSiteContextForResponse(
                            {url}, site_for_cookies, initiator,
                            is_main_frame_navigation,
                            true /* force_ignore_site_for_cookies */),
                        ContextTypeIs(ContextType::SAME_SITE_LAX));
            EXPECT_THAT(
                cookie_util::ComputeSameSiteContextForRequest(
                    method, {site_for_cookies.RepresentativeUrl(), url},
                    site_for_cookies, initiator, is_main_frame_navigation,
                    true /* force_ignore_site_for_cookies */),
                ContextTypeIs(ContextType::SAME_SITE_STRICT));
            EXPECT_THAT(
                cookie_util::ComputeSameSiteContextForResponse(
                    {site_for_cookies.RepresentativeUrl(), url},
                    site_for_cookies, initiator, is_main_frame_navigation,
                    true /* force_ignore_site_for_cookies */),
                ContextTypeIs(ContextType::SAME_SITE_LAX));
          }
          EXPECT_THAT(cookie_util::ComputeSameSiteContextForSubresource(
                          url, site_for_cookies,
                          true /* force_ignore_site_for_cookies */),
                      ContextTypeIs(ContextType::SAME_SITE_STRICT));
        }
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         CookieUtilComputeSameSiteContextTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

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

TEST(CookieUtilTest, GetSamePartyStatus_NotInSet) {
  const bool same_party_attribute_enabled = true;
  CookieOptions options;
  options.set_is_in_nontrivial_first_party_set(false);

  for (bool same_party : {false, true}) {
    for (bool secure : {false, true}) {
      for (bool httponly : {false, true}) {
        for (CookieSameSite same_site : {
                 CookieSameSite::NO_RESTRICTION,
                 CookieSameSite::LAX_MODE,
                 CookieSameSite::STRICT_MODE,
                 CookieSameSite::UNSPECIFIED,
             }) {
          for (SamePartyContext::Type party_context_type : {
                   SamePartyContext::Type::kCrossParty,
                   SamePartyContext::Type::kSameParty,
               }) {
            base::Time now = base::Time::Now();
            std::unique_ptr<CanonicalCookie> cookie =
                CanonicalCookie::CreateUnsafeCookieForTesting(
                    "cookie", "tasty", "example.test", "/", now, now, now, now,
                    secure, httponly, same_site,
                    CookiePriority::COOKIE_PRIORITY_DEFAULT, same_party);

            options.set_same_party_context(
                SamePartyContext(party_context_type));
            EXPECT_EQ(CookieSamePartyStatus::kNoSamePartyEnforcement,
                      cookie_util::GetSamePartyStatus(
                          *cookie, options, same_party_attribute_enabled));
          }
        }
      }
    }
  }
}

TEST(CookieUtilTest, GetSamePartyStatus_FeatureDisabled) {
  const bool same_party_attribute_enabled = false;
  CookieOptions options;
  options.set_is_in_nontrivial_first_party_set(true);

  for (bool same_party : {false, true}) {
    for (bool secure : {false, true}) {
      for (bool httponly : {false, true}) {
        for (CookieSameSite same_site : {
                 CookieSameSite::NO_RESTRICTION,
                 CookieSameSite::LAX_MODE,
                 CookieSameSite::STRICT_MODE,
                 CookieSameSite::UNSPECIFIED,
             }) {
          for (SamePartyContext::Type party_context_type : {
                   SamePartyContext::Type::kCrossParty,
                   SamePartyContext::Type::kSameParty,
               }) {
            base::Time now = base::Time::Now();
            std::unique_ptr<CanonicalCookie> cookie =
                CanonicalCookie::CreateUnsafeCookieForTesting(
                    "cookie", "tasty", "example.test", "/", now, now, now, now,
                    secure, httponly, same_site,
                    CookiePriority::COOKIE_PRIORITY_DEFAULT, same_party);

            options.set_same_party_context(
                SamePartyContext(party_context_type));
            EXPECT_EQ(CookieSamePartyStatus::kNoSamePartyEnforcement,
                      cookie_util::GetSamePartyStatus(
                          *cookie, options, same_party_attribute_enabled));
          }
        }
      }
    }
  }
}

TEST(CookieUtilTest, GetSamePartyStatus_NotSameParty) {
  CookieOptions options;
  options.set_is_in_nontrivial_first_party_set(true);

  for (bool secure : {false, true}) {
    for (bool httponly : {false, true}) {
      for (CookieSameSite same_site : {
               CookieSameSite::NO_RESTRICTION,
               CookieSameSite::LAX_MODE,
               CookieSameSite::STRICT_MODE,
               CookieSameSite::UNSPECIFIED,
           }) {
        for (SamePartyContext::Type party_context_type : {
                 SamePartyContext::Type::kCrossParty,
                 SamePartyContext::Type::kSameParty,
             }) {
          base::Time now = base::Time::Now();
          std::unique_ptr<CanonicalCookie> cookie =
              CanonicalCookie::CreateUnsafeCookieForTesting(
                  "cookie", "tasty", "example.test", "/", now, now, now, now,
                  secure, httponly, same_site,
                  CookiePriority::COOKIE_PRIORITY_DEFAULT,
                  false /* same_party */);

          options.set_same_party_context(SamePartyContext(party_context_type));
          EXPECT_EQ(CookieSamePartyStatus::kNoSamePartyEnforcement,
                    cookie_util::GetSamePartyStatus(
                        *cookie, options,
                        /*same_party_attribute_enabled=*/true));
        }
      }
    }
  }
}

TEST(CookieUtilTest, GetSamePartyStatus_SamePartySemantics) {
  CookieOptions options;
  options.set_is_in_nontrivial_first_party_set(true);

  // Note: some SameParty cookie configurations (e.g. non-Secure cookies) are
  // skipped, because they are invalid.
  for (bool httponly : {false, true}) {
    for (CookieSameSite same_site : {
             CookieSameSite::NO_RESTRICTION,
             CookieSameSite::LAX_MODE,
             CookieSameSite::UNSPECIFIED,
         }) {
      for (CookieOptions::SameSiteCookieContext::ContextType same_site_context :
           {
               CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE,
               CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX,
               CookieOptions::SameSiteCookieContext::ContextType::
                   SAME_SITE_LAX_METHOD_UNSAFE,
               CookieOptions::SameSiteCookieContext::ContextType::
                   SAME_SITE_STRICT,
           }) {
        for (CookieOptions::SameSiteCookieContext::ContextType
                 schemeful_same_site_context :
             {
                 CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE,
                 CookieOptions::SameSiteCookieContext::ContextType::
                     SAME_SITE_LAX,
                 CookieOptions::SameSiteCookieContext::ContextType::
                     SAME_SITE_LAX_METHOD_UNSAFE,
                 CookieOptions::SameSiteCookieContext::ContextType::
                     SAME_SITE_STRICT,
             }) {
          if (same_site_context < schemeful_same_site_context)
            continue;
          options.set_same_site_cookie_context(
              CookieOptions::SameSiteCookieContext(
                  same_site_context, schemeful_same_site_context));

          base::Time now = base::Time::Now();
          std::unique_ptr<CanonicalCookie> cookie =
              CanonicalCookie::CreateUnsafeCookieForTesting(
                  "cookie", "tasty", "example.test", "/", now, now, now, now,
                  true /* secure */, httponly, same_site,
                  CookiePriority::COOKIE_PRIORITY_DEFAULT,
                  true /* same_party */);

          options.set_same_party_context(
              SamePartyContext(SamePartyContext::Type::kCrossParty));
          EXPECT_EQ(CookieSamePartyStatus::kEnforceSamePartyExclude,
                    cookie_util::GetSamePartyStatus(
                        *cookie, options,
                        /*same_party_attribute_enabled=*/true));

          options.set_same_party_context(
              SamePartyContext(SamePartyContext::Type::kSameParty));
          EXPECT_EQ(CookieSamePartyStatus::kEnforceSamePartyInclude,
                    cookie_util::GetSamePartyStatus(
                        *cookie, options,
                        /*same_party_attribute_enabled=*/true));
        }
      }
    }
  }
}

}  // namespace

}  // namespace net
