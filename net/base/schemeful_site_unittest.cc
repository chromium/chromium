// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/schemeful_site.h"

#include "base/test/metrics/histogram_tester.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace net {

TEST(SchemefulSiteTest, DifferentOriginSameRegisterableDomain) {
  // List of origins which should all share a schemeful site.
  url::Origin kTestOrigins[] = {
      url::Origin::Create(GURL("http://a.foo.test")),
      url::Origin::Create(GURL("http://b.foo.test")),
      url::Origin::Create(GURL("http://foo.test")),
      url::Origin::Create(GURL("http://a.b.foo.test"))};

  for (const auto& origin_a : kTestOrigins) {
    for (const auto& origin_b : kTestOrigins) {
      EXPECT_EQ(SchemefulSite(origin_a), SchemefulSite(origin_b));
    }
  }
}

TEST(SchemefulSiteTest, Operators) {
  // Create a list of origins that should all have different schemeful sites.
  // These are in ascending order.
  url::Origin kTestOrigins[] = {
      url::Origin::Create(GURL("data:text/html,<body>Hello World</body>")),
      url::Origin::Create(GURL("file://foo")),
      url::Origin::Create(GURL("http://a.bar.test")),
      url::Origin::Create(GURL("http://c.test")),
      url::Origin::Create(GURL("http://d.test")),
      url::Origin::Create(GURL("http://a.foo.test")),
      url::Origin::Create(GURL("https://a.bar.test")),
      url::Origin::Create(GURL("https://c.test")),
      url::Origin::Create(GURL("https://d.test")),
      url::Origin::Create(GURL("https://a.foo.test"))};

  // Compare each origin to every other origin and ensure the operators work as
  // expected.
  for (size_t first = 0; first < std::size(kTestOrigins); ++first) {
    SchemefulSite site1 = SchemefulSite(kTestOrigins[first]);
    SCOPED_TRACE(site1.GetDebugString());

    EXPECT_EQ(site1, site1);
    EXPECT_FALSE(site1 < site1);

    // Check the operators work on copies.
    SchemefulSite site1_copy = site1;
    EXPECT_EQ(site1, site1_copy);
    EXPECT_FALSE(site1 < site1_copy);

    for (size_t second = first + 1; second < std::size(kTestOrigins);
         ++second) {
      SchemefulSite site2 = SchemefulSite(kTestOrigins[second]);
      SCOPED_TRACE(site2.GetDebugString());

      EXPECT_TRUE(site1 < site2);
      EXPECT_FALSE(site2 < site1);
      EXPECT_FALSE(site1 == site2);
      EXPECT_FALSE(site2 == site1);
    }
  }
}

TEST(SchemefulSiteTest, SchemeUsed) {
  url::Origin origin_a = url::Origin::Create(GURL("https://foo.test"));
  url::Origin origin_b = url::Origin::Create(GURL("http://foo.test"));
  EXPECT_NE(SchemefulSite(origin_a), SchemefulSite(origin_b));
}

TEST(SchemefulSiteTest, PortIgnored) {
  // Both origins are non-opaque.
  url::Origin origin_a = url::Origin::Create(GURL("https://foo.test:80"));
  url::Origin origin_b = url::Origin::Create(GURL("https://foo.test:2395"));

  EXPECT_EQ(SchemefulSite(origin_a), SchemefulSite(origin_b));
}

TEST(SchemefulSiteTest, TopLevelDomainsNotModified) {
  url::Origin origin_tld = url::Origin::Create(GURL("https://com"));
  EXPECT_EQ(url::Origin::Create(GURL("https://com")),
            SchemefulSite(origin_tld).GetInternalOriginForTesting());

  // Unknown TLD's should not be modified.
  url::Origin origin_tld_unknown =
      url::Origin::Create(GURL("https://bar:1234"));
  EXPECT_EQ(url::Origin::Create(GURL("https://bar")),
            SchemefulSite(origin_tld_unknown).GetInternalOriginForTesting());

  // Check for two-part TLDs.
  url::Origin origin_two_part_tld = url::Origin::Create(GURL("http://a.co.uk"));
  EXPECT_EQ(url::Origin::Create(GURL("http://a.co.uk")),
            SchemefulSite(origin_two_part_tld).GetInternalOriginForTesting());
}

TEST(SchemefulSiteTest, NonStandardScheme) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("foo", url::SCHEME_WITH_HOST);
  url::Origin origin = url::Origin::Create(GURL("foo://a.b.test"));
  EXPECT_FALSE(origin.opaque());

  // We should not use registerable domains for non-standard schemes, even if
  // one exists for the host.
  EXPECT_EQ(url::Origin::Create(GURL("foo://a.b.test")),
            SchemefulSite(origin).GetInternalOriginForTesting());
}

