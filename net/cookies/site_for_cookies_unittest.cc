// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/site_for_cookies.h"

#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace net {
namespace {

class SchemelessSiteForCookiesTest : public ::testing::Test {
 public:
  SchemelessSiteForCookiesTest() {
    scope_feature_list_.InitAndDisableFeature(features::kSchemefulSameSite);
  }

 protected:
  base::test::ScopedFeatureList scope_feature_list_;
};

std::string NormalizedScheme(const GURL& url) {
  return url.SchemeIsWSOrWSS() ? ChangeWebSocketSchemeToHttpScheme(url).scheme()
                               : url.scheme();
}

// Tests that all URLs from |equivalent| produce SiteForCookies that match
// URLs in the set and are equivalent to each other, and are distinct and
// don't match |distinct|.
void TestEquivalentAndDistinct(const std::vector<GURL>& equivalent,
                               const std::vector<GURL>& distinct,
                               const std::string& expect_host) {
  for (const GURL& equiv_url_a : equivalent) {
    SiteForCookies equiv_a = SiteForCookies::FromUrl(equiv_url_a);
    EXPECT_EQ(NormalizedScheme(equiv_url_a), equiv_a.scheme());

    EXPECT_EQ(equiv_a.RepresentativeUrl().spec(),
              base::StrCat({equiv_a.scheme(), "://", expect_host, "/"}));

    for (const GURL& equiv_url_b : equivalent) {
      SiteForCookies equiv_b = SiteForCookies::FromUrl(equiv_url_a);

      EXPECT_TRUE(equiv_a.IsEquivalent(equiv_b));
      EXPECT_TRUE(equiv_b.IsEquivalent(equiv_a));
      EXPECT_TRUE(equiv_a.IsFirstParty(equiv_url_a));
      EXPECT_TRUE(equiv_a.IsFirstParty(equiv_url_b));
      EXPECT_TRUE(equiv_b.IsFirstParty(equiv_url_a));
      EXPECT_TRUE(equiv_b.IsFirstParty(equiv_url_b));
    }

    for (const GURL& other_url : distinct) {
      SiteForCookies other = SiteForCookies::FromUrl(other_url);
      EXPECT_EQ(NormalizedScheme(other_url), other.scheme());
      EXPECT_EQ(other.RepresentativeUrl().spec(),
                base::StrCat({other.scheme(), "://", other_url.host(), "/"}));

      EXPECT_FALSE(equiv_a.IsEquivalent(other));
      EXPECT_FALSE(other.IsEquivalent(equiv_a));
      EXPECT_FALSE(equiv_a.IsFirstParty(other_url))
          << equiv_a.ToDebugString() << " " << other_url.spec();
      EXPECT_FALSE(other.IsFirstParty(equiv_url_a));

      EXPECT_TRUE(other.IsFirstParty(other_url));
    }
  }
}

TEST(SiteForCookiesTest, Default) {
  SiteForCookies should_match_none;
  EXPECT_FALSE(should_match_none.IsFirstParty(GURL("http://example.com")));
  EXPECT_FALSE(should_match_none.IsFirstParty(GURL("file:///home/me/.bashrc")));
  EXPECT_FALSE(should_match_none.IsFirstParty(GURL()));

  // Before SiteForCookies existed, empty URL would represent match-none
  EXPECT_TRUE(should_match_none.IsEquivalent(SiteForCookies::FromUrl(GURL())));
  EXPECT_TRUE(should_match_none.RepresentativeUrl().is_empty());
  EXPECT_TRUE(should_match_none.IsEquivalent(
      SiteForCookies::FromOrigin(url::Origin())));

  EXPECT_TRUE(should_match_none.site().opaque());
  EXPECT_EQ("", should_match_none.scheme());
  EXPECT_EQ("SiteForCookies: {site=null; schemefully_same=false}",
            should_match_none.ToDebugString());
}

TEST_F(SchemelessSiteForCookiesTest, Basic) {
  std::vector<GURL> equivalent = {
      GURL("https://example.com"),
      GURL("http://sub1.example.com:42/something"),
      GURL("ws://sub2.example.com/something"),
      // This one is disputable.
      GURL("file://example.com/helo"),
  };

  std::vector<GURL> distinct = {GURL("https://example.org"),
                                GURL("http://com/i_am_a_tld")};

  TestEquivalentAndDistinct(equivalent, distinct, "example.com");
}

// Similar to SchemelessSiteForCookiesTest_Basic with a focus on testing secure
// SFCs.
TEST(SiteForCookiesTest, BasicSecure) {
  std::vector<GURL> equivalent = {GURL("https://example.com"),
                                  GURL("wss://example.com"),
                                  GURL("https://sub1.example.com:42/something"),
                                  GURL("wss://sub2.example.com/something")};

  std::vector<GURL> distinct = {
      GURL("http://example.com"),      GURL("https://example.org"),
      GURL("ws://example.com"),        GURL("https://com/i_am_a_tld"),
      GURL("file://example.com/helo"),
  };

  TestEquivalentAndDistinct(equivalent, distinct, "example.com");
}

// Similar to SchemelessSiteForCookiesTest_Basic with a focus on testing
// insecure SFCs.
TEST(SiteForCookiesTest, BasicInsecure) {
  std::vector<GURL> equivalent = {GURL("http://example.com"),
                                  GURL("ws://example.com"),
                                  GURL("http://sub1.example.com:42/something"),
                                  GURL("ws://sub2.example.com/something")};

  std::vector<GURL> distinct = {
      GURL("https://example.com"),     GURL("http://example.org"),
      GURL("wss://example.com"),       GURL("http://com/i_am_a_tld"),
      GURL("file://example.com/helo"),
  };

  TestEquivalentAndDistinct(equivalent, distinct, "example.com");
}

TEST(SiteForCookiesTest, File) {
  std::vector<GURL> equivalent = {GURL("file:///a/b/c"),
                                  GURL("file:///etc/shaaadow")};

  std::vector<GURL> distinct = {GURL("file://nonlocal/file.txt")};

  TestEquivalentAndDistinct(equivalent, distinct, "");
}

TEST_F(SchemelessSiteForCookiesTest, Extension) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
  std::vector<GURL> equivalent = {GURL("chrome-extension://abc/"),
                                  GURL("chrome-extension://abc/foo.txt"),
                                  GURL("https://abc"), GURL("http://abc"),
                                  // This one is disputable.
                                  GURL("file://abc/bar.txt")};

