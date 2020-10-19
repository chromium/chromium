// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunker.h"

#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using testing::ElementsAre;

namespace blink {

namespace {

class PaintChunkerTest : public testing::Test {
 protected:
  FakeDisplayItemClient client_;
};

DisplayItem::Type DisplayItemType(int offset) {
  auto type =
      static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + offset);
  DCHECK(DisplayItem::IsDrawingType(type));
  return type;
}

class TestChunkerDisplayItem : public DrawingDisplayItem {
 public:
  explicit TestChunkerDisplayItem(
      const DisplayItemClient& client,
      DisplayItem::Type type = DisplayItem::kDrawingFirst,
      const IntRect& visual_rect = IntRect())
      : DrawingDisplayItem(client, type, visual_rect, nullptr) {}
};

class TestChunkerOpaqueDisplayItem : public DrawingDisplayItem {
 public:
  explicit TestChunkerOpaqueDisplayItem(
      const DisplayItemClient& client,
      DisplayItem::Type type = DisplayItem::kDrawingFirst,
      const IntRect& visual_rect = IntRect())
      : DrawingDisplayItem(client, type, visual_rect, nullptr) {
    SetKnownToBeOpaqueForTesting();
  }
};

class TestDisplayItemRequiringSeparateChunk : public ForeignLayerDisplayItem {
 public:
  explicit TestDisplayItemRequiringSeparateChunk(
      const DisplayItemClient& client)
      : ForeignLayerDisplayItem(client,
                                DisplayItem::kForeignLayerPlugin,
                                cc::Layer::Create(),
                                IntPoint()) {}
};

TEST_F(PaintChunkerTest, Empty) {
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  EXPECT_TRUE(chunks.IsEmpty());

  chunker.ResetChunks(&chunks);
  EXPECT_TRUE(chunks.IsEmpty());
}

TEST_F(PaintChunkerTest, SingleNonEmptyRange) {
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 2, id,
                                               DefaultPaintChunkProperties())));

  Vector<PaintChunk> chunks1;
  chunker.ResetChunks(&chunks1);
  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 2, id,
                                               DefaultPaintChunkProperties())));
  EXPECT_TRUE(chunks1.IsEmpty());
}

TEST_F(PaintChunkerTest, SamePropertiesTwiceCombineIntoOneChunk) {
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.UpdateCurrentPaintChunkProperties(&id, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 3, id,
                                               DefaultPaintChunkProperties())));

  Vector<PaintChunk> chunks1;
  chunker.ResetChunks(&chunks1);
  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 3, id,
                                               DefaultPaintChunkProperties())));
  EXPECT_TRUE(chunks1.IsEmpty());
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithSinglePropertyChanging) {
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto simple_transform_node = CreateTransform(
      t0(), TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  auto simple_transform = DefaultPaintChunkProperties();
  simple_transform.SetTransform(*simple_transform_node);

  PaintChunk::Id id2(client_, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(&id2, simple_transform);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto another_transform_node = CreateTransform(
      t0(), TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  auto another_transform = DefaultPaintChunkProperties();
  another_transform.SetTransform(*another_transform_node);
  PaintChunk::Id id3(client_, DisplayItemType(3));
  chunker.UpdateCurrentPaintChunkProperties(&id3, another_transform);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 2, id1,
                                               DefaultPaintChunkProperties()),
                                  IsPaintChunk(2, 3, id2, simple_transform),
                                  IsPaintChunk(3, 4, id3, another_transform)));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithDifferentPropertyChanges) {
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto simple_transform_node = CreateTransform(
      t0(), TransformationMatrix(0, 0, 0, 0, 0, 0), FloatPoint3D(9, 8, 7));
  auto simple_transform = DefaultPaintChunkProperties();
  simple_transform.SetTransform(*simple_transform_node);
  PaintChunk::Id id2(client_, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(&id2, simple_transform);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto simple_effect_node = CreateOpacityEffect(e0(), 0.5f);
  auto simple_transform_and_effect = DefaultPaintChunkProperties();
  simple_transform_and_effect.SetTransform(*simple_transform_node);
  simple_transform_and_effect.SetEffect(*simple_effect_node);
  PaintChunk::Id id3(client_, DisplayItemType(3));
  chunker.UpdateCurrentPaintChunkProperties(&id3, simple_transform_and_effect);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto new_transform_node = CreateTransform(
      t0(), TransformationMatrix(1, 1, 0, 0, 0, 0), FloatPoint3D(9, 8, 7));
  auto simple_transform_and_effect_with_updated_transform =
      DefaultPaintChunkProperties();
  auto new_effect_node = CreateOpacityEffect(e0(), 0.5f);
  simple_transform_and_effect_with_updated_transform.SetTransform(
      *new_transform_node);
  simple_transform_and_effect_with_updated_transform.SetEffect(
      *new_effect_node);
  PaintChunk::Id id4(client_, DisplayItemType(4));
  chunker.UpdateCurrentPaintChunkProperties(
      &id4, simple_transform_and_effect_with_updated_transform);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  // Test that going back to a previous chunk property still creates a new
  // chunk.
  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            simple_transform_and_effect);
  TestChunkerDisplayItem item_after_restore(client_, DisplayItemType(10));
  chunker.IncrementDisplayItemIndex(item_after_restore);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  EXPECT_THAT(
      chunks,
      ElementsAre(
          IsPaintChunk(0, 1, id1, DefaultPaintChunkProperties()),
          IsPaintChunk(1, 3, id2, simple_transform),
          IsPaintChunk(3, 5, id3, simple_transform_and_effect),
          IsPaintChunk(5, 7, id4,
                       simple_transform_and_effect_with_updated_transform),
          IsPaintChunk(7, 9, item_after_restore.GetId(),
                       simple_transform_and_effect)));
}

