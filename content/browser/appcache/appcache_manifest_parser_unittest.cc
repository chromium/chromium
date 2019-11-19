// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <unordered_set>
#include <vector>

#include "base/stl_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/appcache/appcache_manifest_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class AppCacheManifestParserTest : public testing::Test {
};

TEST(AppCacheManifestParserTest, NoData) {
  GURL url;
  AppCacheManifest manifest;
  EXPECT_FALSE(ParseManifest(
      url, "", 0, PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES, manifest));
  EXPECT_FALSE(ParseManifest(url, "CACHE MANIFEST\r", 0,  // Len is 0.
                             PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                             manifest));
}

TEST(AppCacheManifestParserTest, CheckSignature) {
  GURL url;
  AppCacheManifest manifest;

  const std::string kBadSignatures[] = {
    "foo",
    "CACHE MANIFEST;V2\r",          // not followed by whitespace
    "CACHE MANIFEST#bad\r",         // no whitespace before comment
    "cache manifest ",              // wrong case
    "#CACHE MANIFEST\r",            // comment
    "xCACHE MANIFEST\n",            // bad first char
    " CACHE MANIFEST\r",            // begins with whitespace
    "\xEF\xBE\xBF" "CACHE MANIFEST\r",  // bad UTF-8 BOM value
  };

  for (size_t i = 0; i < base::size(kBadSignatures); ++i) {
    const std::string bad = kBadSignatures[i];
    EXPECT_FALSE(ParseManifest(url, bad.c_str(), bad.length(),
                               PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                               manifest));
  }

  const std::string kGoodSignatures[] = {
    "CACHE MANIFEST",
    "CACHE MANIFEST ",
    "CACHE MANIFEST\r",
    "CACHE MANIFEST\n",
    "CACHE MANIFEST\r\n",
    "CACHE MANIFEST\t# ignore me\r",
    "CACHE MANIFEST ignore\r\n",
    "CHROMIUM CACHE MANIFEST\r\n",
    "\xEF\xBB\xBF" "CACHE MANIFEST \r\n",   // BOM present
  };

  for (size_t i = 0; i < base::size(kGoodSignatures); ++i) {
    const std::string good = kGoodSignatures[i];
    EXPECT_TRUE(ParseManifest(url, good.c_str(), good.length(),
                              PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                              manifest));
  }
}

TEST(AppCacheManifestParserTest, HeaderMetrics) {
  const GURL url;

  struct TestCase {
    std::string manifest;
    int expected_false_count;
    int expected_true_count;
  } test_cases[] = {
      {"CACHE MANIFEST\r\n", 1, 0},
      {"CHROMIUM CACHE MANIFEST\r\n", 0, 1},
      {"CACHE MANIFEST#bad\r\n", 0, 0},
  };

  for (const auto& test_case : test_cases) {
    AppCacheManifest manifest;
    base::HistogramTester tester;

    ParseManifest(url, test_case.manifest.c_str(), test_case.manifest.length(),
                  PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES, manifest);
    tester.ExpectBucketCount("appcache.Manifest.ChromeHeader", 0,
                             test_case.expected_false_count);
    tester.ExpectBucketCount("appcache.Manifest.ChromeHeader", 1,
                             test_case.expected_true_count);
  }
}

TEST(AppCacheManifestParserTest, DangerousModeMetrics) {
  const GURL url;

  struct TestCase {
    std::string manifest;
    ParseMode parse_mode;
    int expected_false_count;
    int expected_true_count;
  } test_cases[] = {
      {"CACHE MANIFEST\r\n", PARSE_MANIFEST_PER_STANDARD, 1, 0},
      {"CACHE MANIFEST\r\n", PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES, 0, 1},
      {"CACHE MANIFEST#bad\r\n", PARSE_MANIFEST_PER_STANDARD, 0, 0},
      {"CACHE MANIFEST#bad\r\n", PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES, 0,
       0},
  };

  for (const auto& test_case : test_cases) {
    AppCacheManifest manifest;
    base::HistogramTester tester;

    ParseManifest(url, test_case.manifest.c_str(), test_case.manifest.length(),
                  test_case.parse_mode, manifest);
    tester.ExpectBucketCount("appcache.Manifest.DangerousMode", 0,
                             test_case.expected_false_count);
    tester.ExpectBucketCount("appcache.Manifest.DangerousMode", 1,
                             test_case.expected_true_count);
  }
}

