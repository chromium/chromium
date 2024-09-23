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
namespace {

using testing::ElementsAre;

Vector<wtf_size_t> ChunkIndices(const PendingLayer& layer) {
  Vector<wtf_size_t> indices;
  for (auto it = layer.Chunks().begin(); it != layer.Chunks().end(); ++it)
    indices.push_back(it.IndexInPaintArtifact());
  return indices;
}

bool DefaultIsCompositedScroll(
    const TransformPaintPropertyNode& scroll_translation) {
  DCHECK(scroll_translation.ScrollNode());
  return scroll_translation.HasDirectCompositingReasons();
}

bool Merge(PendingLayer& home,
           const PendingLayer& guest,
           LCDTextPreference lcd_text_preference = LCDTextPreference::kIgnored,
           PendingLayer::IsCompositedScrollFunction is_composited_scroll =
               DefaultIsCompositedScroll) {
  return home.Merge(guest, lcd_text_preference, is_composited_scroll);
}

TEST(PendingLayerTest, Merge) {
  auto& artifact = TestPaintArtifact()
                       .Chunk()
                       .Bounds(gfx::Rect(0, 0, 30, 40))
                       .RectKnownToBeOpaque(gfx::Rect(0, 0, 30, 40))
                       .Chunk()
                       .Bounds(gfx::Rect(10, 20, 30, 40))
                       .RectKnownToBeOpaque(gfx::Rect(10, 20, 30, 40))
                       .Chunk()
                       .Bounds(gfx::Rect(-5, -25, 20, 20))
                       .RectKnownToBeOpaque(gfx::Rect(-5, -25, 20, 20))
                       .Build();

  PendingLayer pending_layer(artifact, artifact.GetPaintChunks()[0]);

  EXPECT_EQ(gfx::RectF(0, 0, 30, 40), pending_layer.BoundsForTesting());
  EXPECT_THAT(ChunkIndices(pending_layer), ElementsAre(0));
  EXPECT_EQ(pending_layer.BoundsForTesting(),
            pending_layer.RectKnownToBeOpaque());

  ASSERT_TRUE(Merge(pending_layer,
                    PendingLayer(artifact, artifact.GetPaintChunks()[1])));

  // Bounds not equal to one PaintChunk.
  EXPECT_EQ(gfx::RectF(0, 0, 40, 60), pending_layer.BoundsForTesting());
  EXPECT_THAT(ChunkIndices(pending_layer), ElementsAre(0, 1));
  EXPECT_EQ(gfx::RectF(0, 0, 30, 40), pending_layer.RectKnownToBeOpaque());

  ASSERT_TRUE(Merge(pending_layer,
                    PendingLayer(artifact, artifact.GetPaintChunks()[2])));

  EXPECT_EQ(gfx::RectF(-5, -25, 45, 85), pending_layer.BoundsForTesting());
  EXPECT_THAT(ChunkIndices(pending_layer), ElementsAre(0, 1, 2));
  EXPECT_EQ(gfx::RectF(0, 0, 30, 40), pending_layer.RectKnownToBeOpaque());
}

TEST(PendingLayerTest, MergeWithGuestTransform) {
  auto* transform = Create2DTranslation(t0(), 20, 25);
  auto& artifact = TestPaintArtifact()
                       .Chunk()
                       .Bounds(gfx::Rect(0, 0, 30, 40))
                       .Chunk(*transform, c0(), e0())
                       .Bounds(gfx::Rect(0, 0, 50, 60))
                       .Build();

  PendingLayer pending_layer(artifact, artifact.GetPaintChunks()[0]);
  ASSERT_TRUE(Merge(pending_layer,
                    PendingLayer(artifact, artifact.GetPaintChunks()[1])));
  EXPECT_EQ(gfx::RectF(0, 0, 70, 85), pending_layer.BoundsForTesting());
  EXPECT_EQ(PropertyTreeState::Root(), pending_layer.GetPropertyTreeState());
}

TEST(PendingLayerTest, MergeWithHomeTransform) {
  auto* transform = Create2DTranslation(t0(), 20, 25);
  auto& artifact = TestPaintArtifact()
                       .Chunk(*transform, c0(), e0())
                       .Bounds(gfx::Rect(0, 0, 30, 40))
                       .Chunk()
                       .Bounds(gfx::Rect(0, 0, 50, 60))
                       .Build();

  PendingLayer pending_layer(artifact, artifact.GetPaintChunks()[0]);
  ASSERT_TRUE(Merge(pending_layer,
                    PendingLayer(artifact, artifact.GetPaintChunks()[1])));
  EXPECT_EQ(gfx::RectF(0, 0, 50, 65), pending_layer.BoundsForTesting());
  EXPECT_EQ(PropertyTreeState::Root(), pending_layer.GetPropertyTreeState());
}

TEST(PendingLayerTest, MergeWithBothTransforms) {
  auto* t1 = Create2DTranslation(t0(), 20, 25);
  auto* t2 = Create2DTranslation(t0(), -20, -25);
  auto& artifact = TestPaintArtifact()
                       .Chunk(*t1, c0(), e0())
                       .Bounds(gfx::Rect(0, 0, 30, 40))
                       .Chunk(*t2, c0(), e0())
                       .Bounds(gfx::Rect(0, 0, 50, 60))
                       .Build();

  PendingLayer pending_layer(artifact, artifact.GetPaintChunks()[0]);
  ASSERT_TRUE(Merge(pending_layer,
                    PendingLayer(artifact, artifact.GetPaintChunks()[1])));
  EXPECT_EQ(gfx::RectF(-20, -25, 70, 90), pending_layer.BoundsForTesting());
  EXPECT_EQ(PropertyTreeState::Root(), pending_layer.GetPropertyTreeState());
}

TEST(PendingLayerTest, MergeSparseTinyLayers) {
  auto& artifact = TestPaintArtifact()
                       .Chunk()
                       .Bounds(gfx::Rect(0, 0, 3, 4))
                       .RectKnownToBeOpaque(gfx::Rect(0, 0, 3, 4))
                       .Chunk()
                       .Bounds(gfx::Rect(20, 20, 3, 4))
                       .RectKnownToBeOpaque(gfx::Rect(20, 20, 3, 4))
                       .Build();

  PendingLayer pending_layer(artifact, artifact.GetPaintChunks()[0]);
  ASSERT_TRUE(Merge(pending_layer,
                    PendingLayer(artifact, artifact.GetPaintChunks()[1])));
  EXPECT_EQ(gfx::RectF(0, 0, 23, 24), pending_layer.BoundsForTesting());
  EXPECT_THAT(ChunkIndices(pending_layer), ElementsAre(0, 1));
}

TEST(PendingLayerTest, DontMergeSparse) {
  auto& artifact = TestPaintArtifact()
                       .Chunk()
                       .Bounds(gfx::Rect(0, 0, 30, 40))
                       .RectKnownToBeOpaque(gfx::Rect(0, 0, 30, 40))
                       .Chunk()
                       .Bounds(gfx::Rect(200, 200, 30, 40))
                       .RectKnownToBeOpaque(gfx::Rect(200, 200, 30, 40))
                       .Build();

  PendingLayer pending_layer(artifact, artifact.GetPaintChunks()[0]);
  ASSERT_FALSE(Merge(pending_layer,
                     PendingLayer(artifact, artifact.GetPaintChunks()[1])));
  EXPECT_EQ(gfx::RectF(0, 0, 30, 40), pending_layer.BoundsForTesting());
  EXPECT_EQ(artifact.GetPaintChunks()[0].properties,
            pending_layer.GetPropertyTreeState());
  EXPECT_THAT(ChunkIndices(pending_layer), ElementsAre(0));
}

TEST(PendingLayerTest, PendingLayerDontMergeSparseWithTransforms) {
  auto* t1 = Create2DTranslation(t0(), 20, 25);
  auto* t2 = Create2DTranslation(t0(), 1000, 1000);
  auto& artifact = TestPaintArtifact()
                       .Chunk(*t1, c0(), e0())
                       .Bounds(gfx::Rect(0, 0, 30, 40))
                       .Chunk(*t2, c0(), e0())
                       .Bounds(gfx::Rect(0, 0, 50, 60))
                       .Build();

  PendingLayer pending_layer(artifact, artifact.GetPaintChunks()[0]);
  ASSERT_FALSE(Merge(pending_layer,
                     PendingLayer(artifact, artifact.GetPaintChunks()[1])));
  EXPECT_EQ(gfx::RectF(0, 0, 30, 40), pending_layer.BoundsForTesting());
  EXPECT_EQ(artifact.GetPaintChunks()[0].properties,
            pending_layer.GetPropertyTreeState());
  EXPECT_THAT(ChunkIndices(pending_layer), ElementsAre(0));
}

TEST(PendingLayerTest, DontMergeSparseInCompositedEffect) {
  auto* t1 = Create2DTranslation(t0(), 20, 25);
  auto* e1 =
      CreateOpacityEffect(e0(), 1.0f, CompositingReason::kWillChangeOpacity);
  auto* t2 = Create2DTranslation(t0(), 1000, 1000);
  auto& artifact = TestPaintArtifact()
                       .Chunk(*t1, c0(), *e1)
                       .Bounds(gfx::Rect(0, 0, 30, 40))
                       .Chunk(*t2, c0(), *e1)
                       .Bounds(gfx::Rect(0, 0, 50, 60))
                       .Build();

  PendingLayer pending_layer(artifact, artifact.GetPaintChunks()[0]);
  ASSERT_FALSE(Merge(pending_layer,
                     PendingLayer(artifact, artifact.GetPaintChunks()[1])));
  EXPECT_EQ(gfx::RectF(0, 0, 30, 40), pending_layer.BoundsForTesting());
  EXPECT_EQ(artifact.GetPaintChunks()[0].properties,
            pending_layer.GetPropertyTreeState());
  EXPECT_THAT(ChunkIndices(pending_layer), ElementsAre(0));
}

TEST(PendingLayerTest, MergeSparseInNonCompositedEffect) {
  auto* t1 = Create2DTranslation(t0(), 20, 25);
  auto* t2 = Create2DTranslation(t0(), 1000, 1000);
  auto* e1 = CreateOpacityEffect(e0(), 1.0f, CompositingReason::kNone);
  auto& artifact = TestPaintArtifact()
                       .Chunk(*t1, c0(), *e1)
                       .Bounds(gfx::Rect(0, 0, 30, 40))
                       .Chunk(*t2, c0(), *e1)
                       .Bounds(gfx::Rect(0, 0, 50, 60))
                       .Build();

  PendingLayer pending_layer(artifact, artifact.GetPaintChunks()[0]);
  EXPECT_FALSE(Merge(pending_layer,
                     PendingLayer(artifact, artifact.GetPaintChunks()[1])));
}

TEST(PendingLayerTest, KnownOpaque) {
  auto& artifact = TestPaintArtifact()
                       .Chunk()
                       .Bounds(gfx::Rect(0, 0, 30, 40))
                       .Chunk()
                       .Bounds(gfx::Rect(0, 0, 25, 35))
                       .RectKnownToBeOpaque(gfx::Rect(0, 0, 25, 35))
                       .Chunk()
                       .Bounds(gfx::Rect(0, 0, 50, 60))
                       .RectKnownToBeOpaque(gfx::Rect(0, 0, 50, 60))
                       .Build();

  PendingLayer pending_layer(artifact, artifact.GetPaintChunks()[0]);
  EXPECT_TRUE(pending_layer.RectKnownToBeOpaque().IsEmpty());

  ASSERT_TRUE(Merge(pending_layer,
                    PendingLayer(artifact, artifact.GetPaintChunks()[1])));
  // Chunk 2 doesn't cover the entire layer, so not opaque.
  EXPECT_EQ(gfx::RectF(0, 0, 25, 35), pending_layer.RectKnownToBeOpaque());
  EXPECT_NE(pending_layer.BoundsForTesting(),
            pending_layer.RectKnownToBeOpaque());

  ASSERT_TRUE(Merge(pending_layer,
                    PendingLayer(artifact, artifact.GetPaintChunks()[2])));
  // Chunk 3 covers the entire layer, so now it's opaque.
  EXPECT_EQ(gfx::RectF(0, 0, 50, 60), pending_layer.BoundsForTesting());
  EXPECT_EQ(pending_layer.BoundsForTesting(),
            pending_layer.RectKnownToBeOpaque());
}

TEST(PendingLayerTest, SolidColor) {
  auto& artifact =
      TestPaintArtifact()
          .Chunk()
          .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
          .IsSolidColor()
          .Chunk()
          .RectDrawing(gfx::Rect(100, 100, 150, 150), Color::kWhite)
          .IsSolidColor()
          .Chunk()
          .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack)
          .RectDrawing(gfx::Rect(100, 100, 150, 150), Color::kWhite)
          .Build();

