// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_anonymization_key_mojom_traits.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "services/network/public/mojom/network_anonymization_key.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace mojo {

// TODO(crbug.com/40870601): Test is failing.
TEST(NetworkAnonymizationKeyMojomTraitsTest,
     SerializeAndDeserializeDoubleKeyWithCrossSiteFlag) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  std::vector<net::NetworkAnonymizationKey> keys = {
      net::NetworkAnonymizationKey(),
      net::NetworkAnonymizationKey::CreateTransient(),
      net::NetworkAnonymizationKey::CreateFromParts(
          net::SchemefulSite(GURL("http://a.test/")), /*is_cross_site=*/true,
          token),
      net::NetworkAnonymizationKey::CreateCrossSite(
          net::SchemefulSite(GURL("http://a.test/"))),
  };
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