TEST(AppCacheManifestParserTest, NoManifestUrl) {
  AppCacheManifest manifest;
  const std::string kData("CACHE MANIFEST\r"
    "relative/tobase.com\r"
    "http://absolute.com/addme.com");
  const GURL kUrl;
  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                            manifest));
  EXPECT_TRUE(manifest.explicit_urls.empty());
  EXPECT_TRUE(manifest.fallback_namespaces.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());
  EXPECT_FALSE(manifest.online_whitelist_all);
}

TEST(AppCacheManifestParserTest, ExplicitUrls) {
  AppCacheManifest manifest;
  const GURL kUrl("http://www.foo.com");
  const std::string kData("CACHE MANIFEST\r"
    "relative/one\r"
    "# some comment\r"
    "http://www.foo.com/two#strip\r\n"
    "NETWORK:\r"
    "  \t CACHE:\r"
    "HTTP://www.diff.com/three\r"
    "FALLBACK:\r"
    " \t # another comment with leading whitespace\n"
    "IGNORE:\r"
    "http://www.foo.com/ignore\r"
    "CACHE: \r"
    "garbage:#!@\r"
    "https://www.foo.com/diffscheme \t \r"
    "  \t relative/four#stripme\n\r"
    "*\r");

  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                            manifest));
  EXPECT_TRUE(manifest.fallback_namespaces.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());
  EXPECT_FALSE(manifest.online_whitelist_all);
  EXPECT_FALSE(manifest.did_ignore_intercept_namespaces);
  EXPECT_FALSE(manifest.did_ignore_fallback_namespaces);

  std::unordered_set<std::string> urls = manifest.explicit_urls;
  const size_t kExpected = 5;
  ASSERT_EQ(kExpected, urls.size());
  EXPECT_TRUE(urls.find("http://www.foo.com/relative/one") != urls.end());
  EXPECT_TRUE(urls.find("http://www.foo.com/two") != urls.end());
  EXPECT_TRUE(urls.find("http://www.diff.com/three") != urls.end());
  EXPECT_TRUE(urls.find("http://www.foo.com/relative/four") != urls.end());

  // Wildcard is treated as a relative URL in explicit section.
  EXPECT_TRUE(urls.find("http://www.foo.com/*") != urls.end());

  // We should get the same results with dangerous features disallowed.
  manifest = AppCacheManifest();
  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_PER_STANDARD, manifest));
  EXPECT_TRUE(manifest.fallback_namespaces.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());
  EXPECT_FALSE(manifest.online_whitelist_all);
  EXPECT_FALSE(manifest.did_ignore_intercept_namespaces);
  EXPECT_FALSE(manifest.did_ignore_fallback_namespaces);

  urls = manifest.explicit_urls;
  ASSERT_EQ(kExpected, urls.size());
  EXPECT_TRUE(urls.find("http://www.foo.com/relative/one") != urls.end());
  EXPECT_TRUE(urls.find("http://www.foo.com/two") != urls.end());
  EXPECT_TRUE(urls.find("http://www.diff.com/three") != urls.end());
  EXPECT_TRUE(urls.find("http://www.foo.com/relative/four") != urls.end());

  // Wildcard is treated as a relative URL in explicit section.
  EXPECT_TRUE(urls.find("http://www.foo.com/*") != urls.end());
}

TEST(AppCacheManifestParserTest, WhitelistUrls) {
  AppCacheManifest manifest;
  const GURL kUrl("http://www.bar.com");
  const std::string kData("CACHE MANIFEST\r"
    "NETWORK:\r"
    "relative/one\r"
    "# a comment\r"
    "http://www.bar.com/two\r"
    "HTTP://www.diff.com/three#strip\n\r"
    "FALLBACK:\r"
    "garbage\r"
    "UNKNOWN:\r"
    "http://www.bar.com/ignore\r"
    "CACHE:\r"
    "NETWORK:\r"
    "https://www.wrongscheme.com\n"
    "relative/four#stripref \t \r"
    "http://www.five.com\r\n"
    "*foo\r");

  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                            manifest));
  EXPECT_TRUE(manifest.explicit_urls.empty());
  EXPECT_TRUE(manifest.fallback_namespaces.empty());
  EXPECT_TRUE(manifest.intercept_namespaces.empty());
  EXPECT_FALSE(manifest.online_whitelist_all);
  EXPECT_FALSE(manifest.did_ignore_intercept_namespaces);
  EXPECT_FALSE(manifest.did_ignore_fallback_namespaces);

  const std::vector<AppCacheNamespace>& online =
      manifest.online_whitelist_namespaces;
  const size_t kExpected = 6;
  ASSERT_EQ(kExpected, online.size());
  EXPECT_EQ(APPCACHE_NETWORK_NAMESPACE, online[0].type);
  EXPECT_FALSE(online[0].is_pattern);
  EXPECT_TRUE(online[0].target_url.is_empty());
  EXPECT_EQ(GURL("http://www.bar.com/relative/one"), online[0].namespace_url);
  EXPECT_EQ(GURL("http://www.bar.com/two"), online[1].namespace_url);
  EXPECT_EQ(GURL("http://www.diff.com/three"), online[2].namespace_url);
  EXPECT_EQ(GURL("http://www.bar.com/relative/four"), online[3].namespace_url);
  EXPECT_EQ(GURL("http://www.five.com"), online[4].namespace_url);
  EXPECT_EQ(GURL("http://www.bar.com/*foo"), online[5].namespace_url);
}