  std::vector<GURL> distinct = {GURL("chrome-extension://def")};

  TestEquivalentAndDistinct(equivalent, distinct, "abc");
}

// Similar to SchemelessSiteForCookiesTest_Extension with a focus on ensuring
// that http(s) schemes are distinct.
TEST(SiteForCookiesTest, Extension) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
  std::vector<GURL> equivalent = {
      GURL("chrome-extension://abc/"),
      GURL("chrome-extension://abc/foo.txt"),
  };

  std::vector<GURL> distinct = {GURL("chrome-extension://def"),
                                GURL("https://abc"), GURL("http://abc"),
                                GURL("file://abc/bar.txt")};

  TestEquivalentAndDistinct(equivalent, distinct, "abc");
}

TEST(SiteForCookiesTest, NonStandard) {
  // If we don't register the scheme, nothing matches, even identical ones
  std::vector<GURL> equivalent;
  std::vector<GURL> distinct = {GURL("non-standard://abc"),
                                GURL("non-standard://abc"),
                                GURL("non-standard://def")};

  // Last parameter is "" since GURL doesn't put the hostname in if
  // the URL is non-standard.
  TestEquivalentAndDistinct(equivalent, distinct, "");
}

TEST_F(SchemelessSiteForCookiesTest, Blob) {
  // This case isn't really well-specified and is inconsistent between
  // different user agents; the behavior chosen here was to be more
  // consistent between url and origin handling.
  //
  // Thanks file API spec for the sample blob URL.
  SiteForCookies from_blob = SiteForCookies::FromUrl(
      GURL("blob:https://example.org/9115d58c-bcda-ff47-86e5-083e9a2153041"));

  EXPECT_TRUE(from_blob.IsFirstParty(GURL("http://sub.example.org/resource")));
  EXPECT_EQ("https", from_blob.scheme());
  EXPECT_EQ("SiteForCookies: {site=https://example.org; schemefully_same=true}",
            from_blob.ToDebugString());
  EXPECT_EQ("https://example.org/", from_blob.RepresentativeUrl().spec());
  EXPECT_TRUE(from_blob.IsEquivalent(
      SiteForCookies::FromUrl(GURL("http://www.example.org:631"))));
}