TEST_F(PaintChunkerTest, BuildChunksFromNestedTransforms) {
  // Test that "nested" transforms linearize using the following
  // sequence of transforms and display items:
  // <root xform>
  //   <paint>
  //   <a xform>
  //     <paint><paint>
  //   </a xform>
  //   <paint>
  // </root xform>
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto simple_transform_node = CreateTransform(
      t0(), TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  auto simple_transform = DefaultPaintChunkProperties();
  simple_transform.SetTransform(*simple_transform_node);
  PaintChunk::Id id2(client_, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(&id2, simple_transform);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  TestChunkerDisplayItem item_after_restore(client_, DisplayItemType(10));
  chunker.IncrementDisplayItemIndex(item_after_restore);

  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 1, id1,
                                               DefaultPaintChunkProperties()),
                                  IsPaintChunk(1, 3, id2, simple_transform),
                                  IsPaintChunk(3, 4, item_after_restore.GetId(),
                                               DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChangingPropertiesWithoutItems) {
  // Test that properties can change without display items being generated.
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto first_transform_node = CreateTransform(
      t0(), TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  auto first_transform = DefaultPaintChunkProperties();
  first_transform.SetTransform(*first_transform_node);
  PaintChunk::Id id2(client_, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(nullptr, first_transform);

  auto second_transform_node = CreateTransform(
      t0(), TransformationMatrix(9, 8, 7, 6, 5, 4), FloatPoint3D(3, 2, 1));
  auto second_transform = DefaultPaintChunkProperties();
  second_transform.SetTransform(*second_transform_node);
  PaintChunk::Id id3(client_, DisplayItemType(3));
  chunker.UpdateCurrentPaintChunkProperties(&id3, second_transform);

  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 1, id1,
                                               DefaultPaintChunkProperties()),
                                  IsPaintChunk(1, 2, id3, second_transform)));
}

TEST_F(PaintChunkerTest, CreatesSeparateChunksWhenRequested) {
  // Tests that the chunker creates a separate chunks for display items which
  // require it.
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  FakeDisplayItemClient client1;
  TestDisplayItemRequiringSeparateChunk i1(client1);
  FakeDisplayItemClient client2;
  TestDisplayItemRequiringSeparateChunk i2(client2);
  FakeDisplayItemClient client3;
  TestDisplayItemRequiringSeparateChunk i3(client3);

  PaintChunk::Id id0(client_, DisplayItemType(0));
  chunker.UpdateCurrentPaintChunkProperties(&id0,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(i1);
  chunker.IncrementDisplayItemIndex(i2);
  TestChunkerDisplayItem after_i2(client_, DisplayItemType(10));
  chunker.IncrementDisplayItemIndex(after_i2);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.UpdateCurrentPaintChunkProperties(&id0,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(i3);

  EXPECT_THAT(
      chunks,
      ElementsAre(
          IsPaintChunk(0, 1, id0, DefaultPaintChunkProperties()),
          IsPaintChunk(1, 2, i1.GetId(), DefaultPaintChunkProperties()),
          IsPaintChunk(2, 3, i2.GetId(), DefaultPaintChunkProperties()),
          IsPaintChunk(3, 5, after_i2.GetId(), DefaultPaintChunkProperties()),
          IsPaintChunk(5, 6, i3.GetId(), DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ForceNewChunkWithNewId) {
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id0(client_, DisplayItemType(0));
  chunker.UpdateCurrentPaintChunkProperties(&id0,
                                            DefaultPaintChunkProperties());
  EXPECT_TRUE(chunker.WillForceNewChunk());
  EXPECT_EQ(0u, chunks.size());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  EXPECT_FALSE(chunker.WillForceNewChunk());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  EXPECT_EQ(1u, chunks.size());

  chunker.SetWillForceNewChunk(true);
  EXPECT_TRUE(chunker.WillForceNewChunk());
  EXPECT_EQ(1u, chunks.size());
  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  EXPECT_EQ(2u, chunks.size());
  EXPECT_FALSE(chunker.WillForceNewChunk());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  EXPECT_EQ(2u, chunks.size());

  chunker.SetWillForceNewChunk(true);
  PaintChunk::Id id2(client_, DisplayItemType(2));
  EXPECT_TRUE(chunker.WillForceNewChunk());
  chunker.UpdateCurrentPaintChunkProperties(&id2,
                                            DefaultPaintChunkProperties());
  EXPECT_EQ(2u, chunks.size());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  EXPECT_EQ(3u, chunks.size());
  EXPECT_FALSE(chunker.WillForceNewChunk());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  EXPECT_THAT(
      chunks,
      ElementsAre(IsPaintChunk(0, 2, id0, DefaultPaintChunkProperties()),
                  IsPaintChunk(2, 4, id1, DefaultPaintChunkProperties()),
                  IsPaintChunk(4, 6, id2, DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ForceNewChunkWithoutNewId) {
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id0(client_, DisplayItemType(0));
  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  EXPECT_TRUE(chunker.WillForceNewChunk());
  EXPECT_EQ(0u, chunks.size());
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(id0.client, id0.type));
  EXPECT_FALSE(chunker.WillForceNewChunk());
  EXPECT_EQ(1u, chunks.size());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  chunker.SetWillForceNewChunk(true);
  EXPECT_TRUE(chunker.WillForceNewChunk());
  EXPECT_EQ(1u, chunks.size());
  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(id1.client, id1.type));
  EXPECT_FALSE(chunker.WillForceNewChunk());
  EXPECT_EQ(2u, chunks.size());
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(client_, DisplayItemType(2)));

  chunker.SetWillForceNewChunk(true);
  EXPECT_TRUE(chunker.WillForceNewChunk());
  EXPECT_EQ(2u, chunks.size());
  PaintChunk::Id id2(client_, DisplayItemType(3));
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(id2.client, id2.type));
  EXPECT_FALSE(chunker.WillForceNewChunk());
  EXPECT_EQ(3u, chunks.size());
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(client_, DisplayItemType(4)));

  EXPECT_THAT(
      chunks,
      ElementsAre(IsPaintChunk(0, 2, id0, DefaultPaintChunkProperties()),
                  IsPaintChunk(2, 4, id1, DefaultPaintChunkProperties()),
                  IsPaintChunk(4, 6, id2, DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, SetAndImmediatelyUnSetWillForceNewChunk) {
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id0(client_, DisplayItemType(0));
  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  EXPECT_TRUE(chunker.WillForceNewChunk());
  EXPECT_EQ(0u, chunks.size());
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(id0.client, id0.type));
  EXPECT_FALSE(chunker.WillForceNewChunk());
  EXPECT_EQ(1u, chunks.size());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  // This should not force a new chunk. Simulates a ScopedPaintChunkHint
  // without any painting in the scope.
  chunker.SetWillForceNewChunk(true);
  chunker.SetWillForceNewChunk(false);
  EXPECT_FALSE(chunker.WillForceNewChunk());
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(client_, DisplayItemType(1)));
  EXPECT_EQ(1u, chunks.size());

  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 3, id0,
                                               DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, NoNewChunkForSamePropertyDifferentIds) {
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id0(client_, DisplayItemType(0));
  chunker.UpdateCurrentPaintChunkProperties(&id0,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 6, id0,
                                               DefaultPaintChunkProperties())));
}

// Ensure that items following a forced chunk begin using the next display
// item's id.
TEST_F(PaintChunkerTest, ChunksFollowingForcedChunk) {
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  FakeDisplayItemClient client;
  TestChunkerDisplayItem before_forced1(client, DisplayItemType(1));
  TestChunkerDisplayItem before_forced2(client, DisplayItemType(2));
  TestDisplayItemRequiringSeparateChunk forced(client);
  TestChunkerDisplayItem after_forced1(client, DisplayItemType(3));
  TestChunkerDisplayItem after_forced2(client, DisplayItemType(4));

  PaintChunk::Id id0(client, DisplayItemType(5));
  chunker.UpdateCurrentPaintChunkProperties(&id0,
                                            DefaultPaintChunkProperties());
  // Both before_forced items should be in a chunk together.
  chunker.IncrementDisplayItemIndex(before_forced1);
  chunker.IncrementDisplayItemIndex(before_forced2);
  // |forced| forces a dedicted paint chunk.
  chunker.IncrementDisplayItemIndex(forced);
  // Both after_forced items should be in a chunk together.
  chunker.IncrementDisplayItemIndex(after_forced1);
  chunker.IncrementDisplayItemIndex(after_forced2);

  EXPECT_THAT(
      chunks,
      ElementsAre(
          IsPaintChunk(0, 2, id0, DefaultPaintChunkProperties()),
          IsPaintChunk(2, 3, forced.GetId(), DefaultPaintChunkProperties()),
          IsPaintChunk(3, 5, after_forced1.GetId(),
                       DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChunkIdsSkippingCache) {
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);

  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto simple_transform_node = CreateTransform(
      t0(), TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  auto simple_transform = DefaultPaintChunkProperties();
  simple_transform.SetTransform(*simple_transform_node);

  FakeDisplayItemClient uncacheable_client;
  uncacheable_client.Invalidate(PaintInvalidationReason::kUncacheable);
  PaintChunk::Id id2(uncacheable_client, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(&id2, simple_transform);

  TestChunkerDisplayItem uncacheable_item(uncacheable_client);
  chunker.IncrementDisplayItemIndex(uncacheable_item);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  TestDisplayItemRequiringSeparateChunk uncacheable_separate_chunk_item(
      uncacheable_client);
  chunker.IncrementDisplayItemIndex(uncacheable_separate_chunk_item);

  TestChunkerDisplayItem after_separate_chunk(client_, DisplayItemType(3));
  chunker.IncrementDisplayItemIndex(after_separate_chunk);

  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  TestChunkerDisplayItem after_restore(client_, DisplayItemType(4));
  chunker.IncrementDisplayItemIndex(after_restore);

  EXPECT_THAT(
      chunks,
      ElementsAre(
          IsPaintChunk(0, 2, id1, DefaultPaintChunkProperties()),
          IsPaintChunk(2, 4, id2, simple_transform),
          IsPaintChunk(4, 5, uncacheable_separate_chunk_item.GetId(),
                       simple_transform),
          IsPaintChunk(5, 6, after_separate_chunk.GetId(), simple_transform),
          IsPaintChunk(6, 7, after_restore.GetId(),
                       DefaultPaintChunkProperties())));
  EXPECT_TRUE(chunks[0].is_cacheable);
  EXPECT_FALSE(chunks[1].is_cacheable);
  EXPECT_FALSE(chunks[2].is_cacheable);
  EXPECT_TRUE(chunks[3].is_cacheable);
  EXPECT_TRUE(chunks[4].is_cacheable);
}

TEST_F(PaintChunkerTest, AddHitTestDataToCurrentChunk) {
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);

  PaintChunk::Id id1(client_, DisplayItemType(1));

  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(
      client_, DisplayItemType(2), IntRect(0, 0, 10, 10)));

  PaintChunk::Id id2(client_, DisplayItemType(3));
  auto transform = Create2DTranslation(t0(), 10, 20);
  PropertyTreeState properties(*transform, c0(), e0());
  chunker.UpdateCurrentPaintChunkProperties(&id2, properties);
  // This is not used as id of the chunk because we already have |id2|.
  PaintChunk::Id hit_test_id(client_, DisplayItem::kHitTest);
  chunker.AddHitTestDataToCurrentChunk(hit_test_id, IntRect(10, 20, 30, 40),
                                       TouchAction::kAuto, false);
  chunker.AddHitTestDataToCurrentChunk(hit_test_id, IntRect(20, 30, 40, 50),
                                       TouchAction::kPan, false);

  chunker.SetWillForceNewChunk(true);
  PaintChunk::Id id3(client_, DisplayItemType(4));
  chunker.AddHitTestDataToCurrentChunk(id3, IntRect(40, 50, 60, 70),
                                       TouchAction::kAuto, false);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(
      client_, DisplayItemType(5), IntRect(0, 0, 10, 10)));

  HitTestData hit_test_data;
  hit_test_data.touch_action_rects = {
      {IntRect(20, 30, 40, 50), TouchAction::kPan}};
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_THAT(
        chunks,
        ElementsAre(IsPaintChunk(0, 1, id1, DefaultPaintChunkProperties(),
                                 nullptr, IntRect(0, 0, 10, 10)),
                    IsPaintChunk(1, 1, id2, properties, &hit_test_data,
                                 IntRect(10, 20, 50, 60)),
                    IsPaintChunk(1, 2, id3, properties, nullptr,
                                 IntRect(0, 0, 100, 120))));
  } else {
    EXPECT_THAT(
        chunks,
        ElementsAre(
            IsPaintChunk(0, 1, id1, DefaultPaintChunkProperties(), nullptr,
                         IntRect(0, 0, 10, 10)),
            IsPaintChunk(1, 1, id2, properties, &hit_test_data,
                         IntRect(20, 30, 40, 50)),
            IsPaintChunk(1, 2, PaintChunk::Id(client_, DisplayItemType(5)),
                         properties, nullptr, IntRect(0, 0, 10, 10))));
  }
}

TEST_F(PaintChunkerTest, AddHitTestDataToCurrentChunkWheelRegionsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kWheelEventRegions);
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);

  PaintChunk::Id id1(client_, DisplayItemType(1));

  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(
      client_, DisplayItemType(2), IntRect(0, 0, 10, 10)));

  PaintChunk::Id id2(client_, DisplayItemType(3));
  auto transform = Create2DTranslation(t0(), 10, 20);
  PropertyTreeState properties(*transform, c0(), e0());
  chunker.UpdateCurrentPaintChunkProperties(&id2, properties);
  // This is not used as id of the chunk because we already have |id2|.
  PaintChunk::Id hit_test_id(client_, DisplayItem::kHitTest);
  chunker.AddHitTestDataToCurrentChunk(hit_test_id, IntRect(10, 20, 30, 40),
                                       TouchAction::kAuto, false);
  chunker.AddHitTestDataToCurrentChunk(hit_test_id, IntRect(20, 30, 40, 50),
                                       TouchAction::kPan, false);
  chunker.AddHitTestDataToCurrentChunk(hit_test_id, IntRect(25, 35, 5, 10),
                                       TouchAction::kAuto, true);

  chunker.SetWillForceNewChunk(true);
  PaintChunk::Id id3(client_, DisplayItemType(4));
  chunker.AddHitTestDataToCurrentChunk(id3, IntRect(40, 50, 60, 70),
                                       TouchAction::kAuto, false);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(
      client_, DisplayItemType(5), IntRect(0, 0, 10, 10)));

  HitTestData hit_test_data;
  hit_test_data.touch_action_rects = {
      {IntRect(20, 30, 40, 50), TouchAction::kPan}};
  hit_test_data.wheel_event_rects = {IntRect(25, 35, 5, 10)};
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_THAT(
        chunks,
        ElementsAre(IsPaintChunk(0, 1, id1, DefaultPaintChunkProperties(),
                                 nullptr, IntRect(0, 0, 10, 10)),
                    IsPaintChunk(1, 1, id2, properties, &hit_test_data,
                                 IntRect(10, 20, 50, 60)),
                    IsPaintChunk(1, 2, id3, properties, nullptr,
                                 IntRect(0, 0, 100, 120))));
  } else {
    EXPECT_THAT(
        chunks,
        ElementsAre(
            IsPaintChunk(0, 1, id1, DefaultPaintChunkProperties(), nullptr,
                         IntRect(0, 0, 10, 10)),
            IsPaintChunk(1, 1, id2, properties, &hit_test_data,
                         IntRect(20, 30, 40, 50)),
            IsPaintChunk(1, 2, PaintChunk::Id(client_, DisplayItemType(5)),
                         properties, nullptr, IntRect(0, 0, 10, 10))));
  }
}

