// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_index.h"

#include <list>
#include <optional>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

constexpr char16_t kUnicodeHighSurrogate = 0xD800;

TEST(CacheStorageIndexTest, TestDefaultConstructor) {
  CacheStorageIndex index;
  EXPECT_EQ(0u, index.num_entries());
  EXPECT_TRUE(index.ordered_cache_metadata().empty());
  EXPECT_EQ(base::ByteSize(0u), index.GetPaddedStorageSize());
}

TEST(CacheStorageIndexTest, UnknownSizeAndPadding) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata(u"foo", std::nullopt,
                                                base::ByteSize(2)));
  EXPECT_EQ(std::nullopt, index.GetPaddedStorageSize());

  EXPECT_TRUE(index.SetCacheSize(u"foo", base::ByteSize(12)));
  EXPECT_EQ(base::ByteSize(14), index.GetPaddedStorageSize());
  EXPECT_TRUE(index.SetCachePadding(u"foo", std::nullopt));
  EXPECT_EQ(std::nullopt, index.GetPaddedStorageSize());
  EXPECT_FALSE(index.SetCachePadding(u"foo", std::nullopt));

  index.DoomCache(u"foo");
  EXPECT_EQ(base::ByteSize(), index.GetPaddedStorageSize());
  index.RestoreDoomedCache();
  EXPECT_EQ(std::nullopt, index.GetPaddedStorageSize());

  EXPECT_TRUE(index.SetCacheSize(u"foo", std::nullopt));
  EXPECT_FALSE(index.SetCacheSize(u"foo", std::nullopt));
  EXPECT_TRUE(index.SetCachePadding(u"foo", base::ByteSize()));
  EXPECT_EQ(std::nullopt, index.GetPaddedStorageSize());
  EXPECT_TRUE(index.SetCacheSize(u"foo", base::ByteSize()));
  EXPECT_EQ(base::ByteSize(), index.GetPaddedStorageSize());
}

TEST(CacheStorageIndexTest, LargeSizeAndPadding) {
  CacheStorageIndex index;
  index.Insert(
      CacheStorageIndex::CacheMetadata(u"foo", base::GiB(3), base::GiB(4)));
  EXPECT_EQ(base::GiB(7), index.GetPaddedStorageSize());
  EXPECT_TRUE(index.SetCacheSize(u"foo", base::GiB(5)));
  EXPECT_EQ(base::GiB(9), index.GetPaddedStorageSize());
}

TEST(CacheStorageIndexTest, TestSetCacheSize) {
  CacheStorageIndex index;

  index.Insert(CacheStorageIndex::CacheMetadata(
      u"foo", /*size=*/base::ByteSize(12), /*padding=*/base::ByteSize(2)));
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"bar", /*size=*/base::ByteSize(19), /*padding=*/base::ByteSize(3)));
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"baz", /*size=*/base::ByteSize(1000), /*padding=*/base::ByteSize(4)));

  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  EXPECT_EQ(base::ByteSize(12 + 2 + 19 + 3 + 1000 + 4),
            index.GetPaddedStorageSize());

  EXPECT_TRUE(index.SetCacheSize(u"baz", base::ByteSize(2000)));
  EXPECT_EQ(base::ByteSize(12 + 2 + 19 + 3 + 2000 + 4),
            index.GetPaddedStorageSize());

  EXPECT_FALSE(index.SetCacheSize(u"baz", base::ByteSize(2000)));
  EXPECT_EQ(base::ByteSize(12 + 2 + 19 + 3 + 2000 + 4),
            index.GetPaddedStorageSize());

  EXPECT_EQ(base::ByteSize(2000), index.GetCacheSizeForTesting(u"baz"));
  EXPECT_EQ(std::nullopt, index.GetCacheSizeForTesting(u"<not-present>"));
}

TEST(CacheStorageIndexTest, TestSetCachePadding) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"foo", /*size=*/base::ByteSize(12), /*padding=*/base::ByteSize(2)));
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"bar", /*size=*/base::ByteSize(19), /*padding=*/base::ByteSize(3)));
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"baz", /*size=*/base::ByteSize(1000), /*padding=*/base::ByteSize(4)));
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  EXPECT_EQ(base::ByteSize(12 + 2 + 19 + 3 + 1000 + 4),
            index.GetPaddedStorageSize());

  EXPECT_TRUE(index.SetCachePadding(u"baz", base::ByteSize(80)));
  EXPECT_EQ(base::ByteSize(12 + 2 + 19 + 3 + 1000 + 80),
            index.GetPaddedStorageSize());

  EXPECT_FALSE(index.SetCachePadding(u"baz", base::ByteSize(80)));
  EXPECT_EQ(base::ByteSize(12 + 2 + 19 + 3 + 1000 + 80),
            index.GetPaddedStorageSize());

  EXPECT_EQ(base::ByteSize(80), index.GetCachePaddingForTesting(u"baz"));
  EXPECT_EQ(std::nullopt, index.GetCachePaddingForTesting(u"<not-present>"));
}

