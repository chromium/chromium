// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidator.h"

#include <utility>
#include "base/functional/function_ref.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/test_paint_artifact.h"
#include "ui/gfx/geometry/rect_conversions.h"

using testing::ElementsAre;

namespace blink {

static constexpr gfx::Vector2dF kDefaultLayerOffset(-9999, -7777);
static constexpr gfx::Size kDefaultLayerBounds(18888, 16666);

class RasterInvalidatorTest : public testing::Test,
                              public PaintTestConfigurations,
                              public RasterInvalidator::Callback {
 public:
  static PropertyTreeState DefaultPropertyTreeState() {
    return PropertyTreeState::Root();
  }

  void ClearGeometryMapperCache() {
    GeometryMapperTransformCache::ClearCache();
    GeometryMapperClipCache::ClearCache();
  }

  void SetUp() override { ClearGeometryMapperCache(); }
  void TearDown() override { ClearGeometryMapperCache(); }

  void FinishCycle(const PaintChunkSubset& chunks) {
    ClearGeometryMapperCache();
    ++sequence_number_;
    for (const auto& chunk : chunks) {
      const_cast<PaintChunk&>(chunk).client_is_just_created = false;
      chunk.properties.ClearChangedToRoot(sequence_number_);
    }
  }

  const Vector<RasterInvalidationInfo>& TrackedRasterInvalidations() {
    DCHECK(invalidator_->GetTracking());
    return invalidator_->GetTracking()->Invalidations();
  }

  void InvalidateRect(const gfx::Rect& rect) override {}

  Persistent<RasterInvalidator> invalidator_ =
      MakeGarbageCollected<RasterInvalidator>(*this);
  int sequence_number_ = 1;
};

INSTANTIATE_PAINT_TEST_SUITE_P(RasterInvalidatorTest);

using MapFunction = base::FunctionRef<void(gfx::Rect&)>;
void MapNothing(gfx::Rect&) {}
void PrintTo(MapFunction, std::ostream*) {}

static gfx::Rect ChunkRectToLayer(const gfx::Rect& rect,
                                  const gfx::Vector2dF& layer_offset,
                                  MapFunction mapper = MapNothing) {
  auto r = rect;
  mapper(r);
  gfx::RectF float_rect(r);
  float_rect.Offset(layer_offset);
  return gfx::ToEnclosingRect(float_rect);
}

static bool CheckChunkInvalidation(
    const RasterInvalidationInfo& info,
    const PaintChunkSubset& chunks,
    wtf_size_t index,
    PaintInvalidationReason reason,
    const gfx::Vector2dF& layer_offset,
    const std::optional<gfx::Rect>& chunk_rect = std::nullopt,
    MapFunction mapper = MapNothing) {
  const auto& chunk = chunks[index];
  return ChunkRectToLayer(chunk_rect ? *chunk_rect : chunk.drawable_bounds,
                          layer_offset, mapper) == info.rect &&
         chunk.id.client_id == info.client_id && reason == info.reason;
}

MATCHER_P5(ChunkInvalidation, chunks, index, reason, layer_offset, mapper, "") {
  return CheckChunkInvalidation(arg, chunks, index, reason, layer_offset,
                                std::nullopt, mapper);
}

MATCHER_P4(ChunkInvalidation, chunks, index, reason, layer_offset, "") {
  return CheckChunkInvalidation(arg, chunks, index, reason, layer_offset);
}

MATCHER_P3(ChunkInvalidation, chunks, index, reason, "") {
  return CheckChunkInvalidation(arg, chunks, index, reason,
                                -kDefaultLayerOffset);
}

MATCHER_P3(IncrementalInvalidation, chunks, index, chunk_rect, "") {
  return CheckChunkInvalidation(arg, chunks, index,
                                PaintInvalidationReason::kIncremental,
                                -kDefaultLayerOffset, chunk_rect);
}

TEST_P(RasterInvalidatorTest, ImplicitFullLayerInvalidation) {
  auto& artifact = TestPaintArtifact().Chunk(0).Build();
  PaintChunkSubset chunks(artifact);

  invalidator_->SetTracksRasterInvalidations(true);
  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  DisplayItemClientId client_id = chunks.begin()->id.client_id;
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(RasterInvalidationInfo{
                  client_id, artifact.ClientDebugName(client_id),
                  gfx::Rect(kDefaultLayerBounds),
                  PaintInvalidationReason::kFullLayer}));
  FinishCycle(chunks);
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(RasterInvalidatorTest, LayerBounds) {
  PaintChunkSubset chunks(TestPaintArtifact().Chunk(0).Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  FinishCycle(chunks);

  invalidator_->SetTracksRasterInvalidations(true);
  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  // No raster invalidations needed if layer origin doesn't change.
  EXPECT_TRUE(TrackedRasterInvalidations().empty());

  auto new_layer_offset = kDefaultLayerOffset;
  new_layer_offset.Add(gfx::Vector2dF(66, 77));
  invalidator_->Generate(chunks, new_layer_offset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  // Change of layer origin causes change of chunk0's transform to layer.
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(
          ChunkInvalidation(chunks, 0, PaintInvalidationReason::kPaintProperty),
          ChunkInvalidation(chunks, 0, PaintInvalidationReason::kPaintProperty,
                            -new_layer_offset)));
  FinishCycle(chunks);
}

TEST_P(RasterInvalidatorTest, LayerOffsetChangeWithCachedSubsequence) {
  PaintChunkSubset chunks(TestPaintArtifact().Chunk(0).Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  FinishCycle(chunks);

  invalidator_->SetTracksRasterInvalidations(true);
  auto new_layer_offset = kDefaultLayerOffset;
  new_layer_offset.Add(gfx::Vector2dF(66, 77));
  PaintChunkSubset new_chunks(
      TestPaintArtifact().Chunk(0).IsMovedFromCachedSubsequence().Build());

  invalidator_->Generate(new_chunks, new_layer_offset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  // Change of layer origin causes change of chunk0's transform to layer.
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(
          ChunkInvalidation(chunks, 0, PaintInvalidationReason::kPaintProperty),
          ChunkInvalidation(chunks, 0, PaintInvalidationReason::kPaintProperty,
                            -new_layer_offset)));
  FinishCycle(chunks);
}

TEST_P(RasterInvalidatorTest, LayerStateChangeWithCachedSubsequence) {
  auto* t1 = Create2DTranslation(t0(), 100, 50);
  PropertyTreeState chunk_state(*t1, c0(), e0());
  PaintChunkSubset chunks(
      TestPaintArtifact().Chunk(0).Properties(chunk_state).Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  FinishCycle(chunks);

  invalidator_->SetTracksRasterInvalidations(true);
  auto new_layer_state = chunk_state;
  PaintChunkSubset new_chunks(TestPaintArtifact()
                                  .Chunk(0)
                                  .Properties(chunk_state)
                                  .IsMovedFromCachedSubsequence()
                                  .Build());

  invalidator_->Generate(new_chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         new_layer_state);
  // Change of layer state causes change of chunk0's transform to layer.
  auto old_mapper = [](gfx::Rect& r) { r.Offset(100, 50); };
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(
          ChunkInvalidation(chunks, 0, PaintInvalidationReason::kPaintProperty,
                            -kDefaultLayerOffset, MapFunction(old_mapper)),
          ChunkInvalidation(chunks, 0,
                            PaintInvalidationReason::kPaintProperty)));
  FinishCycle(chunks);
}

TEST_P(RasterInvalidatorTest, ReorderChunks) {
  PaintChunkSubset chunks(
      TestPaintArtifact().Chunk(0).Chunk(1).Chunk(2).Build());
  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  FinishCycle(chunks);

  // Swap chunk 1 and 2.
  invalidator_->SetTracksRasterInvalidations(true);
  PaintChunkSubset new_chunks(TestPaintArtifact()
                                  .Chunk(0)
                                  .Chunk(2)
                                  .Chunk(1)
                                  .Bounds(gfx::Rect(11, 22, 33, 44))
                                  .DrawableBounds(gfx::Rect(11, 22, 33, 44))
                                  .Build());
  invalidator_->Generate(new_chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(new_chunks, 2,
                                    PaintInvalidationReason::kChunkAppeared),
                  ChunkInvalidation(
                      chunks, 1, PaintInvalidationReason::kChunkDisappeared)));
  FinishCycle(new_chunks);
}

TEST_P(RasterInvalidatorTest, ReorderChunkSubsequences) {
  PaintChunkSubset chunks(
      TestPaintArtifact().Chunk(0).Chunk(1).Chunk(2).Chunk(3).Chunk(4).Build());
  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  FinishCycle(chunks);

  // Swap chunk (1,2) (changed) and (3,4) (moved from cached subsequence).
  invalidator_->SetTracksRasterInvalidations(true);
  PaintChunkSubset new_chunks(TestPaintArtifact()
                                  .Chunk(0)
                                  .Chunk(3)
                                  .IsMovedFromCachedSubsequence()
                                  .Chunk(4)
                                  .IsMovedFromCachedSubsequence()
                                  .Chunk(1)
                                  .Bounds(gfx::Rect(11, 22, 33, 44))
                                  .DrawableBounds(gfx::Rect(11, 22, 33, 44))
                                  .Chunk(2)
                                  .Build());
  invalidator_->Generate(new_chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(new_chunks, 3,
                                    PaintInvalidationReason::kChunkAppeared),
                  ChunkInvalidation(new_chunks, 4,
                                    PaintInvalidationReason::kChunkAppeared),
                  ChunkInvalidation(chunks, 1,
                                    PaintInvalidationReason::kChunkDisappeared),
                  ChunkInvalidation(
                      chunks, 2, PaintInvalidationReason::kChunkDisappeared)));
  FinishCycle(new_chunks);
}

TEST_P(RasterInvalidatorTest, ScrollDown) {
  PaintChunkSubset chunks(
      TestPaintArtifact().Chunk(10).Chunk(11).Chunk(12).Build());
  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  FinishCycle(chunks);

  // Simulate the cull rect moves down on scroll. Chunk(13) appears and
  // Chunk(10) disappears.
  invalidator_->SetTracksRasterInvalidations(true);
  PaintChunkSubset new_chunks(TestPaintArtifact()
                                  .Chunk(11)
                                  .IsMovedFromCachedSubsequence()
                                  .Chunk(12)
                                  .IsMovedFromCachedSubsequence()
                                  .Chunk(13)
                                  .Build());
  invalidator_->Generate(new_chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(new_chunks, 2,
                                    PaintInvalidationReason::kChunkAppeared),
                  ChunkInvalidation(
                      chunks, 0, PaintInvalidationReason::kChunkDisappeared)));
  FinishCycle(new_chunks);
}

