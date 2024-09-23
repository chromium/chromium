// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/common/url_pattern.h"

#include <stddef.h>

#include <memory>

#include "base/strings/stringprintf.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// See url_pattern.h for examples of valid and invalid patterns.

static const int kAllSchemes =
    URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
    URLPattern::SCHEME_FILE | URLPattern::SCHEME_FTP |
    URLPattern::SCHEME_CHROMEUI | URLPattern::SCHEME_EXTENSION |
    URLPattern::SCHEME_FILESYSTEM | URLPattern::SCHEME_WS |
    URLPattern::SCHEME_WSS | URLPattern::SCHEME_DATA |
    URLPattern::SCHEME_UUID_IN_PACKAGE;

TEST(ExtensionURLPatternTest, ParseInvalid) {
  const struct {
    const char* pattern;
    URLPattern::ParseResult expected_result;
  } kInvalidPatterns[] = {
      {"http", URLPattern::ParseResult::kMissingSchemeSeparator},
      {"http:", URLPattern::ParseResult::kWrongSchemeSeparator},
      {"http:/", URLPattern::ParseResult::kWrongSchemeSeparator},
      {"about://", URLPattern::ParseResult::kWrongSchemeSeparator},
      {"http://", URLPattern::ParseResult::kEmptyHost},
      {"http:///", URLPattern::ParseResult::kEmptyHost},
      {"http://:1234/", URLPattern::ParseResult::kEmptyHost},
      {"http://*./", URLPattern::ParseResult::kEmptyHost},
      {"http://*foo/bar", URLPattern::ParseResult::kInvalidHostWildcard},
      {"http://foo.*.bar/baz", URLPattern::ParseResult::kInvalidHostWildcard},
      {"http://fo.*.ba:123/baz", URLPattern::ParseResult::kInvalidHostWildcard},
      {"http:/bar", URLPattern::ParseResult::kWrongSchemeSeparator},
      {"http://bar", URLPattern::ParseResult::kEmptyPath},
      {"http://foo.*/bar", URLPattern::ParseResult::kInvalidHostWildcard}};

  for (size_t i = 0; i < std::size(kInvalidPatterns); ++i) {
    URLPattern pattern(URLPattern::SCHEME_ALL);
    EXPECT_EQ(kInvalidPatterns[i].expected_result,
              pattern.Parse(kInvalidPatterns[i].pattern))
        << kInvalidPatterns[i].pattern;
  }

  {
    // Cannot use a C string, because this contains a null byte.
    std::string null_host("http://\0www/", 12);
    URLPattern pattern(URLPattern::SCHEME_ALL);
    EXPECT_EQ(URLPattern::ParseResult::kInvalidHost, pattern.Parse(null_host))
        << null_host;
  }
}

TEST(ExtensionURLPatternTest, Ports) {
  const struct {
    const std::string pattern;
    URLPattern::ParseResult expected_result;
    const char* expected_port;
  } kTestPatterns[] = {
      {"http://foo:1234/", URLPattern::ParseResult::kSuccess, "1234"},
      {"http://foo:1234/bar", URLPattern::ParseResult::kSuccess, "1234"},
      {"http://*.foo:1234/", URLPattern::ParseResult::kSuccess, "1234"},
      {"http://*.foo:1234/bar", URLPattern::ParseResult::kSuccess, "1234"},
      {"http://foo:/", URLPattern::ParseResult::kInvalidPort, "*"},
      {"http://*:1234/", URLPattern::ParseResult::kSuccess, "1234"},
      {"http://*:*/", URLPattern::ParseResult::kSuccess, "*"},
      {"http://foo:*/", URLPattern::ParseResult::kSuccess, "*"},
      {"http://*.foo:/", URLPattern::ParseResult::kInvalidPort, "*"},
      {"http://foo:com/", URLPattern::ParseResult::kInvalidPort, "*"},
      {"http://foo:123456/", URLPattern::ParseResult::kInvalidPort, "*"},
      {"http://foo:80:80/monkey", URLPattern::ParseResult::kInvalidPort, "*"},
      {"file://foo:1234/bar", URLPattern::ParseResult::kSuccess, "*"},
      {content::GetWebUIURLString("foo:1234/bar"),
       URLPattern::ParseResult::kInvalidPort, "*"},

      // Port-like strings in the path should not trigger a warning.
      {"http://*/:1234", URLPattern::ParseResult::kSuccess, "*"},
      {"http://*.foo/bar:1234", URLPattern::ParseResult::kSuccess, "*"},
      {"http://foo/bar:1234/path", URLPattern::ParseResult::kSuccess, "*"}};

  for (size_t i = 0; i < std::size(kTestPatterns); ++i) {
    URLPattern pattern(URLPattern::SCHEME_ALL);
    EXPECT_EQ(kTestPatterns[i].expected_result,
              pattern.Parse(kTestPatterns[i].pattern))
        << "Got unexpected result for URL pattern: "
        << kTestPatterns[i].pattern;
    EXPECT_EQ(kTestPatterns[i].expected_port, pattern.port())
        << "Got unexpected port for URL pattern: " << kTestPatterns[i].pattern;
  }
}

TEST(ExtensionURLPatternTest, IPv6Patterns) {
  constexpr struct {
    const char* pattern;
    const char* expected_host;
    const char* expected_port;
  } kSuccessTestPatterns[] = {
      {"http://[2607:f8b0:4005:805::200e]/", "[2607:f8b0:4005:805::200e]", "*"},
      {"http://[2607:f8b0:4005:805::200e]/*", "[2607:f8b0:4005:805::200e]",
       "*"},
      {"http://[2607:f8b0:4005:805::200e]:8888/*", "[2607:f8b0:4005:805::200e]",
       "8888"},
  };

  for (const auto& test_case : kSuccessTestPatterns) {
    SCOPED_TRACE(test_case.pattern);
    URLPattern pattern(URLPattern::SCHEME_HTTP);
    EXPECT_EQ(URLPattern::ParseResult::kSuccess,
              pattern.Parse(test_case.pattern));
    EXPECT_EQ(test_case.expected_host, pattern.host());
    EXPECT_EQ(test_case.expected_port, pattern.port());
  }

  constexpr struct {
    const char* pattern;
    URLPattern::ParseResult expected_failure;
  } kFailureTestPatterns[] = {
      // No port specified, but port separator.
      {"http://[2607:f8b0:4005:805::200e]:/*",
       URLPattern::ParseResult::kInvalidPort},
      // No host.
      {"http://[]:8888/*", URLPattern::ParseResult::kEmptyHost},
      // No closing bracket (`]`).
      {"http://[2607:f8b0:4005:805::200e/*",
       URLPattern::ParseResult::kInvalidHost},
      // Two closing brackets (`]]`).
      {"http://[2607:f8b0:4005:805::200e]]/*",
       URLPattern::ParseResult::kInvalidHost},
      // Two open brackets (`[[`).
      {"http://[[2607:f8b0:4005:805::200e]/*",
       URLPattern::ParseResult::kInvalidHost},
      // Too few colons in the last chunk.
      {"http://[2607:f8b0:4005:805:200e]/*",
       URLPattern::ParseResult::kInvalidHost},
      // Non-hex piece.
      {"http://[2607:f8b0:4005:805:200e:12:bogus]/*",
       URLPattern::ParseResult::kInvalidHost},
      {"http://[[2607:f8b0:4005:805::200e]:abc/*",
       URLPattern::ParseResult::kInvalidPort},
  };

  for (const auto& test_case : kFailureTestPatterns) {
    SCOPED_TRACE(test_case.pattern);
    URLPattern pattern(URLPattern::SCHEME_HTTP);
    EXPECT_EQ(test_case.expected_failure, pattern.Parse(test_case.pattern));
  }
}

