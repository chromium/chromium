// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

#include <cstdint>

#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/buildflags.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

namespace test1 {
#include "net/base/registry_controlled_domains/effective_tld_names_unittest1-reversed-inc.cc"
}
namespace test2 {
#include "net/base/registry_controlled_domains/effective_tld_names_unittest2-reversed-inc.cc"
}
namespace test3 {
#include "net/base/registry_controlled_domains/effective_tld_names_unittest3-reversed-inc.cc"
}
namespace test4 {
#include "net/base/registry_controlled_domains/effective_tld_names_unittest4-reversed-inc.cc"
}
namespace test5 {
#include "net/base/registry_controlled_domains/effective_tld_names_unittest5-reversed-inc.cc"
}
namespace test6 {
#include "net/base/registry_controlled_domains/effective_tld_names_unittest6-reversed-inc.cc"
}

}  // namespace

namespace net::registry_controlled_domains {

namespace {

std::string GetDomainFromHost(const std::string& host) {
  return GetDomainAndRegistry(host, EXCLUDE_PRIVATE_REGISTRIES);
}

size_t GetRegistryLengthFromURL(
    const std::string& url,
    UnknownRegistryFilter unknown_filter) {
  return GetRegistryLength(GURL(url),
                           unknown_filter,
                           EXCLUDE_PRIVATE_REGISTRIES);
}

size_t GetRegistryLengthFromURLIncludingPrivate(
    const std::string& url,
    UnknownRegistryFilter unknown_filter) {
  return GetRegistryLength(GURL(url),
                           unknown_filter,
                           INCLUDE_PRIVATE_REGISTRIES);
}

size_t PermissiveGetHostRegistryLength(std::string_view host) {
  return PermissiveGetHostRegistryLength(host, EXCLUDE_UNKNOWN_REGISTRIES,
                                         EXCLUDE_PRIVATE_REGISTRIES);
}

// Only called when using ICU (avoids unused static function error).
#if !BUILDFLAG(USE_PLATFORM_ICU_ALTERNATIVES)
size_t PermissiveGetHostRegistryLength(std::u16string_view host) {
  return PermissiveGetHostRegistryLength(host, EXCLUDE_UNKNOWN_REGISTRIES,
                                         EXCLUDE_PRIVATE_REGISTRIES);
}
#endif

size_t GetCanonicalHostRegistryLength(const std::string& host,
                                      UnknownRegistryFilter unknown_filter) {
  return GetCanonicalHostRegistryLength(host, unknown_filter,
                                        EXCLUDE_PRIVATE_REGISTRIES);
}

size_t GetCanonicalHostRegistryLengthIncludingPrivate(const std::string& host) {
  return GetCanonicalHostRegistryLength(host, EXCLUDE_UNKNOWN_REGISTRIES,
                                        INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

class RegistryControlledDomainTest : public testing::Test {
 protected:
  void UseDomainData(base::span<const uint8_t> graph) {
    // This is undone in TearDown.
    SetFindDomainGraphForTesting(graph);
  }

  bool CompareDomains(const std::string& url1, const std::string& url2) {
    SCOPED_TRACE(url1 + " " + url2);
    GURL g1 = GURL(url1);
    GURL g2 = GURL(url2);
    url::Origin o1 = url::Origin::Create(g1);
    url::Origin o2 = url::Origin::Create(g2);
    EXPECT_EQ(SameDomainOrHost(o1, o2, EXCLUDE_PRIVATE_REGISTRIES),
              SameDomainOrHost(g1, g2, EXCLUDE_PRIVATE_REGISTRIES));
    return SameDomainOrHost(g1, g2, EXCLUDE_PRIVATE_REGISTRIES);
  }

  void TearDown() override { ResetFindDomainGraphForTesting(); }
};

TEST_F(RegistryControlledDomainTest, TestHostIsRegistryIdentifier) {
  UseDomainData(test1::kDafsa);
  // A hostname with a label above the eTLD
  EXPECT_FALSE(HostIsRegistryIdentifier("blah.jp", EXCLUDE_PRIVATE_REGISTRIES));
  EXPECT_FALSE(
      HostIsRegistryIdentifier(".blah.jp", INCLUDE_PRIVATE_REGISTRIES));
  EXPECT_FALSE(
      HostIsRegistryIdentifier(".blah.jp.", INCLUDE_PRIVATE_REGISTRIES));
  // A private TLD
  EXPECT_FALSE(HostIsRegistryIdentifier("priv.no", EXCLUDE_PRIVATE_REGISTRIES));
  EXPECT_TRUE(HostIsRegistryIdentifier("priv.no", INCLUDE_PRIVATE_REGISTRIES));
  EXPECT_TRUE(
      HostIsRegistryIdentifier(".priv.no.", INCLUDE_PRIVATE_REGISTRIES));
  // A hostname that is a TLD
  EXPECT_TRUE(HostIsRegistryIdentifier("jp", EXCLUDE_PRIVATE_REGISTRIES));
  EXPECT_TRUE(HostIsRegistryIdentifier("jp", INCLUDE_PRIVATE_REGISTRIES));
  EXPECT_TRUE(HostIsRegistryIdentifier(".jp", EXCLUDE_PRIVATE_REGISTRIES));
  EXPECT_TRUE(HostIsRegistryIdentifier(".jp", INCLUDE_PRIVATE_REGISTRIES));
  EXPECT_TRUE(HostIsRegistryIdentifier(".jp.", EXCLUDE_PRIVATE_REGISTRIES));
  EXPECT_TRUE(HostIsRegistryIdentifier(".jp.", INCLUDE_PRIVATE_REGISTRIES));
  // A hostname that is a TLD specified by a wildcard rule
  EXPECT_TRUE(
      HostIsRegistryIdentifier("blah.bar.jp", INCLUDE_PRIVATE_REGISTRIES));
  EXPECT_FALSE(
      HostIsRegistryIdentifier("blah.blah.bar.jp", EXCLUDE_PRIVATE_REGISTRIES));
}

TEST_F(RegistryControlledDomainTest, TestGetDomainAndRegistry) {
  UseDomainData(test1::kDafsa);

  struct {
    std::string url;
    std::string expected_domain_and_registry;
  } kTestCases[] = {
      {"http://a.baz.jp/file.html", "baz.jp"},
      {"http://a.baz.jp./file.html", "baz.jp."},
      {"http://ac.jp", ""},
      {"http://a.bar.jp", ""},
      {"http://bar.jp", ""},
      {"http://baz.bar.jp", ""},
      {"http://a.b.baz.bar.jp", "a.b.baz.bar.jp"},

      {"http://baz.pref.bar.jp", "pref.bar.jp"},
      {"http://a.b.bar.baz.com.", "b.bar.baz.com."},

      {"http://a.d.c", "a.d.c"},
      {"http://.a.d.c", "a.d.c"},
      {"http://..a.d.c", "a.d.c"},
      {"http://a.b.c", "b.c"},
      {"http://baz.com", "baz.com"},
      {"http://baz.com.", "baz.com."},

      {"", ""},
      {"http://", ""},
      {"file:///C:/file.html", ""},
      {"http://foo.com..", ""},
      {"http://...", ""},
      {"http://192.168.0.1", ""},
      {"http://[2001:0db8:85a3:0000:0000:8a2e:0370:7334]/", ""},
      {"http://localhost", ""},
      {"http://localhost.", ""},
      {"http:////Comment", ""},
  };
  for (const auto& test_case : kTestCases) {
    const GURL url(test_case.url);
    EXPECT_EQ(test_case.expected_domain_and_registry,
              GetDomainAndRegistry(url, EXCLUDE_PRIVATE_REGISTRIES));
    EXPECT_EQ(test_case.expected_domain_and_registry,
              GetDomainAndRegistry(url::Origin::Create(url),
                                   EXCLUDE_PRIVATE_REGISTRIES));
  }

  // Test std::string version of GetDomainAndRegistry().  Uses the same
  // underpinnings as the GURL version, so this is really more of a check of
  // CanonicalizeHost().
  EXPECT_EQ("baz.jp", GetDomainFromHost("a.baz.jp"));                  // 1
  EXPECT_EQ("baz.jp.", GetDomainFromHost("a.baz.jp."));                // 1
  EXPECT_EQ("", GetDomainFromHost("ac.jp"));                           // 2
  EXPECT_EQ("", GetDomainFromHost("a.bar.jp"));                        // 3
  EXPECT_EQ("", GetDomainFromHost("bar.jp"));                          // 3
  EXPECT_EQ("", GetDomainFromHost("baz.bar.jp"));                      // 3 4
  EXPECT_EQ("a.b.baz.bar.jp", GetDomainFromHost("a.b.baz.bar.jp"));    // 3 4
  EXPECT_EQ("pref.bar.jp", GetDomainFromHost("baz.pref.bar.jp"));      // 5
  EXPECT_EQ("b.bar.baz.com.", GetDomainFromHost("a.b.bar.baz.com."));  // 6
  EXPECT_EQ("a.d.c", GetDomainFromHost("a.d.c"));                      // 7
  EXPECT_EQ("a.d.c", GetDomainFromHost(".a.d.c"));                     // 7
  EXPECT_EQ("a.d.c", GetDomainFromHost("..a.d.c"));                    // 7
  EXPECT_EQ("b.c", GetDomainFromHost("a.b.c"));                        // 7 8
  EXPECT_EQ("baz.com", GetDomainFromHost("baz.com"));                  // none
  EXPECT_EQ("baz.com.", GetDomainFromHost("baz.com."));                // none

  EXPECT_EQ("", GetDomainFromHost(std::string()));
  EXPECT_EQ("", GetDomainFromHost("foo.com.."));
  EXPECT_EQ("", GetDomainFromHost("..."));
  EXPECT_EQ("", GetDomainFromHost("192.168.0.1"));
  EXPECT_EQ("", GetDomainFromHost("[2001:0db8:85a3:0000:0000:8a2e:0370:7334]"));
  EXPECT_EQ("", GetDomainFromHost("localhost."));
  EXPECT_EQ("", GetDomainFromHost(".localhost."));
}

TEST_F(RegistryControlledDomainTest, TestGetRegistryLength) {
  UseDomainData(test1::kDafsa);

  // Test GURL version of GetRegistryLength().
  EXPECT_EQ(2U, GetRegistryLengthFromURL("http://a.baz.jp/file.html",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 1
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://a.baz.jp./file.html",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 1
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://ac.jp",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 2
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://a.bar.jp",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 3
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://bar.jp",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 3
  EXPECT_EQ(2U, GetRegistryLengthFromURL("http://xbar.jp",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 1
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://baz.bar.jp",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 3 4
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://.baz.bar.jp",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 3 4
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://..baz.bar.jp",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 3 4
  EXPECT_EQ(11U, GetRegistryLengthFromURL("http://foo..baz.bar.jp",
                                          EXCLUDE_UNKNOWN_REGISTRIES));  // 3 4
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://xbaz.bar.jp",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 3
  EXPECT_EQ(11U, GetRegistryLengthFromURL("http://x.xbaz.bar.jp",
                                          EXCLUDE_UNKNOWN_REGISTRIES));  // 3
  EXPECT_EQ(12U, GetRegistryLengthFromURL("http://a.b.baz.bar.jp",
                                          EXCLUDE_UNKNOWN_REGISTRIES));  // 4
  EXPECT_EQ(6U, GetRegistryLengthFromURL("http://baz.pref.bar.jp",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 5
  EXPECT_EQ(6U, GetRegistryLengthFromURL("http://z.baz.pref.bar.jp",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 5
  EXPECT_EQ(10U, GetRegistryLengthFromURL("http://p.ref.bar.jp",
                                          EXCLUDE_UNKNOWN_REGISTRIES));  // 5
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://xpref.bar.jp",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 5
  EXPECT_EQ(12U, GetRegistryLengthFromURL("http://baz.xpref.bar.jp",
                                          EXCLUDE_UNKNOWN_REGISTRIES));  // 5
  EXPECT_EQ(6U, GetRegistryLengthFromURL("http://baz..pref.bar.jp",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 5
  EXPECT_EQ(11U, GetRegistryLengthFromURL("http://a.b.bar.baz.com",
                                          EXCLUDE_UNKNOWN_REGISTRIES));  // 6
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://a.d.c",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 7
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://.a.d.c",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 7
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://..a.d.c",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 7
  EXPECT_EQ(1U, GetRegistryLengthFromURL("http://a.b.c",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // 7 8
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://baz.com",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // none
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://baz.com.",
                                         EXCLUDE_UNKNOWN_REGISTRIES));  // none
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://baz.com",
                                         INCLUDE_UNKNOWN_REGISTRIES));  // none
  EXPECT_EQ(4U, GetRegistryLengthFromURL("http://baz.com.",
                                         INCLUDE_UNKNOWN_REGISTRIES));  // none

  EXPECT_EQ(std::string::npos,
      GetRegistryLengthFromURL(std::string(), EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(std::string::npos,
      GetRegistryLengthFromURL("http://", EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(std::string::npos,
      GetRegistryLengthFromURL("file:///C:/file.html",
                               EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://foo.com..",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://...",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://192.168.0.1",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://localhost",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://localhost",
                                         INCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://localhost.",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://localhost.",
                                         INCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http:////Comment",
                                         EXCLUDE_UNKNOWN_REGISTRIES));

  // Test std::string version of GetRegistryLength().  Uses the same
  // underpinnings as the GURL version, so this is really more of a check of
  // CanonicalizeHost().
  EXPECT_EQ(2U, GetCanonicalHostRegistryLength(
                    "a.baz.jp", EXCLUDE_UNKNOWN_REGISTRIES));  // 1
  EXPECT_EQ(3U, GetCanonicalHostRegistryLength(
                    "a.baz.jp.", EXCLUDE_UNKNOWN_REGISTRIES));  // 1
  EXPECT_EQ(0U, GetCanonicalHostRegistryLength(
                    "ac.jp", EXCLUDE_UNKNOWN_REGISTRIES));  // 2
  EXPECT_EQ(0U, GetCanonicalHostRegistryLength(
                    "a.bar.jp", EXCLUDE_UNKNOWN_REGISTRIES));  // 3
  EXPECT_EQ(0U, GetCanonicalHostRegistryLength(
                    "bar.jp", EXCLUDE_UNKNOWN_REGISTRIES));  // 3
  EXPECT_EQ(0U, GetCanonicalHostRegistryLength(
                    "baz.bar.jp", EXCLUDE_UNKNOWN_REGISTRIES));  // 3 4
  EXPECT_EQ(12U, GetCanonicalHostRegistryLength(
                     "a.b.baz.bar.jp", EXCLUDE_UNKNOWN_REGISTRIES));  // 4
  EXPECT_EQ(6U, GetCanonicalHostRegistryLength(
                    "baz.pref.bar.jp", EXCLUDE_UNKNOWN_REGISTRIES));  // 5
  EXPECT_EQ(11U, GetCanonicalHostRegistryLength(
                     "a.b.bar.baz.com", EXCLUDE_UNKNOWN_REGISTRIES));  // 6
  EXPECT_EQ(3U, GetCanonicalHostRegistryLength(
                    "a.d.c", EXCLUDE_UNKNOWN_REGISTRIES));  // 7
  EXPECT_EQ(3U, GetCanonicalHostRegistryLength(
                    ".a.d.c", EXCLUDE_UNKNOWN_REGISTRIES));  // 7
  EXPECT_EQ(3U, GetCanonicalHostRegistryLength(
                    "..a.d.c", EXCLUDE_UNKNOWN_REGISTRIES));  // 7
  EXPECT_EQ(1U, GetCanonicalHostRegistryLength(
                    "a.b.c", EXCLUDE_UNKNOWN_REGISTRIES));  // 7 8
  EXPECT_EQ(0U, GetCanonicalHostRegistryLength(
                    "baz.com", EXCLUDE_UNKNOWN_REGISTRIES));  // none
  EXPECT_EQ(0U, GetCanonicalHostRegistryLength(
                    "baz.com.", EXCLUDE_UNKNOWN_REGISTRIES));  // none
  EXPECT_EQ(3U, GetCanonicalHostRegistryLength(
                    "baz.com", INCLUDE_UNKNOWN_REGISTRIES));  // none
  EXPECT_EQ(4U, GetCanonicalHostRegistryLength(
                    "baz.com.", INCLUDE_UNKNOWN_REGISTRIES));  // none

  EXPECT_EQ(std::string::npos, GetCanonicalHostRegistryLength(
                                   std::string(), EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetCanonicalHostRegistryLength("foo.com..",
                                               EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U,
            GetCanonicalHostRegistryLength("..", EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetCanonicalHostRegistryLength("192.168.0.1",
                                               EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetCanonicalHostRegistryLength("localhost",
                                               EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetCanonicalHostRegistryLength("localhost",
                                               INCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetCanonicalHostRegistryLength("localhost.",
                                               EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetCanonicalHostRegistryLength("localhost.",
                                               INCLUDE_UNKNOWN_REGISTRIES));

  // IDN case.
  EXPECT_EQ(10U, GetCanonicalHostRegistryLength("foo.xn--fiqs8s",
                                                EXCLUDE_UNKNOWN_REGISTRIES));
}

TEST_F(RegistryControlledDomainTest, HostHasRegistryControlledDomain) {
  UseDomainData(test1::kDafsa);

  // Invalid hosts.
  EXPECT_FALSE(HostHasRegistryControlledDomain(
      std::string(), EXCLUDE_UNKNOWN_REGISTRIES, EXCLUDE_PRIVATE_REGISTRIES));
  EXPECT_FALSE(HostHasRegistryControlledDomain(
      "%00asdf", EXCLUDE_UNKNOWN_REGISTRIES, EXCLUDE_PRIVATE_REGISTRIES));

  // Invalid host but valid R.C.D.
  EXPECT_TRUE(HostHasRegistryControlledDomain(
      "%00foo.jp", EXCLUDE_UNKNOWN_REGISTRIES, EXCLUDE_PRIVATE_REGISTRIES));

  // Valid R.C.D. when canonicalized, even with an invalid prefix and an
  // escaped dot.
  EXPECT_TRUE(HostHasRegistryControlledDomain("%00foo.Google%2EjP",
                                              EXCLUDE_UNKNOWN_REGISTRIES,
                                              EXCLUDE_PRIVATE_REGISTRIES));

  // Regular, no match.
  EXPECT_FALSE(HostHasRegistryControlledDomain(
      "bar.notatld", EXCLUDE_UNKNOWN_REGISTRIES, EXCLUDE_PRIVATE_REGISTRIES));

  // Regular, match.
  EXPECT_TRUE(HostHasRegistryControlledDomain(
      "www.Google.Jp", EXCLUDE_UNKNOWN_REGISTRIES, EXCLUDE_PRIVATE_REGISTRIES));
}

TEST_F(RegistryControlledDomainTest, TestSameDomainOrHost) {
  UseDomainData(test2::kDafsa);

  EXPECT_TRUE(CompareDomains("http://a.b.bar.jp/file.html",
                             "http://a.b.bar.jp/file.html"));  // b.bar.jp
  EXPECT_TRUE(CompareDomains("http://a.b.bar.jp/file.html",
                             "http://b.b.bar.jp/file.html"));  // b.bar.jp
  EXPECT_FALSE(CompareDomains("http://a.foo.jp/file.html",     // foo.jp
                              "http://a.not.jp/file.html"));   // not.jp
  EXPECT_FALSE(CompareDomains("http://a.foo.jp/file.html",     // foo.jp
                              "http://a.foo.jp./file.html"));  // foo.jp.
  EXPECT_FALSE(CompareDomains("http://a.com/file.html",        // a.com
                              "http://b.com/file.html"));      // b.com
  EXPECT_TRUE(CompareDomains("http://a.x.com/file.html",
                             "http://b.x.com/file.html"));     // x.com
  EXPECT_TRUE(CompareDomains("http://a.x.com/file.html",
                             "http://.x.com/file.html"));      // x.com
  EXPECT_TRUE(CompareDomains("http://a.x.com/file.html",
                             "http://..b.x.com/file.html"));   // x.com
  EXPECT_TRUE(CompareDomains("http://intranet/file.html",
                             "http://intranet/file.html"));    // intranet
  EXPECT_FALSE(CompareDomains("http://intranet1/file.html",
                              "http://intranet2/file.html"));  // intranet
  EXPECT_TRUE(CompareDomains(
      "http://intranet1.corp.example.com/file.html",
      "http://intranet2.corp.example.com/file.html"));  // intranet
  EXPECT_TRUE(CompareDomains("http://127.0.0.1/file.html",
                             "http://127.0.0.1/file.html"));   // 127.0.0.1
  EXPECT_FALSE(CompareDomains("http://192.168.0.1/file.html",  // 192.168.0.1
                              "http://127.0.0.1/file.html"));  // 127.0.0.1
  EXPECT_FALSE(CompareDomains("file:///C:/file.html",
                              "file:///C:/file.html"));        // no host

  // The trailing dot means different sites - see also
  // https://github.com/mikewest/sec-metadata/issues/15.
  EXPECT_FALSE(
      CompareDomains("https://foo.example.com", "https://foo.example.com."));
}

TEST_F(RegistryControlledDomainTest, TestDefaultData) {
  // Note that no data is set: we're using the default rules.
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://google.com",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://stanford.edu",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://ustreas.gov",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://icann.net",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://ferretcentral.org",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://nowhere.notavaliddomain",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(15U, GetRegistryLengthFromURL("http://nowhere.notavaliddomain",
                                         INCLUDE_UNKNOWN_REGISTRIES));
}

TEST_F(RegistryControlledDomainTest, TestPrivateRegistryHandling) {
  UseDomainData(test1::kDafsa);

  // Testing the same dataset for INCLUDE_PRIVATE_REGISTRIES and
  // EXCLUDE_PRIVATE_REGISTRIES arguments.
  // For the domain data used for this test, the private registries are
  // 'priv.no' and 'private'.

  // Non-private registries.
  EXPECT_EQ(2U, GetRegistryLengthFromURL("http://priv.no",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(2U, GetRegistryLengthFromURL("http://foo.priv.no",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(2U, GetRegistryLengthFromURL("http://foo.jp",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(2U, GetRegistryLengthFromURL("http://www.foo.jp",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://private",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://foo.private",
                                         EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://private",
                                         INCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(7U, GetRegistryLengthFromURL("http://foo.private",
                                         INCLUDE_UNKNOWN_REGISTRIES));

  // Private registries.
  EXPECT_EQ(0U,
      GetRegistryLengthFromURLIncludingPrivate("http://priv.no",
                                               EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(7U,
      GetRegistryLengthFromURLIncludingPrivate("http://foo.priv.no",
                                               EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(2U,
      GetRegistryLengthFromURLIncludingPrivate("http://foo.jp",
                                               EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(2U,
      GetRegistryLengthFromURLIncludingPrivate("http://www.foo.jp",
                                               EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U,
      GetRegistryLengthFromURLIncludingPrivate("http://private",
                                               EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(7U,
      GetRegistryLengthFromURLIncludingPrivate("http://foo.private",
                                               EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U,
      GetRegistryLengthFromURLIncludingPrivate("http://private",
                                               INCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(7U,
      GetRegistryLengthFromURLIncludingPrivate("http://foo.private",
                                               INCLUDE_UNKNOWN_REGISTRIES));
}

TEST_F(RegistryControlledDomainTest, TestDafsaTwoByteOffsets) {
  UseDomainData(test3::kDafsa);

  // Testing to lookup keys in a DAFSA with two byte offsets.
  // This DAFSA is constructed so that labels begin and end with unique
  // characters, which makes it impossible to merge labels. Each inner node
  // is about 100 bytes and a one byte offset can at most add 64 bytes to
  // previous offset. Thus the paths must go over two byte offsets.

  const char key0[] =
      "a.b.6____________________________________________________"
      "________________________________________________6";
  const char key1[] =
      "a.b.7____________________________________________________"
      "________________________________________________7";
  const char key2[] =
      "a.b.a____________________________________________________"
      "________________________________________________8";

  EXPECT_EQ(102U,
            GetCanonicalHostRegistryLength(key0, EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U,
            GetCanonicalHostRegistryLength(key1, EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(102U, GetCanonicalHostRegistryLengthIncludingPrivate(key1));
  EXPECT_EQ(0U,
            GetCanonicalHostRegistryLength(key2, EXCLUDE_UNKNOWN_REGISTRIES));
}

TEST_F(RegistryControlledDomainTest, TestDafsaThreeByteOffsets) {
  UseDomainData(test4::kDafsa);

  // Testing to lookup keys in a DAFSA with three byte offsets.
  // This DAFSA is constructed so that labels begin and end with unique
  // characters, which makes it impossible to merge labels. The byte array
  // has a size of ~54k. A two byte offset can add at most add 8k to the
  // previous offset. Since we can skip only forward in memory, the nodes
  // representing the return values must be located near the end of the byte
  // array. The probability that we can reach from an arbitrary inner node to
  // a return value without using a three byte offset is small (but not zero).
  // The test is repeated with some different keys and with a reasonable
  // probability at least one of the tested paths has go over a three byte
  // offset.

  const char key0[] =
      "a.b.z6___________________________________________________"
      "_________________________________________________z6";
  const char key1[] =
      "a.b.z7___________________________________________________"
      "_________________________________________________z7";
  const char key2[] =
      "a.b.za___________________________________________________"
      "_________________________________________________z8";

  EXPECT_EQ(104U,
            GetCanonicalHostRegistryLength(key0, EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U,
            GetCanonicalHostRegistryLength(key1, EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(104U, GetCanonicalHostRegistryLengthIncludingPrivate(key1));
  EXPECT_EQ(0U,
            GetCanonicalHostRegistryLength(key2, EXCLUDE_UNKNOWN_REGISTRIES));
}

TEST_F(RegistryControlledDomainTest, TestDafsaJoinedPrefixes) {
  UseDomainData(test5::kDafsa);

  // Testing to lookup keys in a DAFSA with compressed prefixes.
  // This DAFSA is constructed from words with similar prefixes but distinct
  // suffixes. The DAFSA will then form a trie with the implicit source node
  // as root.

  const char key0[] = "a.b.ai";
  const char key1[] = "a.b.bj";
  const char key2[] = "a.b.aak";
  const char key3[] = "a.b.bbl";
  const char key4[] = "a.b.aaa";
  const char key5[] = "a.b.bbb";
  const char key6[] = "a.b.aaaam";
  const char key7[] = "a.b.bbbbn";

  EXPECT_EQ(2U,
            GetCanonicalHostRegistryLength(key0, EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U,
            GetCanonicalHostRegistryLength(key1, EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(2U, GetCanonicalHostRegistryLengthIncludingPrivate(key1));
  EXPECT_EQ(3U,
            GetCanonicalHostRegistryLength(key2, EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U,
            GetCanonicalHostRegistryLength(key3, EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(3U, GetCanonicalHostRegistryLengthIncludingPrivate(key3));
  EXPECT_EQ(0U, GetCanonicalHostRegistryLengthIncludingPrivate(key4));
  EXPECT_EQ(0U, GetCanonicalHostRegistryLengthIncludingPrivate(key5));
  EXPECT_EQ(5U,
            GetCanonicalHostRegistryLength(key6, EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(5U,
            GetCanonicalHostRegistryLength(key7, EXCLUDE_UNKNOWN_REGISTRIES));
}

TEST_F(RegistryControlledDomainTest, TestDafsaJoinedSuffixes) {
  UseDomainData(test6::kDafsa);

  // Testing to lookup keys in a DAFSA with compressed suffixes.
  // This DAFSA is constructed from words with similar suffixes but distinct
  // prefixes. The DAFSA will then form a trie with the implicit sink node as
  // root.

  const char key0[] = "a.b.ia";
  const char key1[] = "a.b.jb";
  const char key2[] = "a.b.kaa";
  const char key3[] = "a.b.lbb";
  const char key4[] = "a.b.aaa";
  const char key5[] = "a.b.bbb";
  const char key6[] = "a.b.maaaa";
  const char key7[] = "a.b.nbbbb";

  EXPECT_EQ(2U,
            GetCanonicalHostRegistryLength(key0, EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U,
            GetCanonicalHostRegistryLength(key1, EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(2U, GetCanonicalHostRegistryLengthIncludingPrivate(key1));
  EXPECT_EQ(3U,
            GetCanonicalHostRegistryLength(key2, EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(0U,
            GetCanonicalHostRegistryLength(key3, EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(3U, GetCanonicalHostRegistryLengthIncludingPrivate(key3));
  EXPECT_EQ(0U, GetCanonicalHostRegistryLengthIncludingPrivate(key4));
  EXPECT_EQ(0U, GetCanonicalHostRegistryLengthIncludingPrivate(key5));
  EXPECT_EQ(5U,
            GetCanonicalHostRegistryLength(key6, EXCLUDE_UNKNOWN_REGISTRIES));
  EXPECT_EQ(5U,
            GetCanonicalHostRegistryLength(key7, EXCLUDE_UNKNOWN_REGISTRIES));
}

TEST_F(RegistryControlledDomainTest, Permissive) {
  UseDomainData(test1::kDafsa);

  EXPECT_EQ(std::string::npos, PermissiveGetHostRegistryLength(""));

  // Regular non-canonical host name.
  EXPECT_EQ(2U, PermissiveGetHostRegistryLength("Www.Google.Jp"));
  EXPECT_EQ(3U, PermissiveGetHostRegistryLength("Www.Google.Jp."));

  // Empty returns npos.
  EXPECT_EQ(std::string::npos, PermissiveGetHostRegistryLength(""));

  // Trailing spaces are counted as part of the hostname, meaning this will
  // not match a known registry.
  EXPECT_EQ(0U, PermissiveGetHostRegistryLength("Www.Google.Jp "));

  // Invalid characters at the beginning are OK if the suffix still matches.
  EXPECT_EQ(2U, PermissiveGetHostRegistryLength("*%00#?.Jp"));

  // Escaped period, this will add new components.
  EXPECT_EQ(4U, PermissiveGetHostRegistryLength("Www.Googl%45%2e%4Ap"));

// IDN cases (not supported when not linking ICU).
#if !BUILDFLAG(USE_PLATFORM_ICU_ALTERNATIVES)
  EXPECT_EQ(10U, PermissiveGetHostRegistryLength("foo.xn--fiqs8s"));
  EXPECT_EQ(11U, PermissiveGetHostRegistryLength("foo.xn--fiqs8s."));
  EXPECT_EQ(18U, PermissiveGetHostRegistryLength("foo.%E4%B8%AD%E5%9B%BD"));
  EXPECT_EQ(19U, PermissiveGetHostRegistryLength("foo.%E4%B8%AD%E5%9B%BD."));
  EXPECT_EQ(6U,
            PermissiveGetHostRegistryLength("foo.\xE4\xB8\xAD\xE5\x9B\xBD"));
  EXPECT_EQ(7U,
            PermissiveGetHostRegistryLength("foo.\xE4\xB8\xAD\xE5\x9B\xBD."));
  // UTF-16 IDN.
  EXPECT_EQ(2U, PermissiveGetHostRegistryLength(u"foo.\x4e2d\x56fd"));

  // Fullwidth dot (u+FF0E) that will get canonicalized to a dot.
  EXPECT_EQ(2U, PermissiveGetHostRegistryLength("Www.Google\xEF\xBC\x8Ejp"));
  // Same but also ending in a fullwidth dot.
  EXPECT_EQ(5U, PermissiveGetHostRegistryLength(
                    "Www.Google\xEF\xBC\x8Ejp\xEF\xBC\x8E"));
  // Escaped UTF-8, also with an escaped fullwidth "Jp".
  // "Jp" = U+FF2A, U+FF50, UTF-8 = EF BC AA EF BD 90
  EXPECT_EQ(27U, PermissiveGetHostRegistryLength(
                     "Www.Google%EF%BC%8E%EF%BC%AA%EF%BD%90%EF%BC%8E"));
  // UTF-16 (ending in a dot).
  EXPECT_EQ(3U, PermissiveGetHostRegistryLength(
                    u"Www.Google\xFF0E\xFF2A\xFF50\xFF0E"));
#endif
}

}  // namespace net::registry_controlled_domains
