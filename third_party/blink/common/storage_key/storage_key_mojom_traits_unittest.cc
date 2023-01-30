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

namespace mojom {
class StorageKey;
}  // namespace mojom

namespace blink {
namespace {

TEST(StorageKeyMojomTraitsTest, SerializeAndDeserialize) {
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);
    StorageKey test_keys[] = {
        StorageKey(url::Origin::Create(GURL("https://example.com"))),
        StorageKey(url::Origin::Create(GURL("http://example.com"))),
        StorageKey(url::Origin::Create(GURL("https://example.test"))),
        StorageKey(url::Origin::Create(GURL("https://sub.example.com"))),
        StorageKey(url::Origin::Create(GURL("http://sub2.example.com"))),
        StorageKey::CreateForTesting(
            url::Origin::Create(GURL("https://example.com")),
            url::Origin::Create(GURL("https://example.com"))),
        StorageKey::CreateForTesting(
            url::Origin::Create(GURL("http://example.com")),
            url::Origin::Create(GURL("https://example2.com"))),
        StorageKey::CreateForTesting(
            url::Origin::Create(GURL("https://example.test")),
            url::Origin::Create(GURL("https://example.com"))),
        StorageKey::CreateForTesting(
            url::Origin::Create(GURL("https://sub.example.com")),
            url::Origin::Create(GURL("https://example2.com"))),
        StorageKey::CreateForTesting(
            url::Origin::Create(GURL("http://sub2.example.com")),
            url::Origin::Create(GURL("https://example.com"))),
        StorageKey(url::Origin()),
        StorageKey::CreateWithNonceForTesting(
            url::Origin::Create(GURL("https://.example.com")),
            base::UnguessableToken::Create()),
        StorageKey::CreateWithNonceForTesting(url::Origin(),
                                              base::UnguessableToken::Create()),
        StorageKey::CreateWithOptionalNonce(
            url::Origin::Create(GURL("http://sub2.example.com")),
            net::SchemefulSite(
                url::Origin::Create(GURL("https://example.com"))),
            nullptr, blink::mojom::AncestorChainBit::kCrossSite),
        StorageKey::CreateWithOptionalNonce(
            url::Origin(), net::SchemefulSite(), nullptr,
            blink::mojom::AncestorChainBit::kSameSite),
        StorageKey::CreateWithOptionalNonce(
            url::Origin::Create(GURL("http://example.com")),
            net::SchemefulSite(), nullptr,
            blink::mojom::AncestorChainBit::kSameSite),
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
