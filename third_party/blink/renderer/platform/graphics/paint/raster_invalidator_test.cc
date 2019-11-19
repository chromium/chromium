// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidator.h"

#include <utility>
#include "base/bind_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/test_paint_artifact.h"

namespace blink {

static const IntRect kDefaultLayerBounds(-9999, -7777, 18888, 16666);

class RasterInvalidatorTest : public testing::Test,
                              public PaintTestConfigurations {
 public:
  RasterInvalidatorTest() : invalidator_(base::DoNothing()) {}

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
    // See PaintArtifact::FinishCycle() for the reason of doing this.
    for (auto& chunk : artifact.PaintChunks())
      chunk.properties.ClearChangedToRoot();
  }

  const Vector<RasterInvalidationInfo>& TrackedRasterInvalidations() {
    DCHECK(invalidator_.GetTracking());
    return invalidator_.GetTracking()->Invalidations();
  }

  using MapFunction = base::RepeatingCallback<void(IntRect&)>;
  static IntRect ChunkRectToLayer(
      const IntRect& rect,
      const IntPoint& layer_offset,
      const MapFunction& mapper = base::DoNothing()) {
    auto r = rect;
    mapper.Run(r);
    r.MoveBy(layer_offset);
    return r;
  }

  RasterInvalidator invalidator_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(RasterInvalidatorTest);

#define EXPECT_CHUNK_INVALIDATION_CUSTOM(                               \
    invalidations, index, chunk, expected_reason, layer_offset, mapper) \
  do {                                                                  \
    const auto& info = (invalidations)[index];                          \
    EXPECT_EQ(ChunkRectToLayer((chunk).bounds, layer_offset, mapper),   \
              info.rect);                                               \
    EXPECT_EQ(&(chunk).id.client, info.client);                         \
    EXPECT_EQ(expected_reason, info.reason);                            \
  } while (false)

#define EXPECT_CHUNK_INVALIDATION(invalidations, index, chunk, reason)  \
  EXPECT_CHUNK_INVALIDATION_CUSTOM(invalidations, index, chunk, reason, \
                                   -kDefaultLayerBounds.Location(),     \
                                   base::DoNothing())

#define EXPECT_INCREMENTAL_INVALIDATION(invalidations, index, chunk,         \
                                        chunk_rect)                          \
  do {                                                                       \
    const auto& info = (invalidations)[index];                               \
    EXPECT_EQ(ChunkRectToLayer(chunk_rect, -kDefaultLayerBounds.Location()), \
              info.rect);                                                    \
    EXPECT_EQ(&(chunk).id.client, info.client);                              \
    EXPECT_EQ(PaintInvalidationReason::kIncremental, info.reason);           \
  } while (false)