// Similar to SchemelessSiteForCookiesTest_Blob with a focus on a secure blob.
TEST(SiteForCookiesTest, SecureBlob) {
  SiteForCookies from_blob = SiteForCookies::FromUrl(
      GURL("blob:https://example.org/9115d58c-bcda-ff47-86e5-083e9a2153041"));

  EXPECT_TRUE(from_blob.IsFirstParty(GURL("https://sub.example.org/resource")));
  EXPECT_FALSE(from_blob.IsFirstParty(GURL("http://sub.example.org/resource")));
  EXPECT_EQ("https", from_blob.scheme());
  EXPECT_EQ("SiteForCookies: {site=https://example.org; schemefully_same=true}",
            from_blob.ToDebugString());
  EXPECT_EQ("https://example.org/", from_blob.RepresentativeUrl().spec());
  EXPECT_TRUE(from_blob.IsEquivalent(
      SiteForCookies::FromUrl(GURL("https://www.example.org:631"))));
  EXPECT_FALSE(from_blob.IsEquivalent(
      SiteForCookies::FromUrl(GURL("http://www.example.org:631"))));
}

// Similar to SchemelessSiteForCookiesTest_Blob with a focus on an insecure
// blob.
TEST(SiteForCookiesTest, InsecureBlob) {
  SiteForCookies from_blob = SiteForCookies::FromUrl(
      GURL("blob:http://example.org/9115d58c-bcda-ff47-86e5-083e9a2153041"));

  EXPECT_TRUE(from_blob.IsFirstParty(GURL("http://sub.example.org/resource")));
  EXPECT_FALSE(
      from_blob.IsFirstParty(GURL("https://sub.example.org/resource")));
  EXPECT_EQ("http", from_blob.scheme());
  EXPECT_EQ("SiteForCookies: {site=http://example.org; schemefully_same=true}",
            from_blob.ToDebugString());
  EXPECT_EQ("http://example.org/", from_blob.RepresentativeUrl().spec());
  EXPECT_TRUE(from_blob.IsEquivalent(
      SiteForCookies::FromUrl(GURL("http://www.example.org:631"))));
  EXPECT_FALSE(from_blob.IsEquivalent(
      SiteForCookies::FromUrl(GURL("https://www.example.org:631"))));
}

TEST_F(SchemelessSiteForCookiesTest, Wire) {
  SiteForCookies out;

  // Empty one.
  EXPECT_TRUE(SiteForCookies::FromWire(SchemefulSite(), false, &out));
  EXPECT_TRUE(out.IsNull());

  EXPECT_TRUE(SiteForCookies::FromWire(SchemefulSite(), true, &out));
  EXPECT_TRUE(out.IsNull());

  // Not a valid site. (Scheme should have been converted to https.)
  EXPECT_FALSE(SiteForCookies::FromWire(
      SchemefulSite(GURL("wss://host.example.test")), false, &out));
  EXPECT_TRUE(out.IsNull());

  // Not a valid scheme. (Same result as opaque SchemefulSite.)
  EXPECT_TRUE(SiteForCookies::FromWire(SchemefulSite(GURL("aH://example.test")),
                                       false, &out));
  EXPECT_TRUE(out.IsNull());

  // Not a eTLD + 1 (or something hosty), but this is fine. (Is converted to a
  // registrable domain by SchemefulSite constructor.)
  EXPECT_TRUE(SiteForCookies::FromWire(
      SchemefulSite(GURL("http://sub.example.test")), false, &out));
  EXPECT_FALSE(out.IsNull());
  EXPECT_EQ(
      "SiteForCookies: {site=http://example.test; schemefully_same=false}",
      out.ToDebugString());

  // IP address is fine.
  EXPECT_TRUE(SiteForCookies::FromWire(SchemefulSite(GURL("https://127.0.0.1")),
                                       true, &out));
  EXPECT_FALSE(out.IsNull());
  EXPECT_EQ("SiteForCookies: {site=https://127.0.0.1; schemefully_same=true}",
            out.ToDebugString());

  EXPECT_TRUE(SiteForCookies::FromWire(SchemefulSite(GURL("https://127.0.0.1")),
                                       false, &out));
  EXPECT_FALSE(out.IsNull());
  EXPECT_EQ("SiteForCookies: {site=https://127.0.0.1; schemefully_same=false}",
            out.ToDebugString());

  // An actual eTLD+1 is fine.
  EXPECT_TRUE(SiteForCookies::FromWire(
      SchemefulSite(GURL("http://example.test")), true, &out));
  EXPECT_FALSE(out.IsNull());
  EXPECT_EQ("SiteForCookies: {site=http://example.test; schemefully_same=true}",
            out.ToDebugString());
}

