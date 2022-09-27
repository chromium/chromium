// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/pending_layer.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/test_paint_artifact.h"

namespace blink {

using testing::ElementsAre;

static Vector<wtf_size_t> ChunkIndices(const PendingLayer& layer) {
  Vector<wtf_size_t> indices;
  for (auto it = layer.Chunks().begin(); it != layer.Chunks().end(); ++it)
    indices.push_back(it.IndexInPaintArtifact());
  return indices;
}

TEST(PendingLayerTest, Merge) {
  TestPaintArtifact artifact;
  artifact.Chunk()
      .Bounds(gfx::Rect(0, 0, 30, 40))
      .RectKnownToBeOpaque(gfx::Rect(0, 0, 30, 40));
  artifact.Chunk()
      .Bounds(gfx::Rect(10, 20, 30, 40))
      .RectKnownToBeOpaque(gfx::Rect(10, 20, 30, 40));
  artifact.Chunk()
      .Bounds(gfx::Rect(-5, -25, 20, 20))
      .RectKnownToBeOpaque(gfx::Rect(-5, -25, 20, 20));
  PaintChunkSubset chunks(artifact.Build());

  PendingLayer pending_layer(chunks, chunks.begin());

  EXPECT_EQ(gfx::RectF(0, 0, 30, 40), pending_layer.BoundsForTesting());
  EXPECT_THAT(ChunkIndices(pending_layer), ElementsAre(0));
  EXPECT_EQ(pending_layer.BoundsForTesting(),
            pending_layer.RectKnownToBeOpaque());

  ASSERT_TRUE(pending_layer.Merge(PendingLayer(chunks, chunks.begin() + 1)));

  // Bounds not equal to one PaintChunk.
  EXPECT_EQ(gfx::RectF(0, 0, 40, 60), pending_layer.BoundsForTesting());
  EXPECT_THAT(ChunkIndices(pending_layer), ElementsAre(0, 1));
  EXPECT_EQ(gfx::RectF(0, 0, 30, 40), pending_layer.RectKnownToBeOpaque());

  ASSERT_TRUE(pending_layer.Merge(PendingLayer(chunks, chunks.begin() + 2)));

  EXPECT_EQ(gfx::RectF(-5, -25, 45, 85), pending_layer.BoundsForTesting());
  EXPECT_THAT(ChunkIndices(pending_layer), ElementsAre(0, 1, 2));
  EXPECT_EQ(gfx::RectF(0, 0, 30, 40), pending_layer.RectKnownToBeOpaque());
}

TEST(PendingLayerTest, MergeWithGuestTransform) {
  TestPaintArtifact artifact;
  artifact.Chunk().Bounds(gfx::Rect(0, 0, 30, 40));
  auto transform = Create2DTranslation(t0(), 20, 25);
  artifact.Chunk(*transform, c0(), e0()).Bounds(gfx::Rect(0, 0, 50, 60));
  PaintChunkSubset chunks(artifact.Build());

  PendingLayer pending_layer(chunks, chunks.begin());
  ASSERT_TRUE(pending_layer.Merge(PendingLayer(chunks, chunks.begin() + 1)));
  EXPECT_EQ(gfx::RectF(0, 0, 70, 85), pending_layer.BoundsForTesting());
  EXPECT_EQ(PropertyTreeState::Root(), pending_layer.GetPropertyTreeState());
}

TEST(PendingLayerTest, MergeWithHomeTransform) {
  TestPaintArtifact artifact;
  auto transform = Create2DTranslation(t0(), 20, 25);
  artifact.Chunk(*transform, c0(), e0()).Bounds(gfx::Rect(0, 0, 30, 40));
  artifact.Chunk().Bounds(gfx::Rect(0, 0, 50, 60));
  PaintChunkSubset chunks(artifact.Build());

  PendingLayer pending_layer(chunks, chunks.begin());
  ASSERT_TRUE(pending_layer.Merge(PendingLayer(chunks, chunks.begin() + 1)));
  EXPECT_EQ(gfx::RectF(0, 0, 50, 65), pending_layer.BoundsForTesting());
  EXPECT_EQ(PropertyTreeState::Root(), pending_layer.GetPropertyTreeState());
}

TEST(PendingLayerTest, MergeWithBothTransforms) {
  TestPaintArtifact artifact;
  auto t1 = Create2DTranslation(t0(), 20, 25);
  artifact.Chunk(*t1, c0(), e0()).Bounds(gfx::Rect(0, 0, 30, 40));
  auto t2 = Create2DTranslation(t0(), -20, -25);
  artifact.Chunk(*t2, c0(), e0()).Bounds(gfx::Rect(0, 0, 50, 60));
  PaintChunkSubset chunks(artifact.Build());

  PendingLayer pending_layer(chunks, chunks.begin());
  ASSERT_TRUE(pending_layer.Merge(PendingLayer(chunks, chunks.begin() + 1)));
  EXPECT_EQ(gfx::RectF(-20, -25, 70, 90), pending_layer.BoundsForTesting());
  EXPECT_EQ(PropertyTreeState::Root(), pending_layer.GetPropertyTreeState());
}

TEST(PendingLayerTest, MergeSparseTinyLayers) {
  TestPaintArtifact artifact;
  artifact.Chunk()
      .Bounds(gfx::Rect(0, 0, 3, 4))
      .RectKnownToBeOpaque(gfx::Rect(0, 0, 3, 4));
  artifact.Chunk()
      .Bounds(gfx::Rect(20, 20, 3, 4))
      .RectKnownToBeOpaque(gfx::Rect(20, 20, 3, 4));
  PaintChunkSubset chunks(artifact.Build());

  PendingLayer pending_layer(chunks, chunks.begin());
  ASSERT_TRUE(pending_layer.Merge(PendingLayer(chunks, chunks.begin() + 1)));
  EXPECT_EQ(gfx::RectF(0, 0, 23, 24), pending_layer.BoundsForTesting());
  EXPECT_THAT(ChunkIndices(pending_layer), ElementsAre(0, 1));
}

TEST(PendingLayerTest, DontMergeSparse) {
  TestPaintArtifact artifact;
  artifact.Chunk()
      .Bounds(gfx::Rect(0, 0, 30, 40))
      .RectKnownToBeOpaque(gfx::Rect(0, 0, 30, 40));
  artifact.Chunk()
      .Bounds(gfx::Rect(200, 200, 30, 40))
      .RectKnownToBeOpaque(gfx::Rect(200, 200, 30, 40));
  PaintChunkSubset chunks(artifact.Build());

  PendingLayer pending_layer(chunks, chunks.begin());
  ASSERT_FALSE(pending_layer.Merge(PendingLayer(chunks, chunks.begin() + 1)));
  EXPECT_EQ(gfx::RectF(0, 0, 30, 40), pending_layer.BoundsForTesting());
  EXPECT_EQ(chunks.begin()->properties, pending_layer.GetPropertyTreeState());
  EXPECT_THAT(ChunkIndices(pending_layer), ElementsAre(0));
}

TEST(PendingLayerTest, PendingLayerDontMergeSparseWithTransforms) {
  TestPaintArtifact artifact;
  auto t1 = Create2DTranslation(t0(), 20, 25);
  artifact.Chunk(*t1, c0(), e0()).Bounds(gfx::Rect(0, 0, 30, 40));
  auto t2 = Create2DTranslation(t0(), 1000, 1000);
  artifact.Chunk(*t2, c0(), e0()).Bounds(gfx::Rect(0, 0, 50, 60));
  PaintChunkSubset chunks(artifact.Build());

  PendingLayer pending_layer(chunks, chunks.begin());
  ASSERT_FALSE(pending_layer.Merge(PendingLayer(chunks, chunks.begin() + 1)));
  EXPECT_EQ(gfx::RectF(0, 0, 30, 40), pending_layer.BoundsForTesting());
  EXPECT_EQ(chunks.begin()->properties, pending_layer.GetPropertyTreeState());
  EXPECT_THAT(ChunkIndices(pending_layer), ElementsAre(0));
}

TEST(PendingLayerTest, DontMergeSparseInCompositedEffect) {
  TestPaintArtifact artifact;
  auto t1 = Create2DTranslation(t0(), 20, 25);
  auto e1 =
      CreateOpacityEffect(e0(), 1.0f, CompositingReason::kWillChangeOpacity);
  artifact.Chunk(*t1, c0(), *e1).Bounds(gfx::Rect(0, 0, 30, 40));
  auto t2 = Create2DTranslation(t0(), 1000, 1000);
  artifact.Chunk(*t2, c0(), *e1).Bounds(gfx::Rect(0, 0, 50, 60));
  PaintChunkSubset chunks(artifact.Build());

  PendingLayer pending_layer(chunks, chunks.begin());
  ASSERT_FALSE(pending_layer.Merge(PendingLayer(chunks, chunks.begin() + 1)));
  EXPECT_EQ(gfx::RectF(0, 0, 30, 40), pending_layer.BoundsForTesting());
  EXPECT_EQ(chunks.begin()->properties, pending_layer.GetPropertyTreeState());
  EXPECT_THAT(ChunkIndices(pending_layer), ElementsAre(0));
}

TEST(PendingLayerTest, MergeSparseInNonCompositedEffect) {
  TestPaintArtifact artifact;
  auto t1 = Create2DTranslation(t0(), 20, 25);
  auto t2 = Create2DTranslation(t0(), 1000, 1000);
  auto e1 = CreateOpacityEffect(e0(), 1.0f, CompositingReason::kNone);
  artifact.Chunk(*t1, c0(), *e1).Bounds(gfx::Rect(0, 0, 30, 40));
  artifact.Chunk(*t2, c0(), *e1).Bounds(gfx::Rect(0, 0, 50, 60));
  PaintChunkSubset chunks(artifact.Build());

  PendingLayer pending_layer(chunks, chunks.begin());
  EXPECT_FALSE(pending_layer.Merge(PendingLayer(chunks, chunks.begin() + 1)));
}

TEST(PendingLayerTest, KnownOpaque) {
  TestPaintArtifact artifact;
  artifact.Chunk().Bounds(gfx::Rect(0, 0, 30, 40));
  artifact.Chunk()
      .Bounds(gfx::Rect(0, 0, 25, 35))
      .RectKnownToBeOpaque(gfx::Rect(0, 0, 25, 35));
  artifact.Chunk()
      .Bounds(gfx::Rect(0, 0, 50, 60))
      .RectKnownToBeOpaque(gfx::Rect(0, 0, 50, 60));
  PaintChunkSubset chunks(artifact.Build());

  PendingLayer pending_layer(chunks, chunks.begin());
  EXPECT_TRUE(pending_layer.RectKnownToBeOpaque().IsEmpty());

  ASSERT_TRUE(pending_layer.Merge(PendingLayer(chunks, chunks.begin() + 1)));
  // Chunk 2 doesn't cover the entire layer, so not opaque.
  EXPECT_EQ(gfx::RectF(0, 0, 25, 35), pending_layer.RectKnownToBeOpaque());
  EXPECT_NE(pending_layer.BoundsForTesting(),
            pending_layer.RectKnownToBeOpaque());

  ASSERT_TRUE(pending_layer.Merge(PendingLayer(chunks, chunks.begin() + 2)));
  // Chunk 3 covers the entire layer, so now it's opaque.
  EXPECT_EQ(gfx::RectF(0, 0, 50, 60), pending_layer.BoundsForTesting());
  EXPECT_EQ(pending_layer.BoundsForTesting(),
            pending_layer.RectKnownToBeOpaque());
}

class PendingLayerTextOpaquenessTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 protected:
  bool PrefersLCDText() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PendingLayerTextOpaquenessTest,
                         ::testing::Values(false, true));

