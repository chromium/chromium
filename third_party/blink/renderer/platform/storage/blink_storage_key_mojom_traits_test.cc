// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/storage/blink_storage_key_mojom_traits.h"

#include "base/unguessable_token.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/storage_key/storage_key_mojom_traits.h"
#include "third_party/blink/public/mojom/storage_key/storage_key.mojom-blink.h"
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

  Vector<BlinkStorageKey> keys = {
      BlinkStorageKey(),
      BlinkStorageKey(origin1),
      BlinkStorageKey(origin2),
      BlinkStorageKey(origin3),
      BlinkStorageKey(origin4),
      BlinkStorageKey::CreateWithNonce(origin1, nonce),
      BlinkStorageKey::CreateWithNonce(origin2, nonce),
  };

  for (BlinkStorageKey& key : keys) {
    BlinkStorageKey copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::blink::StorageKey>(
        key, copied));
    EXPECT_EQ(key, copied);
    EXPECT_TRUE(key.GetSecurityOrigin()->IsSameOriginWith(
        copied.GetSecurityOrigin().get()));
    EXPECT_EQ(key.GetNonce(), copied.GetNonce());
  }
}

// Tests serializing from blink::StorageKey and deserializing to
// blink::BlinkStorageKey.
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

  Vector<StorageKey> storage_keys = {
      StorageKey(url_origin1),
      StorageKey(url_origin2),
      StorageKey(url_origin3),
      StorageKey(url_origin4),
      StorageKey::CreateWithNonce(url_origin1, nonce),
      StorageKey::CreateWithNonce(url_origin2, nonce),
  };
  Vector<BlinkStorageKey> blink_storage_keys = {
      BlinkStorageKey(origin1),
      BlinkStorageKey(origin2),
      BlinkStorageKey(origin3),
      BlinkStorageKey(origin4),
      BlinkStorageKey::CreateWithNonce(origin1, nonce),
      BlinkStorageKey::CreateWithNonce(origin2, nonce),
  };

  for (size_t i = 0; i < storage_keys.size(); ++i) {
    auto serialized = mojom::StorageKey::Serialize(&storage_keys[i]);

    BlinkStorageKey deserialized;
    EXPECT_TRUE(mojom::StorageKey::Deserialize(serialized, &deserialized));
    EXPECT_EQ(blink_storage_keys[i], deserialized);
  }
}

// Tests serializing from blink::StorageKey and deserializing to
// blink::BlinkStorageKey.
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

  Vector<StorageKey> storage_keys = {
      StorageKey(url_origin1),
      StorageKey(url_origin2),
      StorageKey(url_origin3),
      StorageKey(url_origin4),
      StorageKey::CreateWithNonce(url_origin1, nonce),
      StorageKey::CreateWithNonce(url_origin2, nonce),
  };

  Vector<BlinkStorageKey> blink_storage_keys = {
      BlinkStorageKey(origin1),
      BlinkStorageKey(origin2),
      BlinkStorageKey(origin3),
      BlinkStorageKey(origin4),
      BlinkStorageKey::CreateWithNonce(origin1, nonce),
      BlinkStorageKey::CreateWithNonce(origin2, nonce),
  };

  for (size_t i = 0; i < storage_keys.size(); ++i) {
    auto serialized = mojom::StorageKey::Serialize(&blink_storage_keys[i]);

    StorageKey deserialized;
    EXPECT_TRUE(mojom::StorageKey::Deserialize(serialized, &deserialized));
    EXPECT_EQ(storage_keys[i], deserialized);
  }
}

}  // namespace blink
