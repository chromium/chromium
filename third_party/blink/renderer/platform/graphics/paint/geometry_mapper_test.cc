// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/geometry_test_helpers.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/box_reflection.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

namespace blink {

class GeometryMapperTest : public testing::Test,
                           public PaintTestConfigurations {
 public:
  const FloatClipRect* GetCachedClip(
      const ClipPaintPropertyNode& descendant_clip,
      const PropertyTreeState& ancestor_property_tree_state) {
    GeometryMapperClipCache::ClipAndTransform clip_and_transform(
        &ancestor_property_tree_state.Clip(),
        &ancestor_property_tree_state.Transform(),
        kIgnorePlatformOverlayScrollbarSize);
    return descendant_clip.GetClipCache().GetCachedClip(clip_and_transform);
  }

  void LocalToAncestorVisualRectInternal(
      const PropertyTreeState& local_state,
      const PropertyTreeState& ancestor_state,
      FloatClipRect& mapping_rect,
      bool& success) {
    GeometryMapper::LocalToAncestorVisualRectInternal(
        local_state, ancestor_state, mapping_rect,
        kIgnorePlatformOverlayScrollbarSize, kNonInclusiveIntersect, success);
  }

  // Variables required by CHECK_MAPPINGS(). The tests should set these
  // variables with proper values before calling CHECK_MAPPINGS().
  PropertyTreeState local_state = PropertyTreeState::Root();
  PropertyTreeState ancestor_state = PropertyTreeState::Root();
  FloatRect input_rect;
  FloatClipRect expected_visual_rect;
  FloatSize expected_translation_2d;
  base::Optional<TransformationMatrix> expected_transform;
  FloatClipRect expected_clip;
  FloatRect expected_transformed_rect;
};

INSTANTIATE_PAINT_TEST_SUITE_P(GeometryMapperTest);

#define EXPECT_FLOAT_RECT_NEAR(expected, actual)                             \
  do {                                                                       \
    EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual, (actual).X(),      \
                        (expected).X());                                     \
    EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual, (actual).Y(),      \
                        (expected).Y());                                     \
    EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual, (actual).Width(),  \
                        (expected).Width());                                 \
    EXPECT_PRED_FORMAT2(geometry_test::AssertAlmostEqual, (actual).Height(), \
                        (expected).Height());                                \
  } while (false)