TEST(SchemefulSiteTest, IPBasedOriginsRemovePort) {
  // IPv4 and IPv6 origins should not be modified, except for removing their
  // ports.
  url::Origin origin_ipv4_a =
      url::Origin::Create(GURL("http://127.0.0.1:1234"));
  url::Origin origin_ipv4_b = url::Origin::Create(GURL("http://127.0.0.1"));
  EXPECT_EQ(url::Origin::Create(GURL("http://127.0.0.1")),
            SchemefulSite(origin_ipv4_a).GetInternalOriginForTesting());
  EXPECT_EQ(SchemefulSite(origin_ipv4_a), SchemefulSite(origin_ipv4_b));

  url::Origin origin_ipv6 = url::Origin::Create(GURL("https://[::1]"));
  EXPECT_EQ(url::Origin::Create(GURL("https://[::1]")),
            SchemefulSite(origin_ipv6).GetInternalOriginForTesting());
}

TEST(SchemefulSiteTest, LocalhostOriginsRemovePort) {
  // Localhost origins should not be modified, except for removing their ports.
  url::Origin localhost_http =
      url::Origin::Create(GURL("http://localhost:1234"));
  EXPECT_EQ(url::Origin::Create(GURL("http://localhost")),
            SchemefulSite(localhost_http).GetInternalOriginForTesting());

  url::Origin localhost_https =
      url::Origin::Create(GURL("https://localhost:1234"));
  EXPECT_EQ(url::Origin::Create(GURL("https://localhost")),
            SchemefulSite(localhost_https).GetInternalOriginForTesting());
}

TEST(SchemefulSiteTest, OpaqueOrigins) {
  url::Origin opaque_origin_a =
      url::Origin::Create(GURL("data:text/html,<body>Hello World</body>"));

  // The schemeful site of an opaque origin should always equal other schemeful
  // site instances of the same origin.
  EXPECT_EQ(SchemefulSite(opaque_origin_a), SchemefulSite(opaque_origin_a));

  url::Origin opaque_origin_b =
      url::Origin::Create(GURL("data:text/html,<body>Hello World</body>"));

  // Two different opaque origins should never have the same SchemefulSite.
  EXPECT_NE(SchemefulSite(opaque_origin_a), SchemefulSite(opaque_origin_b));
}

TEST(SchemefulSiteTest, FileOriginWithoutHostname) {
  SchemefulSite site1(url::Origin::Create(GURL("file:///")));
  SchemefulSite site2(url::Origin::Create(GURL("file:///path/")));

  EXPECT_EQ(site1, site2);
  EXPECT_TRUE(site1.GetInternalOriginForTesting().host().empty());
}

TEST(SchemefulSiteTest, SchemeWithNetworkHost) {
  url::ScopedSchemeRegistryForTests scheme_registry;
  AddStandardScheme("network", url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION);
  AddStandardScheme("non-network", url::SCHEME_WITH_HOST);

  ASSERT_TRUE(IsStandardSchemeWithNetworkHost("network"));
  ASSERT_FALSE(IsStandardSchemeWithNetworkHost("non-network"));

  std::optional<SchemefulSite> network_host_site =
      SchemefulSite::CreateIfHasRegisterableDomain(
          url::Origin::Create(GURL("network://site.example.test:1337")));
  EXPECT_TRUE(network_host_site.has_value());
  EXPECT_EQ("network",
            network_host_site->GetInternalOriginForTesting().scheme());
  EXPECT_EQ("example.test",
            network_host_site->GetInternalOriginForTesting().host());

  std::optional<SchemefulSite> non_network_host_site_null =
      SchemefulSite::CreateIfHasRegisterableDomain(
          url::Origin::Create(GURL("non-network://site.example.test")));
  EXPECT_FALSE(non_network_host_site_null.has_value());
  SchemefulSite non_network_host_site(GURL("non-network://site.example.test"));
  EXPECT_EQ("non-network",
            non_network_host_site.GetInternalOriginForTesting().scheme());
  // The host is used as-is, without attempting to get a registrable domain.
  EXPECT_EQ("site.example.test",
            non_network_host_site.GetInternalOriginForTesting().host());
}