TEST(AppCacheManifestParserTest, FallbackUrls) {
  AppCacheManifest manifest;
  const GURL kUrl("http://glorp.com");
  const std::string kData("CACHE MANIFEST\r"
    "# a comment\r"
    "CACHE:\r"
    "NETWORK:\r"
    "UNKNOWN:\r"
    "FALLBACK:\r"
    "relative/one \t \t http://glorp.com/onefb  \t \r"
    "*\r"
    "https://glorp.com/wrong http://glorp.com/wrongfb\r"
    "http://glorp.com/two#strip relative/twofb\r"
    "HTTP://glorp.com/three relative/threefb#strip\n"
    "http://glorp.com/three http://glorp.com/three-dup\r"
    "http://glorp.com/solo \t \r\n"
    "http://diff.com/ignore http://glorp.com/wronghost\r"
    "http://glorp.com/wronghost http://diff.com/ohwell\r"
    "relative/badscheme ftp://glorp.com/ignored\r"
    "garbage\r\n"
    "CACHE:\r"
    "# only fallback urls in this test\r"
    "FALLBACK:\n"
    "relative/four#strip relative/fourfb#strip\r"
    "http://www.glorp.com/notsame relative/skipped\r");

  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                            manifest));
  EXPECT_TRUE(manifest.explicit_urls.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());
  EXPECT_FALSE(manifest.online_whitelist_all);
  EXPECT_FALSE(manifest.did_ignore_intercept_namespaces);
  EXPECT_FALSE(manifest.did_ignore_fallback_namespaces);

  const std::vector<AppCacheNamespace>& fallbacks =
      manifest.fallback_namespaces;
  const size_t kExpected = 5;
  ASSERT_EQ(kExpected, fallbacks.size());
  EXPECT_EQ(APPCACHE_FALLBACK_NAMESPACE, fallbacks[0].type);
  EXPECT_EQ(APPCACHE_FALLBACK_NAMESPACE, fallbacks[1].type);
  EXPECT_EQ(APPCACHE_FALLBACK_NAMESPACE, fallbacks[2].type);
  EXPECT_EQ(APPCACHE_FALLBACK_NAMESPACE, fallbacks[3].type);
  EXPECT_EQ(APPCACHE_FALLBACK_NAMESPACE, fallbacks[4].type);
  EXPECT_EQ(GURL("http://glorp.com/relative/one"),
            fallbacks[0].namespace_url);
  EXPECT_EQ(GURL("http://glorp.com/onefb"),
            fallbacks[0].target_url);
  EXPECT_EQ(GURL("http://glorp.com/two"),
            fallbacks[1].namespace_url);
  EXPECT_EQ(GURL("http://glorp.com/relative/twofb"),
            fallbacks[1].target_url);
  EXPECT_EQ(GURL("http://glorp.com/three"),
            fallbacks[2].namespace_url);
  EXPECT_EQ(GURL("http://glorp.com/relative/threefb"),
            fallbacks[2].target_url);
  EXPECT_EQ(GURL("http://glorp.com/three"),       // duplicates are stored
            fallbacks[3].namespace_url);
  EXPECT_EQ(GURL("http://glorp.com/three-dup"),
            fallbacks[3].target_url);
  EXPECT_EQ(GURL("http://glorp.com/relative/four"),
            fallbacks[4].namespace_url);
  EXPECT_EQ(GURL("http://glorp.com/relative/fourfb"),
            fallbacks[4].target_url);
  EXPECT_TRUE(manifest.intercept_namespaces.empty());

  // Nothing should be ignored since all namespaces are in scope.
  manifest = AppCacheManifest();
  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_PER_STANDARD, manifest));
  EXPECT_FALSE(manifest.did_ignore_intercept_namespaces);
  EXPECT_FALSE(manifest.did_ignore_fallback_namespaces);
}