#define EXPECT_CLIP_RECT_EQ(expected, actual)                       \
  do {                                                              \
    SCOPED_TRACE("EXPECT_CLIP_RECT_EQ: " #expected " vs " #actual); \
    EXPECT_EQ((expected).IsInfinite(), (actual).IsInfinite());      \
    EXPECT_EQ((expected).HasRadius(), (actual).HasRadius());        \
    EXPECT_EQ((expected).IsTight(), (actual).IsTight());            \
    if (!(expected).IsInfinite())                                   \
      EXPECT_FLOAT_RECT_NEAR((expected).Rect(), (actual).Rect());   \
  } while (false)

#define CHECK_LOCAL_TO_ANCESTOR_VISUAL_RECT()                              \
  do {                                                                     \
    SCOPED_TRACE("Check LocalToAncestorVisualRect");                       \
    FloatClipRect actual_visual_rect(input_rect);                          \
    GeometryMapper::LocalToAncestorVisualRect(local_state, ancestor_state, \
                                              actual_visual_rect);         \
    EXPECT_CLIP_RECT_EQ(expected_visual_rect, actual_visual_rect);         \
  } while (false)

#define CHECK_LOCAL_TO_ANCESTOR_CLIP_RECT()                                   \
  do {                                                                        \
    SCOPED_TRACE("Check LocalToAncestorClipRect");                            \
    FloatClipRect actual_clip_rect;                                           \
    actual_clip_rect =                                                        \
        GeometryMapper::LocalToAncestorClipRect(local_state, ancestor_state); \
    EXPECT_CLIP_RECT_EQ(expected_clip, actual_clip_rect);                     \
  } while (false)

#define CHECK_SOURCE_TO_DESTINATION_RECT()                              \
  do {                                                                  \
    SCOPED_TRACE("Check SourceToDestinationRect");                      \
    auto actual_transformed_rect = input_rect;                          \
    GeometryMapper::SourceToDestinationRect(local_state.Transform(),    \
                                            ancestor_state.Transform(), \
                                            actual_transformed_rect);   \
    EXPECT_FLOAT_RECT_NEAR(expected_transformed_rect,                   \
                           actual_transformed_rect);                    \
  } while (false)

#define CHECK_SOURCE_TO_DESTINATION_PROJECTION()                             \
  do {                                                                       \
    SCOPED_TRACE("Check SourceToDestinationProjection");                     \
    const auto& actual_transform_to_ancestor =                               \
        GeometryMapper::SourceToDestinationProjection(                       \
            local_state.Transform(), ancestor_state.Transform());            \
    if (expected_transform) {                                                \
      EXPECT_EQ(*expected_transform, actual_transform_to_ancestor.Matrix()); \
    } else {                                                                 \
      EXPECT_EQ(expected_translation_2d,                                     \
                actual_transform_to_ancestor.Translation2D());               \
    }                                                                        \
  } while (false)

#define CHECK_CACHED_CLIP()                                                   \
  do {                                                                        \
    if (&ancestor_state.Effect() != &local_state.Effect())                    \
      break;                                                                  \
    SCOPED_TRACE("Check cached clip");                                        \
    const auto& local_clip = local_state.Clip().Unalias();                    \
    const auto* cached_clip = GetCachedClip(local_clip, ancestor_state);      \
    if (&ancestor_state.Clip() == &local_clip ||                              \
        (&ancestor_state.Clip() == local_clip.Parent() &&                     \
         &ancestor_state.Transform() == &local_clip.LocalTransformSpace())) { \
      EXPECT_EQ(nullptr, cached_clip);                                        \
      break;                                                                  \
    }                                                                         \
    ASSERT_NE(nullptr, cached_clip);                                          \
    EXPECT_CLIP_RECT_EQ(expected_clip, *cached_clip);                         \
  } while (false)

// See the data fields of GeometryMapperTest for variables that will be used in
// this macro.
#define CHECK_MAPPINGS()                                                     \
  do {                                                                       \
    CHECK_LOCAL_TO_ANCESTOR_VISUAL_RECT();                                   \
    CHECK_LOCAL_TO_ANCESTOR_CLIP_RECT();                                     \
    CHECK_SOURCE_TO_DESTINATION_RECT();                                      \
    CHECK_SOURCE_TO_DESTINATION_PROJECTION();                                \
    {                                                                        \
      SCOPED_TRACE("Repeated check to test caching");                        \
      CHECK_LOCAL_TO_ANCESTOR_VISUAL_RECT();                                 \
      CHECK_LOCAL_TO_ANCESTOR_CLIP_RECT();                                   \
      CHECK_SOURCE_TO_DESTINATION_RECT();                                    \
      CHECK_SOURCE_TO_DESTINATION_PROJECTION();                              \
    }                                                                        \
    CHECK_CACHED_CLIP();                                                     \
  } while (false)

TEST_P(GeometryMapperTest, Root) {
  input_rect = FloatRect(0, 0, 100, 100);
  expected_visual_rect = FloatClipRect(input_rect);
  expected_transformed_rect = input_rect;
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, IdentityTransform) {
  auto transform = Create2DTranslation(t0(), 0, 0);
  local_state.SetTransform(*transform);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  expected_visual_rect = FloatClipRect(input_rect);
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, TranslationTransform) {
  expected_translation_2d = FloatSize(20, 10);
  auto transform = Create2DTranslation(t0(), 20, 10);
  local_state.SetTransform(*transform);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  expected_transformed_rect.Move(expected_translation_2d);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  CHECK_MAPPINGS();

  FloatRect rect = expected_transformed_rect;
  GeometryMapper::SourceToDestinationRect(t0(), local_state.Transform(), rect);
  EXPECT_FLOAT_RECT_NEAR(input_rect, rect);
}

TEST_P(GeometryMapperTest, TranslationTransformWithAlias) {
  expected_translation_2d = FloatSize(20, 10);
  auto real_transform = Create2DTranslation(t0(), 20, 10);
  auto transform = TransformPaintPropertyNode::CreateAlias(*real_transform);
  local_state.SetTransform(*transform);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  expected_transformed_rect.Move(expected_translation_2d);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  CHECK_MAPPINGS();

  FloatRect rect = expected_transformed_rect;
  GeometryMapper::SourceToDestinationRect(t0(), local_state.Transform(), rect);
  EXPECT_FLOAT_RECT_NEAR(input_rect, rect);
}

TEST_P(GeometryMapperTest, RotationAndScaleTransform) {
  expected_transform = TransformationMatrix().Rotate(45).Scale(2);
  auto transform = CreateTransform(t0(), *expected_transform);
  local_state.SetTransform(*transform);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect.ClearIsTight();
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, RotationAndScaleTransformWithAlias) {
  expected_transform = TransformationMatrix().Rotate(45).Scale(2);
  auto real_transform = CreateTransform(t0(), *expected_transform);
  auto transform = TransformPaintPropertyNode::CreateAlias(*real_transform);
  local_state.SetTransform(*transform);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect.ClearIsTight();
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, RotationAndScaleTransformWithTransformOrigin) {
  expected_transform = TransformationMatrix().Rotate(45).Scale(2);
  auto transform =
      CreateTransform(t0(), *expected_transform, FloatPoint3D(50, 50, 0));
  local_state.SetTransform(*transform);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transform->ApplyTransformOrigin(50, 50, 0);
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect.ClearIsTight();
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, NestedTransforms) {
  auto rotate_transform = TransformationMatrix().Rotate(45);
  auto transform1 = CreateTransform(t0(), rotate_transform);

  auto scale_transform = TransformationMatrix().Scale(2);
  auto transform2 = CreateTransform(*transform1, scale_transform);
  local_state.SetTransform(*transform2);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transform = rotate_transform * scale_transform;
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect.ClearIsTight();
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, NestedTransformsFlattening) {
  auto rotate_transform = TransformationMatrix().Rotate3d(45, 0, 0);
  auto transform1 = CreateTransform(t0(), rotate_transform);

  auto inverse_rotate_transform = TransformationMatrix().Rotate3d(-45, 0, 0);
  TransformPaintPropertyNode::State inverse_state{inverse_rotate_transform};
  inverse_state.flags.flattens_inherited_transform = true;
  auto transform2 =
      TransformPaintPropertyNode::Create(*transform1, std::move(inverse_state));
  local_state.SetTransform(*transform2);

  input_rect = FloatRect(0, 0, 100, 100);
  rotate_transform.FlattenTo2d();
  expected_transform = rotate_transform * inverse_rotate_transform;
  expected_transform->FlattenTo2d();
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect.ClearIsTight();
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, NestedTransformsScaleAndTranslation) {
  auto scale_transform = TransformationMatrix().Scale(2);
  auto transform1 = CreateTransform(t0(), scale_transform);

  auto translate_transform = TransformationMatrix().Translate(100, 0);
  auto transform2 = CreateTransform(*transform1, translate_transform);
  local_state.SetTransform(*transform2);

  input_rect = FloatRect(0, 0, 100, 100);
  // Note: unlike NestedTransforms, the order of these transforms matters. This
  // tests correct order of matrix multiplication.
  expected_transform = scale_transform * translate_transform;
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect.ClearIsTight();
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, NestedTransformsIntermediateDestination) {
  auto translate_transform = TransformationMatrix().Translate(10, 20);
  auto transform1 = CreateTransform(t0(), translate_transform);

  auto scale_transform = TransformationMatrix().Scale(3);
  auto transform2 = CreateTransform(*transform1, scale_transform);

  local_state.SetTransform(*transform2);
  ancestor_state.SetTransform(*transform1);

  expected_transform = scale_transform;
  input_rect = FloatRect(0, 0, 100, 100);
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect.ClearIsTight();
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, SimpleClip) {
  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transformed_rect = input_rect;  // not clipped.
  expected_clip = FloatClipRect(clip->ClipRect());
  expected_visual_rect = expected_clip;
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, SimpleClipWithAlias) {
  auto real_clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 50, 50));
  auto clip = ClipPaintPropertyNode::CreateAlias(*real_clip);
  local_state.SetClip(*clip);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transformed_rect = input_rect;  // not clipped.
  expected_clip = FloatClipRect(clip->Unalias().ClipRect());
  expected_visual_rect = expected_clip;
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, SimpleClipOverlayScrollbars) {
  ClipPaintPropertyNode::State clip_state;
  clip_state.local_transform_space = &t0();
  clip_state.clip_rect = FloatRoundedRect(10, 10, 50, 50);
  clip_state.clip_rect_excluding_overlay_scrollbars =
      FloatClipRect(FloatRect(10, 10, 45, 43));
  auto clip = ClipPaintPropertyNode::Create(c0(), std::move(clip_state));
  local_state.SetClip(*clip);

  input_rect = FloatRect(0, 0, 100, 100);

  FloatClipRect actual_visual_rect(input_rect);
  GeometryMapper::LocalToAncestorVisualRect(
      local_state, ancestor_state, actual_visual_rect,
      kExcludeOverlayScrollbarSizeForHitTesting);
  EXPECT_CLIP_RECT_EQ(FloatClipRect(FloatRect(10, 10, 45, 43)),
                      actual_visual_rect);

  // Check that not passing kExcludeOverlayScrollbarSizeForHitTesting gives
  // a different result.
  actual_visual_rect = FloatClipRect(input_rect);
  GeometryMapper::LocalToAncestorVisualRect(
      local_state, ancestor_state, actual_visual_rect,
      kIgnorePlatformOverlayScrollbarSize);
  EXPECT_CLIP_RECT_EQ(FloatClipRect(FloatRect(10, 10, 50, 50)),
                      actual_visual_rect);

  FloatClipRect actual_clip_rect = GeometryMapper::LocalToAncestorClipRect(
      local_state, ancestor_state, kExcludeOverlayScrollbarSizeForHitTesting);
  EXPECT_CLIP_RECT_EQ(FloatClipRect(FloatRect(10, 10, 45, 43)),
                      actual_clip_rect);

  // Check that not passing kExcludeOverlayScrollbarSizeForHitTesting gives
  // a different result.
  actual_clip_rect = GeometryMapper::LocalToAncestorClipRect(
      local_state, ancestor_state, kIgnorePlatformOverlayScrollbarSize);
  EXPECT_CLIP_RECT_EQ(FloatClipRect(FloatRect(10, 10, 50, 50)),
                      actual_clip_rect);
}

