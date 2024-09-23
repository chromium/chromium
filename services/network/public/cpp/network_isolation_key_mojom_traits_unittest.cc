// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_isolation_key_mojom_traits.h"

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/mojom/network_isolation_key.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace mojo {

TEST(NetworkIsolationKeyMojomTraitsTest, SerializeAndDeserialize) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  std::vector<net::NetworkIsolationKey> keys = {
      net::NetworkIsolationKey(),
      net::NetworkIsolationKey::CreateTransientForTesting(),
      net::NetworkIsolationKey(net::SchemefulSite(GURL("http://a.test/")),
                               net::SchemefulSite()),
      net::NetworkIsolationKey(net::SchemefulSite(GURL("http://a.test/")),
                               net::SchemefulSite(GURL("http://b.test/"))),
      net::NetworkIsolationKey(net::SchemefulSite(GURL("http://foo.a.test/")),
                               net::SchemefulSite(GURL("http://bar.b.test/"))),
      net::NetworkIsolationKey(net::SchemefulSite(GURL("http://a.test/")),
                               net::SchemefulSite(GURL("http://b.test/")),
                               token),
      net::NetworkIsolationKey(net::SchemefulSite(GURL("http://foo.a.test/")),
                               net::SchemefulSite(GURL("http://bar.b.test/")),
                               token)};

  for (auto original : keys) {
    SCOPED_TRACE(original.ToDebugString());
    net::NetworkIsolationKey copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                network::mojom::NetworkIsolationKey>(original, copied));
    EXPECT_EQ(original, copied);
    EXPECT_EQ(original.GetTopFrameSite(), copied.GetTopFrameSite());
    EXPECT_EQ(original.GetFrameSiteForTesting(),
              copied.GetFrameSiteForTesting());
    EXPECT_EQ(original.IsTransient(), copied.IsTransient());
  }
}

}  // namespace mojo