TEST(AppCacheManifestParserTest, FallbackUrlsWithPort) {
  AppCacheManifest manifest;
  const GURL kUrl("http://www.portme.com:1234");
  const std::string kData("CACHE MANIFEST\r"
    "FALLBACK:\r"
    "http://www.portme.com:1234/one relative/onefb\r"
    "HTTP://www.portme.com:9876/wrong http://www.portme.com:1234/ignore\r"
    "http://www.portme.com:1234/stillwrong http://www.portme.com:42/boo\r"
    "relative/two relative/twofb\r"
    "http://www.portme.com:1234/three HTTP://www.portme.com:1234/threefb\r"
    "http://www.portme.com/noport http://www.portme.com:1234/skipped\r"
    "http://www.portme.com:1234/skipme http://www.portme.com/noport\r");

  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                            manifest));
  EXPECT_TRUE(manifest.explicit_urls.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());
  EXPECT_FALSE(manifest.online_whitelist_all);

  const std::vector<AppCacheNamespace>& fallbacks =
      manifest.fallback_namespaces;
  const size_t kExpected = 3;
  ASSERT_EQ(kExpected, fallbacks.size());
  EXPECT_EQ(APPCACHE_FALLBACK_NAMESPACE, fallbacks[0].type);
  EXPECT_EQ(APPCACHE_FALLBACK_NAMESPACE, fallbacks[1].type);
  EXPECT_EQ(APPCACHE_FALLBACK_NAMESPACE, fallbacks[2].type);
  EXPECT_EQ(GURL("http://www.portme.com:1234/one"),
            fallbacks[0].namespace_url);
  EXPECT_EQ(GURL("http://www.portme.com:1234/relative/onefb"),
            fallbacks[0].target_url);
  EXPECT_EQ(GURL("http://www.portme.com:1234/relative/two"),
            fallbacks[1].namespace_url);
  EXPECT_EQ(GURL("http://www.portme.com:1234/relative/twofb"),
            fallbacks[1].target_url);
  EXPECT_EQ(GURL("http://www.portme.com:1234/three"),
            fallbacks[2].namespace_url);
  EXPECT_EQ(GURL("http://www.portme.com:1234/threefb"),
            fallbacks[2].target_url);
  EXPECT_TRUE(manifest.intercept_namespaces.empty());

  // Nothing should be ignored since all namespaces are in scope.
  manifest = AppCacheManifest();
  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_PER_STANDARD, manifest));
  EXPECT_FALSE(manifest.did_ignore_intercept_namespaces);
  EXPECT_FALSE(manifest.did_ignore_fallback_namespaces);
}

TEST(AppCacheManifestParserTest, InterceptUrls) {
  AppCacheManifest manifest;
  const GURL kUrl("http://www.portme.com:1234");
  const std::string kData("CHROMIUM CACHE MANIFEST\r"
    "CHROMIUM-INTERCEPT:\r"
    "http://www.portme.com:1234/one return relative/int1\r"
    "HTTP://www.portme.com:9/wrong return http://www.portme.com:1234/ignore\r"
    "http://www.portme.com:1234/wrong return http://www.portme.com:9/boo\r"
    "relative/two return relative/int2\r"
    "relative/three wrong relative/threefb\r"
    "http://www.portme.com:1234/three return HTTP://www.portme.com:1234/int3\r"
    "http://www.portme.com/noport return http://www.portme.com:1234/skipped\r"
    "http://www.portme.com:1234/skipme return http://www.portme.com/noport\r"
    "relative/wrong/again missing/intercept_type\r");

  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                            manifest));
  EXPECT_TRUE(manifest.fallback_namespaces.empty());
  EXPECT_TRUE(manifest.explicit_urls.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());
  EXPECT_FALSE(manifest.online_whitelist_all);
  EXPECT_FALSE(manifest.did_ignore_intercept_namespaces);
  EXPECT_FALSE(manifest.did_ignore_fallback_namespaces);

  const std::vector<AppCacheNamespace>& intercepts =
      manifest.intercept_namespaces;
  const size_t kExpected = 3;
  ASSERT_EQ(kExpected, intercepts.size());
  EXPECT_EQ(APPCACHE_INTERCEPT_NAMESPACE, intercepts[0].type);
  EXPECT_EQ(APPCACHE_INTERCEPT_NAMESPACE, intercepts[1].type);
  EXPECT_EQ(APPCACHE_INTERCEPT_NAMESPACE, intercepts[2].type);
  EXPECT_EQ(GURL("http://www.portme.com:1234/one"),
            intercepts[0].namespace_url);
  EXPECT_EQ(GURL("http://www.portme.com:1234/relative/int1"),
            intercepts[0].target_url);
  EXPECT_EQ(GURL("http://www.portme.com:1234/relative/two"),
            intercepts[1].namespace_url);
  EXPECT_EQ(GURL("http://www.portme.com:1234/relative/int2"),
            intercepts[1].target_url);
  EXPECT_EQ(GURL("http://www.portme.com:1234/three"),
            intercepts[2].namespace_url);
  EXPECT_EQ(GURL("http://www.portme.com:1234/int3"),
            intercepts[2].target_url);

  // Disallow intercepts this time.
  manifest = AppCacheManifest();
  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_PER_STANDARD, manifest));
  EXPECT_TRUE(manifest.fallback_namespaces.empty());
  EXPECT_TRUE(manifest.explicit_urls.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());
  EXPECT_TRUE(manifest.intercept_namespaces.empty());
  EXPECT_FALSE(manifest.online_whitelist_all);
  EXPECT_TRUE(manifest.did_ignore_intercept_namespaces);
  EXPECT_FALSE(manifest.did_ignore_fallback_namespaces);
}

