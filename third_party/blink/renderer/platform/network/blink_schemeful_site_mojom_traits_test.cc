// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/blink_schemeful_site_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/mojom/schemeful_site.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

TEST(BlinkSchemefulSiteMojomTraitsTest, SerializeDeserialize) {
  Vector<BlinkSchemefulSite> sites = {
      BlinkSchemefulSite(),
      BlinkSchemefulSite(url::Origin::Create(GURL("https://example.com"))),
      BlinkSchemefulSite(url::Origin::Create(GURL("https://sub.example.com"))),
      BlinkSchemefulSite(url::Origin::Create(GURL("https://127.0.0.1"))),
      BlinkSchemefulSite(url::Origin::Create(GURL("https://127.0.0.1:5000"))),
      BlinkSchemefulSite(url::Origin::Create(GURL("https://example.com:1337"))),
      BlinkSchemefulSite(url::Origin::Create(GURL("file:///"))),
      BlinkSchemefulSite(url::Origin::Create(GURL("file:///path"))),
      BlinkSchemefulSite(url::Origin::Create(GURL("file://foo.test/path"))),
      BlinkSchemefulSite(url::Origin::Create(GURL("data:text/plain,foo")))};

  for (BlinkSchemefulSite& in : sites) {
    BlinkSchemefulSite out;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<network::mojom::SchemefulSite>(
            in, out));
    EXPECT_EQ(in, out);
  }
}

// Test that we can serialize from a BlinkSchemefulSite and deserialize to a
// SchemefulSite and vice-versa.
TEST(BlinkSchemefulSiteMojomTraitsTest, SerializeToAndFromEachType) {
  Vector<url::Origin> origins = {
      url::Origin(),
      url::Origin::Create(GURL("https://example.com")),
      url::Origin::Create(GURL("https://sub.example.com")),
      url::Origin::Create(GURL("https://127.0.0.1")),
      url::Origin::Create(GURL("https://127.0.0.1:5000")),
      url::Origin::Create(GURL("https://example.com:1337")),
      url::Origin::Create(GURL("file:///")),
      url::Origin::Create(GURL("file:///path")),
      url::Origin::Create(GURL("file://foo.test/path")),
      url::Origin::Create(GURL("data:text/plain,foo"))};

  Vector<BlinkSchemefulSite> blink_site;
  Vector<net::SchemefulSite> net_site;
  for (const auto& origin : origins) {
    blink_site.emplace_back(origin);
    net_site.emplace_back(origin);
  }

  // From BlinkSchemefulSite to SchemefulSite.
  for (wtf_size_t i = 0; i < blink_site.size(); i++) {
    auto serialized = network::mojom::SchemefulSite::Serialize(&blink_site[i]);

    net::SchemefulSite deserialized;
    EXPECT_TRUE(
        network::mojom::SchemefulSite::Deserialize(serialized, &deserialized));
    EXPECT_EQ(net_site[i], deserialized);
  }

  // From SchemefulSite to BlinkSchemefulSite.
  for (wtf_size_t i = 0; i < blink_site.size(); i++) {
    auto serialized = network::mojom::SchemefulSite::Serialize(&net_site[i]);

    BlinkSchemefulSite deserialized;
    EXPECT_TRUE(
        network::mojom::SchemefulSite::Deserialize(serialized, &deserialized));
    EXPECT_EQ(blink_site[i], deserialized);
  }
}

// Test that an invalid message fails to deserialize.
TEST(BlinkSchemefulSiteMojomTraitsTest, DeserializeFailure) {
  BlinkSchemefulSite site;
  site.site_as_origin_ =
      SecurityOrigin::CreateFromString("https://sub1.sub2.example.com");

  auto serialized = network::mojom::SchemefulSite::Serialize(&site);
  BlinkSchemefulSite deserialized;
  EXPECT_FALSE(
      network::mojom::SchemefulSite::Deserialize(serialized, &deserialized));
}

}  // namespace blink