TEST_P(RasterInvalidatorTest, ScrollUp) {
  PaintChunkSubset chunks(
      TestPaintArtifact().Chunk(11).Chunk(12).Chunk(13).Build());
  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  FinishCycle(chunks);

  // Simulate the cull rect moves up on scroll. Chunk(10) appears and Chunk(13)
  // disappears.
  invalidator_->SetTracksRasterInvalidations(true);
  PaintChunkSubset new_chunks(TestPaintArtifact()
                                  .Chunk(10)
                                  .Chunk(11)
                                  .IsMovedFromCachedSubsequence()
                                  .Chunk(12)
                                  .IsMovedFromCachedSubsequence()
                                  .Build());
  invalidator_->Generate(new_chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(new_chunks, 0,
                                    PaintInvalidationReason::kChunkAppeared),
                  ChunkInvalidation(
                      chunks, 2, PaintInvalidationReason::kChunkDisappeared)));
  FinishCycle(new_chunks);
}

TEST_P(RasterInvalidatorTest, ChunkAppearAndDisappear) {
  PaintChunkSubset chunks(
      TestPaintArtifact().Chunk(0).Chunk(1).Chunk(2).Build());
  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  FinishCycle(chunks);

  // Chunk 1 and 2 disappeared, 3 and 4 appeared.
  invalidator_->SetTracksRasterInvalidations(true);
  PaintChunkSubset new_chunks(
      TestPaintArtifact().Chunk(0).Chunk(3).Chunk(4).Build());
  invalidator_->Generate(new_chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(new_chunks, 1,
                                    PaintInvalidationReason::kChunkAppeared),
                  ChunkInvalidation(new_chunks, 2,
                                    PaintInvalidationReason::kChunkAppeared),
                  ChunkInvalidation(chunks, 1,
                                    PaintInvalidationReason::kChunkDisappeared),
                  ChunkInvalidation(
                      chunks, 2, PaintInvalidationReason::kChunkDisappeared)));
  FinishCycle(new_chunks);
}

