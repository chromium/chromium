// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_isolation_key_mojom_traits.h"

#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "services/network/public/mojom/network_isolation_key.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace mojo {

TEST(NetworkIsolationKeyMojomTraitsTest, SerializeAndDeserialize) {
  std::vector<net::NetworkIsolationKey> keys = {
      net::NetworkIsolationKey(), net::NetworkIsolationKey::CreateTransient(),
      net::NetworkIsolationKey::CreateOpaqueAndNonTransient(),
      net::NetworkIsolationKey(url::Origin::Create(GURL("http://a.test/")),
                               url::Origin::Create(GURL("http://b.test/")))};

  for (auto original : keys) {
    SCOPED_TRACE(original.ToDebugString());
    net::NetworkIsolationKey copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                network::mojom::NetworkIsolationKey>(&original, &copied));
    EXPECT_EQ(original, copied);
  }
}

class NetworkIsolationKeyMojomTraitsWithFrameOriginTest : public testing::Test {
 public:
  NetworkIsolationKeyMojomTraitsWithFrameOriginTest() {
    feature_list_.InitAndEnableFeature(
        net::features::kAppendFrameOriginToNetworkIsolationKey);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(NetworkIsolationKeyMojomTraitsWithFrameOriginTest,
       SerializeAndDeserialize) {
  std::vector<net::NetworkIsolationKey> keys = {
      net::NetworkIsolationKey(), net::NetworkIsolationKey::CreateTransient(),
      net::NetworkIsolationKey::CreateOpaqueAndNonTransient(),
      net::NetworkIsolationKey(url::Origin::Create(GURL("http://a.test/")),
                               url::Origin::Create(GURL("http://b.test/")))};

  for (auto original : keys) {
    SCOPED_TRACE(original.ToDebugString());
    net::NetworkIsolationKey copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                network::mojom::NetworkIsolationKey>(&original, &copied));
    EXPECT_EQ(original, copied);
    EXPECT_EQ(original.GetTopFrameOrigin(), copied.GetTopFrameOrigin());
    EXPECT_EQ(original.GetFrameOrigin(), copied.GetFrameOrigin());
    EXPECT_EQ(original.IsTransient(), copied.IsTransient());
  }

  // Test case where registerable domain does not match origin passed in to
  // NetworkIsolationKey's constructor.

  url::Origin origin_a = url::Origin::Create(GURL("http://a.foo.test/"));
  url::Origin origin_b = url::Origin::Create(GURL("http://b.foo.test/"));
  url::Origin domain = url::Origin::Create(GURL("http://foo.test/"));
  net::NetworkIsolationKey original(origin_a, origin_b);
  EXPECT_EQ(origin_a, original.GetTopFrameOrigin());
  EXPECT_EQ(origin_b, original.GetFrameOrigin());

  net::NetworkIsolationKey copied;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<network::mojom::NetworkIsolationKey>(
          &original, &copied));
  EXPECT_EQ(original, copied);

  EXPECT_EQ(origin_a, copied.GetTopFrameOrigin());
  EXPECT_EQ(origin_b, copied.GetFrameOrigin());
}

}  // namespace mojo
