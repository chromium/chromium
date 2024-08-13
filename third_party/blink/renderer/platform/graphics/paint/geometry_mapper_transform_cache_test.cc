// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_transform_cache.h"

#include <utility>

#include "base/types/optional_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

namespace blink {

class GeometryMapperTransformCacheTest : public testing::Test {
 protected:
  static const GeometryMapperTransformCache& GetTransformCache(
      const TransformPaintPropertyNode& transform) {
    return transform.GetTransformCache();
  }

  static void UpdateScreenTransform(const TransformPaintPropertyNode& node) {
    node.GetTransformCache();  // Ensure the transform cache.
    node.UpdateScreenTransform();
  }

  static bool ScreenTransformUpdated(const TransformPaintPropertyNode& node) {
    return node.GetTransformCache().screen_transform_updated_;
  }

  static const GeometryMapperTransformCache::ScreenTransform*
  GetScreenTransform(const TransformPaintPropertyNode& node) {
    return base::OptionalToPtr(GetTransformCache(node).screen_transform_);
  }

  static void Check2dTranslationToRoot(const TransformPaintPropertyNode& node,
                                       double x,
                                       double y) {
    const auto& cache = GetTransformCache(node);
    EXPECT_EQ(&t0(), cache.root_of_2d_translation());
    EXPECT_EQ(gfx::Vector2dF(x, y), cache.to_2d_translation_root());

    EXPECT_TRUE(ScreenTransformUpdated(node));
    EXPECT_FALSE(GetScreenTransform(node));

    EXPECT_EQ(&t0(), cache.plane_root());
    gfx::Transform actual_to_plane_root;
    cache.ApplyToPlaneRoot(actual_to_plane_root);
    EXPECT_EQ(MakeTranslationMatrix(x, y), actual_to_plane_root);
    gfx::Transform actual_from_plane_root;
    cache.ApplyFromPlaneRoot(actual_from_plane_root);
    EXPECT_EQ(MakeTranslationMatrix(-x, -y), actual_from_plane_root);
  }

  static void CheckRootAsPlaneRoot(
      const TransformPaintPropertyNode& node,
      const TransformPaintPropertyNode& root_of_2d_translation,
      const gfx::Transform& to_plane_root,
      double translate_x,
      double translate_y) {
    const auto& cache = GetTransformCache(node);
    EXPECT_EQ(&root_of_2d_translation, cache.root_of_2d_translation());
    EXPECT_EQ(gfx::Vector2dF(translate_x, translate_y),
              cache.to_2d_translation_root());

    EXPECT_TRUE(ScreenTransformUpdated(node));
    EXPECT_FALSE(GetScreenTransform(node));

    EXPECT_EQ(&t0(), cache.plane_root());
    EXPECT_EQ(to_plane_root, cache.to_plane_root());
    EXPECT_EQ(to_plane_root.InverseOrIdentity(), cache.from_plane_root());

    gfx::Transform actual_to_screen;
    cache.ApplyToScreen(actual_to_screen);
    EXPECT_EQ(to_plane_root, actual_to_screen);
    gfx::Transform actual_projection_from_screen;
    cache.ApplyProjectionFromScreen(actual_projection_from_screen);
    EXPECT_EQ(to_plane_root.InverseOrIdentity(), actual_projection_from_screen);
  }

  static void CheckPlaneRootSameAs2dTranslationRoot(
      const TransformPaintPropertyNode& node,
      const gfx::Transform& to_screen,
      const TransformPaintPropertyNode& plane_root,
      double translate_x,
      double translate_y) {
    const auto& cache = GetTransformCache(node);
    EXPECT_EQ(&plane_root, cache.root_of_2d_translation());
    EXPECT_EQ(gfx::Vector2dF(translate_x, translate_y),
              cache.to_2d_translation_root());

    EXPECT_FALSE(ScreenTransformUpdated(node));
    UpdateScreenTransform(node);
    EXPECT_TRUE(ScreenTransformUpdated(node));
    EXPECT_TRUE(GetScreenTransform(node));

    EXPECT_EQ(&plane_root, cache.plane_root());
    EXPECT_EQ(to_screen, cache.to_screen());
    auto projection_from_screen = to_screen;
    projection_from_screen.Flatten();
    projection_from_screen = projection_from_screen.InverseOrIdentity();
    EXPECT_EQ(projection_from_screen, cache.projection_from_screen());
  }

