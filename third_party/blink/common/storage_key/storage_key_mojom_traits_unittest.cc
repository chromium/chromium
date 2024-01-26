// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key_mojom_traits.h"

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

namespace mojom {
class StorageKey;
}  // namespace mojom

namespace {

TEST(StorageKeyMojomTraitsTest, SerializeAndDeserialize) {
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);
    StorageKey test_keys[] = {
        StorageKey::CreateFromStringForTesting("https://example.com"),
        StorageKey::CreateFromStringForTesting("http://example.com"),
        StorageKey::CreateFromStringForTesting("https://example.test"),
        StorageKey::CreateFromStringForTesting("https://sub.example.com"),
        StorageKey::CreateFromStringForTesting("http://sub2.example.com"),
        StorageKey::Create(url::Origin::Create(GURL("https://example.com")),
                           net::SchemefulSite(GURL("https://example.com")),
                           blink::mojom::AncestorChainBit::kSameSite),
        StorageKey::Create(url::Origin::Create(GURL("http://example.com")),
                           net::SchemefulSite(GURL("https://example2.com")),
                           blink::mojom::AncestorChainBit::kCrossSite),
        StorageKey::Create(url::Origin::Create(GURL("https://example.test")),
                           net::SchemefulSite(GURL("https://example.com")),
                           blink::mojom::AncestorChainBit::kCrossSite),
        StorageKey::Create(url::Origin::Create(GURL("https://sub.example.com")),
                           net::SchemefulSite(GURL("https://example2.com")),
                           blink::mojom::AncestorChainBit::kCrossSite),
        StorageKey::Create(url::Origin::Create(GURL("http://sub2.example.com")),
                           net::SchemefulSite(GURL("https://example.com")),
                           blink::mojom::AncestorChainBit::kCrossSite),
        StorageKey::CreateFirstParty(url::Origin()),
        StorageKey::CreateWithNonce(
            url::Origin::Create(GURL("https://.example.com")),
            base::UnguessableToken::Create()),
        StorageKey::CreateWithNonce(url::Origin(),
                                    base::UnguessableToken::Create()),
        StorageKey::Create(url::Origin::Create(GURL("http://sub2.example.com")),
                           net::SchemefulSite(url::Origin::Create(
                               GURL("https://example.com"))),
                           blink::mojom::AncestorChainBit::kCrossSite),
        StorageKey::Create(url::Origin(), net::SchemefulSite(),
                           blink::mojom::AncestorChainBit::kCrossSite),
        StorageKey::Create(url::Origin::Create(GURL("http://example.com")),
                           net::SchemefulSite(),
                           blink::mojom::AncestorChainBit::kCrossSite),
    };

    for (auto& original : test_keys) {
      StorageKey copied;
      EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::StorageKey>(
          original, copied));
      EXPECT_TRUE(original.ExactMatchForTesting(copied));
    }
  }
}

}  // namespace
}  // namespace blink
