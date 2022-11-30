// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/storage_area_map.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(StorageAreaMapTest, Basics) {
  const String kKey("key");
  const String kValue("value");
  const size_t kValueQuota = kValue.length() * 2;
  const size_t kItemQuota = (kKey.length() + kValue.length()) * 2;
  const String kKey2("key2");
  const size_t kKey2Quota = kKey2.length() * 2;
  const String kValue2("value2");
  const size_t kItem2Quota = (kKey2.length() + kValue2.length()) * 2;
  const size_t kQuota = 1024;  // 1K quota for this test.

  StorageAreaMap map(kQuota);
  String old_value;
  EXPECT_EQ(kQuota, map.quota());

  // Check the behavior of an empty map.
  EXPECT_EQ(0u, map.GetLength());
  EXPECT_TRUE(map.GetKey(0).IsNull());
  EXPECT_TRUE(map.GetKey(100).IsNull());
  EXPECT_TRUE(map.GetItem(kKey).IsNull());
  EXPECT_FALSE(map.RemoveItem(kKey, nullptr));
  EXPECT_EQ(0u, map.quota_used());

  // Check the behavior of a map containing some values.
  EXPECT_TRUE(map.SetItem(kKey, kValue, &old_value));
  EXPECT_TRUE(old_value.IsNull());
  EXPECT_EQ(1u, map.GetLength());
  EXPECT_EQ(kKey, map.GetKey(0));
  EXPECT_TRUE(map.GetKey(1).IsNull());
  EXPECT_EQ(kValue, map.GetItem(kKey));
  EXPECT_TRUE(map.GetItem(kKey2).IsNull());
  EXPECT_EQ(kItemQuota, map.quota_used());
  EXPECT_TRUE(map.RemoveItem(kKey, &old_value));
  EXPECT_EQ(kValue, old_value);
  old_value = String();
  EXPECT_EQ(0u, map.quota_used());

  EXPECT_TRUE(map.SetItem(kKey, kValue, nullptr));
  EXPECT_TRUE(map.SetItem(kKey2, kValue, nullptr));
  EXPECT_EQ(kItemQuota + kKey2Quota + kValueQuota, map.quota_used());
  EXPECT_TRUE(map.SetItem(kKey2, kValue2, &old_value));
  EXPECT_EQ(kValue, old_value);
  EXPECT_EQ(kItemQuota + kItem2Quota, map.quota_used());
  EXPECT_EQ(2u, map.GetLength());
  String key1 = map.GetKey(0);
  String key2 = map.GetKey(1);
  EXPECT_TRUE((key1 == kKey && key2 == kKey2) ||
              (key1 == kKey2 && key2 == kKey))
      << key1 << ", " << key2;
  EXPECT_EQ(key1, map.GetKey(0));
  EXPECT_EQ(key2, map.GetKey(1));
  EXPECT_EQ(kItemQuota + kItem2Quota, map.quota_used());
}

TEST(StorageAreaMapTest, EnforcesQuota) {
  const String kKey("test_key");
  const String kValue("test_value");
  const String kKey2("test_key_2");
  String old_value;

  // A 50 byte quota is too small to hold both keys and values, so we
  // should see the StorageAreaMap enforcing it.
  const size_t kQuota = 50;

  StorageAreaMap map(kQuota);
  EXPECT_TRUE(map.SetItem(kKey, kValue, nullptr));
  EXPECT_FALSE(map.SetItem(kKey2, kValue, nullptr));
  EXPECT_EQ(1u, map.GetLength());
  EXPECT_EQ(kValue, map.GetItem(kKey));
  EXPECT_TRUE(map.GetItem(kKey2).IsNull());

  EXPECT_TRUE(map.RemoveItem(kKey, nullptr));
  EXPECT_EQ(0u, map.GetLength());
  EXPECT_TRUE(map.SetItem(kKey2, kValue, nullptr));
  EXPECT_EQ(1u, map.GetLength());

  // Verify that the SetItemIgnoringQuota method does not do quota checking.
  map.SetItemIgnoringQuota(kKey, kValue);
  EXPECT_GT(map.quota_used(), kQuota);
  EXPECT_EQ(2u, map.GetLength());
  EXPECT_EQ(kValue, map.GetItem(kKey));
  EXPECT_EQ(kValue, map.GetItem(kKey2));

  // When overbudget, a new value of greater size than the existing value can
  // not be set, but a new value of lesser or equal size can be set.
  EXPECT_TRUE(map.SetItem(kKey, kValue, nullptr));
  EXPECT_FALSE(map.SetItem(kKey, kValue + kValue, nullptr));
  EXPECT_TRUE(map.SetItem(kKey, "", nullptr));
  EXPECT_EQ("", map.GetItem(kKey));
  EXPECT_EQ(kValue, map.GetItem(kKey2));
}

TEST(StorageAreaMapTest, Iteration) {
  const int kNumTestItems = 100;
  const size_t kQuota = 102400;  // 100K quota for this test.
  StorageAreaMap map(kQuota);

  // Fill the map with some data.
  for (int i = 0; i < kNumTestItems; ++i)
    EXPECT_TRUE(map.SetItem("key" + String::Number(i), "val", nullptr));
  EXPECT_EQ(unsigned{kNumTestItems}, map.GetLength());

  Vector<String> keys(kNumTestItems);
  // Iterate over all keys.
  for (int i = 0; i < kNumTestItems; ++i)
    keys[i] = map.GetKey(i);

  // Now iterate over some subsets, and make sure the right keys are returned.
  for (int i = 5; i < 15; ++i)
    EXPECT_EQ(keys[i], map.GetKey(i));
  for (int i = kNumTestItems - 5; i >= kNumTestItems - 15; --i)
    EXPECT_EQ(keys[i], map.GetKey(i));
  for (int i = 20; i >= 10; --i)
    EXPECT_EQ(keys[i], map.GetKey(i));
  for (int i = 15; i < 20; ++i)
    EXPECT_EQ(keys[i], map.GetKey(i));
  for (int i = kNumTestItems - 1; i >= 0; --i)
    EXPECT_EQ(keys[i], map.GetKey(i));
  EXPECT_TRUE(map.GetKey(kNumTestItems).IsNull());
}

}  // namespace blink
