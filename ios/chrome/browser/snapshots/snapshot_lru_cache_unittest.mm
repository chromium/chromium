// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_lru_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using SnapshotLRUCacheTest = PlatformTest;

TEST_F(SnapshotLRUCacheTest, Basic) {
  SnapshotLRUCache* cache = [[SnapshotLRUCache alloc] initWithCacheSize:3];

  NSString* value1 = @"Value 1";
  NSString* value2 = @"Value 2";
  NSString* value3 = @"Value 3";
  NSString* value4 = @"Value 4";

  EXPECT_TRUE([cache count] == 0);
  EXPECT_TRUE([cache isEmpty]);

  [cache setObject:value1 forKey:@"VALUE 1"];
  [cache setObject:value2 forKey:@"VALUE 2"];
  [cache setObject:value3 forKey:@"VALUE 3"];
  [cache setObject:value4 forKey:@"VALUE 4"];

  EXPECT_TRUE([cache count] == 3);

  // Check LRU behaviour, the value least recently added value should have been
  // evicted.
  id value = [cache objectForKey:@"VALUE 1"];
  EXPECT_TRUE(value == nil);

  value = [cache objectForKey:@"VALUE 2"];
  EXPECT_TRUE(value == value2);

  // Removing a non existing key shouldn't do anything.
  [cache removeObjectForKey:@"XXX"];
  EXPECT_TRUE([cache count] == 3);

  [cache removeAllObjects];
  EXPECT_TRUE([cache isEmpty]);
}

}  // namespace
