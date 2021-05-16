// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

class CullRectTest : public testing::Test {
 protected:
  const CullRect::ApplyTransformResult kNotExpanded = CullRect::kNotExpanded;
  const CullRect::ApplyTransformResult kExpandedForPartialScrollingContents =
      CullRect::kExpandedForPartialScrollingContents;
  const CullRect::ApplyTransformResult kExpandedForWholeScrollingContents =
      CullRect::kExpandedForWholeScrollingContents;

  // Tests only transforms without clips.
  void ApplyTransforms(CullRect& cull_rect,
                       const TransformPaintPropertyNode& source,
                       const TransformPaintPropertyNode& destination,
                       const absl::optional<CullRect>& old_cull_rect) {
    PropertyTreeState source_state(source, c0(), e0());
    PropertyTreeState destination_state(destination, c0(), e0());
    cull_rect.ApplyPaintProperties(PropertyTreeState::Root(), source_state,
                                   destination_state, old_cull_rect);
  }

  CullRect::ApplyTransformResult ApplyScrollTranslation(
      CullRect& cull_rect,
      const TransformPaintPropertyNode& t) {
    return cull_rect.ApplyScrollTranslation(t, t);
  }

  bool ChangedEnough(const IntRect& old_rect,
                     const IntRect& new_rect,
                     const IntSize* bounds = nullptr) {
    return CullRect(new_rect).ChangedEnough(CullRect(old_rect), bounds);
  }
};

TEST_F(CullRectTest, IntersectsIntRect) {
  CullRect cull_rect(IntRect(0, 0, 50, 50));

  EXPECT_TRUE(cull_rect.Intersects(IntRect(0, 0, 1, 1)));
  EXPECT_FALSE(cull_rect.Intersects(IntRect(51, 51, 1, 1)));
  EXPECT_FALSE(cull_rect.Intersects(IntRect(1, 1, 1, 0)));

  EXPECT_TRUE(CullRect::Infinite().Intersects(IntRect(0, 0, 1, 1)));
  EXPECT_FALSE(CullRect::Infinite().Intersects(IntRect(1, 1, 1, 0)));
  EXPECT_FALSE(CullRect(IntRect()).Intersects(IntRect()));
}

TEST_F(CullRectTest, IntersectsTransformed) {
  CullRect cull_rect(IntRect(0, 0, 50, 50));
  AffineTransform transform;
  transform.Translate(-2, -2);

  EXPECT_TRUE(
      cull_rect.IntersectsTransformed(transform, FloatRect(51, 51, 1, 1)));
  EXPECT_FALSE(cull_rect.Intersects(IntRect(52, 52, 1, 1)));

  EXPECT_TRUE(CullRect::Infinite().IntersectsTransformed(
      transform, FloatRect(51, 51, 1, 1)));
  EXPECT_FALSE(CullRect::Infinite().IntersectsTransformed(
      transform, FloatRect(1, 1, 1, 0)));
}

TEST_F(CullRectTest, Infinite) {
  EXPECT_TRUE(CullRect::Infinite().IsInfinite());
  EXPECT_TRUE(CullRect(LayoutRect::InfiniteIntRect()).IsInfinite());
  EXPECT_FALSE(CullRect(IntRect(0, 0, 100, 100)).IsInfinite());
}

TEST_F(CullRectTest, Move) {
  CullRect cull_rect(IntRect(0, 0, 50, 50));
  cull_rect.Move(IntSize());
  EXPECT_EQ(IntRect(0, 0, 50, 50), cull_rect.Rect());
  cull_rect.Move(IntSize(10, 20));
  EXPECT_EQ(IntRect(10, 20, 50, 50), cull_rect.Rect());
}

TEST_F(CullRectTest, MoveInfinite) {
  CullRect cull_rect = CullRect::Infinite();
  cull_rect.Move(IntSize());
  EXPECT_TRUE(cull_rect.IsInfinite());
  cull_rect.Move(IntSize(10, 20));
  EXPECT_TRUE(cull_rect.IsInfinite());
}

TEST_F(CullRectTest, MoveBy) {
  CullRect cull_rect(IntRect(0, 0, 50, 50));
  cull_rect.MoveBy(IntPoint());
  EXPECT_EQ(IntRect(0, 0, 50, 50), cull_rect.Rect());
  cull_rect.MoveBy(IntPoint(10, 20));
  EXPECT_EQ(IntRect(10, 20, 50, 50), cull_rect.Rect());
}