TEST(CacheStorageIndexTest, TestDoomCache) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"foo", /*size=*/base::ByteSize(12), /*padding=*/base::ByteSize(2)));
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"bar", /*size=*/base::ByteSize(19), /*padding=*/base::ByteSize(3)));
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"baz", /*size=*/base::ByteSize(1000), /*padding=*/base::ByteSize(4)));
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  EXPECT_EQ(base::ByteSize(12 + 2 + 19 + 3 + 1000 + 4),
            index.GetPaddedStorageSize());

  index.DoomCache(u"bar");
  EXPECT_EQ(2u, index.num_entries());
  ASSERT_EQ(2u, index.ordered_cache_metadata().size());
  EXPECT_EQ(base::ByteSize(12 + 2 + 1000 + 4), index.GetPaddedStorageSize());
  index.RestoreDoomedCache();
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  auto it = index.ordered_cache_metadata().begin();
  EXPECT_EQ(u"foo", (it++)->name);
  EXPECT_EQ(u"bar", (it++)->name);
  EXPECT_EQ(u"baz", (it++)->name);
  EXPECT_EQ(base::ByteSize(12 + 2 + 19 + 3 + 1000 + 4),
            index.GetPaddedStorageSize());

  index.DoomCache(u"foo");
  EXPECT_EQ(2u, index.num_entries());
  ASSERT_EQ(2u, index.ordered_cache_metadata().size());
  EXPECT_EQ(base::ByteSize(19 + 3 + 1000 + 4), index.GetPaddedStorageSize());
  index.FinalizeDoomedCache();
  EXPECT_EQ(2u, index.num_entries());
  ASSERT_EQ(2u, index.ordered_cache_metadata().size());
  EXPECT_EQ(base::ByteSize(19 + 3 + 1000 + 4), index.GetPaddedStorageSize());
}

TEST(CacheStorageIndexTest, TestDelete) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"bar", /*size=*/base::ByteSize(19), /*padding=*/base::ByteSize(2)));
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"foo", /*size=*/base::ByteSize(12), /*padding=*/base::ByteSize(3)));
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"baz", /*size=*/base::ByteSize(1000), /*padding=*/base::ByteSize(4)));
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  EXPECT_EQ(base::ByteSize(19 + 2 + 12 + 3 + 1000 + 4),
            index.GetPaddedStorageSize());

  auto it = index.ordered_cache_metadata().begin();
  EXPECT_EQ(u"bar", it->name);
  EXPECT_EQ(base::ByteSize(19u), it->size);
  EXPECT_EQ(base::ByteSize(2u), it->padding);
  it++;
  EXPECT_EQ(u"foo", it->name);
  EXPECT_EQ(base::ByteSize(12u), it->size);
  EXPECT_EQ(base::ByteSize(3u), it->padding);
  it++;
  EXPECT_EQ(u"baz", it->name);
  EXPECT_EQ(base::ByteSize(1000u), it->size);
  EXPECT_EQ(base::ByteSize(4u), it->padding);

  index.Delete(u"bar");
  EXPECT_EQ(2u, index.num_entries());
  ASSERT_EQ(2u, index.ordered_cache_metadata().size());
  EXPECT_EQ(base::ByteSize(12 + 3 + 1000 + 4), index.GetPaddedStorageSize());

  it = index.ordered_cache_metadata().begin();
  EXPECT_EQ(u"foo", it->name);
  EXPECT_EQ(base::ByteSize(12u), it->size);
  EXPECT_EQ(base::ByteSize(3u), it->padding);
  it++;
  EXPECT_EQ(u"baz", it->name);
  EXPECT_EQ(base::ByteSize(1000u), it->size);
  EXPECT_EQ(base::ByteSize(4u), it->padding);

  index.Delete(u"baz");
  EXPECT_EQ(1u, index.num_entries());
  ASSERT_EQ(1u, index.ordered_cache_metadata().size());
  EXPECT_EQ(base::ByteSize(12 + 3), index.GetPaddedStorageSize());

  it = index.ordered_cache_metadata().begin();
  EXPECT_EQ(u"foo", it->name);
  EXPECT_EQ(base::ByteSize(12u), it->size);
  EXPECT_EQ(base::ByteSize(3u), it->padding);
}

TEST(CacheStorageIndexTest, TestInsert) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"foo", /*size=*/base::ByteSize(12), /*padding=*/base::ByteSize(2)));
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"bar", /*size=*/base::ByteSize(19), /*padding=*/base::ByteSize(3)));
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"baz", /*size=*/base::ByteSize(1000), /*padding=*/base::ByteSize(4)));
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  EXPECT_EQ(base::ByteSize(12 + 2 + 19 + 3 + 1000 + 4),
            index.GetPaddedStorageSize());
}

