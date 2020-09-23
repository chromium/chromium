// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidator.h"

#include <utility>
#include "base/bind_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/test_paint_artifact.h"

using testing::ElementsAre;

namespace blink {

static const IntRect kDefaultLayerBounds(-9999, -7777, 18888, 16666);

class RasterInvalidatorTest : public testing::Test,
                              public PaintTestConfigurations {
 public:
  RasterInvalidatorTest() = default;

  static PropertyTreeState DefaultPropertyTreeState() {
    return PropertyTreeState::Root();
  }

  void ClearGeometryMapperCache() {
    GeometryMapperTransformCache::ClearCache();
    GeometryMapperClipCache::ClearCache();
  }

  void SetUp() override { ClearGeometryMapperCache(); }
  void TearDown() override { ClearGeometryMapperCache(); }

  void FinishCycle(PaintArtifact& artifact) {
    artifact.FinishCycle();
    ClearGeometryMapperCache();
    for (auto& chunk : artifact.Chunks())
      chunk.properties.ClearChangedToRoot();
  }

  const Vector<RasterInvalidationInfo>& TrackedRasterInvalidations() {
    DCHECK(invalidator_.GetTracking());
    return invalidator_.GetTracking()->Invalidations();
  }

  RasterInvalidator invalidator_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(RasterInvalidatorTest);

using MapFunction = base::RepeatingCallback<void(IntRect&)>;
static IntRect ChunkRectToLayer(const IntRect& rect,
                                const IntPoint& layer_offset,
                                const MapFunction& mapper = base::DoNothing()) {
  auto r = rect;
  mapper.Run(r);
  r.MoveBy(layer_offset);
  return r;
}

static bool CheckChunkInvalidation(
    const RasterInvalidationInfo& info,
    const PaintChunk& chunk,
    const IntRect& chunk_rect,
    PaintInvalidationReason reason,
    const IntPoint& layer_offset,
    const MapFunction& mapper = base::DoNothing()) {
  return ChunkRectToLayer(chunk_rect, layer_offset, mapper) == info.rect &&
         &chunk.id.client == info.client && reason == info.reason;
}

MATCHER_P4(ChunkInvalidation, chunk, reason, layer_offset, mapper, "") {
  return CheckChunkInvalidation(arg, *chunk, chunk->drawable_bounds, reason,
                                layer_offset, mapper);
}

MATCHER_P2(ChunkInvalidation, chunk, reason, "") {
  return CheckChunkInvalidation(arg, *chunk, chunk->drawable_bounds, reason,
                                -kDefaultLayerBounds.Location());
}

MATCHER_P2(IncrementalInvalidation, chunk, chunk_rect, "") {
  return CheckChunkInvalidation(arg, *chunk, chunk_rect,
                                PaintInvalidationReason::kIncremental,
                                -kDefaultLayerBounds.Location());
}

TEST_P(RasterInvalidatorTest, ImplicitFullLayerInvalidation) {
  auto artifact = TestPaintArtifact().Chunk(0).Build();

  invalidator_.SetTracksRasterInvalidations(true);
  invalidator_.Generate(base::DoNothing(), artifact->Chunks(),
                        kDefaultLayerBounds, DefaultPropertyTreeState());
  const auto& client = artifact->Chunks().begin()->id.client;
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(RasterInvalidationInfo{
                  &client, client.DebugName(),
                  IntRect(IntPoint(), kDefaultLayerBounds.Size()),
                  PaintInvalidationReason::kFullLayer}));
  FinishCycle(*artifact);
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(RasterInvalidatorTest, LayerBounds) {
  auto artifact = TestPaintArtifact().Chunk(0).Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  // No raster invalidations needed if layer origin doesn't change.
  EXPECT_TRUE(TrackedRasterInvalidations().IsEmpty());

  auto new_layer_bounds = kDefaultLayerBounds;
  new_layer_bounds.Move(66, 77);
  invalidator_.Generate(base::DoNothing(), chunks, new_layer_bounds,
                        DefaultPropertyTreeState());
  // Change of layer origin causes change of chunk0's transform to layer.
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(chunks.begin(),
                                    PaintInvalidationReason::kPaintProperty),
                  ChunkInvalidation(
                      chunks.begin(), PaintInvalidationReason::kPaintProperty,
                      -new_layer_bounds.Location(), base::DoNothing())));
  FinishCycle(*artifact);
}