TEST_P(GeometryMapperTest, SimpleClipInclusiveIntersect) {
  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip);

  FloatClipRect actual_clip_rect(FloatRect(60, 10, 10, 10));
  GeometryMapper::LocalToAncestorVisualRect(
      local_state, ancestor_state, actual_clip_rect,
      kIgnorePlatformOverlayScrollbarSize, kInclusiveIntersect);
  EXPECT_CLIP_RECT_EQ(FloatClipRect(FloatRect(60, 10, 0, 10)),
                      actual_clip_rect);

  // Check that not passing kExcludeOverlayScrollbarSizeForHitTesting gives
  // a different result.
  actual_clip_rect.SetRect(FloatRect(60, 10, 10, 10));
  GeometryMapper::LocalToAncestorVisualRect(
      local_state, ancestor_state, actual_clip_rect,
      kIgnorePlatformOverlayScrollbarSize, kNonInclusiveIntersect);
  EXPECT_CLIP_RECT_EQ(FloatClipRect(FloatRect()), actual_clip_rect);
}

TEST_P(GeometryMapperTest, SimpleClipPlusOpacity) {
  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip);

  auto opacity = CreateOpacityEffect(e0(), 0.99);
  local_state.SetEffect(*opacity);

  FloatClipRect actual_clip_rect(FloatRect(60, 10, 10, 10));
  auto intersects = GeometryMapper::LocalToAncestorVisualRect(
      local_state, ancestor_state, actual_clip_rect);

  EXPECT_TRUE(actual_clip_rect.Rect().IsEmpty());
  EXPECT_FALSE(intersects);
}