TEST(AppCacheManifestParserTest, ComboUrls) {
  AppCacheManifest manifest;
  const GURL kUrl("http://combo.com:42");
  const std::string kData("CACHE MANIFEST\r"
    "relative/explicit-1\r"
    "# some comment\r"
    "http://combo.com:99/explicit-2#strip\r"
    "NETWORK:\r"
    "http://combo.com/whitelist-1\r"
    "HTTP://www.diff.com/whitelist-2#strip\r"
    "*\r"
    "CACHE:\n\r"
    "http://www.diff.com/explicit-3\r"
    "FALLBACK:\r"
    "http://combo.com:42/fallback-1 http://combo.com:42/fallback-1b\r"
    "relative/fallback-2 relative/fallback-2b\r"
    "UNKNOWN:\r\n"
    "http://combo.com/ignoreme\r"
    "relative/still-ignored\r"
    "NETWORK:\r\n"
    "relative/whitelist-3#strip\r"
    "http://combo.com:99/whitelist-4\r");
  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                            manifest));
  EXPECT_TRUE(manifest.online_whitelist_all);

  std::unordered_set<std::string> urls = manifest.explicit_urls;
  size_t expected = 3;
  ASSERT_EQ(expected, urls.size());
  EXPECT_TRUE(urls.find("http://combo.com:42/relative/explicit-1") !=
              urls.end());
  EXPECT_TRUE(urls.find("http://combo.com:99/explicit-2") != urls.end());
  EXPECT_TRUE(urls.find("http://www.diff.com/explicit-3") != urls.end());

  const std::vector<AppCacheNamespace>& online =
      manifest.online_whitelist_namespaces;
  expected = 4;
  ASSERT_EQ(expected, online.size());
  EXPECT_EQ(GURL("http://combo.com/whitelist-1"),
                 online[0].namespace_url);
  EXPECT_EQ(GURL("http://www.diff.com/whitelist-2"),
                 online[1].namespace_url);
  EXPECT_EQ(GURL("http://combo.com:42/relative/whitelist-3"),
                 online[2].namespace_url);
  EXPECT_EQ(GURL("http://combo.com:99/whitelist-4"),
                 online[3].namespace_url);

  const std::vector<AppCacheNamespace>& fallbacks =
      manifest.fallback_namespaces;
  expected = 2;
  ASSERT_EQ(expected, fallbacks.size());
  EXPECT_EQ(APPCACHE_FALLBACK_NAMESPACE, fallbacks[0].type);
  EXPECT_EQ(APPCACHE_FALLBACK_NAMESPACE, fallbacks[1].type);
  EXPECT_EQ(GURL("http://combo.com:42/fallback-1"),
            fallbacks[0].namespace_url);
  EXPECT_EQ(GURL("http://combo.com:42/fallback-1b"),
            fallbacks[0].target_url);
  EXPECT_EQ(GURL("http://combo.com:42/relative/fallback-2"),
            fallbacks[1].namespace_url);
  EXPECT_EQ(GURL("http://combo.com:42/relative/fallback-2b"),
            fallbacks[1].target_url);

  EXPECT_TRUE(manifest.intercept_namespaces.empty());
}