TEST_P(RasterInvalidatorTest, InvalidateDrawableBounds) {
  gfx::Rect drawable_bounds(11, 22, 33, 44);
  gfx::Rect bounds(0, 0, 100, 100);
  PaintChunkSubset chunks(TestPaintArtifact()
                              .Chunk(0)
                              .Chunk(1)
                              .Bounds(bounds)
                              .DrawableBounds(drawable_bounds)
                              .Build());
  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  FinishCycle(chunks);

  invalidator_->SetTracksRasterInvalidations(true);
  PaintChunkSubset new_chunks(TestPaintArtifact()
                                  .Chunk(0)
                                  .Chunk(2)
                                  .Bounds(bounds)
                                  .DrawableBounds(drawable_bounds)
                                  .Build());
  invalidator_->Generate(new_chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  // ChunkInvalidation uses the drawable_bounds. We expect raster invalidations
  // based on drawable_bounds instead of bounds.
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(new_chunks, 1,
                                    PaintInvalidationReason::kChunkAppeared),
                  ChunkInvalidation(
                      chunks, 1, PaintInvalidationReason::kChunkDisappeared)));
  FinishCycle(new_chunks);
}

TEST_P(RasterInvalidatorTest, ChunkAppearAtEnd) {
  PaintChunkSubset chunks(TestPaintArtifact().Chunk(0).Build());
  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  FinishCycle(chunks);

  invalidator_->SetTracksRasterInvalidations(true);
  PaintChunkSubset new_chunks(
      TestPaintArtifact().Chunk(0).Chunk(1).Chunk(2).Build());
  invalidator_->Generate(new_chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(new_chunks, 1,
                                    PaintInvalidationReason::kChunkAppeared),
                  ChunkInvalidation(new_chunks, 2,
                                    PaintInvalidationReason::kChunkAppeared)));
  FinishCycle(new_chunks);
}

