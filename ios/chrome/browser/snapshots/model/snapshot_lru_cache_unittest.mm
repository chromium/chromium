// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/snapshots/model/legacy_snapshot_lru_cache.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id_wrapper.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using SnapshotLRUCacheTest = PlatformTest;

// Tests the LRU cache by adding, removing and getting an object by a key.
TEST_F(SnapshotLRUCacheTest, Basic) {
  LegacySnapshotLRUCache<NSString*>* cache =
      [[LegacySnapshotLRUCache alloc] initWithCacheSize:3];

  NSString* value1 = @"Value 1";
  NSString* value2 = @"Value 2";
  NSString* value3 = @"Value 3";
  NSString* value4 = @"Value 4";

  const SnapshotID key1 = SnapshotID(1);
  const SnapshotID key2 = SnapshotID(2);
  const SnapshotID key3 = SnapshotID(3);
  const SnapshotID key4 = SnapshotID(4);

  EXPECT_TRUE([cache count] == 0);
  EXPECT_TRUE([cache isEmpty]);

  [cache setObject:value1 forKey:key1];
  [cache setObject:value2 forKey:key2];
  [cache setObject:value3 forKey:key3];
  [cache setObject:value4 forKey:key4];

  EXPECT_TRUE([cache count] == 3);

  // Check LRU behaviour, the value least recently added value should have been
  // evicted.
  id value = [cache objectForKey:key1];
  EXPECT_TRUE(value == nil);

  value = [cache objectForKey:key2];
  EXPECT_TRUE(value == value2);

  // Removing a non existing key shouldn't do anything.
  [cache removeObjectForKey:SnapshotID(5)];
  EXPECT_TRUE([cache count] == 3);

  [cache removeAllObjects];
  EXPECT_TRUE([cache isEmpty]);
}

// Tests that the LRU cache evicts the least recently "added" value.
TEST_F(SnapshotLRUCacheTest, EvictLeastRecentlyAddedItem) {
  SnapshotLRUCache* cache = [[SnapshotLRUCache alloc] initWithSize:3];
  EXPECT_EQ(3, [cache maxCacheSize]);

  UIImage* image1 = [[UIImage alloc] init];
  UIImage* image2 = [[UIImage alloc] init];
  UIImage* image3 = [[UIImage alloc] init];
  UIImage* image4 = [[UIImage alloc] init];

  SnapshotIDWrapper* key1 = [[SnapshotIDWrapper alloc] initWithIdentifier:1];
  SnapshotIDWrapper* key2 = [[SnapshotIDWrapper alloc] initWithIdentifier:2];
  SnapshotIDWrapper* key3 = [[SnapshotIDWrapper alloc] initWithIdentifier:3];
  SnapshotIDWrapper* key4 = [[SnapshotIDWrapper alloc] initWithIdentifier:4];

  EXPECT_EQ(0, [cache getCount]);

  [cache setObjectWithValue:image1 forKey:key1];
  [cache setObjectWithValue:image2 forKey:key2];
  [cache setObjectWithValue:image3 forKey:key3];
  [cache setObjectWithValue:image4 forKey:key4];

  EXPECT_EQ(3, [cache getCount]);

  // The least recently added value should be evicted.
  id image = [cache getObjectForKey:key1];
  EXPECT_EQ(nil, image);
  EXPECT_EQ(3, [cache getCount]);

  // Other values that was not evicted should be kept.
  image = [cache getObjectForKey:key2];
  EXPECT_EQ(image2, image);
  EXPECT_EQ(3, [cache getCount]);

  // Clean up.
  [cache removeAllObjects];
  EXPECT_EQ(0, [cache getCount]);
}