TEST(SchemefulSiteTest, FileSchemeHasRegistrableDomain) {
  // Test file origin without host.
  url::Origin origin_file =
      url::Origin::Create(GURL("file:///dir1/dir2/file.txt"));
  EXPECT_TRUE(origin_file.host().empty());
  SchemefulSite site_file(origin_file);
  EXPECT_EQ(url::Origin::Create(GURL("file:///")),
            site_file.GetInternalOriginForTesting());

  // Test file origin with host (with registrable domain).
  url::Origin origin_file_with_host =
      url::Origin::Create(GURL("file://host.example.test/file"));
  ASSERT_EQ("host.example.test", origin_file_with_host.host());
  SchemefulSite site_file_with_host(origin_file_with_host);
  EXPECT_EQ(url::Origin::Create(GURL("file://example.test")),
            site_file_with_host.GetInternalOriginForTesting());

  // Test file origin with host same as registrable domain.
  url::Origin origin_file_registrable_domain =
      url::Origin::Create(GURL("file://example.test/file"));
  ASSERT_EQ("example.test", origin_file_registrable_domain.host());
  SchemefulSite site_file_registrable_domain(origin_file_registrable_domain);
  EXPECT_EQ(url::Origin::Create(GURL("file://example.test")),
            site_file_registrable_domain.GetInternalOriginForTesting());

  EXPECT_NE(site_file, site_file_with_host);
  EXPECT_NE(site_file, site_file_registrable_domain);
  EXPECT_EQ(site_file_with_host, site_file_registrable_domain);
}

TEST(SchemefulSiteTest, SerializationConsistent) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome", url::SCHEME_WITH_HOST);

  // List of origins which should all share a schemeful site.
  SchemefulSite kTestSites[] = {
      SchemefulSite(url::Origin::Create(GURL("http://a.foo.test"))),
      SchemefulSite(url::Origin::Create(GURL("https://b.foo.test"))),
      SchemefulSite(url::Origin::Create(GURL("http://b.foo.test"))),
      SchemefulSite(url::Origin::Create(GURL("http://a.b.foo.test"))),
      SchemefulSite(url::Origin::Create(GURL("chrome://a.b.test")))};

  for (const auto& site : kTestSites) {
    SCOPED_TRACE(site.GetDebugString());
    EXPECT_FALSE(site.GetInternalOriginForTesting().opaque());

    std::optional<SchemefulSite> deserialized_site =
        SchemefulSite::Deserialize(site.Serialize());
    EXPECT_TRUE(deserialized_site);
    EXPECT_EQ(site, deserialized_site);
  }
}

TEST(SchemefulSiteTest, SerializationFileSiteWithHost) {
  const struct {
    SchemefulSite site;
    std::string expected;
  } kTestCases[] = {
      {SchemefulSite(GURL("file:///etc/passwd")), "file://"},
      {SchemefulSite(GURL("file://example.com/etc/passwd")),
       "file://example.com"},
      {SchemefulSite(GURL("file://example.com")), "file://example.com"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.site.GetDebugString());
    std::string serialized_site = test_case.site.SerializeFileSiteWithHost();
    EXPECT_EQ(test_case.expected, serialized_site);
    std::optional<SchemefulSite> deserialized_site =
        SchemefulSite::Deserialize(serialized_site);
    EXPECT_TRUE(deserialized_site);
    EXPECT_EQ(test_case.site, deserialized_site);
  }
}

TEST(SchemefulSiteTest, FileURLWithHostEquality) {
  // Two file URLs with different hosts should result in unequal SchemefulSites.
  SchemefulSite site1(GURL("file://foo/some/path.txt"));
  SchemefulSite site2(GURL("file://bar/some/path.txt"));
  EXPECT_NE(site1, site2);

  // Two file URLs with the same host should result in equal SchemefulSites.
  SchemefulSite site3(GURL("file://foo/another/path.pdf"));
  EXPECT_EQ(site1, site3);
}

TEST(SchemefulSiteTest, OpaqueSerialization) {
  // List of origins which should all share a schemeful site.
  SchemefulSite kTestSites[] = {
      SchemefulSite(), SchemefulSite(url::Origin()),
      SchemefulSite(GURL("data:text/html,<body>Hello World</body>"))};

  for (auto& site : kTestSites) {
    std::optional<SchemefulSite> deserialized_site =
        SchemefulSite::DeserializeWithNonce(*site.SerializeWithNonce());
    EXPECT_TRUE(deserialized_site);
    EXPECT_EQ(site, *deserialized_site);
  }
}