TEST_F(PaintChunkerTest, ChunkBoundsAndKnownToBeOpaqueAllOpaqueItems) {
  ScopedCompositeAfterPaintForTest cap(true);
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  FakeDisplayItemClient client1("client1");
  FakeDisplayItemClient client2("client2");
  FakeDisplayItemClient client3("client3");

  auto properties = DefaultPaintChunkProperties();
  chunker.UpdateCurrentPaintChunkProperties(nullptr, properties);
  // Single opaque item.
  chunker.IncrementDisplayItemIndex(TestChunkerOpaqueDisplayItem(
      client1, DisplayItemType(0), IntRect(0, 0, 100, 100)));
  chunker.SetWillForceNewChunk(true);
  // Two opaque items. No empty area in the united bounds.
  chunker.IncrementDisplayItemIndex(TestChunkerOpaqueDisplayItem(
      client1, DisplayItemType(1), IntRect(0, 0, 100, 100)));
  chunker.IncrementDisplayItemIndex(TestChunkerOpaqueDisplayItem(
      client2, DisplayItemType(2), IntRect(0, 100, 100, 50)));
  chunker.SetWillForceNewChunk(true);
  // Two opaque items. Has empty area in the united bounds.
  chunker.IncrementDisplayItemIndex(TestChunkerOpaqueDisplayItem(
      client1, DisplayItemType(3), IntRect(0, 0, 100, 100)));
  chunker.IncrementDisplayItemIndex(TestChunkerOpaqueDisplayItem(
      client3, DisplayItemType(4), IntRect(50, 50, 100, 100)));

  EXPECT_THAT(
      chunks,
      ElementsAre(
          IsPaintChunk(0, 1, PaintChunk::Id(client1, DisplayItemType(0)),
                       properties, nullptr, IntRect(0, 0, 100, 100)),
          IsPaintChunk(1, 3, PaintChunk::Id(client1, DisplayItemType(1)),
                       properties, nullptr, IntRect(0, 0, 100, 150)),
          IsPaintChunk(3, 5, PaintChunk::Id(client1, DisplayItemType(3)),
                       properties, nullptr, IntRect(0, 0, 150, 150))));
  ASSERT_EQ(3u, chunks.size());
  EXPECT_TRUE(chunks[0].known_to_be_opaque);
  EXPECT_TRUE(chunks[1].known_to_be_opaque);
  EXPECT_FALSE(chunks[2].known_to_be_opaque);
}