// Tests that the LRU cache evicts the least recently "accessed" value.
TEST_F(SnapshotLRUCacheTest, EvictLeastRecentlyUsedValue) {
  SnapshotLRUCache* cache = [[SnapshotLRUCache alloc] initWithSize:3];
  EXPECT_EQ(3, [cache maxCacheSize]);

  UIImage* image1 = [[UIImage alloc] init];
  UIImage* image2 = [[UIImage alloc] init];
  UIImage* image3 = [[UIImage alloc] init];
  UIImage* image4 = [[UIImage alloc] init];

  SnapshotIDWrapper* key1 = [[SnapshotIDWrapper alloc] initWithIdentifier:1];
  SnapshotIDWrapper* key2 = [[SnapshotIDWrapper alloc] initWithIdentifier:2];
  SnapshotIDWrapper* key3 = [[SnapshotIDWrapper alloc] initWithIdentifier:3];
  SnapshotIDWrapper* key4 = [[SnapshotIDWrapper alloc] initWithIdentifier:4];

  EXPECT_EQ(0, [cache getCount]);

  [cache setObjectWithValue:image1 forKey:key1];
  [cache setObjectWithValue:image2 forKey:key2];
  [cache setObjectWithValue:image3 forKey:key3];

  // Access the value with `key1`, which was added least recently.
  id image = [cache getObjectForKey:key1];
  EXPECT_EQ(image1, image);
  EXPECT_EQ(3, [cache getCount]);

  // Adding another value evicts the value for `key2`.
  [cache setObjectWithValue:image4 forKey:key4];
  image = [cache getObjectForKey:key2];
  EXPECT_EQ(nil, image);
  EXPECT_EQ(3, [cache getCount]);

  // Clean up.
  [cache removeAllObjects];
  EXPECT_EQ(0, [cache getCount]);
}

// Tests that the LRU cache overrides the value for the same key.
TEST_F(SnapshotLRUCacheTest, OverrideValue) {
  SnapshotLRUCache* cache = [[SnapshotLRUCache alloc] initWithSize:1];
  EXPECT_EQ(1, [cache maxCacheSize]);

  UIImage* image1 = [[UIImage alloc] init];
  UIImage* image2 = [[UIImage alloc] init];

  SnapshotIDWrapper* key1 = [[SnapshotIDWrapper alloc] initWithIdentifier:1];

  EXPECT_EQ(0, [cache getCount]);

  [cache setObjectWithValue:image1 forKey:key1];
  id image = [cache getObjectForKey:key1];
  EXPECT_EQ(image1, image);
  EXPECT_EQ(1, [cache getCount]);

  // Setting a value to the same key should override the value.
  [cache setObjectWithValue:image2 forKey:key1];
  image = [cache getObjectForKey:key1];
  EXPECT_EQ(image2, image);
  EXPECT_EQ(1, [cache getCount]);

  // Clean up.
  [cache removeAllObjects];
  EXPECT_EQ(0, [cache getCount]);
}

// Tests that the LRU cache does nothing for a non-existing and an evicted key.
TEST_F(SnapshotLRUCacheTest, DoNothingForNonExistingKey) {
  SnapshotLRUCache* cache = [[SnapshotLRUCache alloc] initWithSize:1];
  EXPECT_EQ(1, [cache maxCacheSize]);

  UIImage* image1 = [[UIImage alloc] init];
  UIImage* image2 = [[UIImage alloc] init];

  SnapshotIDWrapper* key1 = [[SnapshotIDWrapper alloc] initWithIdentifier:1];
  SnapshotIDWrapper* key2 = [[SnapshotIDWrapper alloc] initWithIdentifier:2];

  EXPECT_EQ(0, [cache getCount]);

  [cache setObjectWithValue:image1 forKey:key1];
  EXPECT_EQ(1, [cache getCount]);

  // Getting a value for a non existing key returns nil.
  UIImage* image = [cache getObjectForKey:key2];
  EXPECT_EQ(nil, image);

  // Removing a non existing key shouldn't do anything.
  [cache removeObjectForKey:key2];
  EXPECT_EQ(1, [cache getCount]);

  // Setting another value evicts the least recently used value.
  [cache setObjectWithValue:image2 forKey:key2];

  // Getting a value for an evicted key returns nil.
  image = [cache getObjectForKey:key1];
  EXPECT_EQ(nil, image);

  // Removing an evicted key shouldn't do anything.
  [cache removeObjectForKey:key1];
  EXPECT_EQ(1, [cache getCount]);

  // Clean up.
  [cache removeAllObjects];
  EXPECT_EQ(0, [cache getCount]);
}

}  // namespace
