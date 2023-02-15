// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/database_helpers.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {
namespace background_fetch {
namespace {

constexpr char kExampleUniqueId[] = "7e57ab1e-c0de-a150-ca75-1e75f005ba11";

bool CacheUrlRoundTrip(const std::string& url) {
  GURL gurl(url);
  GURL round_trip_url = RemoveUniqueParamFromCacheURL(
      MakeCacheUrlUnique(gurl, kExampleUniqueId, 0), kExampleUniqueId);
  return round_trip_url == gurl;
}

TEST(BackgroundFetchDatabaseHelpers, CacheUrlRoundTrip) {
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com/"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com?a=b"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com?a=b&c=d"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com/path"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com/path/"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com/path1/path2"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com/path?a=b&c=d"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com/path/?a=b&c=d"));
}

TEST(BackgroundFetchDatabaseHelpers, GetMetadataStorageKeyFromStorageKey) {
  const blink::StorageKey key =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");
  proto::BackgroundFetchMetadata metadata_proto;
  metadata_proto.set_storage_key(key.Serialize());
  EXPECT_EQ(GetMetadataStorageKey(metadata_proto).Serialize(), key.Serialize());
}

TEST(BackgroundFetchDatabaseHelpers, GetMetadataStorageKeyFromOrigin) {
  auto origin = url::Origin::Create(GURL("http://example.com"));
  const blink::StorageKey key = blink::StorageKey::CreateFirstParty(origin);
  proto::BackgroundFetchMetadata metadata_proto;
  metadata_proto.set_origin(origin.Serialize());
  EXPECT_EQ(GetMetadataStorageKey(metadata_proto).Serialize(), key.Serialize());
}

TEST(BackgroundFetchDatabaseHelpers, GetMetadataStorageKeyNoData) {
  proto::BackgroundFetchMetadata metadata_proto;
  EXPECT_TRUE(GetMetadataStorageKey(metadata_proto).origin().opaque());
}

}  // namespace
}  // namespace background_fetch
}  // namespace content