// Verify percent encoding behavior.
TEST(ExtensionURLPatternTest, PercentEncodedAscii) {
  {
    URLPattern pattern(kAllSchemes);
    ASSERT_EQ(URLPattern::ParseResult::kSuccess,
              pattern.Parse("http://*/%40*"));
    EXPECT_EQ("http", pattern.scheme());
    EXPECT_EQ("", pattern.host());
    EXPECT_TRUE(pattern.match_subdomains());
    EXPECT_FALSE(pattern.match_all_urls());
    EXPECT_EQ("/%40*", pattern.path());
  }
  {
    URLPattern pattern(kAllSchemes);
    ASSERT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse("http://*/@*"));
    EXPECT_EQ("http", pattern.scheme());
    EXPECT_EQ("", pattern.host());
    EXPECT_TRUE(pattern.match_subdomains());
    EXPECT_FALSE(pattern.match_all_urls());
    EXPECT_EQ("/@*", pattern.path());
  }
}

// Verify percent encoding behavior.
TEST(ExtensionURLPatternTest, PercentEncodedNonAscii) {
  {
    URLPattern pattern(kAllSchemes);
    ASSERT_EQ(URLPattern::ParseResult::kSuccess,
              pattern.Parse("http://*/%F0%9F%90%B1*"));
    EXPECT_EQ("http", pattern.scheme());
    EXPECT_EQ("", pattern.host());
    EXPECT_TRUE(pattern.match_subdomains());
    EXPECT_FALSE(pattern.match_all_urls());
    EXPECT_EQ("/%F0%9F%90%B1*", pattern.path());
  }
  {
    URLPattern pattern(kAllSchemes);
    ASSERT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse("http://*/🐱*"));
    EXPECT_EQ("http", pattern.scheme());
    EXPECT_EQ("", pattern.host());
    EXPECT_TRUE(pattern.match_subdomains());
    EXPECT_FALSE(pattern.match_all_urls());
    EXPECT_EQ("/🐱*", pattern.path());
  }
}

// all pages for a given scheme
TEST(ExtensionURLPatternTest, Match1) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse("http://*/*"));
  EXPECT_EQ("http", pattern.scheme());
  EXPECT_EQ("", pattern.host());
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://google.com")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://yahoo.com")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://google.com/foo")));
  EXPECT_FALSE(pattern.MatchesURL(GURL("https://google.com")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://74.125.127.100/search")));
}

// all domains
TEST(ExtensionURLPatternTest, Match2) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse("https://*/foo*"));
  EXPECT_EQ("https", pattern.scheme());
  EXPECT_EQ("", pattern.host());
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/foo*", pattern.path());
  EXPECT_TRUE(pattern.MatchesURL(GURL("https://www.google.com/foo")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("https://www.google.com/foobar")));
  EXPECT_FALSE(pattern.MatchesURL(GURL("http://www.google.com/foo")));
  EXPECT_FALSE(pattern.MatchesURL(GURL("https://www.google.com/")));
  EXPECT_TRUE(pattern.MatchesURL(
      GURL("filesystem:https://www.google.com/foobar/")));
}

// subdomains
TEST(URLPatternTest, Match3) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse("http://*.google.com/foo*bar"));
  EXPECT_EQ("http", pattern.scheme());
  EXPECT_EQ("google.com", pattern.host());
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/foo*bar", pattern.path());
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://google.com/foobar")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://www.google.com/foobar")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://www.google.com/foo?bar")));
  EXPECT_FALSE(pattern.MatchesURL(GURL("http://wwwgoogle.com/foobar")));
  EXPECT_TRUE(pattern.MatchesURL(
      GURL("http://monkey.images.google.com/foooobar")));
  EXPECT_FALSE(pattern.MatchesURL(GURL("http://yahoo.com/foobar")));
  EXPECT_TRUE(pattern.MatchesURL(
      GURL("filesystem:http://google.com/foo/bar")));
  EXPECT_FALSE(pattern.MatchesURL(
      GURL("filesystem:http://google.com/temporary/foobar")));
}

// glob escaping
TEST(ExtensionURLPatternTest, Match5) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse("file:///foo?bar\\*baz"));
  EXPECT_EQ("file", pattern.scheme());
  EXPECT_EQ("", pattern.host());
  EXPECT_FALSE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/foo?bar\\*baz", pattern.path());
  EXPECT_TRUE(pattern.MatchesURL(GURL("file:///foo?bar\\hellobaz")));
  EXPECT_FALSE(pattern.MatchesURL(GURL("file:///fooXbar\\hellobaz")));
}

// ip addresses
TEST(ExtensionURLPatternTest, Match6) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse("http://127.0.0.1/*"));
  EXPECT_EQ("http", pattern.scheme());
  EXPECT_EQ("127.0.0.1", pattern.host());
  EXPECT_FALSE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://127.0.0.1")));
}

// subdomain matching with ip addresses
TEST(ExtensionURLPatternTest, Match7) {
  URLPattern pattern(kAllSchemes);
  // allowed, but useless
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse("http://*.0.0.1/*"));
  EXPECT_EQ("http", pattern.scheme());
  // Canonicalization forces 0.0.1 to 0.0.0.1.
  EXPECT_EQ("0.0.0.1", pattern.host());
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  // Subdomain matching is never done if the argument has an IP address host.
  EXPECT_FALSE(pattern.MatchesURL(GURL("http://127.0.0.1")));
}

// unicode
TEST(ExtensionURLPatternTest, Match8) {
  URLPattern pattern(kAllSchemes);
  // The below is the ASCII encoding of the following URL:
  // http://*.\xe1\x80\xbf/a\xc2\x81\xe1*
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse("http://*.xn--gkd/a%C2%81%E1*"));
  EXPECT_EQ("http", pattern.scheme());
  EXPECT_EQ("xn--gkd", pattern.host());
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/a%C2%81%E1*", pattern.path());
  EXPECT_TRUE(pattern.MatchesURL(
      GURL("http://abc.\xe1\x80\xbf/a\xc2\x81\xe1xyz")));
  EXPECT_TRUE(pattern.MatchesURL(
      GURL("http://\xe1\x80\xbf/a\xc2\x81\xe1\xe1")));
}

// chrome://
TEST(ExtensionURLPatternTest, Match9) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse(content::GetWebUIURLString("favicon/*")));
  EXPECT_EQ(content::kChromeUIScheme, pattern.scheme());
  EXPECT_EQ("favicon", pattern.host());
  EXPECT_FALSE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  EXPECT_TRUE(
      pattern.MatchesURL(content::GetWebUIURL("favicon/http://google.com")));
  EXPECT_TRUE(
      pattern.MatchesURL(content::GetWebUIURL("favicon/https://google.com")));
  EXPECT_FALSE(pattern.MatchesURL(content::GetWebUIURL("history")));
}

// *://
TEST(ExtensionURLPatternTest, Match10) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse("*://*/*"));
  EXPECT_TRUE(pattern.MatchesScheme("http"));
  EXPECT_TRUE(pattern.MatchesScheme("https"));
  EXPECT_FALSE(pattern.MatchesScheme(content::kChromeUIScheme));
  EXPECT_FALSE(pattern.MatchesScheme("file"));
  EXPECT_FALSE(pattern.MatchesScheme("ftp"));
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://127.0.0.1")));
  EXPECT_FALSE(
      pattern.MatchesURL(content::GetWebUIURL("favicon/http://google.com")));
  EXPECT_FALSE(pattern.MatchesURL(GURL("file:///foo/bar")));
  EXPECT_FALSE(pattern.MatchesURL(GURL("file://localhost/foo/bar")));
}

