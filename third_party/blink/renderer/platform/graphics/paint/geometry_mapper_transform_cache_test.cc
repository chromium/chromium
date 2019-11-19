// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_transform_cache.h"

#include <utility>
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
    node.UpdateScreenTransform();
  }

  static bool HasPlaneRootTransform(const TransformPaintPropertyNode& node) {
    return !!node.GetTransformCache().plane_root_transform_;
  }

  static bool HasScreenTransform(const TransformPaintPropertyNode& node) {
    return !!node.GetTransformCache().screen_transform_;
  }

  static void Check2dTranslationToRoot(const TransformPaintPropertyNode& node,
                                       double x,
                                       double y) {
    const auto& cache = GetTransformCache(node);
    EXPECT_EQ(&t0(), cache.root_of_2d_translation());
    EXPECT_EQ(FloatSize(x, y), cache.to_2d_translation_root());

    EXPECT_FALSE(HasPlaneRootTransform(node));
    EXPECT_FALSE(HasScreenTransform(node));
    UpdateScreenTransform(node);
    EXPECT_FALSE(HasPlaneRootTransform(node));
    EXPECT_FALSE(HasScreenTransform(node));

    EXPECT_EQ(&t0(), cache.plane_root());
    TransformationMatrix actual_to_plane_root;
    cache.ApplyToPlaneRoot(actual_to_plane_root);
    EXPECT_EQ(TransformationMatrix().Translate(x, y), actual_to_plane_root);
    TransformationMatrix actual_from_plane_root;
    cache.ApplyFromPlaneRoot(actual_from_plane_root);
    EXPECT_EQ(TransformationMatrix().Translate(-x, -y), actual_from_plane_root);
  }

  static void CheckRootAsPlaneRoot(
      const TransformPaintPropertyNode& node,
      const TransformPaintPropertyNode& root_of_2d_translation,
      const TransformationMatrix& to_plane_root,
      double translate_x,
      double translate_y) {
    const auto& cache = GetTransformCache(node);
    EXPECT_EQ(&root_of_2d_translation, cache.root_of_2d_translation());
    EXPECT_EQ(FloatSize(translate_x, translate_y),
              cache.to_2d_translation_root());

    EXPECT_TRUE(HasPlaneRootTransform(node));
    EXPECT_FALSE(HasScreenTransform(node));
    UpdateScreenTransform(node);
    EXPECT_TRUE(HasPlaneRootTransform(node));
    EXPECT_FALSE(HasScreenTransform(node));

    EXPECT_EQ(&t0(), cache.plane_root());
    EXPECT_EQ(to_plane_root, cache.to_plane_root());
    EXPECT_EQ(to_plane_root.Inverse(), cache.from_plane_root());

    TransformationMatrix actual_to_screen;
    cache.ApplyToScreen(actual_to_screen);
    EXPECT_EQ(to_plane_root, actual_to_screen);
    TransformationMatrix actual_projection_from_screen;
    cache.ApplyProjectionFromScreen(actual_projection_from_screen);
    EXPECT_EQ(to_plane_root.Inverse(), actual_projection_from_screen);
  }

  static void CheckPlaneRootSameAs2dTranslationRoot(
      const TransformPaintPropertyNode& node,
      const TransformationMatrix& to_screen,
      const TransformPaintPropertyNode& plane_root,
      double translate_x,
      double translate_y) {
    const auto& cache = GetTransformCache(node);
    EXPECT_EQ(&plane_root, cache.root_of_2d_translation());
    EXPECT_EQ(FloatSize(translate_x, translate_y),
              cache.to_2d_translation_root());

    EXPECT_FALSE(HasPlaneRootTransform(node));
    EXPECT_FALSE(HasScreenTransform(node));
    UpdateScreenTransform(node);
    EXPECT_FALSE(HasPlaneRootTransform(node));
    EXPECT_TRUE(HasScreenTransform(node));

    EXPECT_EQ(&plane_root, cache.plane_root());
    EXPECT_EQ(to_screen, cache.to_screen());
    auto projection_from_screen = to_screen;
    projection_from_screen.FlattenTo2d();
    projection_from_screen = projection_from_screen.Inverse();
    EXPECT_EQ(projection_from_screen, cache.projection_from_screen());
  }

  static void CheckPlaneRootDifferent2dTranslationRoot(
      const TransformPaintPropertyNode& node,
      const TransformationMatrix& to_screen,
      const TransformPaintPropertyNode& plane_root,
      const TransformationMatrix& to_plane_root,
      const TransformPaintPropertyNode& root_of_2d_translation,
      double translate_x,
      double translate_y) {
    const auto& cache = GetTransformCache(node);
    EXPECT_EQ(&root_of_2d_translation, cache.root_of_2d_translation());
    EXPECT_EQ(FloatSize(translate_x, translate_y),
              cache.to_2d_translation_root());

    EXPECT_TRUE(HasPlaneRootTransform(node));
    EXPECT_FALSE(HasScreenTransform(node));
    UpdateScreenTransform(node);
    EXPECT_TRUE(HasPlaneRootTransform(node));
    EXPECT_TRUE(HasScreenTransform(node));

    EXPECT_EQ(&plane_root, cache.plane_root());
    EXPECT_EQ(to_plane_root, cache.to_plane_root());
    EXPECT_EQ(to_plane_root.Inverse(), cache.from_plane_root());
    EXPECT_EQ(to_screen, cache.to_screen());
    auto projection_from_screen = to_screen;
    projection_from_screen.FlattenTo2d();
    projection_from_screen = projection_from_screen.Inverse();
    EXPECT_EQ(projection_from_screen, cache.projection_from_screen());
  }
};