TEST_F(PaintChunkerTest, ChunkBoundsAndKnownToBeOpaqueWithHitTest) {
  ScopedCompositeAfterPaintForTest cap(true);
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  FakeDisplayItemClient client1("client1");
  FakeDisplayItemClient client2("client2");
  FakeDisplayItemClient client3("client3");

  auto properties = DefaultPaintChunkProperties();
  chunker.UpdateCurrentPaintChunkProperties(nullptr, properties);
  // Hit test rect only.
  chunker.AddHitTestDataToCurrentChunk(
      PaintChunk::Id(client1, DisplayItemType(0)), IntRect(10, 20, 30, 40),
      TouchAction::kAuto, false);
  chunker.SetWillForceNewChunk(true);

  // Hit test rect is smaller than the opaque item.
  chunker.IncrementDisplayItemIndex(TestChunkerOpaqueDisplayItem(
      client1, DisplayItemType(1), IntRect(0, 0, 100, 100)));
  chunker.AddHitTestDataToCurrentChunk(
      PaintChunk::Id(client1, DisplayItemType(2)), IntRect(0, 0, 50, 100),
      TouchAction::kAuto, false);
  chunker.SetWillForceNewChunk(true);
  // Hit test rect is the same as the opaque item.
  chunker.IncrementDisplayItemIndex(TestChunkerOpaqueDisplayItem(
      client1, DisplayItemType(3), IntRect(0, 0, 100, 100)));
  chunker.AddHitTestDataToCurrentChunk(
      PaintChunk::Id(client1, DisplayItemType(4)), IntRect(0, 0, 100, 100),
      TouchAction::kAuto, false);
  chunker.SetWillForceNewChunk(true);
  // Hit test rect is bigger than the opaque item.
  chunker.IncrementDisplayItemIndex(TestChunkerOpaqueDisplayItem(
      client1, DisplayItemType(5), IntRect(0, 0, 100, 100)));
  chunker.AddHitTestDataToCurrentChunk(
      PaintChunk::Id(client1, DisplayItemType(6)), IntRect(0, 100, 200, 100),
      TouchAction::kAuto, false);

  EXPECT_THAT(
      chunks,
      ElementsAre(
          IsPaintChunk(0, 0, PaintChunk::Id(client1, DisplayItemType(0)),
                       properties, nullptr, IntRect(10, 20, 30, 40)),
          IsPaintChunk(0, 1, PaintChunk::Id(client1, DisplayItemType(1)),
                       properties, nullptr, IntRect(0, 0, 100, 100)),
          IsPaintChunk(1, 2, PaintChunk::Id(client1, DisplayItemType(3)),
                       properties, nullptr, IntRect(0, 0, 100, 100)),
          IsPaintChunk(2, 3, PaintChunk::Id(client1, DisplayItemType(5)),
                       properties, nullptr, IntRect(0, 0, 200, 200))));
  ASSERT_EQ(4u, chunks.size());
  EXPECT_FALSE(chunks[0].known_to_be_opaque);
  EXPECT_TRUE(chunks[1].known_to_be_opaque);
  EXPECT_TRUE(chunks[2].known_to_be_opaque);
  EXPECT_FALSE(chunks[3].known_to_be_opaque);
}