// Similar to SchemelessSiteForCookiesTest_Wire except that schemefully_same has
// an effect (makes IsNull() return true if schemefully_same is false).
TEST(SiteForCookiesTest, Wire) {
  SiteForCookies out;

  // Empty one.
  EXPECT_TRUE(SiteForCookies::FromWire(SchemefulSite(), false, &out));
  EXPECT_TRUE(out.IsNull());

  EXPECT_TRUE(SiteForCookies::FromWire(SchemefulSite(), true, &out));
  EXPECT_TRUE(out.IsNull());

  // Not a valid site. (Scheme should have been converted to https.)
  EXPECT_FALSE(SiteForCookies::FromWire(
      SchemefulSite(GURL("wss://host.example.test")), false, &out));
  EXPECT_TRUE(out.IsNull());

  // Not a valid scheme. (Same result as opaque SchemefulSite.)
  EXPECT_TRUE(SiteForCookies::FromWire(SchemefulSite(GURL("aH://example.test")),
                                       false, &out));
  EXPECT_TRUE(out.IsNull());

  // Not a eTLD + 1 (or something hosty), but this is fine. (Is converted to a
  // registrable domain by SchemefulSite constructor.)
  EXPECT_TRUE(SiteForCookies::FromWire(
      SchemefulSite(GURL("http://sub.example.test")), false, &out));
  EXPECT_TRUE(out.IsNull());
  EXPECT_EQ(
      "SiteForCookies: {site=http://example.test; schemefully_same=false}",
      out.ToDebugString());

  // IP address is fine.
  EXPECT_TRUE(SiteForCookies::FromWire(SchemefulSite(GURL("https://127.0.0.1")),
                                       true, &out));
  EXPECT_FALSE(out.IsNull());
  EXPECT_EQ("SiteForCookies: {site=https://127.0.0.1; schemefully_same=true}",
            out.ToDebugString());

  // This one's schemefully_same is false
  EXPECT_TRUE(SiteForCookies::FromWire(SchemefulSite(GURL("https://127.0.0.1")),
                                       false, &out));
  EXPECT_TRUE(out.IsNull());
  EXPECT_EQ("SiteForCookies: {site=https://127.0.0.1; schemefully_same=false}",
            out.ToDebugString());

  // An actual eTLD+1 is fine.
  EXPECT_TRUE(SiteForCookies::FromWire(
      SchemefulSite(GURL("http://example.test")), true, &out));
  EXPECT_FALSE(out.IsNull());
  EXPECT_EQ("SiteForCookies: {site=http://example.test; schemefully_same=true}",
            out.ToDebugString());

  // This one's schemefully_same is false.
  EXPECT_TRUE(SiteForCookies::FromWire(
      SchemefulSite(GURL("http://example.test")), false, &out));
  EXPECT_TRUE(out.IsNull());
  EXPECT_EQ(
      "SiteForCookies: {site=http://example.test; schemefully_same=false}",
      out.ToDebugString());
}