// <all_urls>
TEST(ExtensionURLPatternTest, Match11) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse("<all_urls>"));
  EXPECT_TRUE(pattern.MatchesScheme(content::kChromeUIScheme));
  EXPECT_TRUE(pattern.MatchesScheme("http"));
  EXPECT_TRUE(pattern.MatchesScheme("https"));
  EXPECT_TRUE(pattern.MatchesScheme("file"));
  EXPECT_TRUE(pattern.MatchesScheme("filesystem"));
  EXPECT_TRUE(pattern.MatchesScheme(extensions::kExtensionScheme));
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_TRUE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  EXPECT_TRUE(
      pattern.MatchesURL(content::GetWebUIURL("favicon/http://google.com")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://127.0.0.1")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("file:///foo/bar")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("file://localhost/foo/bar")));

  // Make sure the properties are the same when creating an <all_urls> pattern
  // via SetMatchAllURLs and by parsing <all_urls>.
  URLPattern pattern2(kAllSchemes);
  pattern2.SetMatchAllURLs(true);

  EXPECT_EQ(pattern.valid_schemes(), pattern2.valid_schemes());
  EXPECT_EQ(pattern.match_subdomains(), pattern2.match_subdomains());
  EXPECT_EQ(pattern.path(), pattern2.path());
  EXPECT_EQ(pattern.match_all_urls(), pattern2.match_all_urls());
  EXPECT_EQ(pattern.scheme(), pattern2.scheme());
  EXPECT_EQ(pattern.port(), pattern2.port());
  EXPECT_EQ(pattern.GetAsString(), pattern2.GetAsString());
}

// SCHEME_ALL matches all schemes.
TEST(ExtensionURLPatternTest, Match12) {
  URLPattern pattern(URLPattern::SCHEME_ALL);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse("<all_urls>"));
  EXPECT_TRUE(pattern.MatchesScheme(content::kChromeUIScheme));
  EXPECT_TRUE(pattern.MatchesScheme("http"));
  EXPECT_TRUE(pattern.MatchesScheme("https"));
  EXPECT_TRUE(pattern.MatchesScheme("file"));
  EXPECT_TRUE(pattern.MatchesScheme("filesystem"));
  EXPECT_TRUE(pattern.MatchesScheme("javascript"));
  EXPECT_TRUE(pattern.MatchesScheme("data"));
  EXPECT_TRUE(pattern.MatchesScheme("about"));
  EXPECT_TRUE(pattern.MatchesScheme(extensions::kExtensionScheme));
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_TRUE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  EXPECT_TRUE(
      pattern.MatchesURL(content::GetWebUIURL("favicon/http://google.com")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://127.0.0.1")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("file:///foo/bar")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("file://localhost/foo/bar")));
  EXPECT_TRUE(pattern.MatchesURL(content::GetWebUIURL("newtab")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("about:blank")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("about:version")));
  EXPECT_TRUE(pattern.MatchesURL(
      GURL("data:text/html;charset=utf-8,<html>asdf</html>")));
}

TEST(ExtensionURLPatternTest, DoesntMatchInvalid) {
  URLPattern pattern(kAllSchemes);
  // Even the all_urls pattern shouldn't match an invalid URL.
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse(URLPattern::kAllUrlsPattern));
  EXPECT_FALSE(pattern.MatchesURL(GURL("http:")));
}

TEST(ExtensionURLPatternTest, WildcardMatchesPathlessUrl) {
  URLPattern pattern(URLPattern::SCHEME_ALL);
  // The all_urls pattern should match a valid URL with no path.
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse(URLPattern::kAllUrlsPattern));
  EXPECT_TRUE(pattern.MatchesURL(GURL("javascript:")));
}

TEST(ExtensionURLPatternTest, NonwildcardDoesntMatchPathlessUrl) {
  URLPattern pattern(URLPattern::SCHEME_ALL);
  // Any pattern other than the all_urls pattern should not
  // match a valid URL with no path, because any such pattern
  // must contain a nonempty path.
  EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse("*://*/*"));
  EXPECT_FALSE(pattern.MatchesURL(GURL("javascript:")));
}

static const struct MatchPatterns {
  const char* pattern;
  const char* matches;
} kMatch13UrlPatternTestCases[] = {
  {"about:*", "about:blank"},
  {"about:blank", "about:blank"},
  {"about:*", "about:version"},
  {"chrome-extension://*/*", "chrome-extension://FTW"},
  {"data:*", "data:monkey"},
  {"javascript:*", "javascript:atemyhomework"},
};

// SCHEME_ALL and specific schemes.
TEST(ExtensionURLPatternTest, Match13) {
  for (size_t i = 0; i < std::size(kMatch13UrlPatternTestCases); ++i) {
    URLPattern pattern(URLPattern::SCHEME_ALL);
    EXPECT_EQ(URLPattern::ParseResult::kSuccess,
              pattern.Parse(kMatch13UrlPatternTestCases[i].pattern))
        << " while parsing " << kMatch13UrlPatternTestCases[i].pattern;
    EXPECT_TRUE(pattern.MatchesURL(
        GURL(kMatch13UrlPatternTestCases[i].matches)))
        << " while matching " << kMatch13UrlPatternTestCases[i].matches;
  }

  // Negative test.
  URLPattern pattern(URLPattern::SCHEME_ALL);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse("data:*"));
  EXPECT_FALSE(pattern.MatchesURL(GURL("about:blank")));
}

// file scheme with empty hostname
TEST(ExtensionURLPatternTest, Match14) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse("file:///foo*"));
  EXPECT_EQ("file", pattern.scheme());
  EXPECT_EQ("", pattern.host());
  EXPECT_FALSE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/foo*", pattern.path());
  EXPECT_FALSE(pattern.MatchesURL(GURL("file://foo")));
  EXPECT_FALSE(pattern.MatchesURL(GURL("file://foobar")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("file:///foo")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("file:///foobar")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("file://localhost/foo")));
}

// file scheme without hostname part
TEST(ExtensionURLPatternTest, Match15) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse("file://foo*"));
  EXPECT_EQ("file", pattern.scheme());
  EXPECT_EQ("", pattern.host());
  EXPECT_FALSE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/foo*", pattern.path());
  EXPECT_FALSE(pattern.MatchesURL(GURL("file://foo")));
  EXPECT_FALSE(pattern.MatchesURL(GURL("file://foobar")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("file:///foo")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("file:///foobar")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("file://localhost/foo")));
}

// file scheme with hostname
TEST(ExtensionURLPatternTest, Match16) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse("file://localhost/foo*"));
  EXPECT_EQ("file", pattern.scheme());
  // Since hostname is ignored for file://.
  EXPECT_EQ("", pattern.host());
  EXPECT_FALSE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/foo*", pattern.path());
  EXPECT_FALSE(pattern.MatchesURL(GURL("file://foo")));
  EXPECT_FALSE(pattern.MatchesURL(GURL("file://foobar")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("file:///foo")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("file:///foobar")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("file://localhost/foo")));
}

// Specific port
TEST(ExtensionURLPatternTest, Match17) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse("http://www.example.com:80/foo"));
  EXPECT_EQ("http", pattern.scheme());
  EXPECT_EQ("www.example.com", pattern.host());
  EXPECT_FALSE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/foo", pattern.path());
  EXPECT_EQ("80", pattern.port());
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://www.example.com:80/foo")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://www.example.com/foo")));
  EXPECT_FALSE(pattern.MatchesURL(GURL("http://www.example.com:8080/foo")));
  EXPECT_FALSE(pattern.MatchesURL(
      GURL("filesystem:http://www.example.com:8080/foo/")));
  EXPECT_FALSE(pattern.MatchesURL(
      GURL("filesystem:http://www.example.com/f/foo")));
}

