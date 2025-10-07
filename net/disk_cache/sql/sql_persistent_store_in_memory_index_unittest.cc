// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store_in_memory_index.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

namespace {

const CacheEntryKey::Hash kHash1(1);
const SqlPersistentStore::ResId kResId1(1);
const CacheEntryKey::Hash kHash2(2);
const SqlPersistentStore::ResId kResId2(2);

}  // namespace

TEST(SqlPersistentStoreInMemoryIndexTest, Insert) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_TRUE(index.Contains(kHash1));
}

TEST(SqlPersistentStoreInMemoryIndexTest, InsertDuplicateResId) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_FALSE(index.Insert(kHash2, kResId1));
  EXPECT_TRUE(index.Contains(kHash1));
}

TEST(SqlPersistentStoreInMemoryIndexTest, InsertSameHash) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_TRUE(index.Insert(kHash1, kResId2));
  EXPECT_TRUE(index.Contains(kHash1));
}

TEST(SqlPersistentStoreInMemoryIndexTest, RemoveWithHashAndResId) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  EXPECT_TRUE(index.Remove(kHash1, kResId1));
  EXPECT_FALSE(index.Contains(kHash1));
}

TEST(SqlPersistentStoreInMemoryIndexTest, RemoveWithResId) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  EXPECT_TRUE(index.Remove(kResId1));
  EXPECT_FALSE(index.Contains(kHash1));
}

TEST(SqlPersistentStoreInMemoryIndexTest, RemoveNonExistent) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  EXPECT_FALSE(index.Remove(kResId2));
  EXPECT_FALSE(index.Remove(kHash2, kResId2));
  EXPECT_FALSE(index.Remove(kHash2, kResId1));
  EXPECT_FALSE(index.Remove(kHash1, kResId2));
  EXPECT_TRUE(index.Contains(kHash1));
}

TEST(SqlPersistentStoreInMemoryIndexTest, Clear) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  index.Insert(kHash2, kResId2);
  index.Clear();
  EXPECT_FALSE(index.Contains(kHash1));
  EXPECT_FALSE(index.Contains(kHash2));
}

TEST(SqlPersistentStoreInMemoryIndexTest, MultipleEntries) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  index.Insert(kHash2, kResId2);

  EXPECT_TRUE(index.Contains(kHash1));
  EXPECT_TRUE(index.Contains(kHash2));

  EXPECT_TRUE(index.Remove(kResId1));
  EXPECT_FALSE(index.Contains(kHash1));
  EXPECT_TRUE(index.Contains(kHash2));

  EXPECT_TRUE(index.Remove(kHash2, kResId2));
  EXPECT_FALSE(index.Contains(kHash2));
}

}  // namespace disk_cache