  PendingLayer pending_layer1(artifact, artifact.GetPaintChunks()[0]);
  EXPECT_TRUE(pending_layer1.IsSolidColor());
  EXPECT_EQ(SkColors::kBlack, pending_layer1.ComputeBackgroundColor());
  PendingLayer pending_layer2(artifact, artifact.GetPaintChunks()[1]);
  EXPECT_TRUE(pending_layer2.IsSolidColor());
  EXPECT_EQ(SkColors::kWhite, pending_layer2.ComputeBackgroundColor());
  PendingLayer pending_layer3(artifact, artifact.GetPaintChunks()[2]);
  EXPECT_FALSE(pending_layer3.IsSolidColor());
  EXPECT_TRUE(Merge(pending_layer1, pending_layer2));
  EXPECT_FALSE(pending_layer1.IsSolidColor());
}

class PendingLayerTextOpaquenessTest
    : public testing::Test,
      public testing::WithParamInterface<LCDTextPreference> {
 protected:
  LCDTextPreference GetLCDTextPreference() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PendingLayerTextOpaquenessTest,
    ::testing::Values(LCDTextPreference::kStronglyPreferred,
                      LCDTextPreference::kIgnored));

TEST_P(PendingLayerTextOpaquenessTest, OpaqueTextAndOpaqueText) {
  auto& artifact =
      TestPaintArtifact()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
          .HasText()
          .TextKnownToBeOnOpaqueBackground()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
          .HasText()
          .TextKnownToBeOnOpaqueBackground()
          .Build();
  PendingLayer layer_a(artifact, artifact.GetPaintChunks()[0]);
  PendingLayer layer_b(artifact, artifact.GetPaintChunks()[1]);
  ASSERT_TRUE(Merge(layer_a, layer_b, GetLCDTextPreference()));
  EXPECT_EQ(gfx::RectF(100, 100, 400, 400), layer_a.BoundsForTesting());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, NonOpaqueTextAndOpaqueText) {
  auto& artifact =
      TestPaintArtifact()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
          .HasText()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
          .HasText()
          .TextKnownToBeOnOpaqueBackground()
          .Build();
  PendingLayer layer_a(artifact, artifact.GetPaintChunks()[0]);
  PendingLayer layer_b(artifact, artifact.GetPaintChunks()[1]);
  bool merged = Merge(layer_a, layer_b, GetLCDTextPreference());
  if (GetLCDTextPreference() == LCDTextPreference::kStronglyPreferred) {
    // Not merged because merging would lose TextKnownToBeOnOpaqueBackground().
    ASSERT_FALSE(merged);
  } else {
    ASSERT_TRUE(merged);
    EXPECT_EQ(gfx::RectF(100, 100, 400, 400), layer_a.BoundsForTesting());
    EXPECT_FALSE(layer_a.TextKnownToBeOnOpaqueBackground());
  }
}