TEST(AppCacheManifestParserTest, UnusualUtf8) {
  AppCacheManifest manifest;
  const GURL kUrl("http://bad.com");
  const std::string kData("CACHE MANIFEST\r"
    "\xC0" "invalidutf8\r"
    "nonbmp" "\xF1\x84\xAB\xBC\r");
  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                            manifest));
  std::unordered_set<std::string> urls = manifest.explicit_urls;
  EXPECT_TRUE(urls.find("http://bad.com/%EF%BF%BDinvalidutf8") != urls.end())
      << "manifest byte stream was passed through, not UTF-8-decoded";
  EXPECT_TRUE(urls.find("http://bad.com/nonbmp%F1%84%AB%BC") != urls.end());
}

TEST(AppCacheManifestParserTest, IgnoreAfterSpace) {
  AppCacheManifest manifest;
  const GURL kUrl("http://smorg.borg");
  const std::string kData(
    "CACHE MANIFEST\r"
    "resource.txt this stuff after the white space should be ignored\r");
  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                            manifest));

  std::unordered_set<std::string> urls = manifest.explicit_urls;
  EXPECT_TRUE(urls.find("http://smorg.borg/resource.txt") != urls.end());
}

TEST(AppCacheManifestParserTest, DifferentOriginUrlWithSecureScheme) {
  AppCacheManifest manifest;
  const GURL kUrl("https://www.foo.com");
  const std::string kData("CACHE MANIFEST\r"
    "CACHE: \r"
    "relative/secureschemesameorigin\r"
    "https://www.foo.com/secureschemesameorigin\r"
    "http://www.xyz.com/secureschemedifforigin\r"
    "https://www.xyz.com/secureschemedifforigin\r");

  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                            manifest));
  EXPECT_TRUE(manifest.fallback_namespaces.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());

  std::unordered_set<std::string> urls = manifest.explicit_urls;
  const size_t kExpected = 3;
  ASSERT_EQ(kExpected, urls.size());
  EXPECT_TRUE(urls.find("https://www.foo.com/relative/secureschemesameorigin")
      != urls.end());
  EXPECT_TRUE(urls.find("https://www.foo.com/secureschemesameorigin") !=
      urls.end());
  EXPECT_FALSE(urls.find("http://www.xyz.com/secureschemedifforigin") !=
      urls.end());
  EXPECT_TRUE(urls.find("https://www.xyz.com/secureschemedifforigin") !=
      urls.end());
}

TEST(AppCacheManifestParserTest, PatternMatching) {
  const GURL kUrl("http://foo.com/manifest");
  const std::string kManifestBody(
      "CACHE MANIFEST\r"
      "CACHE: \r"
      "http://foo.com/page.html\r"
      "CHROMIUM-INTERCEPT:\r"
      "http://foo.com/intercept_prefix return /prefix\r"
      "http://foo.com/intercept_pattern return /pattern isPattern\r"
      "http://foo.com/*/intercept_pattern?query return /pattern isPattern\r"
      "FALLBACK:\r"
      "http://foo.com/fallback_prefix  /prefix wrongAnnotation\r"
      "http://foo.com/fallback_pattern* /pattern\tisPattern    \r"
      "NETWORK:\r"
      "*\r"
      "isPattern\r"  // should not be interpretted as a pattern
      "http://foo.com/network_pattern* isPattern\r");


  AppCacheManifest manifest;
  EXPECT_TRUE(ParseManifest(kUrl, kManifestBody.c_str(), kManifestBody.length(),
                            PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                            manifest));
  EXPECT_TRUE(manifest.online_whitelist_all);
  EXPECT_FALSE(manifest.did_ignore_intercept_namespaces);
  EXPECT_FALSE(manifest.did_ignore_fallback_namespaces);
  EXPECT_EQ(1u, manifest.explicit_urls.size());
  EXPECT_EQ(3u, manifest.intercept_namespaces.size());
  EXPECT_EQ(2u, manifest.fallback_namespaces.size());
  EXPECT_EQ(2u, manifest.online_whitelist_namespaces.size());
  EXPECT_EQ(APPCACHE_INTERCEPT_NAMESPACE,
            manifest.intercept_namespaces[0].type);
  EXPECT_EQ(APPCACHE_FALLBACK_NAMESPACE, manifest.fallback_namespaces[0].type);
  EXPECT_EQ(APPCACHE_NETWORK_NAMESPACE,
            manifest.online_whitelist_namespaces[0].type);
  EXPECT_FALSE(manifest.intercept_namespaces[0].is_pattern);
  EXPECT_TRUE(manifest.intercept_namespaces[1].is_pattern);
  EXPECT_TRUE(manifest.intercept_namespaces[2].is_pattern);
  EXPECT_FALSE(manifest.fallback_namespaces[0].is_pattern);
  EXPECT_TRUE(manifest.fallback_namespaces[1].is_pattern);
  EXPECT_FALSE(manifest.online_whitelist_namespaces[0].is_pattern);
  EXPECT_TRUE(manifest.online_whitelist_namespaces[1].is_pattern);
  EXPECT_EQ(
      GURL("http://foo.com/*/intercept_pattern?query"),
      manifest.intercept_namespaces[2].namespace_url);
  EXPECT_EQ(
      GURL("http://foo.com/pattern"),
      manifest.intercept_namespaces[2].target_url);
  EXPECT_EQ(
      GURL("http://foo.com/fallback_pattern*"),
      manifest.fallback_namespaces[1].namespace_url);
  EXPECT_EQ(
      GURL("http://foo.com/pattern"),
      manifest.fallback_namespaces[1].target_url);
  EXPECT_EQ(
      GURL("http://foo.com/isPattern"),
      manifest.online_whitelist_namespaces[0].namespace_url);
  EXPECT_EQ(
      GURL(),
      manifest.online_whitelist_namespaces[0].target_url);
  EXPECT_EQ(
      GURL("http://foo.com/network_pattern*"),
      manifest.online_whitelist_namespaces[1].namespace_url);
  EXPECT_EQ(
      GURL(),
      manifest.online_whitelist_namespaces[1].target_url);
}

