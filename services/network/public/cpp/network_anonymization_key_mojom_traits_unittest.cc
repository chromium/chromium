// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_anonymization_key_mojom_traits.h"

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "services/network/public/mojom/network_anonymization_key.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace mojo {
TEST(NetworkAnonymizationKeyMojomTraitsTest, SerializeAndDeserializeDoubleKey) {
  // Enable double keying.
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(
      net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);

  base::UnguessableToken token = base::UnguessableToken::Create();
  std::vector<net::NetworkAnonymizationKey> keys = {
      net::NetworkAnonymizationKey(),
      net::NetworkAnonymizationKey::CreateTransient(),
      net::NetworkAnonymizationKey(net::SchemefulSite(GURL("http://a.test/")),
                                   net::SchemefulSite(GURL("http://b.test/")),
                                   &token),
      net::NetworkAnonymizationKey(net::SchemefulSite(GURL("http://a.test/")))};

  for (auto& original : keys) {
    SCOPED_TRACE(original.ToDebugString());
    net::NetworkAnonymizationKey copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                network::mojom::NetworkAnonymizationKey>(original, copied));
    EXPECT_EQ(original, copied);
  }
}

// TODO(crbug.com/1371667): Test is failing.
TEST(NetworkAnonymizationKeyMojomTraitsTest,
     DISABLED_SerializeAndDeserializeDoubleKeyWithCrossSiteFlag) {
  // Enable double keying with cross site flag.
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);

  base::UnguessableToken token = base::UnguessableToken::Create();
  std::vector<net::NetworkAnonymizationKey> keys = {
      net::NetworkAnonymizationKey(),
      net::NetworkAnonymizationKey::CreateTransient(),
      net::NetworkAnonymizationKey(net::SchemefulSite(GURL("http://a.test/")),
                                   /*frame_site=*/absl::nullopt,
                                   /*is_cross_site=*/true, token),
      net::NetworkAnonymizationKey(net::SchemefulSite(GURL("http://a.test/")),
                                   /*frame_site=*/absl::nullopt,
                                   /*is_cross_site=*/true)};
  for (auto& original : keys) {
    SCOPED_TRACE(original.ToDebugString());
    net::NetworkAnonymizationKey copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                network::mojom::NetworkAnonymizationKey>(original, copied));
    SCOPED_TRACE(copied.ToDebugString());

    EXPECT_EQ(original, copied);
  }
}

}  // namespace mojo