TEST_P(RasterInvalidatorTest, ImplicitFullLayerInvalidation) {
  auto artifact = TestPaintArtifact().Chunk(0).Build();

  invalidator_.SetTracksRasterInvalidations(true);
  invalidator_.Generate(artifact, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(1u, invalidations.size());
  EXPECT_EQ(IntRect(IntPoint(), kDefaultLayerBounds.Size()),
            invalidations[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kFullLayer, invalidations[0].reason);
  FinishCycle(*artifact);
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(RasterInvalidatorTest, LayerBounds) {
  auto artifact = TestPaintArtifact().Chunk(0).Build();

  invalidator_.Generate(artifact, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  invalidator_.Generate(artifact, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  // No raster invalidations needed if layer origin doesn't change.
  EXPECT_TRUE(TrackedRasterInvalidations().IsEmpty());

  auto new_layer_bounds = kDefaultLayerBounds;
  new_layer_bounds.Move(66, 77);
  invalidator_.Generate(artifact, new_layer_bounds, DefaultPropertyTreeState());
  // Change of layer origin causes change of chunk0's transform to layer.
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(2u, invalidations.size());
  EXPECT_CHUNK_INVALIDATION(invalidations, 0, artifact->PaintChunks()[0],
                            PaintInvalidationReason::kPaintProperty);
  EXPECT_CHUNK_INVALIDATION_CUSTOM(invalidations, 1, artifact->PaintChunks()[0],
                                   PaintInvalidationReason::kPaintProperty,
                                   -new_layer_bounds.Location(),
                                   base::DoNothing());
  FinishCycle(*artifact);
}

TEST_P(RasterInvalidatorTest, ReorderChunks) {
  auto artifact = TestPaintArtifact().Chunk(0).Chunk(1).Chunk(2).Build();
  invalidator_.Generate(artifact, kDefaultLayerBounds,
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
  invalidator_.Generate(new_artifact, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(2u, invalidations.size());
  // Invalidated new chunk 2's old (as artifact->PaintChunks()[1]) and new
  // (as new_artifact->PaintChunks()[2]) bounds.
  EXPECT_CHUNK_INVALIDATION(invalidations, 0, artifact->PaintChunks()[1],
                            PaintInvalidationReason::kChunkReordered);
  EXPECT_CHUNK_INVALIDATION(invalidations, 1, new_artifact->PaintChunks()[2],
                            PaintInvalidationReason::kChunkReordered);
  FinishCycle(*new_artifact);
}

TEST_P(RasterInvalidatorTest, ReorderChunkSubsequences) {
  auto artifact =
      TestPaintArtifact().Chunk(0).Chunk(1).Chunk(2).Chunk(3).Chunk(4).Build();
  invalidator_.Generate(artifact, kDefaultLayerBounds,
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
  invalidator_.Generate(new_artifact, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(3u, invalidations.size());
  // Invalidated new chunk 3's old (as artifact->PaintChunks()[1] and new
  // (as new_artifact->PaintChunks()[3]) bounds.
  EXPECT_CHUNK_INVALIDATION(invalidations, 0, artifact->PaintChunks()[1],
                            PaintInvalidationReason::kChunkReordered);
  EXPECT_CHUNK_INVALIDATION(invalidations, 1, new_artifact->PaintChunks()[3],
                            PaintInvalidationReason::kChunkReordered);
  // Invalidated new chunk 4's new bounds. Didn't invalidate old bounds because
  // it's the same as the new bounds.
  EXPECT_CHUNK_INVALIDATION(invalidations, 2, new_artifact->PaintChunks()[4],
                            PaintInvalidationReason::kChunkReordered);
  FinishCycle(*new_artifact);
}

TEST_P(RasterInvalidatorTest, ChunkAppearAndDisappear) {
  auto artifact = TestPaintArtifact().Chunk(0).Chunk(1).Chunk(2).Build();
  invalidator_.Generate(artifact, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  FinishCycle(*artifact);

  // Chunk 1 and 2 disappeared, 3 and 4 appeared.
  invalidator_.SetTracksRasterInvalidations(true);
  auto new_artifact = TestPaintArtifact().Chunk(0).Chunk(3).Chunk(4).Build();
  invalidator_.Generate(new_artifact, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(4u, invalidations.size());
  EXPECT_CHUNK_INVALIDATION(invalidations, 0, new_artifact->PaintChunks()[1],
                            PaintInvalidationReason::kChunkAppeared);
  EXPECT_CHUNK_INVALIDATION(invalidations, 1, new_artifact->PaintChunks()[2],
                            PaintInvalidationReason::kChunkAppeared);
  EXPECT_CHUNK_INVALIDATION(invalidations, 2, artifact->PaintChunks()[1],
                            PaintInvalidationReason::kChunkDisappeared);
  EXPECT_CHUNK_INVALIDATION(invalidations, 3, artifact->PaintChunks()[2],
                            PaintInvalidationReason::kChunkDisappeared);
  FinishCycle(*new_artifact);
}

TEST_P(RasterInvalidatorTest, ChunkAppearAtEnd) {
  auto artifact = TestPaintArtifact().Chunk(0).Build();
  invalidator_.Generate(artifact, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  auto new_artifact = TestPaintArtifact().Chunk(0).Chunk(1).Chunk(2).Build();
  invalidator_.Generate(new_artifact, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(2u, invalidations.size());
  EXPECT_CHUNK_INVALIDATION(invalidations, 0, new_artifact->PaintChunks()[1],
                            PaintInvalidationReason::kChunkAppeared);
  EXPECT_CHUNK_INVALIDATION(invalidations, 1, new_artifact->PaintChunks()[2],
                            PaintInvalidationReason::kChunkAppeared);
  FinishCycle(*new_artifact);
}

TEST_P(RasterInvalidatorTest, UncacheableChunks) {
  auto artifact =
      TestPaintArtifact().Chunk(0).Chunk(1).Uncacheable().Chunk(2).Build();

  invalidator_.Generate(artifact, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  auto new_artifact =
      TestPaintArtifact().Chunk(0).Chunk(2).Chunk(1).Uncacheable().Build();
  invalidator_.Generate(new_artifact, kDefaultLayerBounds,
                        DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(2u, invalidations.size());
  EXPECT_CHUNK_INVALIDATION(invalidations, 0, new_artifact->PaintChunks()[2],
                            PaintInvalidationReason::kChunkUncacheable);
  EXPECT_CHUNK_INVALIDATION(invalidations, 1, artifact->PaintChunks()[1],
                            PaintInvalidationReason::kChunkUncacheable);
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

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
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

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(1u, invalidations.size());
  // Property change in the layer state should not trigger raster invalidation.
  // |clip2| change should trigger raster invalidation.
  EXPECT_CHUNK_INVALIDATION(invalidations, 0, artifact->PaintChunks()[2],
                            PaintInvalidationReason::kPaintProperty);
  invalidator_.SetTracksRasterInvalidations(false);
  FinishCycle(*artifact);

  // Change chunk1's properties to use a different property tree state.
  auto new_artifact1 = TestPaintArtifact()
                           .Chunk(0)
                           .Properties(artifact->PaintChunks()[0].properties)
                           .Chunk(1)
                           .Properties(artifact->PaintChunks()[2].properties)
                           .Chunk(2)
                           .Properties(artifact->PaintChunks()[2].properties)
                           .Build();

  invalidator_.SetTracksRasterInvalidations(true);
  invalidator_.Generate(new_artifact1, kDefaultLayerBounds, layer_state);
  const auto& invalidations1 = TrackedRasterInvalidations();
  ASSERT_EQ(1u, invalidations1.size());
  EXPECT_CHUNK_INVALIDATION(invalidations1, 0, new_artifact1->PaintChunks()[1],
                            PaintInvalidationReason::kPaintProperty);
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

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  FinishCycle(*artifact);

  // Change clip1 to bigger, which is still bound by clip0, resulting no actual
  // visual change.
  invalidator_.SetTracksRasterInvalidations(true);
  FloatRoundedRect new_clip_rect1(-2000, -2000, 4000, 4000);
  clip1->Update(*clip1->Parent(),
                ClipPaintPropertyNode::State{&clip1->LocalTransformSpace(),
                                             new_clip_rect1});

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().IsEmpty());
  FinishCycle(*artifact);

  // Change clip1 to smaller.
  FloatRoundedRect new_clip_rect2(-500, -500, 1000, 1000);
  clip1->Update(*clip1->Parent(),
                ClipPaintPropertyNode::State{&clip1->LocalTransformSpace(),
                                             new_clip_rect2});

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(4u, invalidations.size());
  // |clip1| change should trigger incremental raster invalidation.
  EXPECT_INCREMENTAL_INVALIDATION(invalidations, 0, artifact->PaintChunks()[1],
                                  IntRect(-1000, -1000, 2000, 500));
  EXPECT_INCREMENTAL_INVALIDATION(invalidations, 1, artifact->PaintChunks()[1],
                                  IntRect(-1000, -500, 500, 1000));
  EXPECT_INCREMENTAL_INVALIDATION(invalidations, 2, artifact->PaintChunks()[1],
                                  IntRect(500, -500, 500, 1000));
  EXPECT_INCREMENTAL_INVALIDATION(invalidations, 3, artifact->PaintChunks()[1],
                                  IntRect(-1000, 500, 2000, 500));
  invalidator_.SetTracksRasterInvalidations(false);
  FinishCycle(*artifact);

  // Change clip1 bigger at one side.
  FloatRoundedRect new_clip_rect3(-500, -500, 2000, 1000);
  clip1->Update(*clip1->Parent(),
                ClipPaintPropertyNode::State{&clip1->LocalTransformSpace(),
                                             new_clip_rect3});

  invalidator_.SetTracksRasterInvalidations(true);
  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  const auto& invalidations1 = TrackedRasterInvalidations();
  ASSERT_EQ(1u, invalidations1.size());
  // |clip1| change should trigger incremental raster invalidation.
  EXPECT_INCREMENTAL_INVALIDATION(invalidations1, 0, artifact->PaintChunks()[1],
                                  IntRect(500, -500, 500, 1000));
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
                      .OutsetForRasterEffects(2)
                      .Build();

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  FloatRoundedRect new_clip_rect(-2000, -2000, 4000, 4000);
  clip->Update(c0(), ClipPaintPropertyNode::State{&t0(), new_clip_rect});

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(1u, invalidations.size());
  auto mapper = [](IntRect& r) { r.Inflate(2); };
  EXPECT_CHUNK_INVALIDATION_CUSTOM(invalidations, 0, artifact->PaintChunks()[0],
                                   PaintInvalidationReason::kPaintProperty,
                                   -kDefaultLayerBounds.Location(),
                                   base::BindRepeating(mapper));
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

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  FinishCycle(*artifact);

  // Change both t1 and t2 but keep t1*t2 unchanged, to test change of
  // LocalTransformSpace of c1.
  invalidator_.SetTracksRasterInvalidations(true);
  t1->Update(t0(), TransformPaintPropertyNode::State{FloatSize(-10, -20)});
  t2->Update(*t1, TransformPaintPropertyNode::State{FloatSize(10, 20)});

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(1u, invalidations.size());
  EXPECT_CHUNK_INVALIDATION(invalidations, 0, artifact->PaintChunks()[0],
                            PaintInvalidationReason::kPaintProperty);
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

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  FinishCycle(*artifact);

  // Change both t1 and t2 but keep t1*t2 unchanged.
  invalidator_.SetTracksRasterInvalidations(true);
  t1->Update(t0(), TransformPaintPropertyNode::State{FloatSize(-10, -20)});
  t2->Update(*t1, TransformPaintPropertyNode::State{FloatSize(10, 20)});

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
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

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  FinishCycle(*artifact);

  // Change layer_transform should not cause raster invalidation in the layer.
  invalidator_.SetTracksRasterInvalidations(true);
  layer_transform->Update(
      *layer_transform->Parent(),
      TransformPaintPropertyNode::State{TransformationMatrix().Scale(10)});

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
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

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().IsEmpty());
  FinishCycle(*artifact);

  // Removing transform nodes above the layer state should not cause raster
  // invalidation in the layer.
  layer_state = DefaultPropertyTreeState();
  transform0->Update(layer_state.Transform(), TransformPaintPropertyNode::State{
                                                  transform0->Translation2D()});

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
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

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(2u, invalidations.size());
  auto mapper0 = [](IntRect& r) { r.Move(10, 20); };
  EXPECT_CHUNK_INVALIDATION_CUSTOM(invalidations, 0, artifact->PaintChunks()[0],
                                   PaintInvalidationReason::kPaintProperty,
                                   -kDefaultLayerBounds.Location(),
                                   base::BindRepeating(mapper0));
  auto mapper1 = [](IntRect& r) { r.Move(30, 50); };
  EXPECT_CHUNK_INVALIDATION_CUSTOM(invalidations, 1, artifact->PaintChunks()[0],
                                   PaintInvalidationReason::kPaintProperty,
                                   -kDefaultLayerBounds.Location(),
                                   base::BindRepeating(mapper1));
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

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  FinishCycle(*artifact);

  // Change chunk_transform by tiny difference, which should be ignored.
  invalidator_.SetTracksRasterInvalidations(true);
  chunk_transform->Update(
      layer_state.Transform(),
      TransformPaintPropertyNode::State{chunk_transform->SlowMatrix()
                                            .Translate(0.0000001, -0.0000001)
                                            .Scale(1.0000001)
                                            .Rotate(0.0000001)});

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
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
    invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
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

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  FinishCycle(*artifact);

  // Scale change from 1e-6 to 2e-6 should be treated as significant.
  invalidator_.SetTracksRasterInvalidations(true);
  chunk_transform->Update(
      layer_state.Transform(),
      TransformPaintPropertyNode::State{TransformationMatrix().Scale(2e-6)});

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  EXPECT_FALSE(TrackedRasterInvalidations().IsEmpty());
  invalidator_.SetTracksRasterInvalidations(false);
  FinishCycle(*artifact);

  // Scale change from 2e-6 to 2e-6 + 1e-15 should be ignored.
  invalidator_.SetTracksRasterInvalidations(true);
  chunk_transform->Update(layer_state.Transform(),
                          TransformPaintPropertyNode::State{
                              TransformationMatrix().Scale(2e-6 + 1e-15)});

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
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

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  FinishCycle(*artifact);

  // Change both t1 and t2 but keep t1*t2 unchanged, to test change of
  // LocalTransformSpace of e1.
  invalidator_.SetTracksRasterInvalidations(true);
  t1->Update(t0(), TransformPaintPropertyNode::State{FloatSize(-10, -20)});
  t2->Update(*t1, TransformPaintPropertyNode::State{FloatSize(10, 20)});

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(1u, invalidations.size());
  auto mapper = [](IntRect& r) { r.Inflate(60); };
  EXPECT_CHUNK_INVALIDATION_CUSTOM(invalidations, 0, artifact->PaintChunks()[0],
                                   PaintInvalidationReason::kPaintProperty,
                                   -kDefaultLayerBounds.Location(),
                                   base::BindRepeating(mapper));
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

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  FinishCycle(*artifact);

  // Change both t1 and t2 but keep t1*t2 unchanged.
  invalidator_.SetTracksRasterInvalidations(true);
  t1->Update(t0(), TransformPaintPropertyNode::State{FloatSize(-10, -20)});
  t2->Update(*t1, TransformPaintPropertyNode::State{FloatSize(10, 20)});

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations().IsEmpty());
  FinishCycle(*artifact);
}

TEST_P(RasterInvalidatorTest, AliasEffectParentChanges) {
  CompositorFilterOperations filter;
  filter.AppendOpacityFilter(0.5);
  // Create an effect and an alias for that effect.
  auto e1 = CreateFilterEffect(e0(), t0(), &c0(), filter);
  auto alias_effect = EffectPaintPropertyNode::CreateAlias(*e1);

  // The artifact has a chunk pointing to the alias.
  PropertyTreeState layer_state = DefaultPropertyTreeState();
  PropertyTreeState chunk_state(t0(), c0(), *alias_effect);
  auto artifact = TestPaintArtifact().Chunk(0).Properties(chunk_state).Build();

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  // Reparent the aliased effect, so the chunk doesn't change the actual alias
  // node, but its parent is now different.
  alias_effect->Update(e0(), EffectPaintPropertyNode::State{});

  // We expect to get invalidations since the effect unaliased effect is
  // actually different now.
  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(1u, invalidations.size());
  EXPECT_CHUNK_INVALIDATION(invalidations, 0, artifact->PaintChunks()[0],
                            PaintInvalidationReason::kPaintProperty);
  FinishCycle(*artifact);
}

TEST_P(RasterInvalidatorTest, NestedAliasEffectParentChanges) {
  CompositorFilterOperations filter;
  filter.AppendOpacityFilter(0.5);
  // Create an effect and an alias for that effect.
  auto e1 = CreateFilterEffect(e0(), t0(), &c0(), filter);
  auto alias_effect_1 = EffectPaintPropertyNode::CreateAlias(*e1);
  auto alias_effect_2 = EffectPaintPropertyNode::CreateAlias(*alias_effect_1);

  // The artifact has a chunk pointing to the nested alias.
  PropertyTreeState layer_state = DefaultPropertyTreeState();
  PropertyTreeState chunk_state(t0(), c0(), *alias_effect_2);
  auto artifact = TestPaintArtifact().Chunk(0).Properties(chunk_state).Build();

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  // Reparent the parent aliased effect, so the chunk doesn't change the actual
  // alias node, but its parent is now different, this also ensures that the
  // nested alias is unchanged.
  alias_effect_1->Update(e0(), EffectPaintPropertyNode::State{});

  // We expect to get invalidations since the effect unaliased effect is
  // actually different now.
  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(1u, invalidations.size());
  EXPECT_CHUNK_INVALIDATION(invalidations, 0, artifact->PaintChunks()[0],
                            PaintInvalidationReason::kPaintProperty);
  FinishCycle(*artifact);
}

TEST_P(RasterInvalidatorTest, EffectWithAliasTransformWhoseParentChanges) {
  auto t1 = CreateTransform(t0(), TransformationMatrix().Scale(5));
  auto alias_transform = TransformPaintPropertyNode::CreateAlias(*t1);

  CompositorFilterOperations filter;
  filter.AppendBlurFilter(0);
  // Create an effect and an alias for that effect.
  auto e1 = CreateFilterEffect(e0(), *alias_transform, &c0(), filter);

  // The artifact has a chunk pointing to the alias.
  PropertyTreeState layer_state = PropertyTreeState::Root();
  PropertyTreeState chunk_state(t0(), c0(), *e1);
  auto artifact = TestPaintArtifact().Chunk(0).Properties(chunk_state).Build();

  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  FinishCycle(*artifact);

  invalidator_.SetTracksRasterInvalidations(true);
  // Reparent the aliased effect, so the chunk doesn't change the actual alias
  // node, but its parent is now different.
  alias_transform->Update(t0(), TransformPaintPropertyNode::State{});

  // We expect to get invalidations since the effect unaliased effect is
  // actually different now.
  invalidator_.Generate(artifact, kDefaultLayerBounds, layer_state);
  const auto& invalidations = TrackedRasterInvalidations();
  ASSERT_EQ(1u, invalidations.size());
  EXPECT_CHUNK_INVALIDATION(invalidations, 0, artifact->PaintChunks()[0],
                            PaintInvalidationReason::kPaintProperty);
  FinishCycle(*artifact);
}

}  // namespace blink