// Explicit port wildcard
TEST(ExtensionURLPatternTest, Match18) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse("http://www.example.com:*/foo"));
  EXPECT_EQ("http", pattern.scheme());
  EXPECT_EQ("www.example.com", pattern.host());
  EXPECT_FALSE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/foo", pattern.path());
  EXPECT_EQ("*", pattern.port());
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://www.example.com:80/foo")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://www.example.com/foo")));
  EXPECT_TRUE(pattern.MatchesURL(GURL("http://www.example.com:8080/foo")));
  EXPECT_FALSE(pattern.MatchesURL(
      GURL("filesystem:http://www.example.com:8080/foo/")));
}

// chrome-extension://
TEST(ExtensionURLPatternTest, Match19) {
  URLPattern pattern(URLPattern::SCHEME_EXTENSION);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse("chrome-extension://ftw/*"));
  EXPECT_EQ(extensions::kExtensionScheme, pattern.scheme());
  EXPECT_EQ("ftw", pattern.host());
  EXPECT_FALSE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  EXPECT_TRUE(pattern.MatchesURL(GURL("chrome-extension://ftw")));
  EXPECT_TRUE(pattern.MatchesURL(
      GURL("chrome-extension://ftw/http://google.com")));
  EXPECT_TRUE(pattern.MatchesURL(
      GURL("chrome-extension://ftw/https://google.com")));
  EXPECT_FALSE(pattern.MatchesURL(GURL("chrome-extension://foobar")));
  EXPECT_TRUE(pattern.MatchesURL(
      GURL("filesystem:chrome-extension://ftw/t/file.txt")));
}

static const struct GetAsStringPatterns {
  const std::string pattern;
} kGetAsStringTestCases[] = {
    {"http://www/"},
    {"http://*/*"},
    {content::GetWebUIURLString("*/*")},
    {content::GetWebUIURLString("newtab/")},
    {"about:*"},
    {"about:blank"},
    {"chrome-extension://*/*"},
    {"chrome-extension://ftw/"},
    {"data:*"},
    {"data:monkey"},
    {"javascript:*"},
    {"javascript:atemyhomework"},
    {"http://www.example.com:8080/foo"},
};

TEST(ExtensionURLPatternTest, GetAsString) {
  for (size_t i = 0; i < std::size(kGetAsStringTestCases); ++i) {
    URLPattern pattern(URLPattern::SCHEME_ALL);
    EXPECT_EQ(URLPattern::ParseResult::kSuccess,
              pattern.Parse(kGetAsStringTestCases[i].pattern))
        << "Error parsing " << kGetAsStringTestCases[i].pattern;
    EXPECT_EQ(kGetAsStringTestCases[i].pattern,
              pattern.GetAsString());
  }
}

testing::AssertionResult Overlaps(const URLPattern& pattern1,
                                  const URLPattern& pattern2) {
  if (!pattern1.OverlapsWith(pattern2)) {
    return testing::AssertionFailure()
        << pattern1.GetAsString() << " does not overlap " <<
                                     pattern2.GetAsString();
  }
  if (!pattern2.OverlapsWith(pattern1)) {
    return testing::AssertionFailure()
        << pattern2.GetAsString() << " does not overlap " <<
                                     pattern1.GetAsString();
  }
  return testing::AssertionSuccess()
      << pattern1.GetAsString() << " overlaps with " << pattern2.GetAsString();
}

TEST(ExtensionURLPatternTest, Overlaps) {
  URLPattern pattern1(kAllSchemes, "http://www.google.com/foo/*");
  URLPattern pattern2(kAllSchemes, "https://www.google.com/foo/*");
  URLPattern pattern3(kAllSchemes, "http://*.google.com/foo/*");
  URLPattern pattern4(kAllSchemes, "http://*.yahooo.com/foo/*");
  URLPattern pattern5(kAllSchemes, "http://www.yahooo.com/bar/*");
  URLPattern pattern6(kAllSchemes,
                      "http://www.yahooo.com/bar/baz/*");
  URLPattern pattern7(kAllSchemes, "file:///*");
  URLPattern pattern8(kAllSchemes, "*://*/*");
  URLPattern pattern9(URLPattern::SCHEME_HTTPS, "*://*/*");
  URLPattern pattern10(kAllSchemes, "<all_urls>");

  EXPECT_TRUE(Overlaps(pattern1, pattern1));
  EXPECT_FALSE(Overlaps(pattern1, pattern2));
  EXPECT_TRUE(Overlaps(pattern1, pattern3));
  EXPECT_FALSE(Overlaps(pattern1, pattern4));
  EXPECT_FALSE(Overlaps(pattern3, pattern4));
  EXPECT_FALSE(Overlaps(pattern4, pattern5));
  EXPECT_TRUE(Overlaps(pattern5, pattern6));

  // Test that scheme restrictions work.
  EXPECT_TRUE(Overlaps(pattern1, pattern8));
  EXPECT_FALSE(Overlaps(pattern1, pattern9));
  EXPECT_TRUE(Overlaps(pattern1, pattern10));

  // Test that '<all_urls>' includes file URLs, while scheme '*' does not.
  EXPECT_FALSE(Overlaps(pattern7, pattern8));
  EXPECT_TRUE(Overlaps(pattern7, pattern10));

  // Test that wildcard schemes are handled correctly, especially when compared
  // to each-other.
  URLPattern pattern11(kAllSchemes, "http://example.com/*");
  URLPattern pattern12(kAllSchemes, "*://example.com/*");
  URLPattern pattern13(kAllSchemes, "*://example.com/foo/*");
  URLPattern pattern14(kAllSchemes, "*://google.com/*");
  EXPECT_TRUE(Overlaps(pattern8, pattern12));
  EXPECT_TRUE(Overlaps(pattern9, pattern12));
  EXPECT_TRUE(Overlaps(pattern10, pattern12));
  EXPECT_TRUE(Overlaps(pattern11, pattern12));
  EXPECT_TRUE(Overlaps(pattern12, pattern13));
  EXPECT_TRUE(Overlaps(pattern11, pattern13));
  EXPECT_FALSE(Overlaps(pattern14, pattern12));
  EXPECT_FALSE(Overlaps(pattern14, pattern13));
}

TEST(ExtensionURLPatternTest, ConvertToExplicitSchemes) {
  URLPatternList all_urls(URLPattern(
      kAllSchemes,
      "<all_urls>").ConvertToExplicitSchemes());

  URLPatternList all_schemes(URLPattern(
      kAllSchemes,
      "*://google.com/foo").ConvertToExplicitSchemes());

  URLPatternList monkey(URLPattern(
      URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
      URLPattern::SCHEME_FTP,
      "http://google.com/monkey").ConvertToExplicitSchemes());

  ASSERT_EQ(11u, all_urls.size());
  ASSERT_EQ(2u, all_schemes.size());
  ASSERT_EQ(1u, monkey.size());

  EXPECT_EQ("http://*/*", all_urls[0].GetAsString());
  EXPECT_EQ("https://*/*", all_urls[1].GetAsString());
  EXPECT_EQ("file:///*", all_urls[2].GetAsString());
  EXPECT_EQ("ftp://*/*", all_urls[3].GetAsString());
  EXPECT_EQ(content::GetWebUIURLString("*/*"), all_urls[4].GetAsString());
  EXPECT_EQ("chrome-extension://*/*", all_urls[5].GetAsString());
  EXPECT_EQ("filesystem://*/*", all_urls[6].GetAsString());
  EXPECT_EQ("ws://*/*", all_urls[7].GetAsString());
  EXPECT_EQ("wss://*/*", all_urls[8].GetAsString());
  EXPECT_EQ("data:/*", all_urls[9].GetAsString());
  EXPECT_EQ("uuid-in-package:/*", all_urls[10].GetAsString());

  EXPECT_EQ("http://google.com/foo", all_schemes[0].GetAsString());
  EXPECT_EQ("https://google.com/foo", all_schemes[1].GetAsString());

  EXPECT_EQ("http://google.com/monkey", monkey[0].GetAsString());
}