TEST_P(RasterInvalidatorTest, ReorderChunks) {
  auto artifact = TestPaintArtifact().Chunk(0).Chunk(1).Chunk(2).Build();
  auto chunks = artifact->Chunks();
  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  FinishCycle(*artifact);

  // Swap chunk 1 and 2.
  invalidator_.SetTracksRasterInvalidations(true);
  auto new_artifact = TestPaintArtifact()
                          .Chunk(0)
                          .Chunk(2)
                          .Chunk(1)
                          .Bounds(IntRect(11, 22, 33, 44))
                          .Build();
  auto new_chunks = new_artifact->Chunks();
  invalidator_.Generate(base::DoNothing(), new_chunks, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  // Invalidated new chunk 2's old (as chunks[{0, 1]) and new
  // (as new_chunks[{0, 2]) bounds.
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(&chunks[{0, 1}],
                                    PaintInvalidationReason::kChunkReordered),
                  ChunkInvalidation(&new_chunks[{0, 2}],
                                    PaintInvalidationReason::kChunkReordered)));
  FinishCycle(*new_artifact);
}

TEST_P(RasterInvalidatorTest, ReorderChunkSubsequences) {
  auto artifact =
      TestPaintArtifact().Chunk(0).Chunk(1).Chunk(2).Chunk(3).Chunk(4).Build();
  auto chunks = artifact->Chunks();
  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  FinishCycle(*artifact);

  // Swap chunk (1,2) and (3,4).
  invalidator_.SetTracksRasterInvalidations(true);
  auto new_artifact = TestPaintArtifact()
                          .Chunk(0)
                          .Chunk(3)
                          .Chunk(4)
                          .Chunk(1)
                          .Bounds(IntRect(11, 22, 33, 44))
                          .Chunk(2)
                          .Build();
  auto new_chunks = new_artifact->Chunks();
  invalidator_.Generate(base::DoNothing(), new_chunks, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  // Invalidated new chunk 3's old (as chunks[{0, 1] and new
  // (as new_chunks[{0, 3]) bounds.
  // Invalidated new chunk 4's new bounds. Didn't invalidate old bounds because
  // it's the same as the new bounds.
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(&chunks[{0, 1}],
                                    PaintInvalidationReason::kChunkReordered),
                  ChunkInvalidation(&new_chunks[{0, 3}],
                                    PaintInvalidationReason::kChunkReordered),
                  ChunkInvalidation(&new_chunks[{0, 4}],
                                    PaintInvalidationReason::kChunkReordered)));
  FinishCycle(*new_artifact);
}

TEST_P(RasterInvalidatorTest, ChunkAppearAndDisappear) {
  auto artifact = TestPaintArtifact().Chunk(0).Chunk(1).Chunk(2).Build();
  auto chunks = artifact->Chunks();
  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  FinishCycle(*artifact);

  // Chunk 1 and 2 disappeared, 3 and 4 appeared.
  invalidator_.SetTracksRasterInvalidations(true);
  auto new_artifact = TestPaintArtifact().Chunk(0).Chunk(3).Chunk(4).Build();
  auto new_chunks = new_artifact->Chunks();
  invalidator_.Generate(base::DoNothing(), new_chunks, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(
          ChunkInvalidation(&new_chunks[{0, 1}],
                            PaintInvalidationReason::kChunkAppeared),
          ChunkInvalidation(&new_chunks[{0, 2}],
                            PaintInvalidationReason::kChunkAppeared),
          ChunkInvalidation(&chunks[{0, 1}],
                            PaintInvalidationReason::kChunkDisappeared),
          ChunkInvalidation(&chunks[{0, 2}],
                            PaintInvalidationReason::kChunkDisappeared)));
  FinishCycle(*new_artifact);
}