TEST_P(PendingLayerTextOpaquenessTest, OpaqueTextAndNonOpaqueText) {
  auto& artifact =
      TestPaintArtifact()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
          .HasText()
          .TextKnownToBeOnOpaqueBackground()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
          .HasText()
          .Build();
  PendingLayer layer_a(artifact, artifact.GetPaintChunks()[0]);
  PendingLayer layer_b(artifact, artifact.GetPaintChunks()[1]);
  bool merged = Merge(layer_a, layer_b, GetLCDTextPreference());
  if (GetLCDTextPreference() == LCDTextPreference::kStronglyPreferred) {
    // Not merged because merging would lose TextKnownToBeOnOpaqueBackground().
    ASSERT_FALSE(merged);
  } else {
    ASSERT_TRUE(merged);
    EXPECT_EQ(gfx::RectF(100, 100, 400, 400), layer_a.BoundsForTesting());
    EXPECT_FALSE(layer_a.TextKnownToBeOnOpaqueBackground());
  }
}

TEST_P(PendingLayerTextOpaquenessTest, NonOpaqueTextAndOpaqueTextCovered) {
  auto& artifact =
      TestPaintArtifact()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(200, 200, 100, 100), Color::kBlack)
          .HasText()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(100, 100, 300, 300), Color::kBlack)
          .RectKnownToBeOpaque(gfx::Rect(200, 200, 100, 100))
          .HasText()
          .TextKnownToBeOnOpaqueBackground()
          .Build();
  PendingLayer layer_a(artifact, artifact.GetPaintChunks()[0]);
  PendingLayer layer_b(artifact, artifact.GetPaintChunks()[1]);
  ASSERT_TRUE(Merge(layer_a, layer_b, GetLCDTextPreference()));
  EXPECT_EQ(gfx::RectF(100, 100, 300, 300), layer_a.BoundsForTesting());
  EXPECT_EQ(gfx::RectF(200, 200, 100, 100), layer_a.RectKnownToBeOpaque());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, OpaqueTextAndNonOpaqueTextCovered) {
  auto& artifact =
      TestPaintArtifact()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
          .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210))
          .HasText()
          .TextKnownToBeOnOpaqueBackground()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(200, 200, 100, 100), Color::kBlack)
          .HasText()
          .Build();
  PendingLayer layer_a(artifact, artifact.GetPaintChunks()[0]);
  PendingLayer layer_b(artifact, artifact.GetPaintChunks()[1]);
  ASSERT_TRUE(Merge(layer_a, layer_b, GetLCDTextPreference()));
  EXPECT_EQ(gfx::RectF(100, 100, 250, 250), layer_a.BoundsForTesting());
  EXPECT_EQ(gfx::RectF(100, 100, 210, 210), layer_a.RectKnownToBeOpaque());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, NoTextAndOpaqueText) {
  auto& artifact =
      TestPaintArtifact()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
          .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210))
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
          .HasText()
          .TextKnownToBeOnOpaqueBackground()
          .Build();
  PendingLayer layer_a(artifact, artifact.GetPaintChunks()[0]);
  PendingLayer layer_b(artifact, artifact.GetPaintChunks()[1]);
  ASSERT_TRUE(Merge(layer_a, layer_b, GetLCDTextPreference()));
  EXPECT_EQ(gfx::RectF(100, 100, 400, 400), layer_a.BoundsForTesting());
  EXPECT_EQ(gfx::RectF(100, 100, 210, 210), layer_a.RectKnownToBeOpaque());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, OpaqueTextAndNoText) {
  auto& artifact =
      TestPaintArtifact()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
          .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210))
          .HasText()
          .TextKnownToBeOnOpaqueBackground()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
          .Build();
  PendingLayer layer_a(artifact, artifact.GetPaintChunks()[0]);
  PendingLayer layer_b(artifact, artifact.GetPaintChunks()[1]);
  ASSERT_TRUE(Merge(layer_a, layer_b, GetLCDTextPreference()));
  EXPECT_EQ(gfx::RectF(100, 100, 400, 400), layer_a.BoundsForTesting());
  EXPECT_EQ(gfx::RectF(100, 100, 210, 210), layer_a.RectKnownToBeOpaque());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, NonOpaqueNoTextAndNonOpaqueText) {
  auto& artifact =
      TestPaintArtifact()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
          .HasText()
          .Build();
  PendingLayer layer_a(artifact, artifact.GetPaintChunks()[0]);
  PendingLayer layer_b(artifact, artifact.GetPaintChunks()[1]);
  ASSERT_TRUE(Merge(layer_a, layer_b, GetLCDTextPreference()));
  EXPECT_EQ(gfx::RectF(100, 100, 400, 400), layer_a.BoundsForTesting());
  EXPECT_FALSE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, NonOpaqueTextAndNonOpaqueNoText) {
  auto& artifact =
      TestPaintArtifact()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
          .HasText()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
          .Build();
  PendingLayer layer_a(artifact, artifact.GetPaintChunks()[0]);
  PendingLayer layer_b(artifact, artifact.GetPaintChunks()[1]);
  ASSERT_TRUE(Merge(layer_a, layer_b, GetLCDTextPreference()));
  EXPECT_EQ(gfx::RectF(100, 100, 400, 400), layer_a.BoundsForTesting());
  EXPECT_FALSE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, OpaqueNoTextAndNonOpaqueText) {
  auto& artifact =
      TestPaintArtifact()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
          .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210))
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(200, 200, 100, 100), Color::kBlack)
          .HasText()
          .Build();
  PendingLayer layer_a(artifact, artifact.GetPaintChunks()[0]);
  PendingLayer layer_b(artifact, artifact.GetPaintChunks()[1]);
  ASSERT_TRUE(Merge(layer_a, layer_b, GetLCDTextPreference()));
  EXPECT_EQ(gfx::RectF(100, 100, 250, 250), layer_a.BoundsForTesting());
  EXPECT_EQ(gfx::RectF(100, 100, 210, 210), layer_a.RectKnownToBeOpaque());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, NonOpaqueTextAndOpaqueNoText) {
  auto& artifact =
      TestPaintArtifact()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(200, 200, 100, 100), Color::kBlack)
          .HasText()
          .Chunk(t0(), c0(), e0())
          .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
          .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210))
          .Build();
  PendingLayer layer_a(artifact, artifact.GetPaintChunks()[0]);
  PendingLayer layer_b(artifact, artifact.GetPaintChunks()[1]);
  ASSERT_TRUE(Merge(layer_a, layer_b, GetLCDTextPreference()));
  EXPECT_EQ(gfx::RectF(100, 100, 250, 250), layer_a.BoundsForTesting());
  EXPECT_EQ(gfx::RectF(100, 100, 210, 210), layer_a.RectKnownToBeOpaque());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

