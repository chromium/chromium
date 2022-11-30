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
    EXPECT_EQ(original.GetFrameSite(), copied.GetFrameSite());
    EXPECT_EQ(original.IsTransient(), copied.IsTransient());
  }
}

}  // namespace mojo