TEST_P(RasterInvalidatorTest, InvalidateDrawableBounds) {
  IntRect drawable_bounds(11, 22, 33, 44);
  IntRect bounds(0, 0, 100, 100);
  auto artifact = TestPaintArtifact()
                      .Chunk(0)
                      .Chunk(1)
                      .Bounds(bounds)
                      .DrawableBounds(drawable_bounds)
                      .Build();
  auto chunks = artifact->Chunks();
  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  auto new_artifact = TestPaintArtifact()
                          .Chunk(0)
                          .Chunk(2)
                          .Bounds(bounds)
                          .DrawableBounds(drawable_bounds)
                          .Build();
  auto new_chunks = new_artifact->Chunks();
  invalidator_.Generate(base::DoNothing(), new_chunks, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  // ChunkInvalidation uses the drawable_bounds. We expect raster invalidations
  // based on drawable_bounds instead of bounds.
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(
          ChunkInvalidation(&new_chunks[{0, 1}],
                            PaintInvalidationReason::kChunkAppeared),
          ChunkInvalidation(&chunks[{0, 1}],
                            PaintInvalidationReason::kChunkDisappeared)));
  FinishCycle(*new_artifact);
}

TEST_P(RasterInvalidatorTest, ChunkAppearAtEnd) {
  auto artifact = TestPaintArtifact().Chunk(0).Build();
  auto chunks = artifact->Chunks();
  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  auto new_artifact = TestPaintArtifact().Chunk(0).Chunk(1).Chunk(2).Build();
  auto new_chunks = new_artifact->Chunks();
  invalidator_.Generate(base::DoNothing(), new_chunks, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(&new_chunks[{0, 1}],
                                    PaintInvalidationReason::kChunkAppeared),
                  ChunkInvalidation(&new_chunks[{0, 2}],
                                    PaintInvalidationReason::kChunkAppeared)));
  FinishCycle(*new_artifact);
}

TEST_P(RasterInvalidatorTest, UncacheableChunks) {
  auto artifact =
      TestPaintArtifact().Chunk(0).Chunk(1).Uncacheable().Chunk(2).Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  auto new_artifact =
      TestPaintArtifact().Chunk(0).Chunk(2).Chunk(1).Uncacheable().Build();
  auto new_chunks = new_artifact->Chunks();
  invalidator_.Generate(base::DoNothing(), new_chunks, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(
          ChunkInvalidation(&new_chunks[{0, 2}],
                            PaintInvalidationReason::kChunkUncacheable),
          ChunkInvalidation(&chunks[{0, 1}],
                            PaintInvalidationReason::kChunkUncacheable)));
  FinishCycle(*new_artifact);
}

// Tests the path based on ClipPaintPropertyNode::Changed().
TEST_P(RasterInvalidatorTest, ClipPropertyChangeRounded) {
  FloatRoundedRect::Radii radii(FloatSize(1, 2), FloatSize(2, 3),
                                FloatSize(3, 4), FloatSize(4, 5));
  FloatRoundedRect clip_rect(FloatRect(-1000, -1000, 2000, 2000), radii);
  auto clip0 = CreateClip(c0(), t0(), clip_rect);
  auto clip2 = CreateClip(*clip0, t0(), clip_rect);

  PropertyTreeState layer_state(t0(), *clip0, e0());
  auto artifact = TestPaintArtifact()
                      .Chunk(0)
                      .Properties(layer_state)
                      .Chunk(1)
                      .Properties(layer_state)
                      .Chunk(2)
                      .Properties(t0(), *clip2, e0())
                      .Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  FinishCycle(*artifact);

  // Change both clip0 and clip2.
  invalidator_.SetTracksRasterInvalidations(true);
  FloatRoundedRect new_clip_rect(FloatRect(-2000, -2000, 4000, 4000), radii);
  clip0->Update(*clip0->Parent(),
                ClipPaintPropertyNode::State{&clip0->LocalTransformSpace(),
                                             new_clip_rect});
  clip2->Update(*clip2->Parent(),
                ClipPaintPropertyNode::State{&clip2->LocalTransformSpace(),
                                             new_clip_rect});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  // Property change in the layer state should not trigger raster invalidation.
  // |clip2| change should trigger raster invalidation.
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  &chunks[{0, 2}], PaintInvalidationReason::kPaintProperty)));
  invalidator_.SetTracksRasterInvalidations(false);
  FinishCycle(*artifact);

  // Change chunk1's properties to use a different property tree state.
  auto new_artifact1 = TestPaintArtifact()
                           .Chunk(0)
                           .Properties(chunks[{0, 0}].properties)
                           .Chunk(1)
                           .Properties(chunks[{0, 2}].properties)
                           .Chunk(2)
                           .Properties(chunks[{0, 2}].properties)
                           .Build();
  auto new_chunks1 = new_artifact1->Chunks();

  invalidator_.SetTracksRasterInvalidations(true);
  invalidator_.Generate(base::DoNothing(), new_chunks1, kDefaultLayerBounds,
                        layer_state);
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(&new_chunks1[{0, 1}],
                                    PaintInvalidationReason::kPaintProperty)));
  invalidator_.SetTracksRasterInvalidations(false);
  FinishCycle(*new_artifact1);
}

