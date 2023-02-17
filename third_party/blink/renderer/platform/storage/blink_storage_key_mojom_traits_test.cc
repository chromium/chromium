// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/storage/blink_storage_key_mojom_traits.h"

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/storage_key/storage_key_mojom_traits.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom-blink.h"
#include "third_party/blink/public/mojom/storage_key/storage_key.mojom-blink.h"
#include "third_party/blink/renderer/platform/network/blink_schemeful_site.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

TEST(BlinkStorageKeyMojomTraitsTest, SerializeAndDeserialize_BlinkStorageKey) {
  scoped_refptr<const SecurityOrigin> origin1 =
      SecurityOrigin::CreateUniqueOpaque();
  scoped_refptr<const SecurityOrigin> origin2 =
      SecurityOrigin::CreateFromString("http://example.site");
  scoped_refptr<const SecurityOrigin> origin3 =
      SecurityOrigin::CreateFromString("https://example.site");
  scoped_refptr<const SecurityOrigin> origin4 =
      SecurityOrigin::CreateFromString("file:///path/to/file");
  base::UnguessableToken nonce = base::UnguessableToken::Create();

  blink::BlinkSchemefulSite site1 = blink::BlinkSchemefulSite(origin1);
  blink::BlinkSchemefulSite site2 = blink::BlinkSchemefulSite(origin2);

  Vector<BlinkStorageKey> keys = {
      BlinkStorageKey(),
      BlinkStorageKey::CreateFirstParty(origin1),
      BlinkStorageKey::CreateFirstParty(origin2),
      BlinkStorageKey::CreateFirstParty(origin3),
      BlinkStorageKey::CreateFirstParty(origin4),
      BlinkStorageKey::Create(origin1, site1,
                              mojom::blink::AncestorChainBit::kCrossSite),
      BlinkStorageKey::Create(origin2, site1,
                              mojom::blink::AncestorChainBit::kCrossSite),
      BlinkStorageKey::Create(origin3, site2,
                              mojom::blink::AncestorChainBit::kCrossSite),
      BlinkStorageKey::Create(origin4, site2,
                              mojom::blink::AncestorChainBit::kCrossSite),
      BlinkStorageKey::CreateWithNonce(origin1, nonce),
      BlinkStorageKey::CreateWithNonce(origin2, nonce),
      BlinkStorageKey::Create(origin2, site2,
                              mojom::blink::AncestorChainBit::kCrossSite),
      BlinkStorageKey::Create(origin1, BlinkSchemefulSite(),
                              mojom::blink::AncestorChainBit::kCrossSite),
      BlinkStorageKey::Create(origin2, BlinkSchemefulSite(),
                              mojom::blink::AncestorChainBit::kCrossSite),
  };

  for (BlinkStorageKey& key : keys) {
    BlinkStorageKey copied;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::StorageKey>(key, copied));
    EXPECT_EQ(key, copied);
    EXPECT_TRUE(key.GetSecurityOrigin()->IsSameOriginWith(
        copied.GetSecurityOrigin().get()));
    EXPECT_EQ(key.GetTopLevelSite(), copied.GetTopLevelSite());
    EXPECT_EQ(key.GetNonce(), copied.GetNonce());
    EXPECT_EQ(key.GetAncestorChainBit(), copied.GetAncestorChainBit());
  }
}