TEST(ExtensionURLPatternTest, IgnorePorts) {
  std::string pattern_str = "http://www.example.com:8080/foo";
  GURL url("http://www.example.com:1234/foo");

  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse(pattern_str));

  EXPECT_EQ(pattern_str, pattern.GetAsString());
  EXPECT_FALSE(pattern.MatchesURL(url));
}

TEST(ExtensionURLPatternTest, IgnoreMissingBackslashes) {
  std::string pattern_str1 = "http://www.example.com/example";
  std::string pattern_str2 = "http://www.example.com/example/*";
  GURL url1("http://www.example.com/example");
  GURL url2("http://www.example.com/example/");

  URLPattern pattern1(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern1.Parse(pattern_str1));
  URLPattern pattern2(kAllSchemes);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern2.Parse(pattern_str2));

  // Same patterns should match same urls.
  EXPECT_TRUE(pattern1.MatchesURL(url1));
  EXPECT_TRUE(pattern2.MatchesURL(url2));
  // The not terminated path should match the terminated pattern.
  EXPECT_TRUE(pattern2.MatchesURL(url1));
  // The terminated path however should not match the unterminated pattern.
  EXPECT_FALSE(pattern1.MatchesURL(url2));
}

TEST(ExtensionURLPatternTest, Equals) {
  const struct {
    const char* pattern1;
    const char* pattern2;
    bool expected_equal;
  } kEqualsTestCases[] = {
    // schemes
    { "http://en.google.com/blah/*/foo",
      "https://en.google.com/blah/*/foo",
      false
    },
    { "https://en.google.com/blah/*/foo",
      "https://en.google.com/blah/*/foo",
      true
    },
    { "https://en.google.com/blah/*/foo",
      "ftp://en.google.com/blah/*/foo",
      false
    },

    // subdomains
    { "https://en.google.com/blah/*/foo",
      "https://fr.google.com/blah/*/foo",
      false
    },
    { "https://www.google.com/blah/*/foo",
      "https://*.google.com/blah/*/foo",
      false
    },
    { "https://*.google.com/blah/*/foo",
      "https://*.google.com/blah/*/foo",
      true
    },

    // domains
    { "http://en.example.com/blah/*/foo",
      "http://en.google.com/blah/*/foo",
      false
    },

    // ports
    { "http://en.google.com:8000/blah/*/foo",
      "http://en.google.com/blah/*/foo",
      false
    },
    { "http://fr.google.com:8000/blah/*/foo",
      "http://fr.google.com:8000/blah/*/foo",
      true
    },
    { "http://en.google.com:8000/blah/*/foo",
      "http://en.google.com:8080/blah/*/foo",
      false
    },

    // paths
    { "http://en.google.com/blah/*/foo",
      "http://en.google.com/blah/*",
      false
    },
    { "http://en.google.com/*",
      "http://en.google.com/",
      false
    },
    { "http://en.google.com/*",
      "http://en.google.com/*",
      true
    },

    // all_urls
    { "<all_urls>",
      "<all_urls>",
      true
    },
    { "<all_urls>",
      "http://*/*",
      false
    }
  };

  for (size_t i = 0; i < std::size(kEqualsTestCases); ++i) {
    std::string message = kEqualsTestCases[i].pattern1;
    message += " ";
    message += kEqualsTestCases[i].pattern2;

    URLPattern pattern1(URLPattern::SCHEME_ALL);
    URLPattern pattern2(URLPattern::SCHEME_ALL);

    pattern1.Parse(kEqualsTestCases[i].pattern1);
    pattern2.Parse(kEqualsTestCases[i].pattern2);
    EXPECT_EQ(kEqualsTestCases[i].expected_equal, pattern1 == pattern2)
        << message;
  }
}

TEST(ExtensionURLPatternTest, CanReusePatternWithParse) {
  URLPattern pattern1(URLPattern::SCHEME_ALL);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern1.Parse("http://aa.com/*"));
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern1.Parse("http://bb.com/*"));

  EXPECT_TRUE(pattern1.MatchesURL(GURL("http://bb.com/path")));
  EXPECT_FALSE(pattern1.MatchesURL(GURL("http://aa.com/path")));

  URLPattern pattern2(URLPattern::SCHEME_ALL, URLPattern::kAllUrlsPattern);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern2.Parse("http://aa.com/*"));

  EXPECT_FALSE(pattern2.MatchesURL(GURL("http://bb.com/path")));
  EXPECT_TRUE(pattern2.MatchesURL(GURL("http://aa.com/path")));
  EXPECT_FALSE(pattern2.MatchesURL(GURL("http://sub.aa.com/path")));

  URLPattern pattern3(URLPattern::SCHEME_ALL, "http://aa.com/*");
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern3.Parse("http://aa.com:88/*"));
  EXPECT_FALSE(pattern3.MatchesURL(GURL("http://aa.com/path")));
  EXPECT_TRUE(pattern3.MatchesURL(GURL("http://aa.com:88/path")));
}

// Returns success if neither |a| nor |b| encompasses the other.
testing::AssertionResult NeitherContains(const URLPattern& a,
                                         const URLPattern& b) {
  if (a.Contains(b)) {
    return testing::AssertionFailure() << a.GetAsString() << " encompasses " <<
                                          b.GetAsString();
  }
  if (b.Contains(a)) {
    return testing::AssertionFailure() << b.GetAsString() << " encompasses " <<
                                          a.GetAsString();
  }
  return testing::AssertionSuccess() <<
      "Neither " << a.GetAsString() << " nor " << b.GetAsString() <<
      " encompass the other";
}

// Returns success if |a| encompasses |b| but not the other way around.
testing::AssertionResult StrictlyContains(const URLPattern& a,
                                          const URLPattern& b) {
  if (!a.Contains(b)) {
    return testing::AssertionFailure() << a.GetAsString() <<
                                          " does not encompass " <<
                                          b.GetAsString();
  }
  if (b.Contains(a)) {
    return testing::AssertionFailure() << b.GetAsString() << " encompasses " <<
                                          a.GetAsString();
  }
  return testing::AssertionSuccess() << a.GetAsString() <<
                                        " strictly encompasses " <<
                                        b.GetAsString();
}

