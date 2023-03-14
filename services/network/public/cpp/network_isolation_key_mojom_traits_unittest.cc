// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_isolation_key_mojom_traits.h"

#include "base/unguessable_token.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/network_isolation_key.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace mojo {

TEST(NetworkIsolationKeyMojomTraitsTest, SerializeAndDeserialize) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  std::vector<net::NetworkIsolationKey> keys = {
      net::NetworkIsolationKey(),
      net::NetworkIsolationKey::CreateTransient(),
      net::NetworkIsolationKey(url::Origin::Create(GURL("http://a.test/")),
                               url::Origin::Create(GURL("http://b.test/"))),
      net::NetworkIsolationKey(url::Origin::Create(GURL("http://foo.a.test/")),
                               url::Origin::Create(GURL("http://bar.b.test/"))),
      net::NetworkIsolationKey(
          net::SchemefulSite(url::Origin::Create(GURL("http://a.test/"))),
          net::SchemefulSite(url::Origin::Create(GURL("http://b.test/"))),
          &token),
      net::NetworkIsolationKey(
          net::SchemefulSite(url::Origin::Create(GURL("http://foo.a.test/"))),
          net::SchemefulSite(url::Origin::Create(GURL("http://bar.b.test/"))),
          &token)};

  for (auto original : keys) {
    SCOPED_TRACE(original.ToDebugString());
    net::NetworkIsolationKey copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                network::mojom::NetworkIsolationKey>(original, copied));
    EXPECT_EQ(original, copied);
    EXPECT_EQ(original.GetTopFrameSite(), copied.GetTopFrameSite());
    if (net::NetworkIsolationKey::GetMode() ==
        net::NetworkIsolationKey::Mode::kFrameSiteEnabled) {
      EXPECT_EQ(original.GetFrameSite(), copied.GetFrameSite());
    }
    EXPECT_EQ(original.IsTransient(), copied.IsTransient());
  }
}

TEST(NetworkIsolationKeyMojomTraitsTest, DeserializeFailure) {
  std::vector<uint8_t> serialized;
  net::NetworkIsolationKey deserialized;

  if (net::NetworkIsolationKey::GetMode() ==
      net::NetworkIsolationKey::Mode::kFrameSiteEnabled) {
    auto empty_top_level_site = network::mojom::NetworkIsolationKey::New(
        /*top_frame_site=*/absl::nullopt,
        net::SchemefulSite(url::Origin::Create(GURL("http://a.test/"))),
        /*nonce=*/absl::nullopt);
    serialized =
        network::mojom::NetworkIsolationKey::Serialize(&empty_top_level_site);
    EXPECT_FALSE(network::mojom::NetworkIsolationKey::Deserialize(
        serialized, &deserialized));

    auto empty_frame_site = network::mojom::NetworkIsolationKey::New(
        net::SchemefulSite(url::Origin::Create(GURL("http://a.test/"))),
        /*frame_site=*/absl::nullopt, /*nonce=*/absl::nullopt);
    serialized =
        network::mojom::NetworkIsolationKey::Serialize(&empty_frame_site);
    EXPECT_FALSE(network::mojom::NetworkIsolationKey::Deserialize(
        serialized, &deserialized));
  }

  auto token_only = network::mojom::NetworkIsolationKey::New(
      /*top_frame_site=*/absl::nullopt, /*frame_site=*/absl::nullopt,
      base::UnguessableToken::Create());
  serialized = network::mojom::NetworkIsolationKey::Serialize(&token_only);
  EXPECT_FALSE(network::mojom::NetworkIsolationKey::Deserialize(serialized,
                                                                &deserialized));
}

}  // namespace mojo