TEST_F(CullRectTest, MoveByInfinite) {
  CullRect cull_rect = CullRect::Infinite();
  cull_rect.MoveBy(IntPoint());
  EXPECT_TRUE(cull_rect.IsInfinite());
  cull_rect.MoveBy(IntPoint(10, 20));
  EXPECT_TRUE(cull_rect.IsInfinite());
}

TEST_F(CullRectTest, ApplyTransform) {
  CullRect cull_rect(IntRect(1, 1, 50, 50));
  auto transform =
      CreateTransform(t0(), TransformationMatrix().Translate(1, 1));
  cull_rect.ApplyTransform(*transform);

  EXPECT_EQ(IntRect(0, 0, 50, 50), cull_rect.Rect());
}

TEST_F(CullRectTest, ApplyTransformInfinite) {
  CullRect cull_rect = CullRect::Infinite();
  auto transform =
      CreateTransform(t0(), TransformationMatrix().Translate(1, 1));
  cull_rect.ApplyTransform(*transform);
  EXPECT_TRUE(cull_rect.IsInfinite());
}

TEST_F(CullRectTest, ApplyScrollTranslationPartialScrollingContents) {
  ScopedCullRectUpdateForTest cull_rect_update(true);

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(20, 10, 40, 50);
  scroll_state.contents_size = IntSize(8000, 8000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation =
      CreateCompositedScrollTranslation(t0(), -3000, -5000, *scroll);

  CullRect cull_rect(IntRect(0, 0, 50, 100));
  EXPECT_EQ(kExpandedForPartialScrollingContents,
            ApplyScrollTranslation(cull_rect, *scroll_translation));

  // Clipped: (20, 10, 30, 50)
  // Inverse transformed: (3020, 5010, 30, 50)
  // Expanded: (-980, 1010, 8030, 8050)
  // Then clipped by the contents rect.
  EXPECT_EQ(IntRect(0, 1010, 7050, 6990), cull_rect.Rect());

  cull_rect = CullRect::Infinite();
  EXPECT_EQ(kExpandedForPartialScrollingContents,
            ApplyScrollTranslation(cull_rect, *scroll_translation));
  // This result differs from the above result in height (7050 vs 7060)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(IntRect(0, 1010, 7060, 6990), cull_rect.Rect());
}

TEST_F(CullRectTest,
       ApplyNonCompositedScrollTranslationPartialScrollingContents) {
  ScopedCullRectUpdateForTest cull_rect_update(true);

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(20, 10, 40, 50);
  scroll_state.contents_size = IntSize(8000, 8000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation =
      CreateScrollTranslation(t0(), -3000, -5000, *scroll);

  CullRect cull_rect(IntRect(0, 0, 50, 100));
  EXPECT_EQ(kNotExpanded,
            ApplyScrollTranslation(cull_rect, *scroll_translation));

  // Clipped: (20, 10, 30, 50)
  // Inverse transformed: (3020, 5010, 30, 50)
  EXPECT_EQ(IntRect(3020, 5010, 30, 50), cull_rect.Rect());

  cull_rect = CullRect::Infinite();
  EXPECT_EQ(kNotExpanded,
            ApplyScrollTranslation(cull_rect, *scroll_translation));
  // This result differs from the above result in height (40 vs 30)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(IntRect(3020, 5010, 40, 50), cull_rect.Rect());
}

TEST_F(CullRectTest, ApplyScrollTranslationNoIntersectionWithContainerRect) {
  ScopedCullRectUpdateForTest cull_rect_update(true);

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(200, 100, 40, 50);
  scroll_state.contents_size = IntSize(2000, 2000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation =
      CreateCompositedScrollTranslation(t0(), -10, -15, *scroll);

  CullRect cull_rect(IntRect(0, 0, 50, 100));
  EXPECT_EQ(kNotExpanded,
            ApplyScrollTranslation(cull_rect, *scroll_translation));
  EXPECT_TRUE(cull_rect.Rect().IsEmpty());
}

TEST_F(CullRectTest,
       ApplyNonCompositedScrollTranslationNoIntersectionWithContainerRect) {
  ScopedCullRectUpdateForTest cull_rect_update(true);

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(200, 100, 40, 50);
  scroll_state.contents_size = IntSize(2000, 2000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation = CreateScrollTranslation(t0(), -10, -15, *scroll);

  CullRect cull_rect(IntRect(0, 0, 50, 100));
  EXPECT_EQ(kNotExpanded,
            ApplyScrollTranslation(cull_rect, *scroll_translation));
  EXPECT_TRUE(cull_rect.Rect().IsEmpty());
}

TEST_F(CullRectTest, ApplyScrollTranslationWholeScrollingContents) {
  ScopedCullRectUpdateForTest cull_rect_update(true);

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(20, 10, 40, 50);
  scroll_state.contents_size = IntSize(2000, 2000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation =
      CreateCompositedScrollTranslation(t0(), -10, -15, *scroll);

  CullRect cull_rect(IntRect(0, 0, 50, 100));
  EXPECT_EQ(kExpandedForWholeScrollingContents,
            ApplyScrollTranslation(cull_rect, *scroll_translation));

  // Clipped: (20, 10, 30, 50)
  // Inverse transformed: (30, 25, 30, 50)
  // Expanded: (-3970, -3975, 8030, 8050)
  // Then clipped by the contents rect.
  EXPECT_EQ(IntRect(0, 0, 2000, 2000), cull_rect.Rect());

  cull_rect = CullRect::Infinite();
  EXPECT_EQ(kExpandedForWholeScrollingContents,
            ApplyScrollTranslation(cull_rect, *scroll_translation));
  EXPECT_EQ(IntRect(0, 0, 2000, 2000), cull_rect.Rect());
}

TEST_F(CullRectTest,
       ApplyNonCompositedScrollTranslationWholeScrollingContents) {
  ScopedCullRectUpdateForTest cull_rect_update(true);

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(20, 10, 40, 50);
  scroll_state.contents_size = IntSize(2000, 2000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation = CreateScrollTranslation(t0(), -10, -15, *scroll);

  CullRect cull_rect(IntRect(0, 0, 50, 100));
  EXPECT_EQ(kNotExpanded,
            ApplyScrollTranslation(cull_rect, *scroll_translation));

  // Clipped: (20, 10, 30, 50)
  // Inverse transformed: (30, 25, 30, 50)
  EXPECT_EQ(IntRect(30, 25, 30, 50), cull_rect.Rect());

  cull_rect = CullRect::Infinite();
  EXPECT_EQ(kNotExpanded,
            ApplyScrollTranslation(cull_rect, *scroll_translation));
  // This result differs from the above result in height (40 vs 30)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(IntRect(30, 25, 40, 50), cull_rect.Rect());
}

TEST_F(CullRectTest, ChangedEnoughEmpty) {
  ScopedCullRectUpdateForTest cull_rect_update(true);
  EXPECT_FALSE(ChangedEnough(IntRect(), IntRect()));
  EXPECT_FALSE(ChangedEnough(IntRect(1, 1, 0, 0), IntRect(2, 2, 0, 0)));
  EXPECT_TRUE(ChangedEnough(IntRect(), IntRect(0, 0, 1, 1)));
  EXPECT_FALSE(ChangedEnough(IntRect(0, 0, 1, 1), IntRect()));
}

TEST_F(CullRectTest, ChangedNotEnough) {
  ScopedCullRectUpdateForTest cull_rect_update(true);
  IntRect old_rect(100, 100, 100, 100);
  EXPECT_FALSE(ChangedEnough(old_rect, old_rect));
  EXPECT_FALSE(ChangedEnough(old_rect, IntRect(100, 100, 90, 90)));
  EXPECT_FALSE(ChangedEnough(old_rect, IntRect(100, 100, 100, 100)));
  EXPECT_FALSE(ChangedEnough(old_rect, IntRect(1, 1, 200, 200)));
}

TEST_F(CullRectTest, ChangedEnoughOnMovement) {
  ScopedCullRectUpdateForTest cull_rect_update(true);
  IntRect old_rect(100, 100, 100, 100);
  IntRect new_rect(old_rect);
  new_rect.Move(500, 0);
  EXPECT_FALSE(ChangedEnough(old_rect, new_rect));
  new_rect.Move(0, 500);
  EXPECT_FALSE(ChangedEnough(old_rect, new_rect));
  new_rect.Move(50, 0);
  EXPECT_TRUE(ChangedEnough(old_rect, new_rect));
  new_rect.Move(-50, 50);
  EXPECT_TRUE(ChangedEnough(old_rect, new_rect));
}

TEST_F(CullRectTest, ChangedEnoughNewRectTouchingEdge) {
  ScopedCullRectUpdateForTest cull_rect_update(true);
  IntSize bounds(500, 500);
  IntRect old_rect(100, 100, 100, 100);
  // Top edge.
  EXPECT_FALSE(ChangedEnough(old_rect, IntRect(100, 50, 100, 200), &bounds));
  EXPECT_TRUE(ChangedEnough(old_rect, IntRect(100, 0, 100, 200), &bounds));
  // Left edge.
  EXPECT_FALSE(ChangedEnough(old_rect, IntRect(50, 100, 200, 100), &bounds));
  EXPECT_TRUE(ChangedEnough(old_rect, IntRect(0, 100, 200, 100), &bounds));
  // Bottom edge.
  EXPECT_FALSE(ChangedEnough(old_rect, IntRect(100, 100, 100, 350), &bounds));
  EXPECT_TRUE(ChangedEnough(old_rect, IntRect(100, 100, 100, 400), &bounds));
  // Right edge.
  EXPECT_FALSE(ChangedEnough(old_rect, IntRect(100, 100, 350, 100), &bounds));
  EXPECT_TRUE(ChangedEnough(old_rect, IntRect(100, 100, 400, 100), &bounds));
}

TEST_F(CullRectTest, ChangedEnoughOldRectTouchingEdge) {
  ScopedCullRectUpdateForTest cull_rect_update(true);
  IntSize bounds(500, 500);
  IntRect new_rect(100, 100, 300, 300);
  // Top edge.
  EXPECT_FALSE(ChangedEnough(IntRect(100, 0, 100, 100), new_rect, &bounds));
  // Left edge.
  EXPECT_FALSE(ChangedEnough(IntRect(0, 100, 100, 100), new_rect, &bounds));
  // Bottom edge.
  EXPECT_FALSE(ChangedEnough(IntRect(300, 400, 100, 100), new_rect, &bounds));
  // Right edge.
  EXPECT_FALSE(ChangedEnough(IntRect(400, 300, 100, 100), new_rect, &bounds));
}

TEST_F(CullRectTest, ApplyPaintPropertiesSameState) {
  ScopedCullRectUpdateForTest cull_rect_update(true);
  auto transform =
      CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(1, 2, 3, 4));
  PropertyTreeState root = PropertyTreeState::Root();
  PropertyTreeState state(*transform, *clip, e0());

  CullRect cull_rect1(IntRect(1, 1, 50, 50));
  cull_rect1.ApplyPaintProperties(state, state, state, absl::nullopt);
  EXPECT_EQ(IntRect(1, 1, 50, 50), cull_rect1.Rect());
  cull_rect1.ApplyPaintProperties(root, state, state, absl::nullopt);
  EXPECT_EQ(IntRect(1, 1, 50, 50), cull_rect1.Rect());

  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(IntSize(1, 1));
  CullRect cull_rect2(IntRect(1, 1, 50, 50));
  // Should ignore old_cull_rect.
  cull_rect2.ApplyPaintProperties(state, state, state, old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect2);
  cull_rect2.ApplyPaintProperties(root, state, state, old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect2);

  CullRect infinite = CullRect::Infinite();
  infinite.ApplyPaintProperties(state, state, state, absl::nullopt);
  EXPECT_TRUE(infinite.IsInfinite());
  infinite.ApplyPaintProperties(root, state, state, absl::nullopt);
  EXPECT_TRUE(infinite.IsInfinite());
}

TEST_F(CullRectTest, ApplyPaintPropertiesWithoutClipScroll) {
  ScopedCullRectUpdateForTest cull_rect_update(true);
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  auto t2 = CreateTransform(*t1, TransformationMatrix().Translate(10, 20));
  PropertyTreeState root = PropertyTreeState::Root();
  PropertyTreeState state1(*t1, c0(), e0());
  PropertyTreeState state2(*t2, c0(), e0());

  CullRect cull_rect1(IntRect(1, 1, 50, 50));
  cull_rect1.ApplyPaintProperties(root, state1, state2, absl::nullopt);
  EXPECT_EQ(IntRect(-9, -19, 50, 50), cull_rect1.Rect());

  CullRect cull_rect2(IntRect(1, 1, 50, 50));
  cull_rect2.ApplyPaintProperties(root, root, state2, absl::nullopt);
  EXPECT_EQ(IntRect(-10, -21, 50, 50), cull_rect2.Rect());

  CullRect old_cull_rect = cull_rect2;
  old_cull_rect.Move(IntSize(1, 1));
  CullRect cull_rect3(IntRect(1, 1, 50, 50));
  // Should ignore old_cull_rect.
  cull_rect3.ApplyPaintProperties(root, root, state2, old_cull_rect);
  EXPECT_EQ(cull_rect2, cull_rect3);

  CullRect infinite = CullRect::Infinite();
  infinite.ApplyPaintProperties(root, root, state2, absl::nullopt);
  EXPECT_TRUE(infinite.IsInfinite());
}

TEST_F(CullRectTest, ApplyTransformsSingleScrollWholeScrollingContents) {
  ScopedCullRectUpdateForTest cull_rect_update(true);

  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  PropertyTreeState state1(*t1, c0(), e0());
  auto ref_scroll_translation_state = CreateCompositedScrollTranslationState(
      state1, -10, -15, IntRect(20, 10, 40, 50), IntSize(2000, 2000));
  auto scroll_translation_state =
      ref_scroll_translation_state.GetPropertyTreeState().Unalias();

  // Same as ApplyScrollTranslationWholeScrollingContents.
  CullRect cull_rect1(IntRect(0, 0, 50, 100));
  cull_rect1.ApplyPaintProperties(state1, state1, scroll_translation_state,
                                  absl::nullopt);
  EXPECT_EQ(IntRect(0, 0, 2000, 2000), cull_rect1.Rect());

  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(IntSize(1, 1));
  CullRect cull_rect2(IntRect(0, 0, 50, 100));
  // Should ignore old_cull_rect.
  cull_rect2.ApplyPaintProperties(state1, state1, scroll_translation_state,
                                  old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect2);

  CullRect cull_rect3 = CullRect::Infinite();
  cull_rect3.ApplyPaintProperties(state1, state1, scroll_translation_state,
                                  absl::nullopt);
  EXPECT_EQ(IntRect(0, 0, 2000, 2000), cull_rect3.Rect());
}

TEST_F(CullRectTest, ApplyTransformsWithOrigin) {
  ScopedCullRectUpdateForTest cull_rect_update(true);
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  auto t2 = CreateTransform(*t1, TransformationMatrix().Scale(0.5),
                            FloatPoint3D(50, 100, 0));
  PropertyTreeState root = PropertyTreeState::Root();
  PropertyTreeState state1(*t1, c0(), e0());
  PropertyTreeState state2(*t2, c0(), e0());
  CullRect cull_rect1(IntRect(0, 0, 50, 200));
  cull_rect1.ApplyPaintProperties(root, state1, state2, absl::nullopt);
  EXPECT_EQ(IntRect(-50, -100, 100, 400), cull_rect1.Rect());
}

TEST_F(CullRectTest, ApplyTransformsSingleScrollPartialScrollingContents) {
  ScopedCullRectUpdateForTest cull_rect_update(true);

  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  PropertyTreeState state1(*t1, c0(), e0());

  auto ref_scroll_translation_state = CreateCompositedScrollTranslationState(
      state1, -3000, -5000, IntRect(20, 10, 40, 50), IntSize(8000, 8000));
  auto scroll_translation_state =
      ref_scroll_translation_state.GetPropertyTreeState().Unalias();

  // Same as ApplyScrollTranslationPartialScrollingContents.
  CullRect cull_rect1(IntRect(0, 0, 50, 100));
  cull_rect1.ApplyPaintProperties(state1, state1, scroll_translation_state,
                                  absl::nullopt);
  EXPECT_EQ(IntRect(0, 1010, 7050, 6990), cull_rect1.Rect());

  CullRect old_cull_rect(IntRect(0, 1100, 7050, 6900));
  CullRect cull_rect2(IntRect(0, 0, 50, 100));
  // Use old_cull_rect if the new cull rect didn't change enough.
  cull_rect2.ApplyPaintProperties(state1, state1, scroll_translation_state,
                                  old_cull_rect);
  EXPECT_EQ(old_cull_rect, cull_rect2);

  old_cull_rect.Move(IntSize(1000, 1000));
  CullRect cull_rect3(IntRect(0, 0, 50, 100));
  // Use the new cull rect if it changed enough.
  cull_rect3.ApplyPaintProperties(state1, state1, scroll_translation_state,
                                  old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect3);

  CullRect cull_rect4 = CullRect::Infinite();
  cull_rect4.ApplyPaintProperties(state1, state1, scroll_translation_state,
                                  absl::nullopt);
  // This result differs from the first result in height (7050 vs 7060)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(IntRect(0, 1010, 7060, 6990), cull_rect4.Rect());
}

TEST_F(CullRectTest, ApplyTransformsEscapingScroll) {
  ScopedCullRectUpdateForTest cull_rect_update(true);

  PropertyTreeState root = PropertyTreeState::Root();
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(111, 222, 333, 444));
  PropertyTreeState state1(*t1, *c1, e0());

  auto ref_scroll_translation_state = CreateCompositedScrollTranslationState(
      state1, -3000, -5000, IntRect(20, 10, 40, 50), IntSize(8000, 8000));
  auto scroll_translation_state =
      ref_scroll_translation_state.GetPropertyTreeState().Unalias();

  auto t2 = CreateTransform(scroll_translation_state.Transform(),
                            TransformationMatrix().Translate(100, 200));
  PropertyTreeState state2(*t2, scroll_translation_state.Clip(), e0());

  CullRect cull_rect1(IntRect(0, 0, 50, 100));
  // Ignore the current cull rect, and apply paint properties from root to
  // state1 on infinite cull rect instead.
  cull_rect1.ApplyPaintProperties(root, state2, state1, absl::nullopt);
  EXPECT_EQ(IntRect(110, 220, 333, 444), cull_rect1.Rect());

  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(IntSize(1, 1));
  CullRect cull_rect2(IntRect(0, 0, 50, 100));
  // Should ignore old_cull_rect.
  cull_rect2.ApplyPaintProperties(root, state2, state1, old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect2);

  CullRect cull_rect3 = CullRect::Infinite();
  cull_rect3.ApplyPaintProperties(root, state2, state1, absl::nullopt);
  EXPECT_EQ(cull_rect1, cull_rect3);
}

TEST_F(CullRectTest, ApplyTransformsSmallScrollContentsAfterBigScrollContents) {
  ScopedCullRectUpdateForTest cull_rect_update(true);

  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  PropertyTreeState state1(*t1, c0(), e0());

  auto ref_scroll_translation_state1 = CreateCompositedScrollTranslationState(
      state1, -10, -15, IntRect(20, 10, 40, 50), IntSize(8000, 8000));
  auto scroll_translation_state1 =
      ref_scroll_translation_state1.GetPropertyTreeState().Unalias();

  auto t2 = CreateTransform(scroll_translation_state1.Transform(),
                            TransformationMatrix().Translate(2000, 3000));
  PropertyTreeState state2(*t2, scroll_translation_state1.Clip(), e0());

  auto ref_scroll_translation_state2 = CreateCompositedScrollTranslationState(
      state2, -10, -15, IntRect(30, 20, 100, 200), IntSize(200, 400));
  auto scroll_translation_state2 =
      ref_scroll_translation_state2.GetPropertyTreeState().Unalias();

  CullRect cull_rect1(IntRect(0, 0, 50, 100));
  cull_rect1.ApplyPaintProperties(state1, state1, scroll_translation_state2,
                                  absl::nullopt);
  EXPECT_EQ(IntRect(0, 0, 200, 400), cull_rect1.Rect());

  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(IntSize(1, 1));
  CullRect cull_rect2(IntRect(0, 0, 50, 100));
  // Should ignore old_cull_rect.
  cull_rect2.ApplyPaintProperties(state1, state1, scroll_translation_state2,
                                  old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect2);
}

TEST_F(CullRectTest, ApplyTransformsBigScrollContentsAfterSmallScrollContents) {
  ScopedCullRectUpdateForTest cull_rect_update(true);

  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  PropertyTreeState state1(*t1, c0(), e0());

  auto ref_scroll_translation_state1 = CreateCompositedScrollTranslationState(
      state1, -10, -15, IntRect(30, 20, 100, 200), IntSize(200, 400));
  auto scroll_translation_state1 =
      ref_scroll_translation_state1.GetPropertyTreeState().Unalias();

  auto t2 = CreateTransform(scroll_translation_state1.Transform(),
                            TransformationMatrix().Translate(10, 20));
  PropertyTreeState state2(*t2, scroll_translation_state1.Clip(), e0());

  auto ref_scroll_translation_state2 = CreateCompositedScrollTranslationState(
      state2, -3000, -5000, IntRect(20, 10, 50, 100), IntSize(10000, 20000));
  auto scroll_translation_state2 =
      ref_scroll_translation_state2.GetPropertyTreeState().Unalias();

  CullRect cull_rect1(IntRect(0, 0, 100, 200));
  cull_rect1.ApplyPaintProperties(state1, state1, scroll_translation_state2,
                                  absl::nullopt);
  // After the first scroll: (-3960, -3965, 8070, 8180)
  // After t2: (-3980, -3975, 8070, 8180)
  // Clipped by the container rect of the second scroll: (20, 10, 50, 100)
  // After the second scroll offset: (3020, 5010, 50, 100)
  // Expanded: (-980, 1010, 8050, 8100)
  // Then clipped by the contents rect.
  EXPECT_EQ(IntRect(0, 1010, 7070, 8100), cull_rect1.Rect());

  CullRect old_cull_rect(IntRect(0, 1100, 7070, 8100));
  CullRect cull_rect2(IntRect(0, 0, 100, 200));
  // Use old_cull_rect if the new cull rect didn't change enough.
  cull_rect2.ApplyPaintProperties(state1, state1, scroll_translation_state2,
                                  old_cull_rect);
  EXPECT_EQ(old_cull_rect, cull_rect2);

  old_cull_rect.Move(IntSize(1000, 1000));
  CullRect cull_rect3(IntRect(0, 0, 100, 200));
  // Use the new cull rect if it changed enough.
  cull_rect3.ApplyPaintProperties(state1, state1, scroll_translation_state2,
                                  old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect3);
}

TEST_F(CullRectTest, NonCompositedTransformUnderClip) {
  ScopedCullRectUpdateForTest cull_rect_update(true);

  PropertyTreeState root = PropertyTreeState::Root();
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(100, 200, 300, 400));
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(10, 20));
  PropertyTreeState state1(*t1, *c1, e0());

  CullRect cull_rect1(IntRect(0, 0, 300, 500));
  cull_rect1.ApplyPaintProperties(root, root, state1, absl::nullopt);
  // Clip by c1, then transformed by t1.
  EXPECT_EQ(IntRect(90, 180, 200, 300), cull_rect1.Rect());

  CullRect cull_rect2(IntRect(0, 0, 300, 500));
  CullRect old_cull_rect(IntRect(133, 244, 333, 444));
  // Should ignore old_cull_rect.
  cull_rect2.ApplyPaintProperties(root, root, state1, old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect2);

  CullRect cull_rect3 = CullRect::Infinite();
  cull_rect3.ApplyPaintProperties(root, root, state1, absl::nullopt);
  EXPECT_EQ(IntRect(90, 180, 300, 400), cull_rect3.Rect());

  CullRect cull_rect4;
  cull_rect4.ApplyPaintProperties(root, root, state1, absl::nullopt);
  EXPECT_EQ(IntRect(), cull_rect4.Rect());
}

TEST_F(CullRectTest, CompositedTranslationUnderClip) {
  ScopedCullRectUpdateForTest cull_rect_update(true);

  PropertyTreeState root = PropertyTreeState::Root();
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(100, 200, 300, 400));
  auto t1 = CreateTransform(
      t0(), TransformationMatrix().Translate(10, 20).Scale3d(2, 3, 1),
      FloatPoint3D(), CompositingReason::kWillChangeTransform);
  PropertyTreeState state1(*t1, *c1, e0());

  CullRect cull_rect1(IntRect(0, 0, 300, 500));
  cull_rect1.ApplyPaintProperties(root, root, state1, absl::nullopt);
  // The result in NonCompositedTransformUnderClip expanded by 2000 (scaled by
  // maximum of 1/2 and 1/3).
  EXPECT_EQ(IntRect(-1955, -1940, 4100, 4100), cull_rect1.Rect());

  CullRect cull_rect2(IntRect(0, 0, 300, 500));
  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(IntSize(200, 200));
  // Use old_cull_rect if the new cull rect didn't change enough.
  cull_rect2.ApplyPaintProperties(root, root, state1, old_cull_rect);
  EXPECT_EQ(old_cull_rect, cull_rect2);

  CullRect cull_rect3(IntRect(0, 0, 300, 500));
  old_cull_rect.Move(IntSize(1000, 1000));
  // Use the new cull rect if it changed enough.
  cull_rect3.ApplyPaintProperties(root, root, state1, old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect3);

  CullRect cull_rect4 = CullRect::Infinite();
  cull_rect4.ApplyPaintProperties(root, root, state1, absl::nullopt);
  EXPECT_EQ(IntRect(-1955, -1940, 4150, 4134), cull_rect4.Rect());

  CullRect cull_rect5;
  cull_rect4.ApplyPaintProperties(root, root, state1, absl::nullopt);
  EXPECT_EQ(IntRect(), cull_rect5.Rect());
}

TEST_F(CullRectTest, ClipAndCompositedScrollAndClip) {
  ScopedCullRectUpdateForTest cull_rect_update(true);

  auto root = PropertyTreeState::Root();
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0, 10000, 100, 100));
  auto t1 = Create2DTranslation(t0(), 0, 10000);
  auto scroll_clip = CreateClip(*c1, *t1, FloatRoundedRect(0, 0, 120, 120));
  auto scroll_translation = CreateCompositedScrollTranslation(
      *t1, 0, 0, IntRect(0, 0, 120, 120), IntSize(10000, 10000));
  auto c2a = CreateClip(*scroll_clip, *scroll_translation,
                        FloatRoundedRect(0, 300, 100, 100));
  auto c2b = CreateClip(*scroll_clip, *scroll_translation,
                        FloatRoundedRect(0, 8000, 100, 100));
  auto t2 =
      CreateTransform(*scroll_translation, TransformationMatrix(),
                      FloatPoint3D(), CompositingReason::kWillChangeTransform);

  // c2a is out of view, but in the expansion area of the composited scroll.
  CullRect cull_rect = CullRect::Infinite();
  cull_rect.ApplyPaintProperties(
      root, root, PropertyTreeState(*scroll_translation, *c2a, e0()),
      absl::nullopt);
  EXPECT_EQ(IntRect(0, 300, 100, 100), cull_rect.Rect());
  // Composited case. The cull rect should be expanded.
  cull_rect = CullRect::Infinite();
  cull_rect.ApplyPaintProperties(root, root, PropertyTreeState(*t2, *c2a, e0()),
                                 absl::nullopt);
  EXPECT_EQ(IntRect(-4000, -3700, 8100, 8100), cull_rect.Rect());

  // c2b is out of the expansion are of the composited scroll.
  cull_rect = CullRect::Infinite();
  cull_rect.ApplyPaintProperties(
      root, root, PropertyTreeState(*scroll_translation, *c2b, e0()),
      absl::nullopt);
  EXPECT_EQ(IntRect(), cull_rect.Rect());
  // Composited case. The cull rect should be still empty.
  cull_rect = CullRect::Infinite();
  cull_rect.ApplyPaintProperties(root, root, PropertyTreeState(*t2, *c2b, e0()),
                                 absl::nullopt);
  EXPECT_EQ(IntRect(), cull_rect.Rect());
}

TEST_F(CullRectTest, IntersectsVerticalRange) {
  CullRect cull_rect(IntRect(0, 0, 50, 100));

  EXPECT_TRUE(cull_rect.IntersectsVerticalRange(LayoutUnit(), LayoutUnit(1)));
  EXPECT_FALSE(
      cull_rect.IntersectsVerticalRange(LayoutUnit(100), LayoutUnit(101)));
}

TEST_F(CullRectTest, IntersectsHorizontalRange) {
  CullRect cull_rect(IntRect(0, 0, 50, 100));

  EXPECT_TRUE(cull_rect.IntersectsHorizontalRange(LayoutUnit(), LayoutUnit(1)));
  EXPECT_FALSE(
      cull_rect.IntersectsHorizontalRange(LayoutUnit(50), LayoutUnit(51)));
}

}  // namespace blink