TEST_P(PendingLayerTextOpaquenessTest, UnitedClippedToOpaque) {
  // Though the second chunk has text not on opaque background, and it's not
  // fully covered by the opaque rect of the first chunk, the non-opaque area
  // is not visible in the final layer, so we still allow the merge.
  auto* clip1 = CreateClip(c0(), t0(), FloatRoundedRect(175, 175, 100, 100));
  auto& artifact =
      TestPaintArtifact()
          .Chunk(t0(), *clip1, e0())
          .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
          .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210))
          .HasText()
          .TextKnownToBeOnOpaqueBackground()
          .Chunk(t0(), *clip1, e0())
          .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
          .HasText()
          .Build();
  PendingLayer layer_a(artifact, artifact.GetPaintChunks()[0]);
  PendingLayer layer_b(artifact, artifact.GetPaintChunks()[1]);
  ASSERT_TRUE(Merge(layer_a, layer_b, GetLCDTextPreference()));
  EXPECT_EQ(gfx::RectF(175, 175, 100, 100), layer_a.BoundsForTesting());
  EXPECT_EQ(gfx::RectF(175, 175, 100, 100), layer_a.RectKnownToBeOpaque());
  EXPECT_TRUE(layer_a.TextKnownToBeOnOpaqueBackground());
}

}  // namespace
}  // namespace blink
