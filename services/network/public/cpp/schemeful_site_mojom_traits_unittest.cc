// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/schemeful_site_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/schemeful_site.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace network {
namespace {

TEST(SchemefulSiteMojomTraitsTest, SerializeAndDeserialize) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);

  std::vector<net::SchemefulSite> keys = {
      net::SchemefulSite(),
      net::SchemefulSite(url::Origin()),
      net::SchemefulSite(url::Origin::Create(GURL("http://a.test"))),
      net::SchemefulSite(url::Origin::Create(GURL("https://a.test"))),
      net::SchemefulSite(url::Origin::Create(GURL("https://subdomain.a.test"))),
      net::SchemefulSite(url::Origin::Create(GURL("https://127.0.0.1"))),
      net::SchemefulSite(url::Origin::Create(GURL("https://127.0.0.1:5000"))),
      net::SchemefulSite(url::Origin::Create(GURL("https://a.test:1337"))),
      net::SchemefulSite(url::Origin::Create(GURL("file:///"))),
      net::SchemefulSite(url::Origin::Create(GURL("file:///path"))),
      net::SchemefulSite(url::Origin::Create(GURL("file://foo.test/path"))),
      net::SchemefulSite(
          url::Origin::Create(GURL("chrome-extension://abcdefghi"))),
      net::SchemefulSite(url::Origin::Create(GURL("data:text/plain,foo")))};

  for (auto original : keys) {
    net::SchemefulSite copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SchemefulSite>(
        original, copied));
    EXPECT_EQ(original, copied);
  }
}

TEST(SchemefulSiteMojomTraitsTest, DeserializeFailure) {
  mojom::SchemefulSitePtr schemeful_site = mojom::SchemefulSite::New();
  // This origin could never be the internal `site_as_origin_` of a cromulent
  // SchemefulSite, so should fail deserialization.
  schemeful_site->site_as_origin =
      url::Origin::Create(GURL("https://not.a.registrable.domain.test:1337"));

  std::vector<uint8_t> serialized =
      mojom::SchemefulSite::Serialize(&schemeful_site);

  net::SchemefulSite deserialized;
  EXPECT_FALSE(mojom::SchemefulSite::Deserialize(serialized, &deserialized));
}

}  // namespace
}  // namespace network
