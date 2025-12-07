// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/disk_cache/sql/cache_entry_key.h"

#include <string>
#include <unordered_set>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {
namespace {

TEST(CacheEntryKeyTest, DefaultConstructor) {
  CacheEntryKey key;
  EXPECT_EQ(key.string(), "");
}

TEST(CacheEntryKeyTest, ValueConstructor) {
  const std::string kValue = "my_key";
  CacheEntryKey key(kValue);
  EXPECT_EQ(key.string(), kValue);
}

TEST(CacheEntryKeyTest, CopyConstructor) {
  const std::string kValue = "my_key";
  CacheEntryKey key1(kValue);
  CacheEntryKey key2(key1);

  EXPECT_EQ(key1.string(), kValue);
  EXPECT_EQ(key2.string(), kValue);
  // The copy should be equal, and this also tests the fast-path where the
  // underlying RefCountedString pointers are identical.
  EXPECT_EQ(key1, key2);
}

TEST(CacheEntryKeyTest, MoveConstructor) {
  const std::string kValue = "my_key";
  CacheEntryKey key1(kValue);
  CacheEntryKey key2(std::move(key1));

  // The moved-to key should have the original value.
  EXPECT_EQ(key2.string(), kValue);
}

TEST(CacheEntryKeyTest, CopyAssignment) {
  const std::string kValue1 = "key1";
  const std::string kValue2 = "key2";
  CacheEntryKey key1(kValue1);
  CacheEntryKey key2(kValue2);

  EXPECT_NE(key1, key2);

  key2 = key1;
  EXPECT_EQ(key1.string(), kValue1);
  EXPECT_EQ(key2.string(), kValue1);
  EXPECT_EQ(key1, key2);
}

TEST(CacheEntryKeyTest, MoveAssignment) {
  const std::string kValue1 = "key1";
  const std::string kValue2 = "key2";
  CacheEntryKey key1(kValue1);
  CacheEntryKey key2(kValue2);

  EXPECT_NE(key1, key2);

  key2 = std::move(key1);
  // The moved-to key should have the original value.
  EXPECT_EQ(key2.string(), kValue1);
}

TEST(CacheEntryKeyTest, ComparisonOperators) {
  CacheEntryKey key_a("a");
  CacheEntryKey key_a_copy("a");
  CacheEntryKey key_b("b");
  CacheEntryKey empty_key("");

  // Operator==
  EXPECT_EQ(key_a, key_a_copy);
  EXPECT_FALSE(key_a == key_b);
  EXPECT_FALSE(key_a == empty_key);
  EXPECT_EQ(CacheEntryKey(), empty_key);

  // Operator<
  EXPECT_LT(key_a, key_b);
  EXPECT_LT(empty_key, key_a);
  EXPECT_FALSE(key_b < key_a);
  EXPECT_FALSE(key_a < key_a);
  EXPECT_FALSE(key_a < key_a_copy);
}

TEST(CacheEntryKeyTest, LessThanOperatorFastPath) {
  // 1. Test with two keys sharing the exact same underlying data object.
  // The new `data_ != other.data_` check should short-circuit to false.
  const std::string kValue = "my_key";
  CacheEntryKey key1(kValue);
  CacheEntryKey key2(key1);

  ASSERT_EQ(key1, key2);
  // An object cannot be less than itself.
  EXPECT_FALSE(key1 < key2);
  EXPECT_FALSE(key2 < key1);

  // 2. Test with two keys having different data objects but the same string
  // value. The `data_ != other.data_` check passes, but the string
  // comparison 'kValue < kValue' is correctly false.
  CacheEntryKey key3(kValue);

  ASSERT_EQ(key1, key3);
  // The pointers are different, but the strings are equal.
  EXPECT_FALSE(key1 < key3);
  EXPECT_FALSE(key3 < key1);
}

TEST(CacheEntryKeyTest, StdHash) {
  std::unordered_set<CacheEntryKey> key_set;

  CacheEntryKey key1("key1");
  CacheEntryKey key2("key2");
  CacheEntryKey key1_copy("key1");

  // Insert keys.
  auto result1 = key_set.insert(key1);
  EXPECT_TRUE(result1.second);  // Insertion should succeed.
  EXPECT_EQ(key_set.size(), 1u);

  auto result2 = key_set.insert(key2);
  EXPECT_TRUE(result2.second);
  EXPECT_EQ(key_set.size(), 2u);

  // Try inserting a duplicate.
  auto result3 = key_set.insert(key1_copy);
  EXPECT_FALSE(result3.second);  // Insertion should fail.
  EXPECT_EQ(key_set.size(), 2u);

  // Find keys.
  EXPECT_EQ(key_set.count(key1), 1u);
  EXPECT_EQ(key_set.count(key2), 1u);
  EXPECT_EQ(key_set.count(key1_copy), 1u);
  EXPECT_EQ(key_set.count(CacheEntryKey("non_existent_key")), 0u);
}

}  // namespace
}  // namespace disk_cache