TEST_P(GeometryMapperTest, SimpleClipPlusOpacityInclusiveIntersect) {
  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip);

  auto opacity = CreateOpacityEffect(e0(), 0.99);
  local_state.SetEffect(*opacity);

  FloatClipRect actual_clip_rect(FloatRect(10, 10, 10, 0));
  auto intersects = GeometryMapper::LocalToAncestorVisualRect(
      local_state, ancestor_state, actual_clip_rect,
      kIgnorePlatformOverlayScrollbarSize, kInclusiveIntersect);

  EXPECT_TRUE(actual_clip_rect.Rect().IsEmpty());
  EXPECT_TRUE(intersects);
}

TEST_P(GeometryMapperTest, RoundedClip) {
  FloatRoundedRect rect(FloatRect(10, 10, 50, 50),
                        FloatRoundedRect::Radii(FloatSize(1, 1), FloatSize(),
                                                FloatSize(), FloatSize()));
  auto clip = CreateClip(c0(), t0(), rect);
  local_state.SetClip(*clip);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  expected_clip = FloatClipRect(clip->ClipRect());
  EXPECT_TRUE(expected_clip.HasRadius());
  expected_visual_rect = expected_clip;
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, ClipPath) {
  FloatRoundedRect rect(FloatRect(10, 10, 50, 50),
                        FloatRoundedRect::Radii(FloatSize(1, 1), FloatSize(),
                                                FloatSize(), FloatSize()));
  auto clip = CreateClipPathClip(c0(), t0(), FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  expected_clip = FloatClipRect(FloatRect(10, 10, 50, 50));
  expected_clip.ClearIsTight();
  expected_visual_rect = expected_clip;
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, TwoClips) {
  FloatRoundedRect clip_rect1(
      FloatRect(10, 10, 30, 40),
      FloatRoundedRect::Radii(FloatSize(1, 1), FloatSize(), FloatSize(),
                              FloatSize()));

  auto clip1 = CreateClip(c0(), t0(), clip_rect1);
  auto clip2 = CreateClip(*clip1, t0(), FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip2);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  expected_clip = FloatClipRect(clip1->ClipRect());
  EXPECT_TRUE(expected_clip.HasRadius());
  expected_visual_rect = expected_clip;
  CHECK_MAPPINGS();

  ancestor_state.SetClip(*clip1);
  expected_clip = FloatClipRect(clip2->ClipRect());
  expected_visual_rect = expected_clip;
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, TwoClipsTransformAbove) {
  auto transform = CreateTransform(t0(), TransformationMatrix());

  FloatRoundedRect clip_rect1(
      FloatRect(10, 10, 50, 50),
      FloatRoundedRect::Radii(FloatSize(1, 1), FloatSize(), FloatSize(),
                              FloatSize()));

  auto clip1 = CreateClip(c0(), *transform, clip_rect1);
  auto clip2 = CreateClip(*clip1, *transform, FloatRoundedRect(10, 10, 30, 40));
  local_state.SetClip(*clip2);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  expected_clip = FloatClipRect(clip2->ClipRect());
  expected_clip.SetHasRadius();
  expected_visual_rect = expected_clip;
  CHECK_MAPPINGS();

  expected_clip = FloatClipRect(clip1->ClipRect());
  EXPECT_TRUE(expected_clip.HasRadius());
  local_state.SetClip(*clip1);
  expected_visual_rect = expected_clip;
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, ClipBeforeTransform) {
  expected_transform = TransformationMatrix().Rotate(45);
  auto transform = CreateTransform(t0(), *expected_transform);
  auto clip = CreateClip(c0(), *transform, FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip);
  local_state.SetTransform(*transform);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_visual_rect = FloatClipRect(input_rect);
  expected_visual_rect.Intersect(FloatClipRect(clip->ClipRect()));
  expected_visual_rect.Map(*expected_transform);
  EXPECT_FALSE(expected_visual_rect.IsTight());
  expected_clip = FloatClipRect(clip->ClipRect());
  expected_clip.Map(*expected_transform);
  EXPECT_FALSE(expected_clip.IsTight());
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, ClipAfterTransform) {
  expected_transform = TransformationMatrix().Rotate(45);
  auto transform = CreateTransform(t0(), *expected_transform);
  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 200, 200));
  local_state.SetClip(*clip);
  local_state.SetTransform(*transform);

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(input_rect);
  expected_visual_rect.Map(*expected_transform);
  expected_visual_rect.Intersect(FloatClipRect(clip->ClipRect()));
  EXPECT_FALSE(expected_visual_rect.IsTight());
  expected_clip = FloatClipRect(clip->ClipRect());
  EXPECT_TRUE(expected_clip.IsTight());
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, TwoClipsWithTransformBetween) {
  auto clip1 = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 200, 200));
  expected_transform = TransformationMatrix().Rotate(45);
  auto transform = CreateTransform(t0(), *expected_transform);
  auto clip2 =
      CreateClip(*clip1, *transform, FloatRoundedRect(10, 10, 200, 200));

  input_rect = FloatRect(0, 0, 100, 100);
  expected_transformed_rect = expected_transform->MapRect(input_rect);

  {
    local_state.SetClip(*clip1);
    local_state.SetTransform(*transform);

    expected_visual_rect = FloatClipRect(input_rect);
    expected_visual_rect.Map(*expected_transform);
    expected_visual_rect.Intersect(FloatClipRect(clip1->ClipRect()));
    EXPECT_FALSE(expected_visual_rect.IsTight());
    expected_clip = FloatClipRect(clip1->ClipRect());
    EXPECT_TRUE(expected_clip.IsTight());
    CHECK_MAPPINGS();
  }

  {
    local_state.SetClip(*clip2);
    local_state.SetTransform(*transform);

    expected_clip = FloatClipRect(clip2->ClipRect());
    expected_clip.Map(*expected_transform);
    expected_clip.Intersect(FloatClipRect(clip1->ClipRect()));
    EXPECT_FALSE(expected_clip.IsTight());

    // All clips are performed in the space of the ancestor. In cases such as
    // this, this means the clip is not tight.
    expected_visual_rect = FloatClipRect(input_rect);
    expected_visual_rect.Map(*expected_transform);
    // Intersect with all clips between local and ancestor, independently mapped
    // to ancestor space.
    expected_visual_rect.Intersect(expected_clip);
    EXPECT_FALSE(expected_visual_rect.IsTight());

    CHECK_MAPPINGS();
  }
}