TEST_P(RasterInvalidatorTest, UncacheableChunks) {
  PaintChunkSubset chunks(
      TestPaintArtifact().Chunk(0).Chunk(1).Uncacheable().Chunk(2).Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  FinishCycle(chunks);

  invalidator_->SetTracksRasterInvalidations(true);
  PaintChunkSubset new_chunks(
      TestPaintArtifact().Chunk(0).Chunk(2).Chunk(1).Uncacheable().Build());
  invalidator_->Generate(new_chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         DefaultPropertyTreeState());
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(new_chunks, 2,
                                    PaintInvalidationReason::kChunkUncacheable),
                  ChunkInvalidation(
                      chunks, 1, PaintInvalidationReason::kChunkUncacheable)));
  FinishCycle(new_chunks);
}

// Tests the path based on ClipPaintPropertyNode::Changed().
TEST_P(RasterInvalidatorTest, ClipPropertyChangeRounded) {
  FloatRoundedRect::Radii radii(gfx::SizeF(1, 2), gfx::SizeF(2, 3),
                                gfx::SizeF(3, 4), gfx::SizeF(4, 5));
  FloatRoundedRect clip_rect(gfx::RectF(-1000, -1000, 2000, 2000), radii);
  auto* clip0 = CreateClip(c0(), t0(), clip_rect);
  auto* clip2 = CreateClip(*clip0, t0(), clip_rect);

  PropertyTreeState layer_state(t0(), *clip0, e0());
  PropertyTreeState chunk_state(t0(), *clip2, e0());
  PaintChunkSubset chunks(TestPaintArtifact()
                              .Chunk(0)
                              .Properties(layer_state)
                              .Chunk(1)
                              .Properties(layer_state)
                              .Chunk(2)
                              .Properties(chunk_state)
                              .Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  // Change both clip0 and clip2.
  invalidator_->SetTracksRasterInvalidations(true);
  FloatRoundedRect new_clip_rect(gfx::RectF(-2000, -2000, 4000, 4000), radii);
  UpdateClip(*clip0, new_clip_rect);
  UpdateClip(*clip2, new_clip_rect);

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  // Property change in the layer state should not trigger raster invalidation.
  // |clip2| change should trigger raster invalidation.
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  chunks, 2, PaintInvalidationReason::kPaintProperty)));
  invalidator_->SetTracksRasterInvalidations(false);
  FinishCycle(chunks);

  // Change chunk1's properties to use a different property tree state.
  PaintChunkSubset new_chunks1(TestPaintArtifact()
                                   .Chunk(0)
                                   .Properties(layer_state)
                                   .Chunk(1)
                                   .Properties(chunk_state)
                                   .Chunk(2)
                                   .Properties(chunk_state)
                                   .Build());

  invalidator_->SetTracksRasterInvalidations(true);
  invalidator_->Generate(new_chunks1, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  new_chunks1, 1, PaintInvalidationReason::kPaintProperty)));
  invalidator_->SetTracksRasterInvalidations(false);
  FinishCycle(new_chunks1);
}

// Tests the path detecting change of PaintChunkInfo::chunk_to_layer_clip.
TEST_P(RasterInvalidatorTest, ClipPropertyChangeSimple) {
  FloatRoundedRect clip_rect(-1000, -1000, 2000, 2000);
  auto* clip0 = CreateClip(c0(), t0(), clip_rect);
  auto* clip1 = CreateClip(*clip0, t0(), clip_rect);

  PropertyTreeState layer_state = PropertyTreeState::Root();
  PaintChunkSubset chunks(
      TestPaintArtifact()
          .Chunk(0)
          .Properties(t0(), *clip0, e0())
          .Bounds(gfx::ToEnclosingRect(clip_rect.Rect()))
          .DrawableBounds(gfx::ToEnclosingRect(clip_rect.Rect()))
          .Chunk(1)
          .Properties(t0(), *clip1, e0())
          .Bounds(gfx::ToEnclosingRect(clip_rect.Rect()))
          .DrawableBounds(gfx::ToEnclosingRect(clip_rect.Rect()))
          .Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  // Change clip1 to bigger, which is still bound by clip0, resulting no actual
  // visual change.
  invalidator_->SetTracksRasterInvalidations(true);
  FloatRoundedRect new_clip_rect1(-2000, -2000, 4000, 4000);
  UpdateClip(*clip1, new_clip_rect1);

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().empty());
  FinishCycle(chunks);

  // Change clip1 to smaller.
  FloatRoundedRect new_clip_rect2(-500, -500, 1000, 1000);
  UpdateClip(*clip1, new_clip_rect2);

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  // |clip1| change should trigger incremental raster invalidation.
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(
          IncrementalInvalidation(chunks, 1,
                                  gfx::Rect(-1000, -1000, 2000, 500)),
          IncrementalInvalidation(chunks, 1, gfx::Rect(-1000, -500, 500, 1000)),
          IncrementalInvalidation(chunks, 1, gfx::Rect(500, -500, 500, 1000)),
          IncrementalInvalidation(chunks, 1,
                                  gfx::Rect(-1000, 500, 2000, 500))));
  invalidator_->SetTracksRasterInvalidations(false);
  FinishCycle(chunks);

  // Change clip1 bigger at one side.
  FloatRoundedRect new_clip_rect3(-500, -500, 2000, 1000);
  UpdateClip(*clip1, new_clip_rect3);

  invalidator_->SetTracksRasterInvalidations(true);
  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  // |clip1| change should trigger incremental raster invalidation.
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(IncrementalInvalidation(
                  chunks, 1, gfx::Rect(500, -500, 500, 1000))));
  invalidator_->SetTracksRasterInvalidations(false);
  FinishCycle(chunks);
}