// Tests serializing from StorageKey and deserializing to BlinkStorageKey.
TEST(BlinkStorageKeyMojomTraitsTest,
     SerializeFromStorageKey_DeserializeToBlinkStorageKey) {
  scoped_refptr<const SecurityOrigin> origin1 =
      SecurityOrigin::CreateUniqueOpaque();
  scoped_refptr<const SecurityOrigin> origin2 =
      SecurityOrigin::CreateFromString("http://example.site");
  scoped_refptr<const SecurityOrigin> origin3 =
      SecurityOrigin::CreateFromString("https://example.site");
  scoped_refptr<const SecurityOrigin> origin4 =
      SecurityOrigin::CreateFromString("file:///path/to/file");
  base::UnguessableToken nonce = base::UnguessableToken::Create();

  url::Origin url_origin1 = origin1->ToUrlOrigin();
  url::Origin url_origin2 = origin2->ToUrlOrigin();
  url::Origin url_origin3 = origin3->ToUrlOrigin();
  url::Origin url_origin4 = origin4->ToUrlOrigin();

  blink::BlinkSchemefulSite blink_site1 = blink::BlinkSchemefulSite(origin1);
  blink::BlinkSchemefulSite blink_site2 = blink::BlinkSchemefulSite(origin2);

  net::SchemefulSite net_site1 = net::SchemefulSite(url_origin1);
  net::SchemefulSite net_site2 = net::SchemefulSite(url_origin2);

  Vector<StorageKey> storage_keys = {
      StorageKey::CreateFirstParty(url_origin1),
      StorageKey::CreateFirstParty(url_origin2),
      StorageKey::CreateFirstParty(url_origin3),
      StorageKey::CreateFirstParty(url_origin4),
      StorageKey::Create(url_origin3, net_site2,
                         mojom::blink::AncestorChainBit::kCrossSite),
      StorageKey::Create(url_origin4, net_site2,
                         mojom::blink::AncestorChainBit::kCrossSite),
      StorageKey::CreateWithNonce(url_origin1, nonce),
      StorageKey::CreateWithNonce(url_origin2, nonce),
      StorageKey::Create(url_origin2, net_site2,
                         mojom::blink::AncestorChainBit::kCrossSite),
      StorageKey::Create(url_origin1, net_site1,
                         mojom::blink::AncestorChainBit::kCrossSite),
      StorageKey::Create(url_origin2, net_site1,
                         mojom::blink::AncestorChainBit::kCrossSite),
  };
  Vector<BlinkStorageKey> blink_storage_keys = {
      BlinkStorageKey::CreateFirstParty(origin1),
      BlinkStorageKey::CreateFirstParty(origin2),
      BlinkStorageKey::CreateFirstParty(origin3),
      BlinkStorageKey::CreateFirstParty(origin4),
      BlinkStorageKey::Create(origin3, blink_site2,
                              mojom::blink::AncestorChainBit::kCrossSite),
      BlinkStorageKey::Create(origin4, blink_site2,
                              mojom::blink::AncestorChainBit::kCrossSite),
      BlinkStorageKey::CreateWithNonce(origin1, nonce),
      BlinkStorageKey::CreateWithNonce(origin2, nonce),
      BlinkStorageKey::Create(origin2, blink_site2,
                              mojom::blink::AncestorChainBit::kCrossSite),
      BlinkStorageKey::Create(origin1, blink_site1,
                              mojom::blink::AncestorChainBit::kCrossSite),
      BlinkStorageKey::Create(origin2, blink_site1,
                              mojom::blink::AncestorChainBit::kCrossSite),
  };

  for (size_t i = 0; i < storage_keys.size(); ++i) {
    auto serialized = mojom::StorageKey::Serialize(&storage_keys[i]);

    BlinkStorageKey deserialized;
    EXPECT_TRUE(mojom::StorageKey::Deserialize(serialized, &deserialized));
    EXPECT_EQ(blink_storage_keys[i], deserialized);
    // The top_level_site doesn't factor into comparisons unless
    // features::kThirdPartyStoragePartitioning is enabled. Since we want
    // to see if the field is correct or not let's check it here.
    EXPECT_EQ(blink_storage_keys[i].GetTopLevelSite(),
              deserialized.GetTopLevelSite());
  }
}