TEST_P(GeometryMapperTest, SiblingTransforms) {
  // These transforms are siblings. Thus mapping from one to the other requires
  // going through the root.
  auto rotate_transform1 = TransformationMatrix().Rotate(45);
  auto transform1 = CreateTransform(t0(), rotate_transform1);

  auto rotate_transform2 = TransformationMatrix().Rotate(-45);
  auto transform2 = CreateTransform(t0(), rotate_transform2);

  auto transform1_state = PropertyTreeState::Root();
  transform1_state.SetTransform(*transform1);
  auto transform2_state = PropertyTreeState::Root();
  transform2_state.SetTransform(*transform2);

  input_rect = FloatRect(0, 0, 100, 100);
  FloatClipRect result_clip(input_rect);
  GeometryMapper::LocalToAncestorVisualRect(transform1_state, transform2_state,
                                            result_clip);
  FloatClipRect expected_clip(FloatRect(-100, 0, 100, 100));
  // We convervatively treat any rotated clip rect as not tight, even if it's
  // rotated by 90 degrees.
  expected_clip.ClearIsTight();
  EXPECT_CLIP_RECT_EQ(expected_clip, result_clip);

  FloatRect result = input_rect;
  GeometryMapper::SourceToDestinationRect(*transform1, *transform2, result);
  EXPECT_FLOAT_RECT_NEAR(FloatRect(-100, 0, 100, 100), result);

  result_clip = FloatClipRect(input_rect);
  GeometryMapper::LocalToAncestorVisualRect(transform2_state, transform1_state,
                                            result_clip);
  expected_clip = FloatClipRect(FloatRect(0, -100, 100, 100));
  expected_clip.ClearIsTight();
  EXPECT_CLIP_RECT_EQ(expected_clip, result_clip);

  result = input_rect;
  GeometryMapper::SourceToDestinationRect(*transform2, *transform1, result);
  EXPECT_FLOAT_RECT_NEAR(FloatRect(0, -100, 100, 100), result);
}