TEST_P(RasterInvalidatorTest, ClipChangeOnCachedSubsequence) {
  FloatRoundedRect clip_rect(-1000, -1000, 2000, 2000);
  auto* c1 = CreateClip(c0(), t0(), clip_rect);

  PropertyTreeState layer_state = PropertyTreeState::Root();
  PaintChunkSubset chunks(
      TestPaintArtifact()
          .Chunk(0)
          .Properties(t0(), *c1, e0())
          .Bounds(gfx::ToEnclosingRect(clip_rect.Rect()))
          .DrawableBounds(gfx::ToEnclosingRect(clip_rect.Rect()))
          .IsMovedFromCachedSubsequence()
          .Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  invalidator_->SetTracksRasterInvalidations(true);
  FloatRoundedRect new_clip_rect(-500, -500, 1000, 1000);
  UpdateClip(*c1, new_clip_rect);
  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(
          IncrementalInvalidation(chunks, 0,
                                  gfx::Rect(-1000, -1000, 2000, 500)),
          IncrementalInvalidation(chunks, 0, gfx::Rect(-1000, -500, 500, 1000)),
          IncrementalInvalidation(chunks, 0, gfx::Rect(500, -500, 500, 1000)),
          IncrementalInvalidation(chunks, 0,
                                  gfx::Rect(-1000, 500, 2000, 500))));
  invalidator_->SetTracksRasterInvalidations(false);
  FinishCycle(chunks);
}

// Tests the path detecting change of PaintChunkInfo::chunk_to_layer_clip.
// The chunk bounds is bigger than the clip because of the outset for raster
// effects, so incremental invalidation is not suitable.
TEST_P(RasterInvalidatorTest, ClipPropertyChangeWithOutsetForRasterEffects) {
  FloatRoundedRect clip_rect(-1000, -1000, 2000, 2000);
  auto* clip = CreateClip(c0(), t0(), clip_rect);

  PropertyTreeState layer_state = PropertyTreeState::Root();
  PaintChunkSubset chunks(
      TestPaintArtifact()
          .Chunk(0)
          .Properties(t0(), *clip, e0())
          .Bounds(gfx::ToEnclosingRect(clip_rect.Rect()))
          .DrawableBounds(gfx::ToEnclosingRect(clip_rect.Rect()))
          .SetRasterEffectOutset(RasterEffectOutset::kWholePixel)
          .Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  invalidator_->SetTracksRasterInvalidations(true);
  FloatRoundedRect new_clip_rect(-2000, -2000, 4000, 4000);
  UpdateClip(*clip, new_clip_rect);

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  auto mapper = [](gfx::Rect& r) { r.Outset(1); };
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  chunks, 0, PaintInvalidationReason::kPaintProperty,
                  -kDefaultLayerOffset, MapFunction(mapper))));
  invalidator_->SetTracksRasterInvalidations(false);
  FinishCycle(chunks);
}

TEST_P(RasterInvalidatorTest, ClipLocalTransformSpaceChange) {
  auto* t1 = CreateTransform(t0(), gfx::Transform());
  auto* t2 = CreateTransform(*t1, gfx::Transform());

  FloatRoundedRect::Radii radii(gfx::SizeF(1, 2), gfx::SizeF(2, 3),
                                gfx::SizeF(3, 4), gfx::SizeF(4, 5));
  FloatRoundedRect clip_rect(gfx::RectF(-1000, -1000, 2000, 2000), radii);
  auto* c1 = CreateClip(c0(), *t1, clip_rect);

  PropertyTreeState layer_state = DefaultPropertyTreeState();
  PaintChunkSubset chunks(
      TestPaintArtifact().Chunk(0).Properties(*t2, *c1, e0()).Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  // Change both t1 and t2 but keep t1*t2 unchanged, to test change of
  // LocalTransformSpace of c1.
  invalidator_->SetTracksRasterInvalidations(true);
  t1->Update(t0(), TransformPaintPropertyNode::State{
                       {MakeTranslationMatrix(-10, -20)}});
  t2->Update(
      *t1, TransformPaintPropertyNode::State{{MakeTranslationMatrix(10, 20)}});

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  chunks, 0, PaintInvalidationReason::kPaintProperty)));
  invalidator_->SetTracksRasterInvalidations(false);
}

