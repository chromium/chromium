// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/shared_dictionary_isolation_key_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/mojom/shared_dictionary_isolation_key.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
namespace {

TEST(SharedDictionaryIsolationKeyMojomTraitsTest, SerializeAndDeserialize) {
  std::vector<net::SharedDictionaryIsolationKey> keys = {
      net::SharedDictionaryIsolationKey(
          url::Origin::Create(GURL("https://a.test")),
          net::SchemefulSite(url::Origin::Create(GURL("https://a.test")))),
      net::SharedDictionaryIsolationKey(
          url::Origin::Create(GURL("https://a.test")),
          net::SchemefulSite(url::Origin::Create(GURL("https://b.test")))),
  };

  for (const auto& original : keys) {
    net::SharedDictionaryIsolationKey copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                mojom::SharedDictionaryIsolationKey>(original, copied))
        << "frame_origin: " << original.frame_origin()
        << ", top_frame_site: " << original.top_frame_site();
    EXPECT_EQ(original, copied);
  }
}

TEST(SharedDictionaryIsolationKeyMojomTraitsTest,
     SerializeFromMojomPtrAndDeserialize) {
  mojom::SharedDictionaryIsolationKeyPtr key =
      mojom::SharedDictionaryIsolationKey::New();
  key->frame_origin = url::Origin::Create(GURL("https://a.test"));
  key->top_frame_site =
      net::SchemefulSite(url::Origin::Create(GURL("https://b.test")));

  std::vector<uint8_t> serialized =
      mojom::SharedDictionaryIsolationKey::Serialize(&key);
  net::SharedDictionaryIsolationKey deserialized;
  EXPECT_TRUE(mojom::SharedDictionaryIsolationKey::Deserialize(serialized,
                                                               &deserialized));
  EXPECT_EQ(
      net::SharedDictionaryIsolationKey(
          url::Origin::Create(GURL("https://a.test")),
          net::SchemefulSite(url::Origin::Create(GURL("https://b.test")))),
      deserialized);
}

TEST(SharedDictionaryIsolationKeyMojomTraitsTest, OpaqueFrameOrigin) {
  mojom::SharedDictionaryIsolationKeyPtr key =
      mojom::SharedDictionaryIsolationKey::New();
  key->top_frame_site =
      net::SchemefulSite(url::Origin::Create(GURL("https://a.test")));

  std::vector<uint8_t> serialized =
      mojom::SharedDictionaryIsolationKey::Serialize(&key);
  net::SharedDictionaryIsolationKey deserialized;
  EXPECT_FALSE(mojom::SharedDictionaryIsolationKey::Deserialize(serialized,
                                                                &deserialized));
}

TEST(SharedDictionaryIsolationKeyMojomTraitsTest, OpaqueTopFrameSite) {
  mojom::SharedDictionaryIsolationKeyPtr key =
      mojom::SharedDictionaryIsolationKey::New();
  key->frame_origin = url::Origin::Create(GURL("https://a.test"));

  std::vector<uint8_t> serialized =
      mojom::SharedDictionaryIsolationKey::Serialize(&key);
  net::SharedDictionaryIsolationKey deserialized;
  EXPECT_FALSE(mojom::SharedDictionaryIsolationKey::Deserialize(serialized,
                                                                &deserialized));
}

TEST(SharedDictionaryIsolationKeyMojomTraitsTest,
     OpaqueFrameOriginOpaqueTopFrameSite) {
  mojom::SharedDictionaryIsolationKeyPtr key =
      mojom::SharedDictionaryIsolationKey::New();

  std::vector<uint8_t> serialized =
      mojom::SharedDictionaryIsolationKey::Serialize(&key);
  net::SharedDictionaryIsolationKey deserialized;
  EXPECT_FALSE(mojom::SharedDictionaryIsolationKey::Deserialize(serialized,
                                                                &deserialized));
}

}  // namespace
}  // namespace network