TEST_P(PendingLayerTextOpaquenessTest, OpaqueTextAndOpaqueText) {
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
      .HasText()
      .TextKnownToBeOnOpaqueBackground()
      .Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
      .HasText()
      .TextKnownToBeOnOpaqueBackground();
  PaintChunkSubset chunks(artifact.Build());
  PendingLayer layer_a(chunks, chunks.begin());
  PendingLayer layer_b(chunks, chunks.begin() + 1);
  ASSERT_TRUE(layer_a.Merge(layer_b, PrefersLCDText()));
  EXPECT_EQ(gfx::RectF(100, 100, 400, 400), layer_a.BoundsForTesting());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, NonOpaqueTextAndOpaqueText) {
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
      .HasText()
      .Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
      .HasText()
      .TextKnownToBeOnOpaqueBackground();
  PaintChunkSubset chunks(artifact.Build());
  PendingLayer layer_a(chunks, chunks.begin());
  PendingLayer layer_b(chunks, chunks.begin() + 1);
  bool merged = layer_a.Merge(layer_b, PrefersLCDText());
  if (PrefersLCDText()) {
    // Not merged because merging would lose TextKnownToBeOnOpaqueBackground().
    ASSERT_FALSE(merged);
  } else {
    ASSERT_TRUE(merged);
    EXPECT_EQ(gfx::RectF(100, 100, 400, 400), layer_a.BoundsForTesting());
    EXPECT_FALSE(layer_a.TextKnownToBeOnOpaqueBackground());
  }
}