// Tests the path detecting change of PaintChunkInfo::chunk_to_layer_clip.
TEST_P(RasterInvalidatorTest, ClipPropertyChangeSimple) {
  FloatRoundedRect clip_rect(-1000, -1000, 2000, 2000);
  auto clip0 = CreateClip(c0(), t0(), clip_rect);
  auto clip1 = CreateClip(*clip0, t0(), clip_rect);

  PropertyTreeState layer_state = PropertyTreeState::Root();
  auto artifact = TestPaintArtifact()
                      .Chunk(0)
                      .Properties(t0(), *clip0, e0())
                      .Bounds(EnclosingIntRect(clip_rect.Rect()))
                      .Chunk(1)
                      .Properties(t0(), *clip1, e0())
                      .Bounds(EnclosingIntRect(clip_rect.Rect()))
                      .Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  FinishCycle(*artifact);

  // Change clip1 to bigger, which is still bound by clip0, resulting no actual
  // visual change.
  invalidator_.SetTracksRasterInvalidations(true);
  FloatRoundedRect new_clip_rect1(-2000, -2000, 4000, 4000);
  clip1->Update(*clip1->Parent(),
                ClipPaintPropertyNode::State{&clip1->LocalTransformSpace(),
                                             new_clip_rect1});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().IsEmpty());
  FinishCycle(*artifact);

  // Change clip1 to smaller.
  FloatRoundedRect new_clip_rect2(-500, -500, 1000, 1000);
  clip1->Update(*clip1->Parent(),
                ClipPaintPropertyNode::State{&clip1->LocalTransformSpace(),
                                             new_clip_rect2});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  // |clip1| change should trigger incremental raster invalidation.
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(IncrementalInvalidation(&chunks[{0, 1}],
                                          IntRect(-1000, -1000, 2000, 500)),
                  IncrementalInvalidation(&chunks[{0, 1}],
                                          IntRect(-1000, -500, 500, 1000)),
                  IncrementalInvalidation(&chunks[{0, 1}],
                                          IntRect(500, -500, 500, 1000)),
                  IncrementalInvalidation(&chunks[{0, 1}],
                                          IntRect(-1000, 500, 2000, 500))));
  invalidator_.SetTracksRasterInvalidations(false);
  FinishCycle(*artifact);

  // Change clip1 bigger at one side.
  FloatRoundedRect new_clip_rect3(-500, -500, 2000, 1000);
  clip1->Update(*clip1->Parent(),
                ClipPaintPropertyNode::State{&clip1->LocalTransformSpace(),
                                             new_clip_rect3});

  invalidator_.SetTracksRasterInvalidations(true);
  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  // |clip1| change should trigger incremental raster invalidation.
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(IncrementalInvalidation(
                  &chunks[{0, 1}], IntRect(500, -500, 500, 1000))));
  invalidator_.SetTracksRasterInvalidations(false);
  FinishCycle(*artifact);
}

