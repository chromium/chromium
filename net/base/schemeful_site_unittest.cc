// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  for (size_t first = 0; first < base::size(kTestOrigins); ++first) {
    SchemefulSite site1 = SchemefulSite(kTestOrigins[first]);
    SCOPED_TRACE(site1.GetDebugString());

    EXPECT_EQ(site1, site1);
    EXPECT_FALSE(site1 < site1);

    // Check the operators work on copies.
    SchemefulSite site1_copy = site1;
    EXPECT_EQ(site1, site1_copy);
    EXPECT_FALSE(site1 < site1_copy);

    for (size_t second = first + 1; second < base::size(kTestOrigins);
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

  base::Optional<SchemefulSite> network_host_site =
      SchemefulSite::CreateIfHasRegisterableDomain(
          url::Origin::Create(GURL("network://site.example.test:1337")));
  EXPECT_TRUE(network_host_site.has_value());
  EXPECT_EQ("network",
            network_host_site->GetInternalOriginForTesting().scheme());
  EXPECT_EQ("example.test",
            network_host_site->GetInternalOriginForTesting().host());

  base::Optional<SchemefulSite> non_network_host_site_null =
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

    base::Optional<SchemefulSite> deserialized_site =
        SchemefulSite::Deserialize(site.Serialize());
    EXPECT_TRUE(deserialized_site);
    EXPECT_EQ(site, deserialized_site);
  }
}

TEST(SchemefulSiteTest, OpaqueSerialization) {
  // List of origins which should all share a schemeful site.
  SchemefulSite kTestSites[] = {
      SchemefulSite(), SchemefulSite(url::Origin()),
      SchemefulSite(GURL("data:text/html,<body>Hello World</body>"))};

  for (auto& site : kTestSites) {
    base::Optional<SchemefulSite> deserialized_site =
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
              base::nullopt)
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

// Test for a hack to work around https://crbug.com/1157010, until a more
// permanent solution is in place. Purely numeric eTLD+1's can't safely be
// stored in url::Origins.  Not only does trying to do so DCHECK, but converting
// them to a GURL, as some code does, results in a URL with an IPv4 domain,
// which is not correct.
TEST(SchemefulSiteTest, NumericEtldPlusOne) {
  base::HistogramTester histogram_tester;
  SchemefulSite site(url::Origin::Create(GURL("https://foo.127.1/")));
  EXPECT_EQ("foo.127.1", site.registrable_domain_or_host_for_testing());
  EXPECT_NE(site, SchemefulSite(GURL("https://127.0.0.1/")));

  SchemefulSite site2(url::Origin::Create(GURL("https://bar.foo.127.1/")));
  EXPECT_EQ("bar.foo.127.1", site2.registrable_domain_or_host_for_testing());
  EXPECT_NE(site2, SchemefulSite(GURL("https://127.0.0.1/")));
  EXPECT_NE(site, site2);

  EXPECT_FALSE(SchemefulSite::CreateIfHasRegisterableDomain(
      url::Origin::Create(GURL("https://foo.127.1/"))));
}

TEST(SchemefulSiteTest, SiteDomainIsSafeHistogram) {
  base::HistogramTester histogram_tester1;
  SchemefulSite site(url::Origin::Create(GURL("https://foo.127.1/")));
  histogram_tester1.ExpectUniqueSample("Net.SiteDomainIsSafe", false, 1);

  base::HistogramTester histogram_tester2;
  SchemefulSite site2(url::Origin::Create(GURL("https://foo.bar.127.1/")));
  histogram_tester2.ExpectUniqueSample("Net.SiteDomainIsSafe", false, 1);

  base::HistogramTester histogram_tester3;
  SchemefulSite site3(url::Origin::Create(GURL("https://127.0.0.1/")));
  histogram_tester3.ExpectUniqueSample("Net.SiteDomainIsSafe", true, 1);

  base::HistogramTester histogram_tester4;
  SchemefulSite site4(url::Origin::Create(GURL("https://foo.test/")));
  histogram_tester4.ExpectUniqueSample("Net.SiteDomainIsSafe", true, 1);

  base::HistogramTester histogram_tester5;
  SchemefulSite site5{url::Origin()};
  histogram_tester5.ExpectTotalCount("Net.SiteDomainIsSafe", 0);
  SchemefulSite site6(
      url::Origin::Create(GURL("data:text/html,<body>Hello World</body>")));
  histogram_tester5.ExpectTotalCount("Net.SiteDomainIsSafe", 0);
}

}  // namespace net