TEST(SiteForCookiesTest, SchemefulSite) {
  const char* kTestCases[] = {"opaque.com",
                              "http://a.com",
                              "https://sub1.example.com:42/something",
                              "https://a.com",
                              "ws://a.com",
                              "wss://a.com",
                              "file://a.com",
                              "file://folder1/folder2/file.txt",
                              "file:///file.txt"};

  for (std::string url : kTestCases) {
    url::Origin origin = url::Origin::Create(GURL(url));
    SiteForCookies from_origin = SiteForCookies::FromOrigin(origin);
    SchemefulSite schemeful_site = SchemefulSite(origin);
    SiteForCookies from_schemeful_site = SiteForCookies(schemeful_site);

    EXPECT_TRUE(from_origin.IsEquivalent(from_schemeful_site));
    EXPECT_TRUE(from_schemeful_site.IsEquivalent(from_origin));
  }
}

TEST(SiteForCookiesTest, CompareWithFrameTreeSiteAndRevise) {
  SchemefulSite secure_example = SchemefulSite(GURL("https://example.com"));
  SchemefulSite insecure_example = SchemefulSite(GURL("http://example.com"));
  SchemefulSite secure_other = SchemefulSite(GURL("https://other.com"));
  SchemefulSite insecure_other = SchemefulSite(GURL("http://other.com"));

  // Other scheme tests.
  url::ScopedSchemeRegistryForTests scoped_registry;
  AddStandardScheme("other", url::SCHEME_WITH_HOST);
  SchemefulSite file_scheme =
      SchemefulSite(GURL("file:///C:/Users/Default/Pictures/photo.png"));
  SchemefulSite file_scheme2 = SchemefulSite(GURL("file:///C:/file.txt"));
  SchemefulSite other_scheme = SchemefulSite(GURL("other://"));

  // This function should work the same regardless the state of Schemeful
  // Same-Site.
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(features::kSchemefulSameSite,
                                            toggle);

    SiteForCookies candidate1 = SiteForCookies(secure_example);
    EXPECT_TRUE(candidate1.CompareWithFrameTreeSiteAndRevise(secure_example));
    EXPECT_FALSE(candidate1.site().opaque());
    EXPECT_TRUE(candidate1.schemefully_same());

    SiteForCookies candidate2 = SiteForCookies(secure_example);
    EXPECT_TRUE(candidate2.CompareWithFrameTreeSiteAndRevise(insecure_example));
    EXPECT_FALSE(candidate2.site().opaque());
    EXPECT_FALSE(candidate2.schemefully_same());

    SiteForCookies candidate3 = SiteForCookies(secure_example);
    EXPECT_FALSE(candidate3.CompareWithFrameTreeSiteAndRevise(secure_other));
    EXPECT_TRUE(candidate3.site().opaque());
    // schemefully_same is N/A if the site() is opaque.

    SiteForCookies candidate4 = SiteForCookies(secure_example);
    EXPECT_FALSE(candidate4.CompareWithFrameTreeSiteAndRevise(insecure_other));
    EXPECT_TRUE(candidate4.site().opaque());
    // schemefully_same is N/A if the site() is opaque.

    // This function's check is bi-directional, so try reversed pairs just in
    // case.
    SiteForCookies candidate2_reversed = SiteForCookies(insecure_example);
    EXPECT_TRUE(
        candidate2_reversed.CompareWithFrameTreeSiteAndRevise(secure_example));
    EXPECT_FALSE(candidate2_reversed.site().opaque());
    EXPECT_FALSE(candidate2_reversed.schemefully_same());

    SiteForCookies candidate3_reversed = SiteForCookies(secure_other);
    EXPECT_FALSE(
        candidate3_reversed.CompareWithFrameTreeSiteAndRevise(secure_example));
    EXPECT_TRUE(candidate3_reversed.site().opaque());
    // schemefully_same is N/A if the site() is opaque.

    SiteForCookies candidate4_reversed = SiteForCookies(insecure_other);
    EXPECT_FALSE(
        candidate4_reversed.CompareWithFrameTreeSiteAndRevise(secure_example));
    EXPECT_TRUE(candidate4_reversed.site().opaque());
    // schemefully_same is N/A if the site() is opaque.

    // Now try some different schemes.
    SiteForCookies candidate5 = SiteForCookies(file_scheme);
    EXPECT_TRUE(candidate5.CompareWithFrameTreeSiteAndRevise(file_scheme2));
    EXPECT_FALSE(candidate5.site().opaque());
    EXPECT_TRUE(candidate5.schemefully_same());

    SiteForCookies candidate6 = SiteForCookies(file_scheme);
    EXPECT_FALSE(candidate6.CompareWithFrameTreeSiteAndRevise(other_scheme));
    EXPECT_TRUE(candidate6.site().opaque());
    // schemefully_same is N/A if the site() is opaque.

    SiteForCookies candidate5_reversed = SiteForCookies(file_scheme2);
    EXPECT_TRUE(
        candidate5_reversed.CompareWithFrameTreeSiteAndRevise(file_scheme));
    EXPECT_FALSE(candidate5_reversed.site().opaque());
    EXPECT_TRUE(candidate5_reversed.schemefully_same());

    SiteForCookies candidate6_reversed = SiteForCookies(other_scheme);
    EXPECT_FALSE(
        candidate6_reversed.CompareWithFrameTreeSiteAndRevise(file_scheme));
    EXPECT_TRUE(candidate6_reversed.site().opaque());
    // schemefully_same is N/A if the site() is opaque.
  }
}

