// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"

namespace blink {

TEST(PaintChunkTest, MatchesSame) {
  auto properties = PropertyTreeState::Root();
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  client.Validate();
  DisplayItem::Id id(client.Id(), DisplayItem::kDrawingFirst);
  EXPECT_TRUE(PaintChunk(0, 1, client, id, properties)
                  .Matches(PaintChunk(0, 1, client, id, properties)));
}

TEST(PaintChunkTest, MatchesEqual) {
  auto properties = PropertyTreeState::Root();
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  client.Validate();
  DisplayItem::Id id(client.Id(), DisplayItem::kDrawingFirst);
  DisplayItem::Id id_equal = id;
  EXPECT_TRUE(PaintChunk(0, 1, client, id, properties)
                  .Matches(PaintChunk(0, 1, client, id_equal, properties)));
  EXPECT_TRUE(PaintChunk(0, 1, client, id_equal, properties)
                  .Matches(PaintChunk(0, 1, client, id, properties)));
}

TEST(PaintChunkTest, IdNotMatches) {
  auto properties = PropertyTreeState::Root();
  FakeDisplayItemClient& client1 =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  client1.Validate();
  DisplayItem::Id id1(client1.Id(), DisplayItem::kDrawingFirst);

  FakeDisplayItemClient& client2 =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  client2.Validate();
  DisplayItem::Id id2(client2.Id(), DisplayItem::kDrawingFirst);
  EXPECT_FALSE(PaintChunk(0, 1, client2, id2, properties)
                   .Matches(PaintChunk(0, 1, client1, id1, properties)));
}

TEST(PaintChunkTest, IdNotMatchesUncacheable) {
  auto properties = PropertyTreeState::Root();
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  client.Invalidate(PaintInvalidationReason::kUncacheable);
  DisplayItem::Id id(client.Id(), DisplayItem::kDrawingFirst);
  EXPECT_FALSE(PaintChunk(0, 1, client, id, properties)
                   .Matches(PaintChunk(0, 1, client, id, properties)));
}

TEST(PaintChunkTest, IdNotMatchesJustCreated) {
  auto properties = PropertyTreeState::Root();
  FakeDisplayItemClient* client = MakeGarbageCollected<FakeDisplayItemClient>();
  EXPECT_TRUE(client->IsJustCreated());
  // Invalidation won't change the "just created" status.
  client->Invalidate();
  EXPECT_TRUE(client->IsJustCreated());

  DisplayItem::Id id(client->Id(), DisplayItem::kDrawingFirst);
  // A chunk of a newly created client doesn't match any chunk because it's
  // never cached.
  EXPECT_FALSE(PaintChunk(0, 1, *client, id, properties)
                   .Matches(PaintChunk(0, 1, *client, id, properties)));

  client->Validate();
  EXPECT_TRUE(PaintChunk(0, 1, *client, id, properties)
                  .Matches(PaintChunk(0, 1, *client, id, properties)));

  // Delete the current object and create a new object at the same address.
  client = MakeGarbageCollected<FakeDisplayItemClient>();
  EXPECT_TRUE(client->IsJustCreated());
  EXPECT_FALSE(PaintChunk(0, 1, *client, id, properties)
                   .Matches(PaintChunk(0, 1, *client, id, properties)));
}

}  // namespace blink
