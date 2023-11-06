// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_isolation_key_mojom_traits.h"

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/network_isolation_key.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace mojo {

class NetworkIsolationKeyMojomTraitsTestWithNikMode
    : public testing::Test,
      public testing::WithParamInterface<net::NetworkIsolationKey::Mode> {
 public:
  NetworkIsolationKeyMojomTraitsTestWithNikMode() {
    switch (GetParam()) {
      case net::NetworkIsolationKey::Mode::kFrameSiteEnabled:
        scoped_feature_list_.InitWithFeatures(
            {},
            {net::features::kEnableCrossSiteFlagNetworkIsolationKey,
             net::features::kEnableFrameSiteSharedOpaqueNetworkIsolationKey});
        break;

      case net::NetworkIsolationKey::Mode::kFrameSiteWithSharedOpaqueEnabled:
        scoped_feature_list_.InitWithFeatures(
            {net::features::kEnableFrameSiteSharedOpaqueNetworkIsolationKey},
            {
                net::features::kEnableCrossSiteFlagNetworkIsolationKey,
            });
        break;

      case net::NetworkIsolationKey::Mode::kCrossSiteFlagEnabled:
        scoped_feature_list_.InitWithFeatures(
            {net::features::kEnableCrossSiteFlagNetworkIsolationKey},
            {net::features::kEnableFrameSiteSharedOpaqueNetworkIsolationKey});
        break;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    Tests,
    NetworkIsolationKeyMojomTraitsTestWithNikMode,
    testing::ValuesIn(
        {net::NetworkIsolationKey::Mode::kFrameSiteEnabled,
         net::NetworkIsolationKey::Mode::kCrossSiteFlagEnabled,
         net::NetworkIsolationKey::Mode::kFrameSiteWithSharedOpaqueEnabled}),
    [](const testing::TestParamInfo<net::NetworkIsolationKey::Mode>& info) {
      switch (info.param) {
        case net::NetworkIsolationKey::Mode::kFrameSiteEnabled:
          return "FrameSiteEnabled";
        case net::NetworkIsolationKey::Mode::kCrossSiteFlagEnabled:
          return "CrossSiteFlagEnabled";
        case net::NetworkIsolationKey::Mode::kFrameSiteWithSharedOpaqueEnabled:
          return "FrameSiteSharedOpaqueEnabled";
      }
    });

TEST_P(NetworkIsolationKeyMojomTraitsTestWithNikMode, SerializeAndDeserialize) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  std::vector<net::NetworkIsolationKey> keys = {
      net::NetworkIsolationKey(),
      net::NetworkIsolationKey::CreateTransientForTesting(),
      net::NetworkIsolationKey(url::Origin::Create(GURL("http://a.test/")),
                               url::Origin()),
      net::NetworkIsolationKey(url::Origin::Create(GURL("http://a.test/")),
                               url::Origin::Create(GURL("http://b.test/"))),
      net::NetworkIsolationKey(url::Origin::Create(GURL("http://foo.a.test/")),
                               url::Origin::Create(GURL("http://bar.b.test/"))),
      net::NetworkIsolationKey(
          net::SchemefulSite(url::Origin::Create(GURL("http://a.test/"))),
          net::SchemefulSite(url::Origin::Create(GURL("http://b.test/"))),
          token),
      net::NetworkIsolationKey(
          net::SchemefulSite(url::Origin::Create(GURL("http://foo.a.test/"))),
          net::SchemefulSite(url::Origin::Create(GURL("http://bar.b.test/"))),
          token)};

  for (auto original : keys) {
    SCOPED_TRACE(original.ToDebugString());
    net::NetworkIsolationKey copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                network::mojom::NetworkIsolationKey>(original, copied));
    EXPECT_EQ(original, copied);
    EXPECT_EQ(original.GetTopFrameSite(), copied.GetTopFrameSite());
    switch (net::NetworkIsolationKey::GetMode()) {
      case net::NetworkIsolationKey::Mode::kFrameSiteEnabled:
      case net::NetworkIsolationKey::Mode::kFrameSiteWithSharedOpaqueEnabled:
        EXPECT_EQ(original.GetFrameSiteForTesting(),
                  copied.GetFrameSiteForTesting());
        break;
      case net::NetworkIsolationKey::Mode::kCrossSiteFlagEnabled:
        EXPECT_EQ(original.GetIsCrossSiteForTesting(),
                  copied.GetIsCrossSiteForTesting());
        break;
    }
    EXPECT_EQ(original.IsTransient(), copied.IsTransient());
  }
}

}  // namespace mojo