TEST(AppCacheManifestParserTest, IgnoreDangerousFallbacks) {
  const GURL kUrl("http://foo.com/scope/manifest?with_query_args");
  const std::string kData(
      "CACHE MANIFEST\r"
      "FALLBACK:\r"
      "http://foo.com/scope/  fallback_url\r"
      "http://foo.com/out_of_scope/ fallback_url\r");

  // Scope matching depends on resolving "." as a relative url.
  EXPECT_EQ(kUrl.GetWithoutFilename().spec(),
            std::string("http://foo.com/scope/"));

  AppCacheManifest manifest;
  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                            manifest));
  EXPECT_FALSE(manifest.did_ignore_fallback_namespaces);
  EXPECT_EQ(2u, manifest.fallback_namespaces.size());

  manifest = AppCacheManifest();
  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_PER_STANDARD, manifest));
  EXPECT_TRUE(manifest.did_ignore_fallback_namespaces);
  EXPECT_EQ(1u, manifest.fallback_namespaces.size());
  EXPECT_EQ(GURL("http://foo.com/scope/"),
            manifest.fallback_namespaces[0].namespace_url);
}

TEST(AppCacheManifestParserTest, NetworkPatternMetrics) {
  const GURL url("http://foo.com/scope/manifest?with_query_args");

  struct TestCase {
    std::string manifest;
    int expected_false_count;
    int expected_true_count;
  } test_cases[] = {
      {"", 1, 0},
      {"FALLBACK:\rhttp://foo.com/fallback /url\r", 1, 0},
      {"FALLBACK:\rhttp://foo.com/fallback_pattern* /pattern isPattern\r", 1,
       0},
      {"NETWORK:\r*\r", 1, 0},
      {"NETWORK:\rhttp://foo.com/network\r", 1, 0},
      {"NETWORK:\rhttp://foo.com/network_pattern* isPattern\r", 0, 1},
      {"CHROMIUM-INTERCEPT:\rhttp://foo.com/intercept return /url\r", 1, 0},
      {"CHROMIUM-INTERCEPT:\r"
       "http://foo.com/intercept* return /pattern isPattern\r",
       1, 0},
  };

  for (const auto& test_case : test_cases) {
    AppCacheManifest manifest;
    base::HistogramTester tester;
    std::string manifest_text =
        std::string("CACHE MANIFEST\r") + test_case.manifest;

    ParseManifest(url, manifest_text.c_str(), manifest_text.length(),
                  PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES, manifest);
    tester.ExpectBucketCount("appcache.Manifest.NetworkPattern", 0,
                             test_case.expected_false_count);
    tester.ExpectBucketCount("appcache.Manifest.NetworkPattern", 1,
                             test_case.expected_true_count);
  }
}

