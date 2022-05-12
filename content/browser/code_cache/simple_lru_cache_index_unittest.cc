// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/code_cache/simple_lru_cache_index.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

constexpr auto kEmptyEntrySize = SimpleLruCacheIndex::kEmptyEntrySize;

TEST(SimpleLruCacheIndexTest, Empty) {
  const std::string kKey = "hello";
  SimpleLruCacheIndex cache(/*capacity=*/100 * 1024);

  EXPECT_EQ(cache.GetSize(), 0u);
  EXPECT_FALSE(cache.Get(kKey));
}

TEST(SimpleLruCacheIndexTest, PutAndGet) {
  const std::string kKey1("key1");
  const std::string kKey2("key2");
  const std::string kKey3("key3");
  const std::string kKey4("key4");

  SimpleLruCacheIndex cache(/*capacity=*/100 * 1024);

  EXPECT_EQ(cache.GetSize(), 0u);
  EXPECT_FALSE(cache.Get(kKey1));
  EXPECT_FALSE(cache.Get(kKey2));
  EXPECT_FALSE(cache.Get(kKey3));
  EXPECT_FALSE(cache.Get(kKey4));

  cache.Put(kKey1, 1);
  EXPECT_EQ(cache.GetSize(), 1 + kEmptyEntrySize);
  EXPECT_TRUE(cache.Get(kKey1));
  EXPECT_FALSE(cache.Get(kKey2));
  EXPECT_FALSE(cache.Get(kKey3));
  EXPECT_FALSE(cache.Get(kKey4));

  cache.Put(kKey1, 2);
  EXPECT_EQ(cache.GetSize(), 2 + kEmptyEntrySize);
  EXPECT_TRUE(cache.Get(kKey1));
  EXPECT_FALSE(cache.Get(kKey2));
  EXPECT_FALSE(cache.Get(kKey3));
  EXPECT_FALSE(cache.Get(kKey4));

  cache.Put(kKey2, 3);
  EXPECT_EQ(cache.GetSize(), 5 + 2 * kEmptyEntrySize);
  EXPECT_TRUE(cache.Get(kKey1));
  EXPECT_TRUE(cache.Get(kKey2));
  EXPECT_FALSE(cache.Get(kKey3));
  EXPECT_FALSE(cache.Get(kKey4));

  cache.Put(kKey4, 4);
  EXPECT_EQ(cache.GetSize(), 9 + 3 * kEmptyEntrySize);
  EXPECT_TRUE(cache.Get(kKey1));
  EXPECT_TRUE(cache.Get(kKey2));
  EXPECT_FALSE(cache.Get(kKey3));
  EXPECT_TRUE(cache.Get(kKey4));
}

TEST(SimpleLruCacheIndexTest, PutAndEvict) {
  const std::string kKey("key1");

  SimpleLruCacheIndex cache(/*capacity=*/kEmptyEntrySize + 1);

  EXPECT_EQ(cache.GetSize(), 0u);
  EXPECT_FALSE(cache.Get(kKey));

  // This entry is immediately evicted because the size excceeds the capacity.
  cache.Put(kKey, 2);
  EXPECT_EQ(cache.GetSize(), 0u);
  EXPECT_FALSE(cache.Get(kKey));

  // This entry stays.
  cache.Put(kKey, 1);
  EXPECT_EQ(cache.GetSize(), 1 + kEmptyEntrySize);
  EXPECT_TRUE(cache.Get(kKey));

  // An updated entry can also be evicted.
  cache.Put(kKey, 2);
  EXPECT_EQ(cache.GetSize(), 0u);
  EXPECT_FALSE(cache.Get(kKey));
}

TEST(SimpleLruCacheIndexTest, LRU) {
  const std::string kKey1("key1");
  const std::string kKey2("key2");
  const std::string kKey3("key3");
  const std::string kKey4("key4");

  SimpleLruCacheIndex cache(kEmptyEntrySize * 2);

  cache.Put(kKey1, 0);
  cache.Put(kKey2, 0);
  cache.Put(kKey3, 0);
  cache.Put(kKey4, 0);

  // The last two entries are kept.
  EXPECT_EQ(cache.GetSize(), 2 * kEmptyEntrySize);
  EXPECT_FALSE(cache.Get(kKey1));
  EXPECT_FALSE(cache.Get(kKey2));
  EXPECT_TRUE(cache.Get(kKey3));
  EXPECT_TRUE(cache.Get(kKey4));
}

TEST(SimpleLruCacheIndexTest, LRUAndGet) {
  const std::string kKey1("key1");
  const std::string kKey2("key2");
  const std::string kKey3("key3");
  const std::string kKey4("key4");

  SimpleLruCacheIndex cache(kEmptyEntrySize * 2);

  cache.Put(kKey1, 0);
  cache.Put(kKey2, 0);
  // This call updates the access time.
  EXPECT_TRUE(cache.Get(kKey1));
  cache.Put(kKey3, 0);

  EXPECT_EQ(cache.GetSize(), 2 * kEmptyEntrySize);
  EXPECT_TRUE(cache.Get(kKey1));
  EXPECT_FALSE(cache.Get(kKey2));
  EXPECT_TRUE(cache.Get(kKey3));
  EXPECT_FALSE(cache.Get(kKey4));
}

TEST(SimpleLruCacheIndexTest, Delete) {
  const std::string kKey1("key1");
  const std::string kKey2("key2");
  const std::string kKey3("key3");
  const std::string kKey4("key4");

  SimpleLruCacheIndex cache(/*capacity=*/1024 * 1024);

  cache.Put(kKey1, 1);
  cache.Put(kKey2, 2);
  cache.Put(kKey3, 3);
  cache.Put(kKey4, 4);

  EXPECT_EQ(cache.GetSize(), 4 * kEmptyEntrySize + 10);
  EXPECT_TRUE(cache.Get(kKey1));
  EXPECT_TRUE(cache.Get(kKey2));
  EXPECT_TRUE(cache.Get(kKey3));
  EXPECT_TRUE(cache.Get(kKey4));

  cache.Delete(kKey2);
  EXPECT_EQ(cache.GetSize(), 3 * kEmptyEntrySize + 8);
  EXPECT_TRUE(cache.Get(kKey1));
  EXPECT_FALSE(cache.Get(kKey2));
  EXPECT_TRUE(cache.Get(kKey3));
  EXPECT_TRUE(cache.Get(kKey4));

  cache.Delete(kKey2);
  EXPECT_EQ(cache.GetSize(), 3 * kEmptyEntrySize + 8);
  EXPECT_TRUE(cache.Get(kKey1));
  EXPECT_FALSE(cache.Get(kKey2));
  EXPECT_TRUE(cache.Get(kKey3));
  EXPECT_TRUE(cache.Get(kKey4));

  cache.Delete(kKey1);
  cache.Delete(kKey2);
  cache.Delete(kKey3);
  cache.Delete(kKey4);
  EXPECT_EQ(cache.GetSize(), 0u);
  EXPECT_FALSE(cache.Get(kKey1));
  EXPECT_FALSE(cache.Get(kKey2));
  EXPECT_FALSE(cache.Get(kKey3));
  EXPECT_FALSE(cache.Get(kKey4));
}

}  // namespace
}  // namespace content
