// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_lru_cache.h"

#import "ios/chrome/browser/snapshots/snapshot_id.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using SnapshotLRUCacheTest = PlatformTest;

TEST_F(SnapshotLRUCacheTest, Basic) {
  SnapshotLRUCache<NSString*>* cache =
      [[SnapshotLRUCache alloc] initWithCacheSize:3];

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

}  // namespace