// Tests the path detecting change of PaintChunkInfo::chunk_to_layer_clip.
// The chunk bounds is bigger than the clip because of the outset for raster
// effects, so incremental invalidation is not suitable.
TEST_P(RasterInvalidatorTest, ClipPropertyChangeWithOutsetForRasterEffects) {
  FloatRoundedRect clip_rect(-1000, -1000, 2000, 2000);
  auto clip = CreateClip(c0(), t0(), clip_rect);

  PropertyTreeState layer_state = PropertyTreeState::Root();
  auto artifact = TestPaintArtifact()
                      .Chunk(0)
                      .Properties(t0(), *clip, e0())
                      .Bounds(EnclosingIntRect(clip_rect.Rect()))
                      .SetRasterEffectOutset(RasterEffectOutset::kWholePixel)
                      .Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  FloatRoundedRect new_clip_rect(-2000, -2000, 4000, 4000);
  clip->Update(c0(), ClipPaintPropertyNode::State{&t0(), new_clip_rect});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  auto mapper = [](IntRect& r) { r.Inflate(1); };
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(
          chunks.begin(), PaintInvalidationReason::kPaintProperty,
          -kDefaultLayerBounds.Location(), base::BindRepeating(mapper))));
  invalidator_.SetTracksRasterInvalidations(false);
  FinishCycle(*artifact);
}

TEST_P(RasterInvalidatorTest, ClipLocalTransformSpaceChange) {
  auto t1 = CreateTransform(t0(), TransformationMatrix());
  auto t2 = CreateTransform(*t1, TransformationMatrix());

  FloatRoundedRect::Radii radii(FloatSize(1, 2), FloatSize(2, 3),
                                FloatSize(3, 4), FloatSize(4, 5));
  FloatRoundedRect clip_rect(FloatRect(-1000, -1000, 2000, 2000), radii);
  auto c1 = CreateClip(c0(), *t1, clip_rect);

  PropertyTreeState layer_state = DefaultPropertyTreeState();
  auto artifact =
      TestPaintArtifact().Chunk(0).Properties(*t2, *c1, e0()).Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  FinishCycle(*artifact);

  // Change both t1 and t2 but keep t1*t2 unchanged, to test change of
  // LocalTransformSpace of c1.
  invalidator_.SetTracksRasterInvalidations(true);
  t1->Update(t0(), TransformPaintPropertyNode::State{FloatSize(-10, -20)});
  t2->Update(*t1, TransformPaintPropertyNode::State{FloatSize(10, 20)});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  chunks.begin(), PaintInvalidationReason::kPaintProperty)));
  invalidator_.SetTracksRasterInvalidations(false);
}

// This is based on ClipLocalTransformSpaceChange, but tests the no-invalidation
// path by letting the clip's LocalTransformSpace be the same as the chunk's
// transform.
TEST_P(RasterInvalidatorTest, ClipLocalTransformSpaceChangeNoInvalidation) {
  auto t1 = CreateTransform(t0(), TransformationMatrix());
  auto t2 = CreateTransform(*t1, TransformationMatrix());

  FloatRoundedRect::Radii radii(FloatSize(1, 2), FloatSize(2, 3),
                                FloatSize(3, 4), FloatSize(4, 5));
  FloatRoundedRect clip_rect(FloatRect(-1000, -1000, 2000, 2000), radii);
  // This set is different from ClipLocalTransformSpaceChange.
  auto c1 = CreateClip(c0(), *t2, clip_rect);

  PropertyTreeState layer_state = DefaultPropertyTreeState();
  auto artifact =
      TestPaintArtifact().Chunk(0).Properties(*t2, *c1, e0()).Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  FinishCycle(*artifact);

  // Change both t1 and t2 but keep t1*t2 unchanged.
  invalidator_.SetTracksRasterInvalidations(true);
  t1->Update(t0(), TransformPaintPropertyNode::State{FloatSize(-10, -20)});
  t2->Update(*t1, TransformPaintPropertyNode::State{FloatSize(10, 20)});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().IsEmpty());
  FinishCycle(*artifact);
}