  static void CheckPlaneRootDifferent2dTranslationRoot(
      const TransformPaintPropertyNode& node,
      const gfx::Transform& to_screen,
      const TransformPaintPropertyNode& plane_root,
      const gfx::Transform& to_plane_root,
      const TransformPaintPropertyNode& root_of_2d_translation,
      double translate_x,
      double translate_y) {
    const auto& cache = GetTransformCache(node);
    EXPECT_EQ(&root_of_2d_translation, cache.root_of_2d_translation());
    EXPECT_EQ(gfx::Vector2dF(translate_x, translate_y),
              cache.to_2d_translation_root());

    EXPECT_FALSE(ScreenTransformUpdated(node));
    UpdateScreenTransform(node);
    EXPECT_TRUE(ScreenTransformUpdated(node));
    EXPECT_TRUE(GetScreenTransform(node));

    EXPECT_EQ(&plane_root, cache.plane_root());
    EXPECT_EQ(to_plane_root, cache.to_plane_root());
    EXPECT_EQ(to_plane_root.InverseOrIdentity(), cache.from_plane_root());
    EXPECT_EQ(to_screen, cache.to_screen());
    auto projection_from_screen = to_screen;
    projection_from_screen.Flatten();
    projection_from_screen = projection_from_screen.InverseOrIdentity();
    EXPECT_EQ(projection_from_screen, cache.projection_from_screen());
  }

  static bool HasAnimationToPlaneRoot(const TransformPaintPropertyNode& node) {
    return node.GetTransformCache().has_animation_to_plane_root();
  }

  static bool HasAnimationToScreen(const TransformPaintPropertyNode& node) {
    return node.GetTransformCache().has_animation_to_screen();
  }
};

TEST_F(GeometryMapperTransformCacheTest, All2dTranslations) {
  auto* t1 = Create2DTranslation(t0(), 1, 2);
  auto* t2 = Create2DTranslation(*t1, 0, 0);
  auto* t3 = Create2DTranslation(*t2, 7, 8);

  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);
  Check2dTranslationToRoot(*t2, 1, 2);
  Check2dTranslationToRoot(*t3, 8, 10);
}

TEST_F(GeometryMapperTransformCacheTest, RootAsPlaneRootWithIntermediateScale) {
  auto* t1 = Create2DTranslation(t0(), 1, 2);
  auto* t2 = CreateTransform(*t1, MakeScaleMatrix(3));
  auto* t3 = Create2DTranslation(*t2, 7, 8);

  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);
  auto to_plane_root = MakeTranslationMatrix(1, 2);
  to_plane_root.Scale(3);
  CheckRootAsPlaneRoot(*t2, *t2, to_plane_root, 0, 0);
  to_plane_root.Translate(7, 8);
  CheckRootAsPlaneRoot(*t3, *t2, to_plane_root, 7, 8);
}

TEST_F(GeometryMapperTransformCacheTest,
       IntermediatePlaneRootSameAs2dTranslationRoot) {
  auto* t1 = Create2DTranslation(t0(), 1, 2);
  auto* t2 = CreateTransform(*t1, MakeRotationMatrix(0, 45, 0));
  auto* t3 = Create2DTranslation(*t2, 7, 8);

  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);
  auto to_screen = MakeTranslationMatrix(1, 2);
  to_screen.RotateAboutYAxis(45);
  CheckPlaneRootSameAs2dTranslationRoot(*t2, to_screen, *t2, 0, 0);
  to_screen.Translate(7, 8);
  CheckPlaneRootSameAs2dTranslationRoot(*t3, to_screen, *t2, 7, 8);
}

TEST_F(GeometryMapperTransformCacheTest,
       IntermediatePlaneRootDifferent2dTranslationRoot) {
  auto* t1 = Create2DTranslation(t0(), 1, 2);
  auto* t2 = CreateTransform(*t1, MakeRotationMatrix(0, 45, 0));
  auto* t3 = CreateTransform(*t2, MakeScaleMatrix(3));
  auto* t4 = Create2DTranslation(*t3, 7, 8);

  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);

  auto to_screen = MakeTranslationMatrix(1, 2);
  to_screen.RotateAboutYAxis(45);
  CheckPlaneRootSameAs2dTranslationRoot(*t2, to_screen, *t2, 0, 0);

  auto to_plane_root = MakeScaleMatrix(3);
  to_screen.Scale(3, 3);
  CheckPlaneRootDifferent2dTranslationRoot(*t3, to_screen, *t2, to_plane_root,
                                           *t3, 0, 0);

  to_plane_root.Translate(7, 8);
  to_screen.Translate(7, 8);
  CheckPlaneRootDifferent2dTranslationRoot(*t4, to_screen, *t2, to_plane_root,
                                           *t3, 7, 8);
}