// Tests serializing from BlinkStorageKey and deserializing to StorageKey.
TEST(BlinkStorageKeyMojomTraitsTest,
     SerializeFromBlinkStorageKey_DeserializeToStorageKey) {
  url::Origin url_origin1;
  url::Origin url_origin2 = url::Origin::Create(GURL("http://example.site"));
  url::Origin url_origin3 = url::Origin::Create(GURL("https://example.site"));
  url::Origin url_origin4 = url::Origin::Create(GURL("file:///path/to/file"));
  base::UnguessableToken nonce = base::UnguessableToken::Create();

  scoped_refptr<const SecurityOrigin> origin1 =
      SecurityOrigin::CreateFromUrlOrigin(url_origin1);
  scoped_refptr<const SecurityOrigin> origin2 =
      SecurityOrigin::CreateFromUrlOrigin(url_origin2);
  scoped_refptr<const SecurityOrigin> origin3 =
      SecurityOrigin::CreateFromUrlOrigin(url_origin3);
  scoped_refptr<const SecurityOrigin> origin4 =
      SecurityOrigin::CreateFromUrlOrigin(url_origin4);

  blink::BlinkSchemefulSite blink_site1 = blink::BlinkSchemefulSite(origin1);
  blink::BlinkSchemefulSite blink_site2 = blink::BlinkSchemefulSite(origin2);

  net::SchemefulSite net_site1 = net::SchemefulSite(url_origin1);
  net::SchemefulSite net_site2 = net::SchemefulSite(url_origin2);

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);
    Vector<StorageKey> storage_keys = {
        StorageKey::CreateFirstParty(url_origin1),
        StorageKey::CreateFirstParty(url_origin2),
        StorageKey::CreateFirstParty(url_origin3),
        StorageKey::CreateFirstParty(url_origin4),
        StorageKey::Create(url_origin3, net_site2,
                           mojom::blink::AncestorChainBit::kCrossSite),
        StorageKey::Create(url_origin4, net_site2,
                           mojom::blink::AncestorChainBit::kCrossSite),
        StorageKey::CreateWithNonce(url_origin1, nonce),
        StorageKey::CreateWithNonce(url_origin2, nonce),
        StorageKey::Create(url_origin2, net_site2,
                           mojom::blink::AncestorChainBit::kCrossSite),
        StorageKey::Create(url_origin1, net_site1,
                           mojom::blink::AncestorChainBit::kCrossSite),
        StorageKey::Create(url_origin2, net_site1,
                           mojom::blink::AncestorChainBit::kCrossSite),
    };

    Vector<BlinkStorageKey> blink_storage_keys = {
        BlinkStorageKey::CreateFirstParty(origin1),
        BlinkStorageKey::CreateFirstParty(origin2),
        BlinkStorageKey::CreateFirstParty(origin3),
        BlinkStorageKey::CreateFirstParty(origin4),
        BlinkStorageKey::Create(origin3, blink_site2,
                                mojom::blink::AncestorChainBit::kCrossSite),
        BlinkStorageKey::Create(origin4, blink_site2,
                                mojom::blink::AncestorChainBit::kCrossSite),
        BlinkStorageKey::CreateWithNonce(origin1, nonce),
        BlinkStorageKey::CreateWithNonce(origin2, nonce),
        BlinkStorageKey::Create(origin2, blink_site2,
                                mojom::blink::AncestorChainBit::kCrossSite),
        BlinkStorageKey::Create(origin1, blink_site1,
                                mojom::blink::AncestorChainBit::kCrossSite),
        BlinkStorageKey::Create(origin2, blink_site1,
                                mojom::blink::AncestorChainBit::kCrossSite),
    };

    for (size_t i = 0; i < storage_keys.size(); ++i) {
      auto serialized = mojom::StorageKey::Serialize(&blink_storage_keys[i]);

      StorageKey deserialized;
      EXPECT_TRUE(mojom::StorageKey::Deserialize(serialized, &deserialized));
      EXPECT_EQ(storage_keys[i], deserialized);

      // Ensure the comparison works if `kThirdPartyStoragePartitioning` is
      // force enabled. This verifies `top_level_site_` and
      // `ancestor_chain_bit_`.
      EXPECT_EQ(
          storage_keys[i].CopyWithForceEnabledThirdPartyStoragePartitioning(),
          deserialized.CopyWithForceEnabledThirdPartyStoragePartitioning());
    }
  }
}

}  // namespace blink
