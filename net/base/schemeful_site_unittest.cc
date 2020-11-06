// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/schemeful_site.h"

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

}  // namespace net