TEST_P(RasterInvalidatorTest, TransformPropertyChange) {
  auto layer_transform = CreateTransform(t0(), TransformationMatrix().Scale(5));
  auto transform0 = Create2DTranslation(*layer_transform, 10, 20);
  auto transform1 = Create2DTranslation(*transform0, -50, -60);

  PropertyTreeState layer_state(*layer_transform, c0(), e0());
  auto artifact = TestPaintArtifact()
                      .Chunk(0)
                      .Properties(*transform0, c0(), e0())
                      .Chunk(1)
                      .Properties(*transform1, c0(), e0())
                      .Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  FinishCycle(*artifact);

  // Change layer_transform should not cause raster invalidation in the layer.
  invalidator_.SetTracksRasterInvalidations(true);
  layer_transform->Update(
      *layer_transform->Parent(),
      TransformPaintPropertyNode::State{TransformationMatrix().Scale(10)});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().IsEmpty());
  FinishCycle(*artifact);

  // Inserting another node between layer_transform and transform0 and letting
  // the new node become the transform of the layer state should not cause
  // raster invalidation in the layer. This simulates a composited layer is
  // scrolled from its original location.
  auto new_layer_transform = Create2DTranslation(*layer_transform, -100, -200);
  layer_state = PropertyTreeState(*new_layer_transform, c0(), e0());
  transform0->Update(*new_layer_transform, TransformPaintPropertyNode::State{
                                               transform0->Translation2D()});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().IsEmpty());
  FinishCycle(*artifact);

  // Removing transform nodes above the layer state should not cause raster
  // invalidation in the layer.
  layer_state = DefaultPropertyTreeState();
  transform0->Update(layer_state.Transform(), TransformPaintPropertyNode::State{
                                                  transform0->Translation2D()});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().IsEmpty());
  FinishCycle(*artifact);

  // Change transform0 and transform1, while keeping the combined transform0
  // and transform1 unchanged for chunk 2. We should invalidate only chunk 0
  // for changed paint property.
  transform0->Update(layer_state.Transform(),
                     TransformPaintPropertyNode::State{
                         transform0->Translation2D() + FloatSize(20, 30)});
  transform1->Update(*transform0,
                     TransformPaintPropertyNode::State{
                         transform1->Translation2D() + FloatSize(-20, -30)});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  auto mapper0 = [](IntRect& r) { r.Move(10, 20); };
  auto mapper1 = [](IntRect& r) { r.Move(30, 50); };
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(
          ChunkInvalidation(
              chunks.begin(), PaintInvalidationReason::kPaintProperty,
              -kDefaultLayerBounds.Location(), base::BindRepeating(mapper0)),
          ChunkInvalidation(
              chunks.begin(), PaintInvalidationReason::kPaintProperty,
              -kDefaultLayerBounds.Location(), base::BindRepeating(mapper1))));
  invalidator_.SetTracksRasterInvalidations(false);
  FinishCycle(*artifact);
}

TEST_P(RasterInvalidatorTest, TransformPropertyTinyChange) {
  auto layer_transform = CreateTransform(t0(), TransformationMatrix().Scale(5));
  auto chunk_transform = Create2DTranslation(*layer_transform, 10, 20);

  PropertyTreeState layer_state(*layer_transform, c0(), e0());
  auto artifact = TestPaintArtifact()
                      .Chunk(0)
                      .Properties(*chunk_transform, c0(), e0())
                      .Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  FinishCycle(*artifact);

  // Change chunk_transform by tiny difference, which should be ignored.
  invalidator_.SetTracksRasterInvalidations(true);
  chunk_transform->Update(
      layer_state.Transform(),
      TransformPaintPropertyNode::State{chunk_transform->SlowMatrix()
                                            .Translate(0.0000001, -0.0000001)
                                            .Scale(1.0000001)
                                            .Rotate(0.0000001)});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().IsEmpty());
  FinishCycle(*artifact);

  // Tiny differences should accumulate and cause invalidation when the
  // accumulation is large enough.
  bool invalidated = false;
  for (int i = 0; i < 100 && !invalidated; i++) {
    chunk_transform->Update(
        layer_state.Transform(),
        TransformPaintPropertyNode::State{chunk_transform->SlowMatrix()
                                              .Translate(0.0000001, -0.0000001)
                                              .Scale(1.0000001)
                                              .Rotate(0.0000001)});
    invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                          layer_state);
    invalidated = !TrackedRasterInvalidations().IsEmpty();
    FinishCycle(*artifact);
  }
  EXPECT_TRUE(invalidated);
}