TEST(SiteForCookiesTest, CompareWithFrameTreeSiteAndReviseOpaque) {
  url::Origin opaque1 = url::Origin();
  url::Origin opaque2 = url::Origin();

  SchemefulSite opaque_site1 = SchemefulSite(opaque1);
  SchemefulSite opaque_site2 = SchemefulSite(opaque2);
  SchemefulSite example = SchemefulSite(GURL("https://example.com"));

  // Opaque origins are able to match on the frame comparison.
  SiteForCookies candidate1 = SiteForCookies(opaque_site1);
  EXPECT_TRUE(candidate1.CompareWithFrameTreeSiteAndRevise(opaque_site1));
  EXPECT_TRUE(candidate1.site().opaque());
  // schemefully_same is N/A if the site() is opaque.
  EXPECT_EQ(candidate1.site(), opaque_site1);

  SiteForCookies candidate2 = SiteForCookies(opaque_site1);
  EXPECT_TRUE(candidate2.CompareWithFrameTreeSiteAndRevise(opaque_site2));
  EXPECT_TRUE(candidate2.site().opaque());
  // schemefully_same is N/A if the site() is opaque.
  EXPECT_EQ(candidate2.site(), opaque_site1);

  // But if only one is opaque they won't match.
  SiteForCookies candidate3 = SiteForCookies(example);
  EXPECT_FALSE(candidate3.CompareWithFrameTreeSiteAndRevise(opaque_site1));
  EXPECT_TRUE(candidate3.site().opaque());
  // schemefully_same is N/A if the site() is opaque.
  EXPECT_NE(candidate3.site(), opaque_site1);

  SiteForCookies candidate4 = SiteForCookies(opaque_site1);
  EXPECT_FALSE(candidate4.CompareWithFrameTreeSiteAndRevise(example));
  EXPECT_TRUE(candidate4.site().opaque());
  // schemefully_same is N/A if the site() is opaque.
  EXPECT_EQ(candidate4.site(), opaque_site1);
}

TEST(SiteForCookiesTest, NotSchemefullySameEquivalent) {
  SiteForCookies first =
      SiteForCookies::FromUrl(GURL("https://www.example.com"));
  SiteForCookies second =
      SiteForCookies::FromUrl(GURL("https://www.example.com"));
  // Smoke check that two SFCs should match when they're the same.
  EXPECT_TRUE(first.IsEquivalent(second));
  EXPECT_TRUE(second.IsEquivalent(first));

  // Two SFC should not be equivalent to each other when one of their
  // schemefully_same_ flags is false, even if they're otherwise the same, when
  // Schemeful Same-Site is enabled.
  second.SetSchemefullySameForTesting(false);
  EXPECT_FALSE(first.IsEquivalent(second));
  EXPECT_FALSE(second.IsEquivalent(first));

  // However, they should match if both their schemefully_same_ flags are false.
  // Because they're both considered null at that point.
  first.SetSchemefullySameForTesting(false);
  EXPECT_TRUE(first.IsEquivalent(second));
  EXPECT_TRUE(second.IsEquivalent(first));
}

}  // namespace