TEST(SchemefulSiteTest, FromWire) {
  SchemefulSite out;

  // Opaque origin.
  EXPECT_TRUE(SchemefulSite::FromWire(url::Origin(), &out));
  EXPECT_TRUE(out.opaque());

  // Valid origin.
  EXPECT_TRUE(SchemefulSite::FromWire(
      url::Origin::Create(GURL("https://example.test")), &out));
  EXPECT_EQ(SchemefulSite(url::Origin::Create(GURL("https://example.test"))),
            out);

  // Invalid origin (not a registrable domain).
  EXPECT_FALSE(SchemefulSite::FromWire(
      url::Origin::Create(GURL("https://sub.example.test")), &out));

  // Invalid origin (non-default port).
  EXPECT_FALSE(SchemefulSite::FromWire(
      url::Origin::Create(GURL("https://example.test:1337")), &out));
}

TEST(SchemefulSiteTest, CreateIfHasRegisterableDomain) {
  for (const auto& site : std::initializer_list<std::string>{
           "http://a.bar.test",
           "http://c.test",
           "http://a.foo.test",
           "https://a.bar.test",
           "https://c.test",
           "https://a.foo.test",
       }) {
    url::Origin origin = url::Origin::Create(GURL(site));
    EXPECT_THAT(SchemefulSite::CreateIfHasRegisterableDomain(origin),
                testing::Optional(SchemefulSite(origin)))
        << "site = \"" << site << "\"";
  }

  for (const auto& site : std::initializer_list<std::string>{
           "data:text/html,<body>Hello World</body>",
           "file:///",
           "file://foo",
           "http://127.0.0.1:1234",
           "https://127.0.0.1:1234",
       }) {
    url::Origin origin = url::Origin::Create(GURL(site));
    EXPECT_EQ(SchemefulSite::CreateIfHasRegisterableDomain(origin),
              std::nullopt)
        << "site = \"" << site << "\"";
  }
}

TEST(SchemefulSiteTest, ConvertWebSocketToHttp) {
  SchemefulSite ws_site(url::Origin::Create(GURL("ws://site.example.test")));
  SchemefulSite http_site(
      url::Origin::Create(GURL("http://site.example.test")));
  SchemefulSite wss_site(url::Origin::Create(GURL("wss://site.example.test")));
  SchemefulSite https_site(
      url::Origin::Create(GURL("https://site.example.test")));

  ASSERT_NE(ws_site, wss_site);
  ASSERT_NE(ws_site, http_site);
  ASSERT_NE(ws_site, https_site);
  ASSERT_NE(wss_site, http_site);
  ASSERT_NE(wss_site, https_site);

  ws_site.ConvertWebSocketToHttp();
  wss_site.ConvertWebSocketToHttp();

  EXPECT_EQ(ws_site, http_site);
  EXPECT_EQ(wss_site, https_site);

  // Does not change non-WebSocket sites.
  SchemefulSite http_site_copy(http_site);
  http_site_copy.ConvertWebSocketToHttp();
  EXPECT_EQ(http_site, http_site_copy);
  EXPECT_EQ(url::kHttpScheme,
            http_site_copy.GetInternalOriginForTesting().scheme());

  SchemefulSite file_site(url::Origin::Create(GURL("file:///")));
  file_site.ConvertWebSocketToHttp();
  EXPECT_EQ(url::kFileScheme, file_site.GetInternalOriginForTesting().scheme());
}

TEST(SchemefulSiteTest, GetGURL) {
  struct {
    url::Origin origin;
    GURL wantGURL;
  } kTestCases[] = {
      {
          url::Origin::Create(GURL("data:text/html,<body>Hello World</body>")),
          GURL(),
      },
      {url::Origin::Create(GURL("file://foo")), GURL("file:///")},
      {url::Origin::Create(GURL("http://a.bar.test")), GURL("http://bar.test")},
      {url::Origin::Create(GURL("http://c.test")), GURL("http://c.test")},
      {url::Origin::Create(GURL("http://c.test:8000")), GURL("http://c.test")},
      {
          url::Origin::Create(GURL("https://a.bar.test")),
          GURL("https://bar.test"),
      },
      {
          url::Origin::Create(GURL("https://c.test")),
          GURL("https://c.test"),
      },
      {
          url::Origin::Create(GURL("https://c.test:1337")),
          GURL("https://c.test"),
      },
  };

  for (const auto& testcase : kTestCases) {
    SchemefulSite site(testcase.origin);
    EXPECT_EQ(site.GetURL(), testcase.wantGURL);
  }
}

TEST(SchemefulSiteTest, InternalValue) {
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  SchemefulSite site(origin);
  EXPECT_EQ(site.internal_value(), origin);
  url::Origin opaque_origin;
  SchemefulSite opaque_site(opaque_origin);
  EXPECT_EQ(opaque_site.internal_value(), opaque_origin);
}

}  // namespace net
