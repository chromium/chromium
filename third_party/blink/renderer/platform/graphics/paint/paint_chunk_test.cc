// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"

namespace blink {

TEST(PaintChunkTest, MatchesSame) {
  auto properties = PropertyTreeState::Root();
  FakeDisplayItemClient client;
  client.Validate();
  DisplayItem::Id id(client, DisplayItem::kDrawingFirst);
  EXPECT_TRUE(PaintChunk(0, 1, id, properties)
                  .Matches(PaintChunk(0, 1, id, properties)));
}

TEST(PaintChunkTest, MatchesEqual) {
  auto properties = PropertyTreeState::Root();
  FakeDisplayItemClient client;
  client.Validate();
  DisplayItem::Id id(client, DisplayItem::kDrawingFirst);
  DisplayItem::Id id_equal = id;
  EXPECT_TRUE(PaintChunk(0, 1, id, properties)
                  .Matches(PaintChunk(0, 1, id_equal, properties)));
  EXPECT_TRUE(PaintChunk(0, 1, id_equal, properties)
                  .Matches(PaintChunk(0, 1, id, properties)));
}

TEST(PaintChunkTest, IdNotMatches) {
  auto properties = PropertyTreeState::Root();
  FakeDisplayItemClient client1;
  client1.Validate();
  DisplayItem::Id id1(client1, DisplayItem::kDrawingFirst);

  FakeDisplayItemClient client2;
  client2.Validate();
  DisplayItem::Id id2(client2, DisplayItem::kDrawingFirst);
  EXPECT_FALSE(PaintChunk(0, 1, id2, properties)
                   .Matches(PaintChunk(0, 1, id1, properties)));
}

TEST(PaintChunkTest, IdNotMatchesUncacheable) {
  auto properties = PropertyTreeState::Root();
  FakeDisplayItemClient client;
  client.Invalidate(PaintInvalidationReason::kUncacheable);
  DisplayItem::Id id(client, DisplayItem::kDrawingFirst);
  EXPECT_FALSE(PaintChunk(0, 1, id, properties)
                   .Matches(PaintChunk(0, 1, id, properties)));
}

TEST(PaintChunkTest, IdNotMatchesJustCreated) {
  auto properties = PropertyTreeState::Root();
  absl::optional<FakeDisplayItemClient> client;
  client.emplace();
  EXPECT_TRUE(client->IsJustCreated());
  // Invalidation won't change the "just created" status.
  client->Invalidate();
  EXPECT_TRUE(client->IsJustCreated());

  DisplayItem::Id id(*client, DisplayItem::kDrawingFirst);
  // A chunk of a newly created client doesn't match any chunk because it's
  // never cached.
  EXPECT_FALSE(PaintChunk(0, 1, id, properties)
                   .Matches(PaintChunk(0, 1, id, properties)));

  client->Validate();
  EXPECT_TRUE(PaintChunk(0, 1, id, properties)
                  .Matches(PaintChunk(0, 1, id, properties)));

  // Delete the current object and create a new object at the same address.
  client = absl::nullopt;
  client.emplace();
  EXPECT_TRUE(client->IsJustCreated());
  EXPECT_FALSE(PaintChunk(0, 1, id, properties)
                   .Matches(PaintChunk(0, 1, id, properties)));
}

}  // namespace blink