TEST_P(RasterInvalidatorTest, TransformPropertyTinyChangeScale) {
  auto layer_transform = CreateTransform(t0(), TransformationMatrix().Scale(5));
  auto chunk_transform =
      CreateTransform(*layer_transform, TransformationMatrix().Scale(1e-6));
  IntRect chunk_bounds(0, 0, 10000000, 10000000);

  PropertyTreeState layer_state(*layer_transform, c0(), e0());
  auto artifact = TestPaintArtifact()
                      .Chunk(0)
                      .Properties(*chunk_transform, c0(), e0())
                      .Bounds(chunk_bounds)
                      .Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  FinishCycle(*artifact);

  // Scale change from 1e-6 to 2e-6 should be treated as significant.
  invalidator_.SetTracksRasterInvalidations(true);
  chunk_transform->Update(
      layer_state.Transform(),
      TransformPaintPropertyNode::State{TransformationMatrix().Scale(2e-6)});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  EXPECT_FALSE(TrackedRasterInvalidations().IsEmpty());
  invalidator_.SetTracksRasterInvalidations(false);
  FinishCycle(*artifact);

  // Scale change from 2e-6 to 2e-6 + 1e-15 should be ignored.
  invalidator_.SetTracksRasterInvalidations(true);
  chunk_transform->Update(layer_state.Transform(),
                          TransformPaintPropertyNode::State{
                              TransformationMatrix().Scale(2e-6 + 1e-15)});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().IsEmpty());
  invalidator_.SetTracksRasterInvalidations(false);
  FinishCycle(*artifact);
}

TEST_P(RasterInvalidatorTest, EffectLocalTransformSpaceChange) {
  auto t1 = CreateTransform(t0(), TransformationMatrix());
  auto t2 = CreateTransform(*t1, TransformationMatrix());
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(20);
  auto e1 = CreateFilterEffect(e0(), *t1, &c0(), filter);

  PropertyTreeState layer_state = DefaultPropertyTreeState();
  auto artifact =
      TestPaintArtifact().Chunk(0).Properties(*t2, c0(), *e1).Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  FinishCycle(*artifact);

  // Change both t1 and t2 but keep t1*t2 unchanged, to test change of
  // LocalTransformSpace of e1.
  invalidator_.SetTracksRasterInvalidations(true);
  t1->Update(t0(), TransformPaintPropertyNode::State{FloatSize(-10, -20)});
  t2->Update(*t1, TransformPaintPropertyNode::State{FloatSize(10, 20)});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  auto mapper = [](IntRect& r) { r.Inflate(60); };
  EXPECT_THAT(
      TrackedRasterInvalidations(),
      ElementsAre(ChunkInvalidation(
          chunks.begin(), PaintInvalidationReason::kPaintProperty,
          -kDefaultLayerBounds.Location(), base::BindRepeating(mapper))));
  invalidator_.SetTracksRasterInvalidations(false);
  FinishCycle(*artifact);
}

// This is based on EffectLocalTransformSpaceChange, but tests the no-
// invalidation path by letting the effect's LocalTransformSpace be the same as
// the chunk's transform.
TEST_P(RasterInvalidatorTest, EffectLocalTransformSpaceChangeNoInvalidation) {
  auto t1 = CreateTransform(t0(), TransformationMatrix());
  auto t2 = CreateTransform(*t1, TransformationMatrix());
  // This setup is different from EffectLocalTransformSpaceChange.
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(20);
  auto e1 = CreateFilterEffect(e0(), *t2, &c0(), filter);

  PropertyTreeState layer_state = DefaultPropertyTreeState();
  auto artifact =
      TestPaintArtifact().Chunk(0).Properties(*t2, c0(), *e1).Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  FinishCycle(*artifact);

  // Change both t1 and t2 but keep t1*t2 unchanged.
  invalidator_.SetTracksRasterInvalidations(true);
  t1->Update(t0(), TransformPaintPropertyNode::State{FloatSize(-10, -20)});
  t2->Update(*t1, TransformPaintPropertyNode::State{FloatSize(10, 20)});

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().IsEmpty());
  FinishCycle(*artifact);
}