TEST(CacheStorageIndexTest, TestMoveOperator) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"foo", /*size=*/base::ByteSize(12), /*padding=*/base::ByteSize(2)));
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"bar", /*size=*/base::ByteSize(19), /*padding=*/base::ByteSize(3)));
  index.Insert(CacheStorageIndex::CacheMetadata(
      u"baz", /*size=*/base::ByteSize(1000), /*padding=*/base::ByteSize(4)));

  CacheStorageIndex index2;
  index2 = std::move(index);

  EXPECT_EQ(3u, index2.num_entries());
  EXPECT_EQ(3u, index2.ordered_cache_metadata().size());
  ASSERT_EQ(base::ByteSize(12 + 2 + 19 + 3 + 1000 + 4),
            index2.GetPaddedStorageSize());

  EXPECT_EQ(0u, index.num_entries());
  EXPECT_TRUE(index.ordered_cache_metadata().empty());
  EXPECT_EQ(base::ByteSize(0u), index.GetPaddedStorageSize());

  auto it = index2.ordered_cache_metadata().begin();
  EXPECT_EQ(u"foo", it->name);
  EXPECT_EQ(base::ByteSize(12u), it->size);
  EXPECT_EQ(base::ByteSize(2u), it->padding);
  it++;
  EXPECT_EQ(u"bar", it->name);
  EXPECT_EQ(base::ByteSize(19u), it->size);
  EXPECT_EQ(base::ByteSize(3u), it->padding);
  it++;
  EXPECT_EQ(u"baz", it->name);
  EXPECT_EQ(base::ByteSize(1000u), it->size);
  EXPECT_EQ(base::ByteSize(4u), it->padding);

  EXPECT_EQ(3u, index2.num_entries());
  ASSERT_EQ(3u, index2.ordered_cache_metadata().size());
  EXPECT_EQ(base::ByteSize(12 + 2 + 19 + 3 + 1000 + 4),
            index2.GetPaddedStorageSize());
}

// This test verifies that all u16string cache name that have an invalid UTF-16
// character are stored as is and not replaced by the 'replacement character'.
TEST(CacheStorageIndexTest, TestInvalidCacheName) {
  CacheStorageIndex index;

  // Insert a cache with an invalid UTF-16 character.
  std::u16string invalid_utf16_name = u"bad";
  // kUnicodeHighSurrogate is considered an invalid UTF-16 character
  // when not followed by a low surrogate.
  invalid_utf16_name.push_back(kUnicodeHighSurrogate);
  index.Insert(CacheStorageIndex::CacheMetadata(invalid_utf16_name,
                                                /*size=*/base::ByteSize(12),
                                                /*padding=*/base::ByteSize(2)));

  // Verify that the cache was inserted.
  EXPECT_EQ(1u, index.num_entries());
  ASSERT_EQ(1u, index.ordered_cache_metadata().size());
  EXPECT_EQ(base::ByteSize(12 + 2), index.GetPaddedStorageSize());

  // Verify that the cache name is stored correctly.
  auto it = index.ordered_cache_metadata().begin();
  EXPECT_EQ(invalid_utf16_name, it->name);
  EXPECT_EQ(base::ByteSize(12), it->size);
  EXPECT_EQ(base::ByteSize(2), it->padding);

  // Verify that the invalid character is stored as is.
  const std::u16string& stored_cache_name = it->name;
  EXPECT_EQ(stored_cache_name.size(), invalid_utf16_name.size());
  for (size_t i = 0; i < invalid_utf16_name.size(); ++i) {
    EXPECT_EQ(stored_cache_name[i], invalid_utf16_name[i]);
  }

  // Attempt to set the cache size and padding for the invalid name.
  EXPECT_TRUE(index.SetCacheSize(invalid_utf16_name, base::ByteSize(20)));
  EXPECT_EQ(base::ByteSize(20 + 2), index.GetPaddedStorageSize());

  EXPECT_TRUE(index.SetCachePadding(invalid_utf16_name, base::ByteSize(5)));
  EXPECT_EQ(base::ByteSize(20 + 5), index.GetPaddedStorageSize());

  // Verify the updated size and padding.
  EXPECT_EQ(base::ByteSize(20),
            index.GetCacheSizeForTesting(invalid_utf16_name));
  EXPECT_EQ(base::ByteSize(5),
            index.GetCachePaddingForTesting(invalid_utf16_name));

  // Delete the cache with the invalid name.
  index.Delete(invalid_utf16_name);
  EXPECT_EQ(0u, index.num_entries());
  ASSERT_EQ(0u, index.ordered_cache_metadata().size());
  EXPECT_EQ(base::ByteSize(0), index.GetPaddedStorageSize());
}

}  // namespace content