TEST_F(GeometryMapperTransformCacheTest, TransformUpdate) {
  auto* t1 = Create2DTranslation(t0(), 1, 2);
  auto* t2 = Create2DTranslation(*t1, 0, 0);
  auto* t3 = Create2DTranslation(*t2, 7, 8);

  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);
  Check2dTranslationToRoot(*t2, 1, 2);
  Check2dTranslationToRoot(*t3, 8, 10);

  // Change t2 to a scale.
  GeometryMapperTransformCache::ClearCache();
  t2->Update(*t1, TransformPaintPropertyNode::State{{MakeScaleMatrix(3)}});
  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);
  auto to_plane_root = MakeTranslationMatrix(1, 2);
  to_plane_root.Scale(3);
  CheckRootAsPlaneRoot(*t2, *t2, to_plane_root, 0, 0);
  to_plane_root.Translate(7, 8);
  CheckRootAsPlaneRoot(*t3, *t2, to_plane_root, 7, 8);

  // Change t2 to a 3d transform so that it becomes a plane root.
  GeometryMapperTransformCache::ClearCache();
  t2->Update(*t1,
             TransformPaintPropertyNode::State{{MakeRotationMatrix(0, 45, 0)}});
  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);

  auto t2_to_screen = MakeTranslationMatrix(1, 2);
  t2_to_screen.RotateAboutYAxis(45);
  CheckPlaneRootSameAs2dTranslationRoot(*t2, t2_to_screen, *t2, 0, 0);
  auto t3_to_screen = t2_to_screen;
  t3_to_screen.Translate(7, 8);
  CheckPlaneRootSameAs2dTranslationRoot(*t3, t3_to_screen, *t2, 7, 8);

  auto* t2_screen_transform = GetScreenTransform(*t2);
  ASSERT_TRUE(t2_screen_transform);
  auto* t3_screen_transform = GetScreenTransform(*t3);
  ASSERT_TRUE(t3_screen_transform);

  // UpdateScreenTransform should not reallocate screen_transform_.
  UpdateScreenTransform(*t2);
  EXPECT_TRUE(ScreenTransformUpdated(*t2));
  UpdateScreenTransform(*t3);
  EXPECT_TRUE(ScreenTransformUpdated(*t3));
  EXPECT_EQ(t2_screen_transform, GetScreenTransform(*t2));
  EXPECT_EQ(t3_screen_transform, GetScreenTransform(*t3));

  // Invalidating cache should invalidate screen_transform_ but not free it.
  GeometryMapperTransformCache::ClearCache();
  t3->Update(
      *t2, TransformPaintPropertyNode::State{{MakeTranslationMatrix(28, 27)}});
  EXPECT_FALSE(ScreenTransformUpdated(*t2));
  EXPECT_FALSE(ScreenTransformUpdated(*t3));
  EXPECT_EQ(t2_screen_transform, GetScreenTransform(*t2));
  EXPECT_EQ(t3_screen_transform, GetScreenTransform(*t3));

  // Update screen transforms (by CheckPlaneRootSameAs2dTranslationRoot()).
  // Screen transforms should be valid and have expected values.
  CheckPlaneRootSameAs2dTranslationRoot(*t2, t2_to_screen, *t2, 0, 0);
  t3_to_screen = t2_to_screen;
  t3_to_screen.Translate(28, 27);
  CheckPlaneRootSameAs2dTranslationRoot(*t3, t3_to_screen, *t2, 28, 27);
  // The pointers should be also the same as before.
  EXPECT_EQ(t2_screen_transform, GetScreenTransform(*t2));
  EXPECT_EQ(t3_screen_transform, GetScreenTransform(*t3));

  // Change t2 back to a 2d translation.
  GeometryMapperTransformCache::ClearCache();
  t2->Update(
      *t1, TransformPaintPropertyNode::State{{MakeTranslationMatrix(11, 12)}});
  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);
  Check2dTranslationToRoot(*t2, 1 + 11, 2 + 12);
  Check2dTranslationToRoot(*t3, 1 + 11 + 28, 2 + 12 + 27);
}

}  // namespace blink