TEST_P(GeometryMapperTest, SiblingTransformsWithClip) {
  // These transforms are siblings. Thus mapping from one to the other requires
  // going through the root.
  auto rotate_transform1 = TransformationMatrix().Rotate(45);
  auto transform1 = CreateTransform(t0(), rotate_transform1);

  auto rotate_transform2 = TransformationMatrix().Rotate(-45);
  auto transform2 = CreateTransform(t0(), rotate_transform2);

  auto clip = CreateClip(c0(), *transform2, FloatRoundedRect(10, 20, 30, 40));

  auto transform1_state = PropertyTreeState::Root();
  transform1_state.SetTransform(*transform1);
  auto transform2_and_clip_state = PropertyTreeState::Root();
  transform2_and_clip_state.SetTransform(*transform2);
  transform2_and_clip_state.SetClip(*clip);

  bool success;
  input_rect = FloatRect(0, 0, 100, 100);
  FloatClipRect result(input_rect);
  LocalToAncestorVisualRectInternal(transform1_state, transform2_and_clip_state,
                                    result, success);
  // Fails, because the clip of the destination state is not an ancestor of the
  // clip of the source state. A known bug in SPv1 would make such query,
  // in such case, no clips are applied.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_FALSE(success);
  } else {
    EXPECT_TRUE(success);
    FloatClipRect expected(FloatRect(-100, 0, 100, 100));
    expected.ClearIsTight();
    EXPECT_CLIP_RECT_EQ(expected, result);
  }

  result = FloatClipRect(input_rect);
  GeometryMapper::LocalToAncestorVisualRect(transform2_and_clip_state,
                                            transform1_state, result);
  FloatClipRect expected(FloatRect(20, -40, 40, 30));
  // This is because the combined Rotate(45) and Rotate(-45) is not exactly a
  // translation-only transform due to calculation errors.
  expected.ClearIsTight();
  EXPECT_CLIP_RECT_EQ(expected, result);
}