// This is based on ClipLocalTransformSpaceChange, but tests the no-invalidation
// path by letting the clip's LocalTransformSpace be the same as the chunk's
// transform.
TEST_P(RasterInvalidatorTest, ClipLocalTransformSpaceChangeNoInvalidation) {
  auto* t1 = CreateTransform(t0(), gfx::Transform());
  auto* t2 = CreateTransform(*t1, gfx::Transform());

  FloatRoundedRect::Radii radii(gfx::SizeF(1, 2), gfx::SizeF(2, 3),
                                gfx::SizeF(3, 4), gfx::SizeF(4, 5));
  FloatRoundedRect clip_rect(gfx::RectF(-1000, -1000, 2000, 2000), radii);
  // This set is different from ClipLocalTransformSpaceChange.
  auto* c1 = CreateClip(c0(), *t2, clip_rect);

  PropertyTreeState layer_state = DefaultPropertyTreeState();
  PaintChunkSubset chunks(
      TestPaintArtifact().Chunk(0).Properties(*t2, *c1, e0()).Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  // Change both t1 and t2 but keep t1*t2 unchanged.
  invalidator_->SetTracksRasterInvalidations(true);
  t1->Update(t0(), TransformPaintPropertyNode::State{
                       {MakeTranslationMatrix(-10, -20)}});
  t2->Update(
      *t1, TransformPaintPropertyNode::State{{MakeTranslationMatrix(10, 20)}});

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().empty());
  FinishCycle(chunks);
}

TEST_P(RasterInvalidatorTest, TransformPropertyChange) {
  auto* layer_transform = CreateTransform(t0(), MakeScaleMatrix(5));
  auto* transform0 = Create2DTranslation(*layer_transform, 10, 20);
  auto* transform1 = Create2DTranslation(*transform0, -50, -60);

  PropertyTreeState layer_state(*layer_transform, c0(), e0());
  PaintChunkSubset chunks(TestPaintArtifact()
                              .Chunk(0)
                              .Properties(*transform0, c0(), e0())
                              .Chunk(1)
                              .Properties(*transform1, c0(), e0())
                              .Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  // Change layer_transform should not cause raster invalidation in the layer.
  invalidator_->SetTracksRasterInvalidations(true);
  layer_transform->Update(
      *layer_transform->Parent(),
      TransformPaintPropertyNode::State{{MakeScaleMatrix(10)}});

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().empty());
  FinishCycle(chunks);

  // Inserting another node between layer_transform and transform0 and letting
  // the new node become the transform of the layer state should not cause
  // raster invalidation in the layer. This simulates a composited layer is
  // scrolled from its original location.
  auto* new_layer_transform = Create2DTranslation(*layer_transform, -100, -200);
  layer_state = PropertyTreeState(*new_layer_transform, c0(), e0());
  transform0->Update(*new_layer_transform,
                     TransformPaintPropertyNode::State{{transform0->Matrix()}});

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().empty());
  FinishCycle(chunks);

  // Removing transform nodes above the layer state should not cause raster
  // invalidation in the layer.
  layer_state = DefaultPropertyTreeState();
  transform0->Update(layer_state.Transform(),
                     TransformPaintPropertyNode::State{{transform0->Matrix()}});

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().empty());
  FinishCycle(chunks);

  // Change transform0 and transform1, while keeping the combined transform0
  // and transform1 unchanged for chunk 2. We should invalidate only chunk 0
  // for changed paint property.
  transform0->Update(
      layer_state.Transform(),
      TransformPaintPropertyNode::State{
          {transform0->Matrix() * MakeTranslationMatrix(20, 30)}});
  transform1->Update(*transform0, TransformPaintPropertyNode::State{
                                      {transform1->Matrix() *
                                       MakeTranslationMatrix(-20, -30)}});

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  auto mapper0 = [](gfx::Rect& r) { r.Offset(10, 20); };
  auto mapper1 = [](gfx::Rect& r) { r.Offset(30, 50); };
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(
          ChunkInvalidation(chunks, 0, PaintInvalidationReason::kPaintProperty,
                            -kDefaultLayerOffset, MapFunction(mapper0)),
          ChunkInvalidation(chunks, 0, PaintInvalidationReason::kPaintProperty,
                            -kDefaultLayerOffset, MapFunction(mapper1))));
  invalidator_->SetTracksRasterInvalidations(false);
  FinishCycle(chunks);
}

TEST_P(RasterInvalidatorTest, TransformPropertyTinyChange) {
  auto* layer_transform = CreateTransform(t0(), MakeScaleMatrix(5));
  auto* chunk_transform = Create2DTranslation(*layer_transform, 10, 20);

  PropertyTreeState layer_state(*layer_transform, c0(), e0());
  PaintChunkSubset chunks(TestPaintArtifact()
                              .Chunk(0)
                              .Properties(*chunk_transform, c0(), e0())
                              .Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  // Change chunk_transform by tiny difference, which should be ignored.
  invalidator_->SetTracksRasterInvalidations(true);

  auto matrix_with_tiny_change = [](const gfx::Transform matrix) {
    gfx::Transform m = matrix;
    m.Translate(0.0001, -0.0001);
    m.Scale(1.000001);
    m.Rotate(0.000001);
    return m;
  };

  chunk_transform->Update(
      layer_state.Transform(),
      TransformPaintPropertyNode::State{
          {matrix_with_tiny_change(chunk_transform->Matrix())}});

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().empty());
  FinishCycle(chunks);

  // Tiny differences should accumulate and cause invalidation when the
  // accumulation is large enough.
  bool invalidated = false;
  for (int i = 0; i < 100 && !invalidated; i++) {
    chunk_transform->Update(
        layer_state.Transform(),
        TransformPaintPropertyNode::State{
            {matrix_with_tiny_change(chunk_transform->Matrix())}});
    invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                           layer_state);
    invalidated = !TrackedRasterInvalidations().empty();
    FinishCycle(chunks);
  }
  EXPECT_TRUE(invalidated);
}