TEST_P(RasterInvalidatorTest, AliasEffectParentChanges) {
  CompositorFilterOperations filter;
  filter.AppendOpacityFilter(0.5);
  // Create an effect and an alias for that effect.
  auto e1 = CreateFilterEffect(e0(), t0(), &c0(), filter);
  auto alias_effect = EffectPaintPropertyNodeAlias::Create(*e1);

  // The artifact has a chunk pointing to the alias.
  PropertyTreeState layer_state = DefaultPropertyTreeState();
  PropertyTreeStateOrAlias chunk_state(t0(), c0(), *alias_effect);
  auto artifact = TestPaintArtifact().Chunk(0).Properties(chunk_state).Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  // Reparent the aliased effect, so the chunk doesn't change the actual alias
  // node, but its parent is now different.
  alias_effect->SetParent(e0());

  // We expect to get invalidations since the effect unaliased effect is
  // actually different now.
  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  chunks.begin(), PaintInvalidationReason::kPaintProperty)));
  FinishCycle(*artifact);
}

TEST_P(RasterInvalidatorTest, NestedAliasEffectParentChanges) {
  CompositorFilterOperations filter;
  filter.AppendOpacityFilter(0.5);
  // Create an effect and an alias for that effect.
  auto e1 = CreateFilterEffect(e0(), t0(), &c0(), filter);
  auto alias_effect_1 = EffectPaintPropertyNodeAlias::Create(*e1);
  auto alias_effect_2 = EffectPaintPropertyNodeAlias::Create(*alias_effect_1);

  // The artifact has a chunk pointing to the nested alias.
  PropertyTreeState layer_state = DefaultPropertyTreeState();
  PropertyTreeStateOrAlias chunk_state(t0(), c0(), *alias_effect_2);
  auto artifact = TestPaintArtifact().Chunk(0).Properties(chunk_state).Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  // Reparent the parent aliased effect, so the chunk doesn't change the actual
  // alias node, but its parent is now different, this also ensures that the
  // nested alias is unchanged.
  alias_effect_1->SetParent(e0());

  // We expect to get invalidations since the effect unaliased effect is
  // actually different now.
  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  chunks.begin(), PaintInvalidationReason::kPaintProperty)));
  FinishCycle(*artifact);
}

TEST_P(RasterInvalidatorTest, EffectWithAliasTransformWhoseParentChanges) {
  auto t1 = CreateTransform(t0(), TransformationMatrix().Scale(5));
  auto alias_transform = TransformPaintPropertyNodeAlias::Create(*t1);

  CompositorFilterOperations filter;
  filter.AppendBlurFilter(0);
  // Create an effect and an alias for that effect.
  auto e1 = CreateFilterEffect(e0(), *alias_transform, &c0(), filter);

  // The artifact has a chunk pointing to the alias.
  PropertyTreeState layer_state = PropertyTreeState::Root();
  PropertyTreeState chunk_state(t0(), c0(), *e1);
  auto artifact = TestPaintArtifact().Chunk(0).Properties(chunk_state).Build();
  auto chunks = artifact->Chunks();

  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  // Reparent the aliased effect, so the chunk doesn't change the actual alias
  // node, but its parent is now different.
  alias_transform->SetParent(t0());

  // We expect to get invalidations since the effect unaliased effect is
  // actually different now.
  invalidator_.Generate(base::DoNothing(), chunks, kDefaultLayerBounds,
                        layer_state);
  EXPECT_THAT(TrackedRasterInvalidations(),
              ElementsAre(ChunkInvalidation(
                  chunks.begin(), PaintInvalidationReason::kPaintProperty)));
  FinishCycle(*artifact);
}

}  // namespace blink