TEST(AppCacheManifestParserTest, FallbackPatternMetrics) {
  const GURL url("http://foo.com/scope/manifest?with_query_args");

  struct TestCase {
    std::string manifest;
    int expected_false_count;
    int expected_true_count;
  } test_cases[] = {
      {"", 1, 0},
      {"FALLBACK:\rhttp://foo.com/fallback /url\r", 1, 0},
      {"FALLBACK:\rhttp://foo.com/fallback_pattern* /pattern isPattern\r", 0,
       1},
      {"NETWORK:\r*\r", 1, 0},
      {"NETWORK:\rhttp://foo.com/network\r", 1, 0},
      {"NETWORK:\rhttp://foo.com/network_pattern* isPattern\r", 1, 0},
      {"CHROMIUM-INTERCEPT:\rhttp://foo.com/intercept return /url\r", 1, 0},
      {"CHROMIUM-INTERCEPT:\r"
       "http://foo.com/intercept* return /pattern isPattern\r",
       1, 0},
  };

  for (const auto& test_case : test_cases) {
    AppCacheManifest manifest;
    base::HistogramTester tester;
    std::string manifest_text =
        std::string("CACHE MANIFEST\r") + test_case.manifest;

    ParseManifest(url, manifest_text.c_str(), manifest_text.length(),
                  PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES, manifest);
    tester.ExpectBucketCount("appcache.Manifest.FallbackPattern", 0,
                             test_case.expected_false_count);
    tester.ExpectBucketCount("appcache.Manifest.FallbackPattern", 1,
                             test_case.expected_true_count);
  }
}

TEST(AppCacheManifestParserTest, InterceptUsageMetrics) {
  const GURL url("http://foo.com/scope/manifest?with_query_args");

  struct TestCase {
    std::string manifest;
    int expected_none_count;
    int expected_exact_count;
    int expected_pattern_count;
  } test_cases[] = {
      {"", 1, 0, 0},
      {"FALLBACK:\rhttp://foo.com/fallback /url\r", 1, 0, 0},
      {"FALLBACK:\rhttp://foo.com/fallback_pattern* /pattern isPattern\r", 1, 0,
       0},
      {"NETWORK:\r*\r", 1, 0, 0},
      {"NETWORK:\rhttp://foo.com/network\r", 1, 0, 0},
      {"NETWORK:\rhttp://foo.com/network_pattern* isPattern\r", 1, 0, 0},
      {"CHROMIUM-INTERCEPT:\rhttp://foo.com/intercept return /url\r", 0, 1, 0},
      {"CHROMIUM-INTERCEPT:\r"
       "http://foo.com/intercept* return /pattern isPattern\r",
       0, 0, 1},
  };

  for (const auto& test_case : test_cases) {
    AppCacheManifest manifest;
    base::HistogramTester tester;
    std::string manifest_text =
        std::string("CACHE MANIFEST\r") + test_case.manifest;

    ParseManifest(url, manifest_text.c_str(), manifest_text.length(),
                  PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES, manifest);
    tester.ExpectBucketCount("appcache.Manifest.InterceptUsage", 0,
                             test_case.expected_none_count);
    tester.ExpectBucketCount("appcache.Manifest.InterceptUsage", 1,
                             test_case.expected_exact_count);
    tester.ExpectBucketCount("appcache.Manifest.InterceptUsage", 2,
                             test_case.expected_pattern_count);
  }
}

TEST(AppCacheManifestParserTest, PatternMetrics) {
  const GURL url("http://foo.com/scope/manifest?with_query_args");

  struct TestCase {
    std::string manifest;
    int expected_false_count;
    int expected_true_count;
  } test_cases[] = {
      {"", 1, 0},
      {"FALLBACK:\rhttp://foo.com/fallback /url\r", 1, 0},
      {"FALLBACK:\rhttp://foo.com/fallback_pattern* /pattern isPattern\r", 0,
       1},
      {"NETWORK:\r*\r", 1, 0},
      {"NETWORK:\rhttp://foo.com/network\r", 1, 0},
      {"NETWORK:\rhttp://foo.com/network_pattern* isPattern\r", 0, 1},
      {"CHROMIUM-INTERCEPT:\rhttp://foo.com/intercept return /url\r", 1, 0},
      {"CHROMIUM-INTERCEPT:\r"
       "http://foo.com/intercept* return /pattern isPattern\r",
       0, 1},
  };

  for (const auto& test_case : test_cases) {
    AppCacheManifest manifest;
    base::HistogramTester tester;
    std::string manifest_text =
        std::string("CACHE MANIFEST\r") + test_case.manifest;

    ParseManifest(url, manifest_text.c_str(), manifest_text.length(),
                  PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES, manifest);
    tester.ExpectBucketCount("appcache.Manifest.Pattern", 0,
                             test_case.expected_false_count);
    tester.ExpectBucketCount("appcache.Manifest.Pattern", 1,
                             test_case.expected_true_count);
  }
}

}  // namespace content