TEST_F(PaintChunkerTest, ChunkBoundsAndKnownToBeOpaqueMixedOpaquenessItems) {
  ScopedCompositeAfterPaintForTest cap(true);
  Vector<PaintChunk> chunks;
  PaintChunker chunker(chunks);
  FakeDisplayItemClient client1("client1");
  FakeDisplayItemClient client2("client2");
  IntRect visual_rect1(0, 0, 100, 100);
  IntRect visual_rect2(50, 50, 50, 50);

  auto properties = DefaultPaintChunkProperties();
  chunker.UpdateCurrentPaintChunkProperties(nullptr, properties);
  // Single translucent item .
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(client1, DisplayItemType(1), visual_rect1));
  chunker.SetWillForceNewChunk(true);
  // Two items, one translucent, one opaque. The opaque item doesn't contain
  // the translucent item.
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(client1, DisplayItemType(2), visual_rect1));
  chunker.IncrementDisplayItemIndex(
      TestChunkerOpaqueDisplayItem(client2, DisplayItemType(3), visual_rect2));
  chunker.SetWillForceNewChunk(true);
  // Two items, one translucent, one opaque, with the same visual rect.
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(client1, DisplayItemType(4), visual_rect1));
  chunker.IncrementDisplayItemIndex(
      TestChunkerOpaqueDisplayItem(client1, DisplayItemType(5), visual_rect1));
  chunker.SetWillForceNewChunk(true);
  // Two items, one opaque, one translucent. The opaque item contains the
  // translucent item.
  chunker.IncrementDisplayItemIndex(
      TestChunkerOpaqueDisplayItem(client1, DisplayItemType(6), visual_rect1));
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(client2, DisplayItemType(7), visual_rect2));

  chunker.ResetChunks(nullptr);

  EXPECT_THAT(
      chunks,
      ElementsAre(
          IsPaintChunk(0, 1, PaintChunk::Id(client1, DisplayItemType(1)),
                       properties, nullptr, IntRect(0, 0, 100, 100)),
          IsPaintChunk(1, 3, PaintChunk::Id(client1, DisplayItemType(2)),
                       properties, nullptr, IntRect(0, 0, 100, 100)),
          IsPaintChunk(3, 5, PaintChunk::Id(client1, DisplayItemType(4)),
                       properties, nullptr, IntRect(0, 0, 100, 100)),
          IsPaintChunk(5, 7, PaintChunk::Id(client1, DisplayItemType(6)),
                       properties, nullptr, IntRect(0, 0, 100, 100))));
  ASSERT_EQ(4u, chunks.size());
  EXPECT_FALSE(chunks[0].known_to_be_opaque);
  EXPECT_FALSE(chunks[1].known_to_be_opaque);
  EXPECT_TRUE(chunks[2].known_to_be_opaque);
  EXPECT_TRUE(chunks[3].known_to_be_opaque);
}

}  // namespace
}  // namespace blink
