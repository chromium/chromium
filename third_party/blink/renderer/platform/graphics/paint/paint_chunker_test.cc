// Copyright 2015 The Chromium Authors
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
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "ui/gfx/geometry/skia_conversions.h"

using testing::ElementsAre;

namespace blink {

namespace {

class PaintChunkerTest : public testing::Test {
 protected:
  Persistent<FakeDisplayItemClient> client_ =
      MakeGarbageCollected<FakeDisplayItemClient>();
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
      DisplayItemClientId client_id,
      DisplayItem::Type type = DisplayItem::kDrawingFirst,
      const gfx::Rect& visual_rect = gfx::Rect())
      : DrawingDisplayItem(client_id,
                           type,
                           visual_rect,
                           PaintRecord(),
                           RasterEffectOutset::kNone) {}
};

PaintRecord OpaquePaintRecord(const gfx::Rect& visual_rect) {
  PaintRecorder recorder;
  auto* canvas = recorder.beginRecording();
  cc::PaintFlags flags;
  flags.setColor(SK_ColorBLACK);
  canvas->drawRect(gfx::RectToSkRect(visual_rect), flags);
  return recorder.finishRecordingAsPicture();
}

class TestChunkerOpaqueDisplayItem : public DrawingDisplayItem {
 public:
  explicit TestChunkerOpaqueDisplayItem(
      DisplayItemClientId client_id,
      DisplayItem::Type type = DisplayItem::kDrawingFirst,
      const gfx::Rect& visual_rect = gfx::Rect())
      : DrawingDisplayItem(client_id,
                           type,
                           visual_rect,
                           OpaquePaintRecord(visual_rect),
                           RasterEffectOutset::kNone) {}
};

class TestDisplayItemRequiringSeparateChunk : public ForeignLayerDisplayItem {
 public:
  explicit TestDisplayItemRequiringSeparateChunk(DisplayItemClientId client_id)
      : ForeignLayerDisplayItem(client_id,
                                DisplayItem::kForeignLayerPlugin,
                                cc::Layer::Create(),
                                gfx::Point(),
                                RasterEffectOutset::kNone,
                                PaintInvalidationReason::kJustCreated) {}
};

TEST_F(PaintChunkerTest, Empty) {
  PaintChunks chunks;
  {
    PaintChunker chunker(chunks);
    EXPECT_TRUE(chunks.empty());
    chunker.Finish();
    EXPECT_TRUE(chunks.empty());
  }
  EXPECT_TRUE(chunks.empty());
}

TEST_F(PaintChunkerTest, SingleNonEmptyRange) {
  PaintChunks chunks;
  PaintChunk::Id id(client_->Id(), DisplayItemType(1));
  {
    PaintChunker chunker(chunks);
    chunker.UpdateCurrentPaintChunkProperties(id, *client_,
                                              DefaultPaintChunkProperties());
    chunker.IncrementDisplayItemIndex(*client_,
                                      TestChunkerDisplayItem(client_->Id()));
    chunker.IncrementDisplayItemIndex(*client_,
                                      TestChunkerDisplayItem(client_->Id()));

    EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(
                            0, 2, id, DefaultPaintChunkProperties())));
    chunker.Finish();
    EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(
                            0, 2, id, DefaultPaintChunkProperties())));
  }
  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 2, id,
                                               DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, SamePropertiesTwiceCombineIntoOneChunk) {
  PaintChunks chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id(client_->Id(), DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id, *client_,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  chunker.UpdateCurrentPaintChunkProperties(id, *client_,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 3, id,
                                               DefaultPaintChunkProperties())));

  chunker.Finish();
  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 3, id,
                                               DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithSinglePropertyChanging) {
  PaintChunks chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id1(client_->Id(), DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id1, *client_,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  auto* simple_transform_node = CreateTransform(
      t0(), gfx::Transform::Affine(0, 1, 2, 3, 4, 5), gfx::Point3F(9, 8, 7));
  auto simple_transform = DefaultPaintChunkProperties();
  simple_transform.SetTransform(*simple_transform_node);

  PaintChunk::Id id2(client_->Id(), DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(id2, *client_, simple_transform);
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  auto* another_transform_node = CreateTransform(
      t0(), gfx::Transform::Affine(0, 1, 2, 3, 4, 5), gfx::Point3F(9, 8, 7));
  auto another_transform = DefaultPaintChunkProperties();
  another_transform.SetTransform(*another_transform_node);
  PaintChunk::Id id3(client_->Id(), DisplayItemType(3));
  chunker.UpdateCurrentPaintChunkProperties(id3, *client_, another_transform);
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  chunker.Finish();
  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 2, id1,
                                               DefaultPaintChunkProperties()),
                                  IsPaintChunk(2, 3, id2, simple_transform),
                                  IsPaintChunk(3, 4, id3, another_transform)));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithDifferentPropertyChanges) {
  PaintChunks chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id1(client_->Id(), DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id1, *client_,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  auto* simple_transform_node = CreateTransform(
      t0(), gfx::Transform::Affine(0, 0, 0, 0, 0, 0), gfx::Point3F(9, 8, 7));
  auto simple_transform = DefaultPaintChunkProperties();
  simple_transform.SetTransform(*simple_transform_node);
  PaintChunk::Id id2(client_->Id(), DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(id2, *client_, simple_transform);
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  auto* simple_effect_node = CreateOpacityEffect(e0(), 0.5f);
  auto simple_transform_and_effect = DefaultPaintChunkProperties();
  simple_transform_and_effect.SetTransform(*simple_transform_node);
  simple_transform_and_effect.SetEffect(*simple_effect_node);
  PaintChunk::Id id3(client_->Id(), DisplayItemType(3));
  chunker.UpdateCurrentPaintChunkProperties(id3, *client_,
                                            simple_transform_and_effect);
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  auto* new_transform_node = CreateTransform(
      t0(), gfx::Transform::Affine(1, 1, 0, 0, 0, 0), gfx::Point3F(9, 8, 7));
  auto simple_transform_and_effect_with_updated_transform =
      DefaultPaintChunkProperties();
  auto* new_effect_node = CreateOpacityEffect(e0(), 0.5f);
  simple_transform_and_effect_with_updated_transform.SetTransform(
      *new_transform_node);
  simple_transform_and_effect_with_updated_transform.SetEffect(
      *new_effect_node);
  PaintChunk::Id id4(client_->Id(), DisplayItemType(4));
  chunker.UpdateCurrentPaintChunkProperties(
      id4, *client_, simple_transform_and_effect_with_updated_transform);
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  // Test that going back to a previous chunk property still creates a new
  // chunk.
  chunker.UpdateCurrentPaintChunkProperties(simple_transform_and_effect);
  TestChunkerDisplayItem item_after_restore(client_->Id(), DisplayItemType(10));
  chunker.IncrementDisplayItemIndex(*client_, item_after_restore);
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  chunker.Finish();
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
  PaintChunks chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id1(client_->Id(), DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id1, *client_,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  auto* simple_transform_node = CreateTransform(
      t0(), gfx::Transform::Affine(0, 1, 2, 3, 4, 5), gfx::Point3F(9, 8, 7));
  auto simple_transform = DefaultPaintChunkProperties();
  simple_transform.SetTransform(*simple_transform_node);
  PaintChunk::Id id2(client_->Id(), DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(id2, *client_, simple_transform);
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  chunker.UpdateCurrentPaintChunkProperties(DefaultPaintChunkProperties());
  TestChunkerDisplayItem item_after_restore(client_->Id(), DisplayItemType(10));
  chunker.IncrementDisplayItemIndex(*client_, item_after_restore);

  chunker.Finish();
  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 1, id1,
                                               DefaultPaintChunkProperties()),
                                  IsPaintChunk(1, 3, id2, simple_transform),
                                  IsPaintChunk(3, 4, item_after_restore.GetId(),
                                               DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChangingPropertiesWithoutItems) {
  // Test that properties can change without display items being generated.
  PaintChunks chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id1(client_->Id(), DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id1, *client_,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  auto* first_transform_node = CreateTransform(
      t0(), gfx::Transform::Affine(0, 1, 2, 3, 4, 5), gfx::Point3F(9, 8, 7));
  auto first_transform = DefaultPaintChunkProperties();
  first_transform.SetTransform(*first_transform_node);
  PaintChunk::Id id2(client_->Id(), DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(first_transform);

  auto* second_transform_node = CreateTransform(
      t0(), gfx::Transform::Affine(9, 8, 7, 6, 5, 4), gfx::Point3F(3, 2, 1));
  auto second_transform = DefaultPaintChunkProperties();
  second_transform.SetTransform(*second_transform_node);
  PaintChunk::Id id3(client_->Id(), DisplayItemType(3));
  chunker.UpdateCurrentPaintChunkProperties(id3, *client_, second_transform);

  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  chunker.Finish();
  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 1, id1,
                                               DefaultPaintChunkProperties()),
                                  IsPaintChunk(1, 2, id3, second_transform)));
}

TEST_F(PaintChunkerTest, CreatesSeparateChunksWhenRequested) {
  // Tests that the chunker creates a separate chunks for display items which
  // require it.
  PaintChunks chunks;
  PaintChunker chunker(chunks);
  FakeDisplayItemClient& client1 =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  TestDisplayItemRequiringSeparateChunk i1(client1.Id());
  FakeDisplayItemClient& client2 =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  TestDisplayItemRequiringSeparateChunk i2(client2.Id());
  FakeDisplayItemClient& client3 =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  TestDisplayItemRequiringSeparateChunk i3(client3.Id());

  PaintChunk::Id id0(client_->Id(), DisplayItemType(0));
  chunker.UpdateCurrentPaintChunkProperties(id0, *client_,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  chunker.IncrementDisplayItemIndex(*client_, i1);
  chunker.IncrementDisplayItemIndex(*client_, i2);
  TestChunkerDisplayItem after_i2(client_->Id(), DisplayItemType(10));
  chunker.IncrementDisplayItemIndex(*client_, after_i2);
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  chunker.UpdateCurrentPaintChunkProperties(id0, *client_,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(*client_, i3);

  chunker.Finish();
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
  PaintChunks chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id0(client_->Id(), DisplayItemType(0));
  chunker.UpdateCurrentPaintChunkProperties(id0, *client_,
                                            DefaultPaintChunkProperties());
  EXPECT_TRUE(chunker.WillForceNewChunkForTesting());
  EXPECT_EQ(0u, chunks.size());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  EXPECT_FALSE(chunker.WillForceNewChunkForTesting());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  EXPECT_EQ(1u, chunks.size());

  chunker.SetWillForceNewChunk();
  EXPECT_TRUE(chunker.WillForceNewChunkForTesting());
  EXPECT_EQ(1u, chunks.size());
  PaintChunk::Id id1(client_->Id(), DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id1, *client_,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  EXPECT_EQ(2u, chunks.size());
  EXPECT_FALSE(chunker.WillForceNewChunkForTesting());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  EXPECT_EQ(2u, chunks.size());

  chunker.SetWillForceNewChunk();
  PaintChunk::Id id2(client_->Id(), DisplayItemType(2));
  EXPECT_TRUE(chunker.WillForceNewChunkForTesting());
  chunker.UpdateCurrentPaintChunkProperties(id2, *client_,
                                            DefaultPaintChunkProperties());
  EXPECT_EQ(2u, chunks.size());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  EXPECT_EQ(3u, chunks.size());
  EXPECT_FALSE(chunker.WillForceNewChunkForTesting());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  chunker.Finish();
  EXPECT_THAT(
      chunks,
      ElementsAre(IsPaintChunk(0, 2, id0, DefaultPaintChunkProperties()),
                  IsPaintChunk(2, 4, id1, DefaultPaintChunkProperties()),
                  IsPaintChunk(4, 6, id2, DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ForceNewChunkWithoutNewId) {
  PaintChunks chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id0(client_->Id(), DisplayItemType(0));
  chunker.UpdateCurrentPaintChunkProperties(DefaultPaintChunkProperties());
  EXPECT_TRUE(chunker.WillForceNewChunkForTesting());
  EXPECT_EQ(0u, chunks.size());
  chunker.IncrementDisplayItemIndex(
      *client_, TestChunkerDisplayItem(id0.client_id, id0.type));
  EXPECT_FALSE(chunker.WillForceNewChunkForTesting());
  EXPECT_EQ(1u, chunks.size());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  chunker.SetWillForceNewChunk();
  EXPECT_TRUE(chunker.WillForceNewChunkForTesting());
  EXPECT_EQ(1u, chunks.size());
  PaintChunk::Id id1(client_->Id(), DisplayItemType(1));
  chunker.IncrementDisplayItemIndex(
      *client_, TestChunkerDisplayItem(id1.client_id, id1.type));
  EXPECT_FALSE(chunker.WillForceNewChunkForTesting());
  EXPECT_EQ(2u, chunks.size());
  chunker.IncrementDisplayItemIndex(
      *client_, TestChunkerDisplayItem(client_->Id(), DisplayItemType(2)));

  chunker.SetWillForceNewChunk();
  EXPECT_TRUE(chunker.WillForceNewChunkForTesting());
  EXPECT_EQ(2u, chunks.size());
  PaintChunk::Id id2(client_->Id(), DisplayItemType(3));
  chunker.IncrementDisplayItemIndex(
      *client_, TestChunkerDisplayItem(id2.client_id, id2.type));
  EXPECT_FALSE(chunker.WillForceNewChunkForTesting());
  EXPECT_EQ(3u, chunks.size());
  chunker.IncrementDisplayItemIndex(
      *client_, TestChunkerDisplayItem(client_->Id(), DisplayItemType(4)));

  chunker.Finish();
  EXPECT_THAT(
      chunks,
      ElementsAre(IsPaintChunk(0, 2, id0, DefaultPaintChunkProperties()),
                  IsPaintChunk(2, 4, id1, DefaultPaintChunkProperties()),
                  IsPaintChunk(4, 6, id2, DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, NoNewChunkForSamePropertyDifferentIds) {
  PaintChunks chunks;
  PaintChunker chunker(chunks);
  PaintChunk::Id id0(client_->Id(), DisplayItemType(0));
  chunker.UpdateCurrentPaintChunkProperties(id0, *client_,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  PaintChunk::Id id1(client_->Id(), DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id1, *client_,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  chunker.UpdateCurrentPaintChunkProperties(DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  chunker.Finish();
  EXPECT_THAT(chunks, ElementsAre(IsPaintChunk(0, 6, id0,
                                               DefaultPaintChunkProperties())));
}

// Ensure that items following a forced chunk begin using the next display
// item's id.
TEST_F(PaintChunkerTest, ChunksFollowingForcedChunk) {
  PaintChunks chunks;
  PaintChunker chunker(chunks);
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  TestChunkerDisplayItem before_forced1(client.Id(), DisplayItemType(1));
  TestChunkerDisplayItem before_forced2(client.Id(), DisplayItemType(2));
  TestDisplayItemRequiringSeparateChunk forced(client.Id());
  TestChunkerDisplayItem after_forced1(client.Id(), DisplayItemType(3));
  TestChunkerDisplayItem after_forced2(client.Id(), DisplayItemType(4));

  PaintChunk::Id id0(client.Id(), DisplayItemType(5));
  chunker.UpdateCurrentPaintChunkProperties(id0, *client_,
                                            DefaultPaintChunkProperties());
  // Both before_forced items should be in a chunk together.
  chunker.IncrementDisplayItemIndex(*client_, before_forced1);
  chunker.IncrementDisplayItemIndex(*client_, before_forced2);
  // |forced| forces a dedicted paint chunk.
  chunker.IncrementDisplayItemIndex(*client_, forced);
  // Both after_forced items should be in a chunk together.
  chunker.IncrementDisplayItemIndex(*client_, after_forced1);
  chunker.IncrementDisplayItemIndex(*client_, after_forced2);

  chunker.Finish();
  EXPECT_THAT(
      chunks,
      ElementsAre(
          IsPaintChunk(0, 2, id0, DefaultPaintChunkProperties()),
          IsPaintChunk(2, 3, forced.GetId(), DefaultPaintChunkProperties()),
          IsPaintChunk(3, 5, after_forced1.GetId(),
                       DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChunkIdsSkippingCache) {
  PaintChunks chunks;
  PaintChunker chunker(chunks);

  PaintChunk::Id id1(client_->Id(), DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id1, *client_,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));
  chunker.IncrementDisplayItemIndex(*client_,
                                    TestChunkerDisplayItem(client_->Id()));

  auto* simple_transform_node = CreateTransform(
      t0(), gfx::Transform::Affine(0, 1, 2, 3, 4, 5), gfx::Point3F(9, 8, 7));
  auto simple_transform = DefaultPaintChunkProperties();
  simple_transform.SetTransform(*simple_transform_node);

  FakeDisplayItemClient& uncacheable_client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  uncacheable_client.Invalidate(PaintInvalidationReason::kUncacheable);
  PaintChunk::Id id2(uncacheable_client.Id(), DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(id2, uncacheable_client,
                                            simple_transform);

  TestChunkerDisplayItem uncacheable_item(uncacheable_client.Id());
  chunker.IncrementDisplayItemIndex(uncacheable_client, uncacheable_item);
  chunker.IncrementDisplayItemIndex(
      uncacheable_client, TestChunkerDisplayItem(uncacheable_client.Id()));

  TestDisplayItemRequiringSeparateChunk uncacheable_separate_chunk_item(
      uncacheable_client.Id());
  chunker.IncrementDisplayItemIndex(uncacheable_client,
                                    uncacheable_separate_chunk_item);

  TestChunkerDisplayItem after_separate_chunk(client_->Id(),
                                              DisplayItemType(3));
  chunker.IncrementDisplayItemIndex(*client_, after_separate_chunk);

  chunker.UpdateCurrentPaintChunkProperties(DefaultPaintChunkProperties());
  TestChunkerDisplayItem after_restore(client_->Id(), DisplayItemType(4));
  chunker.IncrementDisplayItemIndex(*client_, after_restore);

  chunker.Finish();
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
  PaintChunks chunks;
  PaintChunker chunker(chunks);

  PaintChunk::Id id1(client_->Id(), DisplayItemType(1));

  chunker.UpdateCurrentPaintChunkProperties(id1, *client_,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(
      *client_, TestChunkerDisplayItem(client_->Id(), DisplayItemType(2),
                                       gfx::Rect(0, 0, 10, 10)));

  PaintChunk::Id id2(client_->Id(), DisplayItemType(3));
  auto* transform = Create2DTranslation(t0(), 10, 20);
  PropertyTreeState properties(*transform, c0(), e0());
  chunker.UpdateCurrentPaintChunkProperties(id2, *client_, properties);
  // This is not used as id of the chunk because we already have |id2|.
  PaintChunk::Id hit_test_id(client_->Id(), DisplayItem::kHitTest);
  chunker.AddHitTestDataToCurrentChunk(
      hit_test_id, *client_, gfx::Rect(10, 20, 30, 40), TouchAction::kAuto,
      false, cc::HitTestOpaqueness::kMixed);
  chunker.AddHitTestDataToCurrentChunk(
      hit_test_id, *client_, gfx::Rect(20, 30, 40, 50), TouchAction::kPan,
      false, cc::HitTestOpaqueness::kMixed);

  chunker.SetWillForceNewChunk();
  PaintChunk::Id id3(client_->Id(), DisplayItemType(4));
  chunker.AddHitTestDataToCurrentChunk(id3, *client_, gfx::Rect(40, 50, 60, 70),
                                       TouchAction::kAuto, false,
                                       cc::HitTestOpaqueness::kMixed);
  chunker.IncrementDisplayItemIndex(
      *client_, TestChunkerDisplayItem(client_->Id(), DisplayItemType(5),
                                       gfx::Rect(0, 0, 10, 10)));

  chunker.Finish();
  auto* hit_test_data = MakeGarbageCollected<HitTestData>();
  hit_test_data->touch_action_rects = {
      {gfx::Rect(20, 30, 40, 50), TouchAction::kPan}};
  EXPECT_THAT(chunks,
              ElementsAre(IsPaintChunk(0, 1, id1, DefaultPaintChunkProperties(),
                                       nullptr, gfx::Rect(0, 0, 10, 10)),
                          IsPaintChunk(1, 1, id2, properties, hit_test_data,
                                       gfx::Rect(10, 20, 50, 60)),
                          IsPaintChunk(1, 2, id3, properties, nullptr,
                                       gfx::Rect(0, 0, 100, 120))));
}

TEST_F(PaintChunkerTest, AddHitTestDataToCurrentChunkWheelRegionsEnabled) {
  PaintChunks chunks;
  PaintChunker chunker(chunks);

  PaintChunk::Id id1(client_->Id(), DisplayItemType(1));

  chunker.UpdateCurrentPaintChunkProperties(id1, *client_,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(
      *client_, TestChunkerDisplayItem(client_->Id(), DisplayItemType(2),
                                       gfx::Rect(0, 0, 10, 10)));

  PaintChunk::Id id2(client_->Id(), DisplayItemType(3));
  auto* transform = Create2DTranslation(t0(), 10, 20);
  PropertyTreeState properties(*transform, c0(), e0());
  chunker.UpdateCurrentPaintChunkProperties(id2, *client_, properties);
  // This is not used as id of the chunk because we already have |id2|.
  PaintChunk::Id hit_test_id(client_->Id(), DisplayItem::kHitTest);
  chunker.AddHitTestDataToCurrentChunk(
      hit_test_id, *client_, gfx::Rect(10, 20, 30, 40), TouchAction::kAuto,
      false, cc::HitTestOpaqueness::kMixed);
  chunker.AddHitTestDataToCurrentChunk(
      hit_test_id, *client_, gfx::Rect(20, 30, 40, 50), TouchAction::kPan,
      false, cc::HitTestOpaqueness::kMixed);
  chunker.AddHitTestDataToCurrentChunk(
      hit_test_id, *client_, gfx::Rect(25, 35, 5, 10), TouchAction::kAuto, true,
      cc::HitTestOpaqueness::kMixed);

  chunker.SetWillForceNewChunk();
  PaintChunk::Id id3(client_->Id(), DisplayItemType(4));
  chunker.AddHitTestDataToCurrentChunk(id3, *client_, gfx::Rect(40, 50, 60, 70),
                                       TouchAction::kAuto, false,
                                       cc::HitTestOpaqueness::kMixed);
  chunker.IncrementDisplayItemIndex(
      *client_, TestChunkerDisplayItem(client_->Id(), DisplayItemType(5),
                                       gfx::Rect(0, 0, 10, 10)));

  chunker.Finish();
  auto* hit_test_data = MakeGarbageCollected<HitTestData>();
  hit_test_data->touch_action_rects = {
      {gfx::Rect(20, 30, 40, 50), TouchAction::kPan}};
  hit_test_data->wheel_event_rects = {gfx::Rect(25, 35, 5, 10)};
  EXPECT_THAT(chunks,
              ElementsAre(IsPaintChunk(0, 1, id1, DefaultPaintChunkProperties(),
                                       nullptr, gfx::Rect(0, 0, 10, 10)),
                          IsPaintChunk(1, 1, id2, properties, hit_test_data,
                                       gfx::Rect(10, 20, 50, 60)),
                          IsPaintChunk(1, 2, id3, properties, nullptr,
                                       gfx::Rect(0, 0, 100, 120))));
}

TEST_F(PaintChunkerTest, ChunkBoundsAndKnownToBeOpaqueAllOpaqueItems) {
  PaintChunks chunks;
  PaintChunker chunker(chunks);
  FakeDisplayItemClient& client1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("client1");
  FakeDisplayItemClient& client2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("client2");
  FakeDisplayItemClient& client3 =
      *MakeGarbageCollected<FakeDisplayItemClient>("client3");

  auto properties = DefaultPaintChunkProperties();
  chunker.UpdateCurrentPaintChunkProperties(properties);
  // Single opaque item.
  chunker.IncrementDisplayItemIndex(
      client1, TestChunkerOpaqueDisplayItem(client1.Id(), DisplayItemType(0),
                                            gfx::Rect(0, 0, 100, 100)));
  chunker.SetWillForceNewChunk();
  // Two opaque items. No empty area in the united bounds.
  chunker.IncrementDisplayItemIndex(
      client1, TestChunkerOpaqueDisplayItem(client1.Id(), DisplayItemType(1),
                                            gfx::Rect(0, 0, 100, 100)));
  chunker.IncrementDisplayItemIndex(
      client2, TestChunkerOpaqueDisplayItem(client2.Id(), DisplayItemType(2),
                                            gfx::Rect(0, 100, 100, 50)));
  chunker.SetWillForceNewChunk();
  // Two opaque items. Has empty area in the united bounds.
  chunker.IncrementDisplayItemIndex(
      client1, TestChunkerOpaqueDisplayItem(client1.Id(), DisplayItemType(3),
                                            gfx::Rect(0, 0, 100, 100)));
  chunker.IncrementDisplayItemIndex(
      client3, TestChunkerOpaqueDisplayItem(client3.Id(), DisplayItemType(4),
                                            gfx::Rect(50, 50, 100, 100)));

  chunker.Finish();
  EXPECT_THAT(
      chunks,
      ElementsAre(
          IsPaintChunk(0, 1, PaintChunk::Id(client1.Id(), DisplayItemType(0)),
                       properties, nullptr, gfx::Rect(0, 0, 100, 100)),
          IsPaintChunk(1, 3, PaintChunk::Id(client1.Id(), DisplayItemType(1)),
                       properties, nullptr, gfx::Rect(0, 0, 100, 150)),
          IsPaintChunk(3, 5, PaintChunk::Id(client1.Id(), DisplayItemType(3)),
                       properties, nullptr, gfx::Rect(0, 0, 150, 150))));
  ASSERT_EQ(3u, chunks.size());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), chunks[0].rect_known_to_be_opaque);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 150), chunks[1].rect_known_to_be_opaque);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), chunks[2].rect_known_to_be_opaque);
}

TEST_F(PaintChunkerTest, ChunkBoundsAndKnownToBeOpaqueWithHitTest) {
  PaintChunks chunks;
  PaintChunker chunker(chunks);
  FakeDisplayItemClient& client1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("client1");

  auto properties = DefaultPaintChunkProperties();
  chunker.UpdateCurrentPaintChunkProperties(properties);
  // Hit test rect only.
  chunker.AddHitTestDataToCurrentChunk(
      PaintChunk::Id(client1.Id(), DisplayItemType(0)), client1,
      gfx::Rect(10, 20, 30, 40), TouchAction::kAuto, false,
      cc::HitTestOpaqueness::kMixed);
  chunker.SetWillForceNewChunk();

  // Hit test rect is smaller than the opaque item.
  chunker.IncrementDisplayItemIndex(
      client1, TestChunkerOpaqueDisplayItem(client1.Id(), DisplayItemType(1),
                                            gfx::Rect(0, 0, 100, 100)));
  chunker.AddHitTestDataToCurrentChunk(
      PaintChunk::Id(client1.Id(), DisplayItemType(2)), client1,
      gfx::Rect(0, 0, 50, 100), TouchAction::kAuto, false,
      cc::HitTestOpaqueness::kMixed);
  chunker.SetWillForceNewChunk();
  // Hit test rect is the same as the opaque item.
  chunker.IncrementDisplayItemIndex(
      client1, TestChunkerOpaqueDisplayItem(client1.Id(), DisplayItemType(3),
                                            gfx::Rect(0, 0, 100, 100)));
  chunker.AddHitTestDataToCurrentChunk(
      PaintChunk::Id(client1.Id(), DisplayItemType(4)), client1,
      gfx::Rect(0, 0, 100, 100), TouchAction::kAuto, false,
      cc::HitTestOpaqueness::kMixed);
  chunker.SetWillForceNewChunk();
  // Hit test rect is bigger than the opaque item.
  chunker.IncrementDisplayItemIndex(
      client1, TestChunkerOpaqueDisplayItem(client1.Id(), DisplayItemType(5),
                                            gfx::Rect(0, 0, 100, 100)));
  chunker.AddHitTestDataToCurrentChunk(
      PaintChunk::Id(client1.Id(), DisplayItemType(6)), client1,
      gfx::Rect(0, 100, 200, 100), TouchAction::kAuto, false,
      cc::HitTestOpaqueness::kMixed);

  chunker.Finish();

  EXPECT_THAT(
      chunks,
      ElementsAre(
          IsPaintChunk(0, 0, PaintChunk::Id(client1.Id(), DisplayItemType(0)),
                       properties, nullptr, gfx::Rect(10, 20, 30, 40)),
          IsPaintChunk(0, 1, PaintChunk::Id(client1.Id(), DisplayItemType(1)),
                       properties, nullptr, gfx::Rect(0, 0, 100, 100)),
          IsPaintChunk(1, 2, PaintChunk::Id(client1.Id(), DisplayItemType(3)),
                       properties, nullptr, gfx::Rect(0, 0, 100, 100)),
          IsPaintChunk(2, 3, PaintChunk::Id(client1.Id(), DisplayItemType(5)),
                       properties, nullptr, gfx::Rect(0, 0, 200, 200))));
  ASSERT_EQ(4u, chunks.size());
  EXPECT_EQ(gfx::Rect(), chunks[0].rect_known_to_be_opaque);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), chunks[1].rect_known_to_be_opaque);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), chunks[2].rect_known_to_be_opaque);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), chunks[3].rect_known_to_be_opaque);
}

TEST_F(PaintChunkerTest, ChunkBoundsAndKnownToBeOpaqueMixedOpaquenessItems) {
  PaintChunks chunks;
  PaintChunker chunker(chunks);
  FakeDisplayItemClient& client1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("client1");
  FakeDisplayItemClient& client2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("client2");
  gfx::Rect visual_rect1(0, 0, 100, 100);
  gfx::Rect visual_rect2(50, 50, 50, 50);

  auto properties = DefaultPaintChunkProperties();
  chunker.UpdateCurrentPaintChunkProperties(properties);
  // Single translucent item .
  chunker.IncrementDisplayItemIndex(
      *client_,
      TestChunkerDisplayItem(client1.Id(), DisplayItemType(1), visual_rect1));
  chunker.SetWillForceNewChunk();
  // Two items, one translucent, one opaque. The opaque item doesn't contain
  // the translucent item.
  chunker.IncrementDisplayItemIndex(
      *client_,
      TestChunkerDisplayItem(client1.Id(), DisplayItemType(2), visual_rect1));
  chunker.IncrementDisplayItemIndex(
      *client_, TestChunkerOpaqueDisplayItem(client2.Id(), DisplayItemType(3),
                                             visual_rect2));
  chunker.SetWillForceNewChunk();
  // Two items, one translucent, one opaque, with the same visual rect.
  chunker.IncrementDisplayItemIndex(
      *client_,
      TestChunkerDisplayItem(client1.Id(), DisplayItemType(4), visual_rect1));
  chunker.IncrementDisplayItemIndex(
      *client_, TestChunkerOpaqueDisplayItem(client1.Id(), DisplayItemType(5),
                                             visual_rect1));
  chunker.SetWillForceNewChunk();
  // Two items, one opaque, one translucent. The opaque item contains the
  // translucent item.
  chunker.IncrementDisplayItemIndex(
      *client_, TestChunkerOpaqueDisplayItem(client1.Id(), DisplayItemType(6),
                                             visual_rect1));
  chunker.IncrementDisplayItemIndex(
      *client_,
      TestChunkerDisplayItem(client2.Id(), DisplayItemType(7), visual_rect2));

  chunker.Finish();
  EXPECT_THAT(
      chunks,
      ElementsAre(
          IsPaintChunk(0, 1, PaintChunk::Id(client1.Id(), DisplayItemType(1)),
                       properties, nullptr, gfx::Rect(0, 0, 100, 100)),
          IsPaintChunk(1, 3, PaintChunk::Id(client1.Id(), DisplayItemType(2)),
                       properties, nullptr, gfx::Rect(0, 0, 100, 100)),
          IsPaintChunk(3, 5, PaintChunk::Id(client1.Id(), DisplayItemType(4)),
                       properties, nullptr, gfx::Rect(0, 0, 100, 100)),
          IsPaintChunk(5, 7, PaintChunk::Id(client1.Id(), DisplayItemType(6)),
                       properties, nullptr, gfx::Rect(0, 0, 100, 100))));
  ASSERT_EQ(4u, chunks.size());
  EXPECT_EQ(gfx::Rect(), chunks[0].rect_known_to_be_opaque);
  EXPECT_EQ(gfx::Rect(50, 50, 50, 50), chunks[1].rect_known_to_be_opaque);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), chunks[2].rect_known_to_be_opaque);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), chunks[3].rect_known_to_be_opaque);
}

}  // namespace
}  // namespace blink