TEST_F(GeometryMapperTransformCacheTest, All2dTranslations) {
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  auto t2 = CreateTransform(*t1, TransformationMatrix());
  auto t3 = CreateTransform(*t2, TransformationMatrix().Translate(7, 8));

  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);
  Check2dTranslationToRoot(*t2, 1, 2);
  Check2dTranslationToRoot(*t3, 8, 10);
}

TEST_F(GeometryMapperTransformCacheTest, RootAsPlaneRootWithIntermediateScale) {
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  auto t2 = CreateTransform(*t1, TransformationMatrix().Scale(3));
  auto t3 = CreateTransform(*t2, TransformationMatrix().Translate(7, 8));

  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);
  auto to_plane_root = TransformationMatrix().Translate(1, 2).Scale(3);
  CheckRootAsPlaneRoot(*t2, *t2, to_plane_root, 0, 0);
  to_plane_root = to_plane_root.Translate(7, 8);
  CheckRootAsPlaneRoot(*t3, *t2, to_plane_root, 7, 8);
}

TEST_F(GeometryMapperTransformCacheTest,
       IntermediatePlaneRootSameAs2dTranslationRoot) {
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  auto t2 = CreateTransform(*t1, TransformationMatrix().Rotate3d(0, 45, 0));
  auto t3 = CreateTransform(*t2, TransformationMatrix().Translate(7, 8));

  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);
  auto to_screen = TransformationMatrix().Translate(1, 2).Rotate3d(0, 45, 0);
  CheckPlaneRootSameAs2dTranslationRoot(*t2, to_screen, *t2, 0, 0);
  to_screen = to_screen.Translate(7, 8);
  CheckPlaneRootSameAs2dTranslationRoot(*t3, to_screen, *t2, 7, 8);
}

TEST_F(GeometryMapperTransformCacheTest,
       IntermediatePlaneRootDifferent2dTranslationRoot) {
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  auto t2 = CreateTransform(*t1, TransformationMatrix().Rotate3d(0, 45, 0));
  auto t3 = CreateTransform(*t2, TransformationMatrix().Scale(3));
  auto t4 = CreateTransform(*t3, TransformationMatrix().Translate(7, 8));

  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);

  auto to_screen = TransformationMatrix().Translate(1, 2).Rotate3d(0, 45, 0);
  CheckPlaneRootSameAs2dTranslationRoot(*t2, to_screen, *t2, 0, 0);

  auto to_plane_root = TransformationMatrix().Scale(3);
  to_screen = to_screen.Scale(3);
  CheckPlaneRootDifferent2dTranslationRoot(*t3, to_screen, *t2, to_plane_root,
                                           *t3, 0, 0);

  to_plane_root = to_plane_root.Translate(7, 8);
  to_screen = to_screen.Translate(7, 8);
  CheckPlaneRootDifferent2dTranslationRoot(*t4, to_screen, *t2, to_plane_root,
                                           *t3, 7, 8);
}

TEST_F(GeometryMapperTransformCacheTest, TransformUpdate) {
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  auto t2 = CreateTransform(*t1, TransformationMatrix());
  auto t3 = CreateTransform(*t2, TransformationMatrix().Translate(7, 8));

  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);
  Check2dTranslationToRoot(*t2, 1, 2);
  Check2dTranslationToRoot(*t3, 8, 10);

  // Change t2 to a scale.
  GeometryMapperTransformCache::ClearCache();
  t2->Update(
      *t1, TransformPaintPropertyNode::State{TransformationMatrix().Scale(3)});
  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);
  auto to_plane_root = TransformationMatrix().Translate(1, 2).Scale(3);
  CheckRootAsPlaneRoot(*t2, *t2, to_plane_root, 0, 0);
  to_plane_root = to_plane_root.Translate(7, 8);
  CheckRootAsPlaneRoot(*t3, *t2, to_plane_root, 7, 8);

  // Change t2 to a 3d transform so that it becomes a plane root.
  GeometryMapperTransformCache::ClearCache();
  t2->Update(*t1, TransformPaintPropertyNode::State{
                      TransformationMatrix().Rotate3d(0, 45, 0)});
  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);
  auto to_screen = TransformationMatrix().Translate(1, 2).Rotate3d(0, 45, 0);
  CheckPlaneRootSameAs2dTranslationRoot(*t2, to_screen, *t2, 0, 0);
  to_screen = to_screen.Translate(7, 8);
  CheckPlaneRootSameAs2dTranslationRoot(*t3, to_screen, *t2, 7, 8);

  // Change t2 back to a 2d translation.
  GeometryMapperTransformCache::ClearCache();
  t2->Update(*t1, TransformPaintPropertyNode::State{FloatSize(11, 12)});
  Check2dTranslationToRoot(t0(), 0, 0);
  Check2dTranslationToRoot(*t1, 1, 2);
  Check2dTranslationToRoot(*t2, 12, 14);
  Check2dTranslationToRoot(*t3, 19, 22);
}

}  // namespace blink
