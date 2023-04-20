// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_storage_isolation_key.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {
const GURL kUrl1("https://origin1.test/");
const net::SchemefulSite kSite1(GURL("https://origin1.test/"));
const net::SchemefulSite kSite2(GURL("https://origin2.test/"));
}  // namespace

TEST(SharedDictionaryStorageIsolationKey, OpaqueOrigin) {
  const absl::optional<SharedDictionaryStorageIsolationKey> isolation_key =
      SharedDictionaryStorageIsolationKey::MaybeCreate(
          url::Origin::Create(kUrl1).DeriveNewOpaqueOrigin(),
          net::NetworkIsolationKey(kSite1, kSite1));
  EXPECT_FALSE(isolation_key);
}

TEST(SharedDictionaryStorageIsolationKey, TransientNetworkIsolationKey) {
  const absl::optional<SharedDictionaryStorageIsolationKey> isolation_key =
      SharedDictionaryStorageIsolationKey::MaybeCreate(
          url::Origin::Create(kUrl1),
          net::NetworkIsolationKey(kSite1, kSite1,
                                   base::UnguessableToken::Create()));
  EXPECT_FALSE(isolation_key);
}

TEST(SharedDictionaryStorageIsolationKey, SameOriginSameNetworkIsokationKey) {
  const absl::optional<SharedDictionaryStorageIsolationKey> isolation_key1 =
      SharedDictionaryStorageIsolationKey::MaybeCreate(
          url::Origin::Create(kUrl1), net::NetworkIsolationKey(kSite1, kSite1));
  const absl::optional<SharedDictionaryStorageIsolationKey> isolation_key2 =
      SharedDictionaryStorageIsolationKey::MaybeCreate(
          url::Origin::Create(kUrl1), net::NetworkIsolationKey(kSite1, kSite1));
  ASSERT_TRUE(isolation_key1);
  ASSERT_TRUE(isolation_key2);
  EXPECT_EQ(*isolation_key1, *isolation_key2);
}

TEST(SharedDictionaryStorageIsolationKey,
     DifferentOriginSameNetworkIsokationKey) {
  const absl::optional<SharedDictionaryStorageIsolationKey> isolation_key1 =
      SharedDictionaryStorageIsolationKey::MaybeCreate(
          url::Origin::Create(GURL("https://www1.origin1.test/")),
          net::NetworkIsolationKey(kSite1, kSite1));
  const absl::optional<SharedDictionaryStorageIsolationKey> isolation_key2 =
      SharedDictionaryStorageIsolationKey::MaybeCreate(
          url::Origin::Create(GURL("https://www2.origin1.test/")),
          net::NetworkIsolationKey(kSite1, kSite1));
  ASSERT_TRUE(isolation_key1);
  ASSERT_TRUE(isolation_key2);
  EXPECT_NE(*isolation_key1, *isolation_key2);
}

TEST(SharedDictionaryStorageIsolationKey,
     SameOriginDifferentNetworkIsokationKey) {
  const absl::optional<SharedDictionaryStorageIsolationKey> isolation_key1 =
      SharedDictionaryStorageIsolationKey::MaybeCreate(
          url::Origin::Create(kUrl1), net::NetworkIsolationKey(kSite1, kSite1));
  const absl::optional<SharedDictionaryStorageIsolationKey> isolation_key2 =
      SharedDictionaryStorageIsolationKey::MaybeCreate(
          url::Origin::Create(kUrl1), net::NetworkIsolationKey(kSite2, kSite1));
  ASSERT_TRUE(isolation_key1);
  ASSERT_TRUE(isolation_key2);
  EXPECT_NE(*isolation_key1, *isolation_key2);
}

}  // namespace network