TEST_P(RasterInvalidatorTest, TransformPropertyTinyChangeScale) {
  auto* layer_transform = CreateTransform(t0(), MakeScaleMatrix(5));
  auto* chunk_transform =
      CreateTransform(*layer_transform, MakeScaleMatrix(1e-6));
  gfx::Rect chunk_bounds(0, 0, 10000000, 10000000);

  PropertyTreeState layer_state(*layer_transform, c0(), e0());
  PaintChunkSubset chunks(TestPaintArtifact()
                              .Chunk(0)
                              .Properties(*chunk_transform, c0(), e0())
                              .Bounds(chunk_bounds)
                              .DrawableBounds(chunk_bounds)
                              .Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  // Scale change from 1e-6 to 2e-6 should be treated as significant.
  invalidator_->SetTracksRasterInvalidations(true);
  chunk_transform->Update(
      layer_state.Transform(),
      TransformPaintPropertyNode::State{{MakeScaleMatrix(2e-6)}});

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_FALSE(TrackedRasterInvalidations().empty());
  invalidator_->SetTracksRasterInvalidations(false);
  FinishCycle(chunks);

  // Scale change from 2e-6 to 2e-6 + 1e-15 should be ignored.
  invalidator_->SetTracksRasterInvalidations(true);
  chunk_transform->Update(
      layer_state.Transform(),
      TransformPaintPropertyNode::State{{MakeScaleMatrix(2e-6 + 1e-15)}});

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().empty());
  invalidator_->SetTracksRasterInvalidations(false);
  FinishCycle(chunks);
}

TEST_P(RasterInvalidatorTest, EffectLocalTransformSpaceChange) {
  auto* t1 = CreateTransform(t0(), gfx::Transform());
  auto* t2 = CreateTransform(*t1, gfx::Transform());
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(20);
  auto* e1 = CreateFilterEffect(e0(), *t1, &c0(), filter);
  auto* clip_expander = CreatePixelMovingFilterClipExpander(c0(), *e1);

  PropertyTreeState layer_state = DefaultPropertyTreeState();
  PaintChunkSubset chunks(TestPaintArtifact()
                              .Chunk(0)
                              .Properties(*t2, *clip_expander, *e1)
                              .Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  // Change both t1 and t2 but keep t1*t2 unchanged, to test change of
  // LocalTransformSpace of e1.
  invalidator_->SetTracksRasterInvalidations(true);
  t1->Update(t0(), TransformPaintPropertyNode::State{
                       {MakeTranslationMatrix(-10, -20)}});
  t2->Update(
      *t1, TransformPaintPropertyNode::State{{MakeTranslationMatrix(10, 20)}});

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  auto mapper = [](gfx::Rect& r) { r.Outset(60); };
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  chunks, 0, PaintInvalidationReason::kPaintProperty,
                  -kDefaultLayerOffset, MapFunction(mapper))));
  invalidator_->SetTracksRasterInvalidations(false);
  FinishCycle(chunks);
}

// This is based on EffectLocalTransformSpaceChange, but tests the no-
// invalidation path by letting the effect's LocalTransformSpace be the same as
// the chunk's transform.
TEST_P(RasterInvalidatorTest, EffectLocalTransformSpaceChangeNoInvalidation) {
  auto* t1 = CreateTransform(t0(), gfx::Transform());
  auto* t2 = CreateTransform(*t1, gfx::Transform());
  // This setup is different from EffectLocalTransformSpaceChange.
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(20);
  auto* e1 = CreateFilterEffect(e0(), *t2, &c0(), filter);

  PropertyTreeState layer_state = DefaultPropertyTreeState();
  PaintChunkSubset chunks(
      TestPaintArtifact().Chunk(0).Properties(*t2, c0(), *e1).Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  // Change both t1 and t2 but keep t1*t2 unchanged.
  invalidator_->SetTracksRasterInvalidations(true);
  t1->Update(t0(), TransformPaintPropertyNode::State{
                       {MakeTranslationMatrix(-10, -20)}});
  t2->Update(
      *t1, TransformPaintPropertyNode::State{{MakeTranslationMatrix(10, 20)}});

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().empty());
  FinishCycle(chunks);
}