TEST(ExtensionURLPatternTest, Subset) {
  URLPattern pattern1(kAllSchemes, "http://www.google.com/foo/*");
  URLPattern pattern2(kAllSchemes, "https://www.google.com/foo/*");
  URLPattern pattern3(kAllSchemes, "http://*.google.com/foo/*");
  URLPattern pattern4(kAllSchemes, "http://*.yahooo.com/foo/*");
  URLPattern pattern5(kAllSchemes, "http://www.yahooo.com/bar/*");
  URLPattern pattern6(kAllSchemes, "http://www.yahooo.com/bar/baz/*");
  URLPattern pattern7(kAllSchemes, "file:///*");
  URLPattern pattern8(kAllSchemes, "*://*/*");
  URLPattern pattern9(URLPattern::SCHEME_HTTPS, "*://*/*");
  URLPattern pattern10(kAllSchemes, "<all_urls>");
  URLPattern pattern11(kAllSchemes, "http://example.com/*");
  URLPattern pattern12(kAllSchemes, "*://example.com/*");
  URLPattern pattern13(kAllSchemes, "*://example.com/foo/*");
  URLPattern pattern14(kAllSchemes, "http://yahoo.com/*");
  URLPattern pattern15(kAllSchemes, "http://*.yahoo.com/*");

  // All patterns should encompass themselves.
  EXPECT_TRUE(pattern1.Contains(pattern1));
  EXPECT_TRUE(pattern2.Contains(pattern2));
  EXPECT_TRUE(pattern3.Contains(pattern3));
  EXPECT_TRUE(pattern4.Contains(pattern4));
  EXPECT_TRUE(pattern5.Contains(pattern5));
  EXPECT_TRUE(pattern6.Contains(pattern6));
  EXPECT_TRUE(pattern7.Contains(pattern7));
  EXPECT_TRUE(pattern8.Contains(pattern8));
  EXPECT_TRUE(pattern9.Contains(pattern9));
  EXPECT_TRUE(pattern10.Contains(pattern10));
  EXPECT_TRUE(pattern11.Contains(pattern11));
  EXPECT_TRUE(pattern12.Contains(pattern12));
  EXPECT_TRUE(pattern13.Contains(pattern13));

  // pattern1's relationship to the other patterns.
  EXPECT_TRUE(NeitherContains(pattern1, pattern2));
  EXPECT_TRUE(StrictlyContains(pattern3, pattern1));
  EXPECT_TRUE(NeitherContains(pattern1, pattern4));
  EXPECT_TRUE(NeitherContains(pattern1, pattern5));
  EXPECT_TRUE(NeitherContains(pattern1, pattern6));
  EXPECT_TRUE(NeitherContains(pattern1, pattern7));
  EXPECT_TRUE(StrictlyContains(pattern8, pattern1));
  EXPECT_TRUE(NeitherContains(pattern1, pattern9));
  EXPECT_TRUE(StrictlyContains(pattern10, pattern1));
  EXPECT_TRUE(NeitherContains(pattern1, pattern11));
  EXPECT_TRUE(NeitherContains(pattern1, pattern12));
  EXPECT_TRUE(NeitherContains(pattern1, pattern13));

  // pattern2's relationship to the other patterns.
  EXPECT_TRUE(NeitherContains(pattern2, pattern3));
  EXPECT_TRUE(NeitherContains(pattern2, pattern4));
  EXPECT_TRUE(NeitherContains(pattern2, pattern5));
  EXPECT_TRUE(NeitherContains(pattern2, pattern6));
  EXPECT_TRUE(NeitherContains(pattern2, pattern7));
  EXPECT_TRUE(StrictlyContains(pattern8, pattern2));
  EXPECT_TRUE(StrictlyContains(pattern9, pattern2));
  EXPECT_TRUE(StrictlyContains(pattern10, pattern2));
  EXPECT_TRUE(NeitherContains(pattern2, pattern11));
  EXPECT_TRUE(NeitherContains(pattern2, pattern12));
  EXPECT_TRUE(NeitherContains(pattern2, pattern13));

  // Specifically test file:// URLs.
  EXPECT_TRUE(NeitherContains(pattern7, pattern8));
  EXPECT_TRUE(NeitherContains(pattern7, pattern9));
  EXPECT_TRUE(StrictlyContains(pattern10, pattern7));

  // <all_urls> encompasses everything.
  EXPECT_TRUE(StrictlyContains(pattern10, pattern1));
  EXPECT_TRUE(StrictlyContains(pattern10, pattern2));
  EXPECT_TRUE(StrictlyContains(pattern10, pattern3));
  EXPECT_TRUE(StrictlyContains(pattern10, pattern4));
  EXPECT_TRUE(StrictlyContains(pattern10, pattern5));
  EXPECT_TRUE(StrictlyContains(pattern10, pattern6));
  EXPECT_TRUE(StrictlyContains(pattern10, pattern7));
  EXPECT_TRUE(StrictlyContains(pattern10, pattern8));
  EXPECT_TRUE(StrictlyContains(pattern10, pattern9));
  EXPECT_TRUE(StrictlyContains(pattern10, pattern11));
  EXPECT_TRUE(StrictlyContains(pattern10, pattern12));
  EXPECT_TRUE(StrictlyContains(pattern10, pattern13));

  // More...
  EXPECT_TRUE(StrictlyContains(pattern12, pattern11));
  EXPECT_TRUE(NeitherContains(pattern11, pattern13));
  EXPECT_TRUE(StrictlyContains(pattern12, pattern13));
  EXPECT_TRUE(StrictlyContains(pattern15, pattern14));
}

TEST(ExtensionURLPatternTest, MatchesSingleOrigin) {
  EXPECT_FALSE(
      URLPattern(URLPattern::SCHEME_ALL, "http://*/").MatchesSingleOrigin());
  EXPECT_FALSE(URLPattern(URLPattern::SCHEME_ALL, "https://*.google.com/*")
                   .MatchesSingleOrigin());
  EXPECT_TRUE(URLPattern(URLPattern::SCHEME_ALL, "http://google.com/")
                  .MatchesSingleOrigin());
  EXPECT_TRUE(URLPattern(URLPattern::SCHEME_ALL, "http://google.com/*")
                  .MatchesSingleOrigin());
  EXPECT_TRUE(URLPattern(URLPattern::SCHEME_ALL, "http://www.google.com/")
                  .MatchesSingleOrigin());
  EXPECT_FALSE(URLPattern(URLPattern::SCHEME_ALL, "*://www.google.com/")
                   .MatchesSingleOrigin());
  EXPECT_FALSE(URLPattern(URLPattern::SCHEME_ALL, "http://*.com/")
                   .MatchesSingleOrigin());
  EXPECT_FALSE(URLPattern(URLPattern::SCHEME_ALL, "http://*.google.com/foo/bar")
                   .MatchesSingleOrigin());
  EXPECT_TRUE(
      URLPattern(URLPattern::SCHEME_ALL, "http://www.google.com/foo/bar")
          .MatchesSingleOrigin());
  EXPECT_FALSE(URLPattern(URLPattern::SCHEME_HTTPS, "*://*.google.com/foo/bar")
                   .MatchesSingleOrigin());
  EXPECT_TRUE(URLPattern(URLPattern::SCHEME_HTTPS, "https://www.google.com/")
                  .MatchesSingleOrigin());
  EXPECT_FALSE(URLPattern(URLPattern::SCHEME_HTTP,
                          "http://*.google.com/foo/bar").MatchesSingleOrigin());
  EXPECT_TRUE(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com/foo/bar")
          .MatchesSingleOrigin());
}

TEST(ExtensionURLPatternTest, TrailingDotDomain) {
  const GURL normal_domain("http://example.com/");
  const GURL trailing_dot_domain("http://example.com./");

  // Both patterns should match trailing dot and non trailing dot domains. More
  // information about this not obvious behaviour can be found in [1].
  //
  // RFC 1738 [2] specifies clearly that the <host> part of a URL is supposed to
  // contain a fully qualified domain name:
  //
  // 3.1. Common Internet Scheme Syntax
  //      //<user>:<password>@<host>:<port>/<url-path>
  //
  //  host
  //      The fully qualified domain name of a network host
  //
  // [1] http://www.dns-sd.org./TrailingDotsInDomainNames.html
  // [2] http://www.ietf.org/rfc/rfc1738.txt

  const URLPattern pattern(URLPattern::SCHEME_HTTP, "*://example.com/*");
  EXPECT_TRUE(pattern.MatchesURL(normal_domain));
  EXPECT_TRUE(pattern.MatchesURL(trailing_dot_domain));

  const URLPattern trailing_pattern(URLPattern::SCHEME_HTTP,
                                    "*://example.com./*");
  EXPECT_TRUE(trailing_pattern.MatchesURL(normal_domain));
  EXPECT_TRUE(trailing_pattern.MatchesURL(trailing_dot_domain));
}