TEST(SiteForCookiesTest, SameScheme) {
  struct TestCase {
    const char* first;
    const char* second;
    bool expected_value;
  };

  const TestCase kTestCases[] = {
      {"http://a.com", "http://a.com", true},
      {"https://a.com", "https://a.com", true},
      {"ws://a.com", "ws://a.com", true},
      {"wss://a.com", "wss://a.com", true},
      {"https://a.com", "wss://a.com", true},
      {"wss://a.com", "https://a.com", true},
      {"http://a.com", "ws://a.com", true},
      {"ws://a.com", "http://a.com", true},
      {"file://a.com", "file://a.com", true},
      {"file://folder1/folder2/file.txt", "file://folder1/folder2/file.txt",
       true},
      {"ftp://a.com", "ftp://a.com", true},
      {"http://a.com", "file://a.com", false},
      {"ws://a.com", "wss://a.com", false},
      {"wss://a.com", "ws://a.com", false},
      {"https://a.com", "http://a.com", false},
      {"file://a.com", "https://a.com", false},
      {"https://a.com", "file://a.com", false},
      {"file://a.com", "ftp://a.com", false},
      {"ftp://a.com", "file://a.com", false},
  };

  for (const TestCase& t : kTestCases) {
    SiteForCookies first = SiteForCookies::FromUrl(GURL(t.first));
    SchemefulSite second(GURL(t.second));
    EXPECT_FALSE(first.IsNull());
    first.MarkIfCrossScheme(second);
    EXPECT_EQ(first.schemefully_same(), t.expected_value);
  }
}

TEST(SiteForCookiesTest, SameSchemeOpaque) {
  url::Origin not_opaque_secure =
      url::Origin::Create(GURL("https://site.example"));
  url::Origin not_opaque_nonsecure =
      url::Origin::Create(GURL("http://site.example"));
  // Check an opaque origin made from a triple origin and one from the default
  // constructor.
  const url::Origin kOpaqueOrigins[] = {
      not_opaque_secure.DeriveNewOpaqueOrigin(),
      not_opaque_nonsecure.DeriveNewOpaqueOrigin(), url::Origin()};

  for (const url::Origin& origin : kOpaqueOrigins) {
    SiteForCookies secure_sfc = SiteForCookies::FromOrigin(not_opaque_secure);
    EXPECT_FALSE(secure_sfc.IsNull());
    SiteForCookies nonsecure_sfc =
        SiteForCookies::FromOrigin(not_opaque_nonsecure);
    EXPECT_FALSE(nonsecure_sfc.IsNull());

    SchemefulSite site(origin);

    EXPECT_TRUE(secure_sfc.schemefully_same());
    secure_sfc.MarkIfCrossScheme(site);
    EXPECT_FALSE(secure_sfc.schemefully_same());

    EXPECT_TRUE(nonsecure_sfc.schemefully_same());
    nonsecure_sfc.MarkIfCrossScheme(site);
    EXPECT_FALSE(nonsecure_sfc.schemefully_same());

    SiteForCookies opaque_sfc = SiteForCookies(site);
    EXPECT_TRUE(opaque_sfc.IsNull());
    // Slightly implementation detail specific as the value isn't relevant for
    // null SFCs.
    EXPECT_FALSE(nonsecure_sfc.schemefully_same());
  }
}

// Quick correctness check that the less-than operator works as expected.
TEST(SiteForCookiesTest, LessThan) {
  SiteForCookies first = SiteForCookies::FromUrl(GURL("https://example.com"));
  SiteForCookies second =
      SiteForCookies::FromUrl(GURL("https://examplelonger.com"));
  SiteForCookies third =
      SiteForCookies::FromUrl(GURL("https://examplelongerstill.com"));

  SiteForCookies null1 = SiteForCookies();
  SiteForCookies null2 =
      SiteForCookies::FromUrl(GURL("https://examplelongerstillstill.com"));
  null2.SetSchemefullySameForTesting(false);

  EXPECT_LT(first, second);
  EXPECT_LT(second, third);
  EXPECT_LT(first, third);
  EXPECT_LT(null1, first);
  EXPECT_LT(null2, first);

  EXPECT_FALSE(second < first);
  EXPECT_FALSE(first < null1);
  EXPECT_FALSE(first < null2);
  EXPECT_FALSE(null1 < null2);
  EXPECT_FALSE(null2 < null1);
}

}  // namespace net