TEST_P(RasterInvalidatorTest, AliasEffectParentChanges) {
  CompositorFilterOperations filter;
  filter.AppendOpacityFilter(0.5);
  // Create an effect and an alias for that effect.
  auto* e1 = CreateFilterEffect(e0(), t0(), &c0(), filter);
  auto* alias_effect = EffectPaintPropertyNodeAlias::Create(*e1);

  // The artifact has a chunk pointing to the alias.
  PropertyTreeState layer_state = DefaultPropertyTreeState();
  PropertyTreeStateOrAlias chunk_state(t0(), c0(), *alias_effect);
  PaintChunkSubset chunks(
      TestPaintArtifact().Chunk(0).Properties(chunk_state).Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  invalidator_->SetTracksRasterInvalidations(true);
  // Reparent the aliased effect, so the chunk doesn't change the actual alias
  // node, but its parent is now different.
  alias_effect->SetParent(e0());

  // We expect to get invalidations since the effect unaliased effect is
  // actually different now.
  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  chunks, 0, PaintInvalidationReason::kPaintProperty)));
  FinishCycle(chunks);
}

TEST_P(RasterInvalidatorTest, NestedAliasEffectParentChanges) {
  CompositorFilterOperations filter;
  filter.AppendOpacityFilter(0.5);
  // Create an effect and an alias for that effect.
  auto* e1 = CreateFilterEffect(e0(), t0(), &c0(), filter);
  auto* alias_effect_1 = EffectPaintPropertyNodeAlias::Create(*e1);
  auto* alias_effect_2 = EffectPaintPropertyNodeAlias::Create(*alias_effect_1);

  // The artifact has a chunk pointing to the nested alias.
  PropertyTreeState layer_state = DefaultPropertyTreeState();
  PropertyTreeStateOrAlias chunk_state(t0(), c0(), *alias_effect_2);
  PaintChunkSubset chunks(
      TestPaintArtifact().Chunk(0).Properties(chunk_state).Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  invalidator_->SetTracksRasterInvalidations(true);
  // Reparent the parent aliased effect, so the chunk doesn't change the actual
  // alias node, but its parent is now different, this also ensures that the
  // nested alias is unchanged.
  alias_effect_1->SetParent(e0());

  // We expect to get invalidations since the effect unaliased effect is
  // actually different now.
  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  chunks, 0, PaintInvalidationReason::kPaintProperty)));
  FinishCycle(chunks);
}

TEST_P(RasterInvalidatorTest, EffectWithAliasTransformWhoseParentChanges) {
  auto* t1 = CreateTransform(t0(), MakeScaleMatrix(5));
  auto* alias_transform = TransformPaintPropertyNodeAlias::Create(*t1);

  CompositorFilterOperations filter;
  filter.AppendBlurFilter(0);
  // Create an effect and an alias for that effect.
  auto* e1 = CreateFilterEffect(e0(), *alias_transform, &c0(), filter);

  // The artifact has a chunk pointing to the alias.
  PropertyTreeState layer_state = PropertyTreeState::Root();
  PropertyTreeState chunk_state(t0(), c0(), *e1);
  PaintChunkSubset chunks(
      TestPaintArtifact().Chunk(0).Properties(chunk_state).Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  invalidator_->SetTracksRasterInvalidations(true);
  // Reparent the aliased effect, so the chunk doesn't change the actual alias
  // node, but its parent is now different.
  alias_transform->SetParent(t0());

  // We expect to get invalidations since the effect unaliased effect is
  // actually different now.
  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  chunks, 0, PaintInvalidationReason::kPaintProperty)));
  FinishCycle(chunks);
}

TEST_P(RasterInvalidatorTest, EffectChangeSimple) {
  PropertyTreeState layer_state = DefaultPropertyTreeState();
  auto* e1 = CreateOpacityEffect(e0(), t0(), &c0(), 0.5);
  PropertyTreeState chunk_state(t0(), c0(), *e1);
  PaintChunkSubset chunks(
      TestPaintArtifact().Chunk(0).Properties(chunk_state).Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  invalidator_->SetTracksRasterInvalidations(true);
  EffectPaintPropertyNode::State state{&t0(), &c0()};
  state.opacity = 0.9;
  e1->Update(*e1->Parent(), std::move(state));

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  chunks, 0, PaintInvalidationReason::kPaintProperty)));
  FinishCycle(chunks);
}

TEST_P(RasterInvalidatorTest, EffectChangeOnCachedSubsequence) {
  PropertyTreeState layer_state = DefaultPropertyTreeState();
  auto* e1 = CreateOpacityEffect(e0(), t0(), &c0(), 0.5);
  PropertyTreeState chunk_state(t0(), c0(), *e1);
  PaintChunkSubset chunks(TestPaintArtifact()
                              .Chunk(0)
                              .Properties(chunk_state)
                              .IsMovedFromCachedSubsequence()
                              .Build());

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  FinishCycle(chunks);

  invalidator_->SetTracksRasterInvalidations(true);
  EffectPaintPropertyNode::State state{&t0(), &c0()};
  state.opacity = 0.9;
  e1->Update(*e1->Parent(), std::move(state));

  invalidator_->Generate(chunks, kDefaultLayerOffset, kDefaultLayerBounds,
                         layer_state);
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  chunks, 0, PaintInvalidationReason::kPaintProperty)));
  FinishCycle(chunks);
}

}  // namespace blink