TEST(ExtensionURLPatternTest, MatchesEffectiveTLD) {
  namespace rcd = net::registry_controlled_domains;

  constexpr struct {
    const char* pattern;
    bool matches_public_tld;
    bool matches_public_or_private_tld;
    bool matches_public_or_unknown_tld;
  } tests[] = {
      // <all_urls> obviously implies all hosts.
      {"*://*/*", true, true, true},
      {"*://*:*/*", true, true, true},
      {"<all_urls>", true, true, true},

      // Matching a single scheme effectively all hosts.
      {"http://*/*", true, true, true},
      {"https://*/*", true, true, true},

      // Specifying a path under any origin is effectively all hosts.
      {"*://*/maps", true, true, true},

      // Matching a given (e)TLD is effectively all hosts.
      {"https://*.com/*", true, true, true},
      {"*://*.co.uk/*", true, true, true},

      // Matching an arbitrary domain with a given path or port is effectively
      // all hosts.
      {"*://*.com/maps", true, true, true},
      {"http://*.com:80/*", true, true, true},

      // Typically, we don't include private registries (like appspot.com) as
      // matching an eTLD - there's legitimate reasons to want to always run on
      // *.appspot.com, and we shouldn't say that it's close enough to every
      // site. However, we should correctly report that it's a TLD wildcard
      // pattern if we include private registries.
      {"*://*.appspot.com/*", false, true, false},

      // Unrecognized TLD-like domains should not be treated as matching an
      // effective TLD unless unknown TLDs are explicitly included.
      {"*://*.notatld/*", false, false, true},

      // All example.com sites is clearly not all hosts, or a TLD wildcard.
      {"*://*.example.com/*", false, false, false},
  };

  for (const auto& test : tests) {
    const URLPattern pattern(URLPattern::SCHEME_ALL, test.pattern);
    EXPECT_EQ(test.matches_public_tld,
              pattern.MatchesEffectiveTld(rcd::EXCLUDE_PRIVATE_REGISTRIES))
        << test.pattern;
    EXPECT_EQ(test.matches_public_or_private_tld,
              pattern.MatchesEffectiveTld(rcd::INCLUDE_PRIVATE_REGISTRIES))
        << test.pattern;
    EXPECT_EQ(test.matches_public_or_unknown_tld,
              pattern.MatchesEffectiveTld(rcd::EXCLUDE_PRIVATE_REGISTRIES,
                                          rcd::INCLUDE_UNKNOWN_REGISTRIES))
        << test.pattern;
    EXPECT_EQ(test.matches_public_or_unknown_tld ||
                  test.matches_public_or_private_tld,
              pattern.MatchesEffectiveTld(rcd::INCLUDE_PRIVATE_REGISTRIES,
                                          rcd::INCLUDE_UNKNOWN_REGISTRIES))
        << test.pattern;
  }
}

// Test that URLPattern properly canonicalizes uncanonicalized hosts.
TEST(ExtensionURLPatternTest, UncanonicalizedUrl) {
  {
    // Simple case: canonicalization should lowercase the host. This is
    // important, since gOoGle.com would never be matched in practice.
    const URLPattern pattern(URLPattern::SCHEME_ALL, "*://*.gOoGle.com/*");
    EXPECT_TRUE(pattern.MatchesURL(GURL("https://google.com")));
    EXPECT_TRUE(pattern.MatchesURL(GURL("https://maps.google.com")));
    EXPECT_FALSE(pattern.MatchesURL(GURL("https://example.com")));
    EXPECT_EQ("*://*.google.com/*", pattern.GetAsString());
  }

  {
    // Trickier case: internationalization with UTF8 characters (the first 'g'
    // isn't actually a 'g').
    const URLPattern pattern(URLPattern::SCHEME_ALL, "https://*.ɡoogle.com/*");
    constexpr char kCanonicalizedHost[] = "xn--oogle-qmc.com";
    EXPECT_EQ(kCanonicalizedHost, pattern.host());
    EXPECT_EQ(base::StringPrintf("https://*.%s/*", kCanonicalizedHost),
              pattern.GetAsString());
    EXPECT_FALSE(pattern.MatchesURL(GURL("https://google.com")));
    // The pattern should match the canonicalized host, and the original
    // UTF8 version.
    EXPECT_TRUE(pattern.MatchesURL(
        GURL(base::StringPrintf("https://%s/", kCanonicalizedHost))));
    EXPECT_TRUE(pattern.MatchesHost("ɡoogle.com"));
  }

  {
    // Sometimes, canonicalization can fail (such as here, where we have invalid
    // unicode characters). In that case, URLPattern parsing should also fail.
    URLPattern pattern(URLPattern::SCHEME_ALL);
    EXPECT_EQ(URLPattern::ParseResult::kInvalidHost,
              pattern.Parse("https://\xef\xb7\x90zyx.com/*"));
  }
}

// Tests URLPattern::CreateIntersection().
TEST(ExtensionURLPatternTest, Intersection) {
  struct {
    std::string pattern1;
    std::string pattern2;
    std::string expected_intersection;
  } test_cases[] = {
      // Identical.
      {"<all_urls>", "<all_urls>", "<all_urls>"},
      {"https://google.com/*", "https://google.com/*", "https://google.com/*"},

      // <all_urls> always returns the other pattern.
      {"<all_urls>", "https://*.google.com/*", "https://*.google.com/*"},
      {"<all_urls>", "*://*/*", "*://*/*"},

      // Scheme intersection.
      {"https://google.com/*", "*://google.com/*", "https://google.com/*"},

      // Host intersection.
      {"https://*.google.com/*", "https://google.com/*",
       "https://google.com/*"},
      {"https://*.maps.google.com/*", "https://*.google.com/*",
       "https://*.maps.google.com/*"},

      // Path intersection.
      {"https://google.com/*", "https://google.com/foo*",
       "https://google.com/foo*"},
      {"https://google.com/foo*", "https://google.com/foo",
       "https://google.com/foo"},

      // Paths can be interesting, and we support intersections on a best-effort
      // basis.
      {"https://google.com/*a*", "https://google.com/*",
       "https://google.com/*a*"},
      {"https://google.com/foo*", "https://google.com/fo*",
       "https://google.com/foo*"},
      {"https://google.com/*a*", "https://google.com/*ab*",
       "https://google.com/*ab*"},
      // Technically, these do intersect - e.g., https://google.com/ab. However,
      // we don't support that level of path intersection.
      {"https://google.com/*a*", "https://google.com/*b*", ""},

      // Port intersection.
      {"https://google.com/*", "https://google.com:80/*",
       "https://google.com:80/*"},
      {"https://google.com:*/*", "https://google.com:*/*",
       "https://google.com/*"},

      // Multi-component intersection (the fun ones).
      {"https://*.google.com/maps", "https://google.com/*",
       "https://google.com/maps"},
      {"*://google.com/*", "https://*/*", "https://google.com/*"},
      {"*://*.com/foo", "https://google.com/*", "https://google.com/foo"},

      // No intersection.
      {"*://*/foo", "*://*/bar", ""},
      {"http://*/*", "https://*/*", ""},
      {"*://*.com/*", "https://chromium.org/*", ""},

      // File URLs.
      {"file:///usr/me", "file:///*", "file:///usr/me"},
      {"file:///usr/*", "file:///*", "file:///usr/*"},
      {"file:///etc/passwd", "file:///usr/*", ""},
  };

  constexpr int kValidSchemes = URLPattern::SCHEME_ALL;
  constexpr char kTestCaseDescriptionTemplate[] =
      "Running Test Case:\n"
      "    Pattern1:        %s\n"
      "    Pattern2:        %s\n"
      "    Expected Result: %s";
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf(
        kTestCaseDescriptionTemplate, test_case.pattern1.c_str(),
        test_case.pattern2.c_str(), test_case.expected_intersection.c_str()));

    URLPattern pattern1(kValidSchemes);
    ASSERT_EQ(URLPattern::ParseResult::kSuccess,
              pattern1.Parse(test_case.pattern1))
        << "Pattern failed to parse: " << test_case.pattern1;
    URLPattern pattern2(kValidSchemes);
    ASSERT_EQ(URLPattern::ParseResult::kSuccess,
              pattern2.Parse(test_case.pattern2))
        << "Pattern failed to parse: " << test_case.pattern2;

    // Intersection of two URLPatterns should be identical regardless of which
    // is the "first".
    std::optional<URLPattern> intersection1 =
        pattern1.CreateIntersection(pattern2);
    std::optional<URLPattern> intersection2 =
        pattern2.CreateIntersection(pattern1);

    if (test_case.expected_intersection.empty()) {
      EXPECT_EQ(std::nullopt, intersection1) << intersection1->GetAsString();
      EXPECT_EQ(std::nullopt, intersection2) << intersection2->GetAsString();
    } else {
      ASSERT_TRUE(intersection1);
      EXPECT_EQ(test_case.expected_intersection, intersection1->GetAsString());
      ASSERT_TRUE(intersection2);
      EXPECT_EQ(test_case.expected_intersection, intersection2->GetAsString());
    }
  }
}