TEST_P(GeometryMapperTest, FilterWithClipsAndTransforms) {
  auto transform_above_effect = Create2DTranslation(t0(), 40, 50);
  auto transform_below_effect =
      Create2DTranslation(*transform_above_effect, 20, 30);

  // This clip is between transformAboveEffect and the effect.
  auto clip_above_effect = CreateClip(c0(), *transform_above_effect,
                                      FloatRoundedRect(-100, -100, 200, 200));
  // This clip is between the effect and transformBelowEffect.
  auto clip_below_effect =
      CreateClip(*clip_above_effect, *transform_above_effect,
                 FloatRoundedRect(10, 10, 100, 100));

  CompositorFilterOperations filters;
  filters.AppendBlurFilter(20);
  auto effect = CreateFilterEffect(e0(), *transform_above_effect,
                                   clip_above_effect.get(), filters);

  local_state =
      PropertyTreeState(*transform_below_effect, *clip_below_effect, *effect);

  input_rect = FloatRect(0, 0, 100, 100);
  // 1. transformBelowEffect
  auto output = input_rect;
  output.Move(transform_below_effect->Translation2D());
  // 2. clipBelowEffect
  output.Intersect(clip_below_effect->ClipRect().Rect());
  EXPECT_EQ(FloatRect(20, 30, 90, 80), output);
  // 3. effect (the outset is 3 times of blur amount).
  output = filters.MapRect(output);
  EXPECT_EQ(FloatRect(-40, -30, 210, 200), output);
  // 4. clipAboveEffect
  output.Intersect(clip_above_effect->ClipRect().Rect());
  EXPECT_EQ(FloatRect(-40, -30, 140, 130), output);
  // 5. transformAboveEffect
  output.Move(transform_above_effect->Translation2D());
  EXPECT_EQ(FloatRect(0, 20, 140, 130), output);

  expected_translation_2d = transform_above_effect->Translation2D() +
                            transform_below_effect->Translation2D();
  expected_transformed_rect = input_rect;
  expected_transformed_rect.Move(expected_translation_2d);
  expected_visual_rect = FloatClipRect(output);
  expected_visual_rect.ClearIsTight();
  expected_clip = FloatClipRect(FloatRect(50, 60, 90, 90));
  expected_clip.ClearIsTight();
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, FilterWithClipsAndTransformsWithAlias) {
  auto transform_above_effect = Create2DTranslation(t0(), 40, 50);
  auto transform_below_effect =
      Create2DTranslation(*transform_above_effect, 20, 30);

  // This clip is between transformAboveEffect and the effect.
  auto clip_above_effect = CreateClip(c0(), *transform_above_effect,
                                      FloatRoundedRect(-100, -100, 200, 200));
  // This clip is between the effect and transformBelowEffect.
  auto clip_below_effect =
      CreateClip(*clip_above_effect, *transform_above_effect,
                 FloatRoundedRect(10, 10, 100, 100));

  CompositorFilterOperations filters;
  filters.AppendBlurFilter(20);
  auto real_effect = CreateFilterEffect(e0(), *transform_above_effect,
                                        clip_above_effect.get(), filters);
  auto effect = EffectPaintPropertyNode::CreateAlias(*real_effect);

  local_state =
      PropertyTreeState(*transform_below_effect, *clip_below_effect, *effect);

  input_rect = FloatRect(0, 0, 100, 100);
  // 1. transformBelowEffect
  auto output = input_rect;
  output.Move(transform_below_effect->Translation2D());
  // 2. clipBelowEffect
  output.Intersect(clip_below_effect->ClipRect().Rect());
  EXPECT_EQ(FloatRect(20, 30, 90, 80), output);
  // 3. effect (the outset is 3 times of blur amount).
  output = filters.MapRect(output);
  EXPECT_EQ(FloatRect(-40, -30, 210, 200), output);
  // 4. clipAboveEffect
  output.Intersect(clip_above_effect->ClipRect().Rect());
  EXPECT_EQ(FloatRect(-40, -30, 140, 130), output);
  // 5. transformAboveEffect
  output.Move(transform_above_effect->Translation2D());
  EXPECT_EQ(FloatRect(0, 20, 140, 130), output);

  expected_translation_2d = transform_above_effect->Translation2D() +
                            transform_below_effect->Translation2D();
  expected_transformed_rect = input_rect;
  expected_transformed_rect.Move(expected_translation_2d);
  expected_visual_rect = FloatClipRect(output);
  expected_visual_rect.ClearIsTight();
  expected_clip = FloatClipRect(FloatRect(50, 60, 90, 90));
  expected_clip.ClearIsTight();
  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, ReflectionWithPaintOffset) {
  CompositorFilterOperations filters;
  filters.AppendReferenceFilter(paint_filter_builder::BuildBoxReflectFilter(
      BoxReflection(BoxReflection::kHorizontalReflection, 0), nullptr));
  auto effect = CreateFilterEffect(e0(), filters, FloatPoint(100, 100));
  local_state.SetEffect(*effect);

  input_rect = FloatRect(100, 100, 50, 50);
  expected_transformed_rect = input_rect;
  // Reflection is at (50, 100, 50, 50).
  expected_visual_rect = FloatClipRect(FloatRect(50, 100, 100, 50));
  expected_visual_rect.ClearIsTight();

  CHECK_MAPPINGS();
}