TEST_P(PendingLayerTextOpaquenessTest, OpaqueTextAndNonOpaqueText) {
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
      .HasText()
      .TextKnownToBeOnOpaqueBackground()
      .Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
      .HasText();
  PaintChunkSubset chunks(artifact.Build());
  PendingLayer layer_a(chunks, chunks.begin());
  PendingLayer layer_b(chunks, chunks.begin() + 1);
  bool merged = layer_a.Merge(layer_b, PrefersLCDText());
  if (PrefersLCDText()) {
    // Not merged because merging would lose TextKnownToBeOnOpaqueBackground().
    ASSERT_FALSE(merged);
  } else {
    ASSERT_TRUE(merged);
    EXPECT_EQ(gfx::RectF(100, 100, 400, 400), layer_a.BoundsForTesting());
    EXPECT_FALSE(layer_a.TextKnownToBeOnOpaqueBackground());
  }
}

TEST_P(PendingLayerTextOpaquenessTest, NonOpaqueTextAndOpaqueTextCovered) {
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(200, 200, 100, 100), Color::kBlack)
      .HasText()
      .Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(100, 100, 300, 300), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(200, 200, 100, 100))
      .HasText()
      .TextKnownToBeOnOpaqueBackground();
  PaintChunkSubset chunks(artifact.Build());
  PendingLayer layer_a(chunks, chunks.begin());
  PendingLayer layer_b(chunks, chunks.begin() + 1);
  ASSERT_TRUE(layer_a.Merge(layer_b, PrefersLCDText()));
  EXPECT_EQ(gfx::RectF(100, 100, 300, 300), layer_a.BoundsForTesting());
  EXPECT_EQ(gfx::RectF(200, 200, 100, 100), layer_a.RectKnownToBeOpaque());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, OpaqueTextAndNonOpaqueTextCovered) {
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210))
      .HasText()
      .TextKnownToBeOnOpaqueBackground()
      .Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(200, 200, 100, 100), Color::kBlack)
      .HasText();
  PaintChunkSubset chunks(artifact.Build());
  PendingLayer layer_a(chunks, chunks.begin());
  PendingLayer layer_b(chunks, chunks.begin() + 1);
  ASSERT_TRUE(layer_a.Merge(layer_b, PrefersLCDText()));
  EXPECT_EQ(gfx::RectF(100, 100, 250, 250), layer_a.BoundsForTesting());
  EXPECT_EQ(gfx::RectF(100, 100, 210, 210), layer_a.RectKnownToBeOpaque());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, NoTextAndOpaqueText) {
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210))
      .Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
      .HasText()
      .TextKnownToBeOnOpaqueBackground();
  PaintChunkSubset chunks(artifact.Build());
  PendingLayer layer_a(chunks, chunks.begin());
  PendingLayer layer_b(chunks, chunks.begin() + 1);
  ASSERT_TRUE(layer_a.Merge(layer_b, PrefersLCDText()));
  EXPECT_EQ(gfx::RectF(100, 100, 400, 400), layer_a.BoundsForTesting());
  EXPECT_EQ(gfx::RectF(100, 100, 210, 210), layer_a.RectKnownToBeOpaque());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, OpaqueTextAndNoText) {
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210))
      .HasText()
      .TextKnownToBeOnOpaqueBackground()
      .Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack);
  PaintChunkSubset chunks(artifact.Build());
  PendingLayer layer_a(chunks, chunks.begin());
  PendingLayer layer_b(chunks, chunks.begin() + 1);
  ASSERT_TRUE(layer_a.Merge(layer_b, PrefersLCDText()));
  EXPECT_EQ(gfx::RectF(100, 100, 400, 400), layer_a.BoundsForTesting());
  EXPECT_EQ(gfx::RectF(100, 100, 210, 210), layer_a.RectKnownToBeOpaque());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, NonOpaqueNoTextAndNonOpaqueText) {
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
      .Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
      .HasText();
  PaintChunkSubset chunks(artifact.Build());
  PendingLayer layer_a(chunks, chunks.begin());
  PendingLayer layer_b(chunks, chunks.begin() + 1);
  ASSERT_TRUE(layer_a.Merge(layer_b, PrefersLCDText()));
  EXPECT_EQ(gfx::RectF(100, 100, 400, 400), layer_a.BoundsForTesting());
  EXPECT_FALSE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, NonOpaqueTextAndNonOpaqueNoText) {
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
      .HasText()
      .Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack);
  PaintChunkSubset chunks(artifact.Build());
  PendingLayer layer_a(chunks, chunks.begin());
  PendingLayer layer_b(chunks, chunks.begin() + 1);
  ASSERT_TRUE(layer_a.Merge(layer_b, PrefersLCDText()));
  EXPECT_EQ(gfx::RectF(100, 100, 400, 400), layer_a.BoundsForTesting());
  EXPECT_FALSE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, OpaqueNoTextAndNonOpaqueText) {
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210))
      .Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(200, 200, 100, 100), Color::kBlack)
      .HasText();
  PaintChunkSubset chunks(artifact.Build());
  PendingLayer layer_a(chunks, chunks.begin());
  PendingLayer layer_b(chunks, chunks.begin() + 1);
  ASSERT_TRUE(layer_a.Merge(layer_b, PrefersLCDText()));
  EXPECT_EQ(gfx::RectF(100, 100, 250, 250), layer_a.BoundsForTesting());
  EXPECT_EQ(gfx::RectF(100, 100, 210, 210), layer_a.RectKnownToBeOpaque());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, NonOpaqueTextAndOpaqueNoText) {
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(200, 200, 100, 100), Color::kBlack)
      .HasText()
      .Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210));
  PaintChunkSubset chunks(artifact.Build());
  PendingLayer layer_a(chunks, chunks.begin());
  PendingLayer layer_b(chunks, chunks.begin() + 1);
  ASSERT_TRUE(layer_a.Merge(layer_b, PrefersLCDText()));
  EXPECT_EQ(gfx::RectF(100, 100, 250, 250), layer_a.BoundsForTesting());
  EXPECT_EQ(gfx::RectF(100, 100, 210, 210), layer_a.RectKnownToBeOpaque());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, UnitedClippedToOpaque) {
  // Though the second chunk has text not on opaque background, and it's not
  // fully covered by the opaque rect of the first chunk, the non-opaque area
  // is not visible in the final layer, so we still allow the merge.
  auto clip1 = CreateClip(c0(), t0(), FloatRoundedRect(175, 175, 100, 100));
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clip1, e0())
      .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210))
      .HasText()
      .TextKnownToBeOnOpaqueBackground()
      .Chunk(t0(), *clip1, e0())
      .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
      .HasText();
  PaintChunkSubset chunks(artifact.Build());
  PendingLayer layer_a(chunks, chunks.begin());
  PendingLayer layer_b(chunks, chunks.begin() + 1);
  ASSERT_TRUE(layer_a.Merge(layer_b, PrefersLCDText()));
  EXPECT_EQ(gfx::RectF(175, 175, 100, 100), layer_a.BoundsForTesting());
  EXPECT_EQ(gfx::RectF(100, 100, 210, 210), layer_a.RectKnownToBeOpaque());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

}  // namespace blink