// Tests the special case of URLPattern::CreateIntersection() with different
// valid schemes.
TEST(ExtensionURLPatternTest, ValidSchemeIntersection) {
  // Special case: scheme mask intersection.
  struct {
    int scheme1;
    int scheme2;
    int expected_scheme;
  } scheme_test_cases[] = {
      {URLPattern::SCHEME_ALL, URLPattern::SCHEME_ALL, URLPattern::SCHEME_ALL},
      {URLPattern::SCHEME_ALL, URLPattern::SCHEME_HTTP,
       URLPattern::SCHEME_HTTP},
      {URLPattern::SCHEME_HTTPS | URLPattern::SCHEME_HTTP,
       URLPattern::SCHEME_HTTP, URLPattern::SCHEME_HTTP},
      {URLPattern::SCHEME_HTTP, URLPattern::SCHEME_HTTPS,
       URLPattern::SCHEME_NONE},
  };

  for (const auto test_case : scheme_test_cases) {
    SCOPED_TRACE(base::StringPrintf("Test Case: %d, %d, %d", test_case.scheme1,
                                    test_case.scheme2,
                                    test_case.expected_scheme));
    URLPattern pattern1(test_case.scheme1);
    ASSERT_EQ(URLPattern::ParseResult::kSuccess,
              pattern1.Parse(URLPattern::kAllUrlsPattern));
    URLPattern pattern2(test_case.scheme2);
    ASSERT_EQ(URLPattern::ParseResult::kSuccess,
              pattern2.Parse(URLPattern::kAllUrlsPattern));
    std::optional<URLPattern> intersection1 =
        pattern1.CreateIntersection(pattern2);
    std::optional<URLPattern> intersection2 =
        pattern2.CreateIntersection(pattern1);

    if (test_case.expected_scheme == URLPattern::SCHEME_NONE) {
      EXPECT_EQ(std::nullopt, intersection1) << intersection1->GetAsString();
      EXPECT_EQ(std::nullopt, intersection2) << intersection2->GetAsString();
    } else {
      ASSERT_TRUE(intersection1);
      EXPECT_EQ(test_case.expected_scheme, intersection1->valid_schemes());
      ASSERT_TRUE(intersection2);
      EXPECT_EQ(test_case.expected_scheme, intersection2->valid_schemes());
    }
  }
}

// Tests that <all_urls> patterns correctly check schemes when testing if one
// contains the other.
TEST(ExtensionURLPatternTest, ContainsSchemes) {
  const URLPattern http(URLPattern::SCHEME_HTTP, URLPattern::kAllUrlsPattern);
  const URLPattern chrome(URLPattern::SCHEME_CHROMEUI,
                          URLPattern::kAllUrlsPattern);
  const URLPattern http_and_https(
      URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS,
      URLPattern::kAllUrlsPattern);
  const URLPattern http_https_and_chrome(URLPattern::SCHEME_HTTP |
                                             URLPattern::SCHEME_HTTPS |
                                             URLPattern::SCHEME_CHROMEUI,
                                         URLPattern::kAllUrlsPattern);

  // A map between each URLPattern and the other patterns it should contain.
  const std::map<const URLPattern*, std::set<const URLPattern*>> contains_map =
      {
          {&http, {}},
          {&chrome, {}},
          {&http_and_https, {&http}},
          {&http_https_and_chrome, {&http, &http_and_https, &chrome}},
      };

  const URLPattern* all_patterns[] = {&http, &chrome, &http_and_https,
                                      &http_https_and_chrome};

  // Verify that each pattern contains exactly the expected patterns.
  for (const auto& entry : contains_map) {
    const URLPattern* pattern = entry.first;
    const std::set<const URLPattern*>& contains_patterns = entry.second;
    for (const URLPattern* other_pattern : all_patterns) {
      SCOPED_TRACE(base::StringPrintf("Checking if %d contains %d",
                                      pattern->valid_schemes(),
                                      other_pattern->valid_schemes()));
      bool expect_contains =
          // Patterns should always contain themselves.
          pattern == other_pattern || contains_patterns.count(other_pattern);
      EXPECT_EQ(expect_contains, pattern->Contains(*other_pattern));
    }
  }

  // Fun edge case for bonus points: |http| doesn't contain all the valid
  // schemes of the other pattern, but does in practice (since the scheme is
  // restricted to http by the match pattern).
  EXPECT_TRUE(http.Contains(
      URLPattern(URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS,
                 "http://google.com/*")));
}

// Tests the handling of whitespace, along with various "."s.
TEST(ExtensionURLPatternTest, WhitespaceHostParsing) {
  constexpr char const* kHosts[] = {
      ".", " ", " .", ". ", ". .", ". . .", " . ",
  };

  for (const char* host : kHosts) {
    SCOPED_TRACE(base::StringPrintf("Testing Host: '%s'", host));

    std::string pattern_str = base::StringPrintf("https://%s/*", host);
    URLPattern pattern(URLPattern::SCHEME_HTTPS);
    EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse(pattern_str));

    std::string match_subdomains_pattern_str =
        base::StringPrintf("https://*.%s/*", host);
    URLPattern match_subdomains_pattern(URLPattern::SCHEME_HTTPS);
    EXPECT_EQ(URLPattern::ParseResult::kSuccess,
              match_subdomains_pattern.Parse(match_subdomains_pattern_str));

    GURL url(base::StringPrintf("https://%s/foo", host));
    EXPECT_TRUE(url.is_valid());
    GURL subdomain_url(base::StringPrintf("https://foo.%s/foo", host));
    EXPECT_TRUE(subdomain_url.is_valid());

    // Both the root pattern and the subdomain-matching pattern should match
    // the root URL.
    EXPECT_TRUE(pattern.MatchesURL(url)) << url;
    EXPECT_TRUE(match_subdomains_pattern.MatchesURL(url)) << url;

    // Only the subdomain-matching pattern should match the subdomain URL.
    EXPECT_FALSE(pattern.MatchesURL(subdomain_url)) << subdomain_url;
    EXPECT_TRUE(match_subdomains_pattern.MatchesURL(subdomain_url))
        << subdomain_url;
  }
}

}  // namespace