TEST_P(GeometryMapperTest, InvertedClip) {
  // This test is invalid for CAP.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 50, 50));
  PropertyTreeState dest(t0(), *clip, e0());

  FloatClipRect visual_rect(FloatRect(0, 0, 10, 200));
  EXPECT_TRUE(visual_rect.IsTight());

  GeometryMapper::LocalToAncestorVisualRect(PropertyTreeState::Root(), dest,
                                            visual_rect);

  // The "ancestor" clip is below the source clip in this case, so
  // LocalToAncestorVisualRect must fall back to the original rect, mapped
  // into the root space.
  EXPECT_EQ(FloatRect(0, 0, 10, 200), visual_rect.Rect());
  EXPECT_TRUE(visual_rect.IsTight());
}

TEST_P(GeometryMapperTest, Precision) {
  auto t1 = CreateTransform(t0(), TransformationMatrix().Scale(32767));
  auto t2 = CreateTransform(*t1, TransformationMatrix().Rotate(1));
  auto t3 = CreateTransform(*t2, TransformationMatrix());
  auto t4 = CreateTransform(*t3, TransformationMatrix());
  EXPECT_TRUE(
      GeometryMapper::SourceToDestinationProjection(*t4, *t4).IsIdentity());
  EXPECT_TRUE(
      GeometryMapper::SourceToDestinationProjection(*t3, *t4).IsIdentity());
  EXPECT_TRUE(
      GeometryMapper::SourceToDestinationProjection(*t2, *t4).IsIdentity());
  EXPECT_TRUE(
      GeometryMapper::SourceToDestinationProjection(*t3, *t2).IsIdentity());
  EXPECT_TRUE(
      GeometryMapper::SourceToDestinationProjection(*t4, *t2).IsIdentity());
  EXPECT_TRUE(
      GeometryMapper::SourceToDestinationProjection(*t4, *t3).IsIdentity());
  EXPECT_TRUE(
      GeometryMapper::SourceToDestinationProjection(*t2, *t3).IsIdentity());
}

}  // namespace blink
