// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_id_wrapper.h"

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class SnapshotIDWrapperTest : public PlatformTest {};

// Tests the comparison of 2 SnapshotIDWrappers with a same identifier.
TEST_F(SnapshotIDWrapperTest, SameIdentifierWithDifferentInitializer) {
  SnapshotIDWrapper* id1 =
      [[SnapshotIDWrapper alloc] initWithSnapshotID:SnapshotID(42)];
  SnapshotIDWrapper* id2 = [[SnapshotIDWrapper alloc] initWithIdentifier:42];

  EXPECT_EQ([id1 identifier], [id2 identifier]);
  EXPECT_NSEQ(id1, id2);
  EXPECT_NE(id1, id2);
}

// Tests the comparison of 2 SnapshotIDWrappers with a different identifier.
TEST_F(SnapshotIDWrapperTest, CompareSnapshotIDWrappers) {
  SnapshotIDWrapper* id1 =
      [[SnapshotIDWrapper alloc] initWithSnapshotID:SnapshotID(1)];
  SnapshotIDWrapper* id2 = [[SnapshotIDWrapper alloc] initWithIdentifier:42];

  EXPECT_NSNE(id1, id2);
  EXPECT_NE(id1, id2);
}

// Tests that NSMutableSet works correctly with SnapshotIDWrapper.
TEST_F(SnapshotIDWrapperTest, ReturnSameHashValueForSameIdentifier) {
  SnapshotIDWrapper* id1 = [[SnapshotIDWrapper alloc] initWithIdentifier:42];
  SnapshotIDWrapper* id2 = [[SnapshotIDWrapper alloc] initWithIdentifier:42];
  SnapshotIDWrapper* id3 = [[SnapshotIDWrapper alloc] initWithIdentifier:43];

  EXPECT_NSEQ(id1, id2);
  EXPECT_NE(id1, id2);

  // NSMutableSet uses `hash` method to identify a unique object.
  NSMutableSet<SnapshotIDWrapper*>* set = [[NSMutableSet alloc] init];
  [set addObject:id1];
  [set addObject:id2];
  [set addObject:id3];
  EXPECT_EQ(set.count, 2ul);
}

}  // anonymous namespace
