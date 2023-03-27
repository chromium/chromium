// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class CullRectTest : public testing::Test,
                     public testing::WithParamInterface<bool>,
                     private ScopedCompositeScrollAfterPaintForTest {
 protected:
  CullRectTest() : ScopedCompositeScrollAfterPaintForTest(GetParam()) {}

  bool ApplyPaintProperties(
      CullRect& cull_rect,
      const PropertyTreeState& root,
      const PropertyTreeState& source,
      const PropertyTreeState& destination,
      const absl::optional<CullRect>& old_cull_rect = absl::nullopt) {
    return cull_rect.ApplyPaintProperties(root, source, destination,
                                          old_cull_rect, disable_expansion_);
  }

  bool ApplyScrollTranslation(CullRect& cull_rect,
                              const TransformPaintPropertyNode& t) {
    return cull_rect.ApplyScrollTranslation(t, t, disable_expansion_);
  }

  bool ChangedEnough(const gfx::Rect& old_rect,
                     const gfx::Rect& new_rect,
                     const absl::optional<gfx::Rect>& bounds = absl::nullopt) {
    return CullRect(new_rect).ChangedEnough(CullRect(old_rect), bounds);
  }

  bool disable_expansion_ = false;
};

INSTANTIATE_TEST_SUITE_P(All, CullRectTest, ::testing::Bool());

TEST_P(CullRectTest, IntersectsRect) {
  CullRect cull_rect(gfx::Rect(0, 0, 50, 50));

  EXPECT_TRUE(cull_rect.Intersects(gfx::Rect(0, 0, 1, 1)));
  EXPECT_FALSE(cull_rect.Intersects(gfx::Rect(51, 51, 1, 1)));
  EXPECT_FALSE(cull_rect.Intersects(gfx::Rect(1, 1, 1, 0)));

  EXPECT_TRUE(CullRect::Infinite().Intersects(gfx::Rect(0, 0, 1, 1)));
  EXPECT_FALSE(CullRect::Infinite().Intersects(gfx::Rect(1, 1, 1, 0)));
  EXPECT_FALSE(CullRect(gfx::Rect()).Intersects(gfx::Rect()));
}

TEST_P(CullRectTest, IntersectsTransformed) {
  CullRect cull_rect(gfx::Rect(0, 0, 50, 50));
  AffineTransform transform;
  transform.Translate(-2, -2);

  EXPECT_TRUE(
      cull_rect.IntersectsTransformed(transform, gfx::RectF(51, 51, 1, 1)));
  EXPECT_FALSE(cull_rect.Intersects(gfx::Rect(52, 52, 1, 1)));

  EXPECT_TRUE(CullRect::Infinite().IntersectsTransformed(
      transform, gfx::RectF(51, 51, 1, 1)));
  EXPECT_FALSE(CullRect::Infinite().IntersectsTransformed(
      transform, gfx::RectF(1, 1, 1, 0)));
}

TEST_P(CullRectTest, Infinite) {
  EXPECT_TRUE(CullRect::Infinite().IsInfinite());
  EXPECT_TRUE(CullRect(LayoutRect::InfiniteIntRect()).IsInfinite());
  EXPECT_FALSE(CullRect(gfx::Rect(0, 0, 100, 100)).IsInfinite());
}

TEST_P(CullRectTest, Move) {
  CullRect cull_rect(gfx::Rect(0, 0, 50, 50));
  cull_rect.Move(gfx::Vector2d());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), cull_rect.Rect());
  cull_rect.Move(gfx::Vector2d(10, 20));
  EXPECT_EQ(gfx::Rect(10, 20, 50, 50), cull_rect.Rect());
}

TEST_P(CullRectTest, MoveInfinite) {
  CullRect cull_rect = CullRect::Infinite();
  cull_rect.Move(gfx::Vector2d());
  EXPECT_TRUE(cull_rect.IsInfinite());
  cull_rect.Move(gfx::Vector2d(10, 20));
  EXPECT_TRUE(cull_rect.IsInfinite());
}

TEST_P(CullRectTest, ApplyTransform) {
  CullRect cull_rect(gfx::Rect(1, 1, 50, 50));
  auto transform = CreateTransform(t0(), MakeTranslationMatrix(1, 1));
  cull_rect.ApplyTransform(*transform);

  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), cull_rect.Rect());
}

TEST_P(CullRectTest, ApplyTransformInfinite) {
  CullRect cull_rect = CullRect::Infinite();
  auto transform = CreateTransform(t0(), MakeTranslationMatrix(1, 1));
  cull_rect.ApplyTransform(*transform);
  EXPECT_TRUE(cull_rect.IsInfinite());
}

TEST_P(CullRectTest, ApplyScrollTranslationPartialScrollingContents) {
  auto state = CreateCompositedScrollTranslationState(
      PropertyTreeState::Root(), -3000, -5000, gfx::Rect(20, 10, 40, 50),
      gfx::Size(8000, 8000));
  auto& scroll_translation = state.Transform();

  CullRect cull_rect(gfx::Rect(0, 0, 50, 100));
  EXPECT_TRUE(ApplyScrollTranslation(cull_rect, scroll_translation));

  // Clipped: (20, 10, 30, 50)
  // Inverse transformed: (3020, 5010, 30, 50)
  // Expanded: (-980, 1010, 8030, 8050)
  // Then clipped by the contents rect.
  EXPECT_EQ(gfx::Rect(20, 1010, 7030, 7000), cull_rect.Rect());

  cull_rect = CullRect::Infinite();
  EXPECT_TRUE(ApplyScrollTranslation(cull_rect, scroll_translation));
  // This result differs from the above result in width (7030 vs 7040)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(gfx::Rect(20, 1010, 7040, 7000), cull_rect.Rect());
}

TEST_P(CullRectTest,
       ApplyNonCompositedScrollTranslationPartialScrollingContents) {
  auto state = CreateScrollTranslationState(PropertyTreeState::Root(), -3000,
                                            -5000, gfx::Rect(20, 10, 40, 50),
                                            gfx::Size(8000, 8000));
  auto& scroll_translation = state.Transform();

  CullRect cull_rect(gfx::Rect(0, 0, 50, 100));
  if (RuntimeEnabledFeatures::CompositeScrollAfterPaintEnabled()) {
    // Same as ApplyScrollTranslationPartialScrollingContents.
    EXPECT_TRUE(ApplyScrollTranslation(cull_rect, scroll_translation));
    EXPECT_EQ(gfx::Rect(20, 1010, 7030, 7000), cull_rect.Rect());
    cull_rect = CullRect::Infinite();
    EXPECT_TRUE(ApplyScrollTranslation(cull_rect, scroll_translation));
    EXPECT_EQ(gfx::Rect(20, 1010, 7040, 7000), cull_rect.Rect());
  } else {
    EXPECT_FALSE(ApplyScrollTranslation(cull_rect, scroll_translation));

    // Clipped: (20, 10, 30, 50)
    // Inverse transformed: (3020, 5010, 30, 50)
    EXPECT_EQ(gfx::Rect(3020, 5010, 30, 50), cull_rect.Rect());

    cull_rect = CullRect::Infinite();
    EXPECT_FALSE(ApplyScrollTranslation(cull_rect, scroll_translation));
    // This result differs from the above result in width (40 vs 30)
    // because it's not clipped by the infinite input cull rect.
    EXPECT_EQ(gfx::Rect(3020, 5010, 40, 50), cull_rect.Rect());
  }
}

TEST_P(CullRectTest,
       ApplyScrollTranslationPartialScrollingContentsWithoutExpansion) {
  disable_expansion_ = true;
  auto state = CreateCompositedScrollTranslationState(
      PropertyTreeState::Root(), -3000, -5000, gfx::Rect(20, 10, 40, 50),
      gfx::Size(8000, 8000));
  auto& scroll_translation = state.Transform();

  CullRect cull_rect(gfx::Rect(0, 0, 50, 100));
  EXPECT_FALSE(ApplyScrollTranslation(cull_rect, scroll_translation));

  // Clipped: (20, 10, 30, 50)
  // Inverse transformed: (3020, 5010, 30, 50)
  EXPECT_EQ(gfx::Rect(3020, 5010, 30, 50), cull_rect.Rect());

  cull_rect = CullRect::Infinite();
  EXPECT_FALSE(ApplyScrollTranslation(cull_rect, scroll_translation));
  // This result differs from the above result in width (40 vs 30)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(gfx::Rect(3020, 5010, 40, 50), cull_rect.Rect());
}

TEST_P(CullRectTest, ApplyScrollTranslationNoIntersectionWithContainerRect) {
  auto state = CreateCompositedScrollTranslationState(
      PropertyTreeState::Root(), -10, -15, gfx::Rect(200, 100, 40, 50),
      gfx::Size(2000, 2000));
  auto& scroll_translation = state.Transform();

  CullRect cull_rect(gfx::Rect(0, 0, 50, 100));
  EXPECT_FALSE(ApplyScrollTranslation(cull_rect, scroll_translation));
  EXPECT_TRUE(cull_rect.Rect().IsEmpty());
}

TEST_P(CullRectTest,
       ApplyNonCompositedScrollTranslationNoIntersectionWithContainerRect) {
  auto state = CreateCompositedScrollTranslationState(
      PropertyTreeState::Root(), -10, -15, gfx::Rect(200, 100, 40, 50),
      gfx::Size(2000, 2000));
  auto& scroll_translation = state.Transform();

  CullRect cull_rect(gfx::Rect(0, 0, 50, 100));
  EXPECT_FALSE(ApplyScrollTranslation(cull_rect, scroll_translation));
  EXPECT_TRUE(cull_rect.Rect().IsEmpty());
}

TEST_P(CullRectTest, ApplyScrollTranslationWholeScrollingContents) {
  auto state = CreateCompositedScrollTranslationState(
      PropertyTreeState::Root(), -10, -15, gfx::Rect(20, 10, 40, 50),
      gfx::Size(2000, 2000));
  auto& scroll_translation = state.Transform();

  CullRect cull_rect(gfx::Rect(0, 0, 50, 100));
  EXPECT_TRUE(ApplyScrollTranslation(cull_rect, scroll_translation));

  // Clipped: (20, 10, 30, 50)
  // Inverse transformed: (30, 25, 30, 50)
  // Expanded: (-3970, -3975, 8030, 8050)
  // Then clipped by the contents rect.
  EXPECT_EQ(gfx::Rect(20, 10, 2000, 2000), cull_rect.Rect());

  cull_rect = CullRect::Infinite();
  EXPECT_TRUE(ApplyScrollTranslation(cull_rect, scroll_translation));
  EXPECT_EQ(gfx::Rect(20, 10, 2000, 2000), cull_rect.Rect());
}

TEST_P(CullRectTest,
       ApplyNonCompositedScrollTranslationWholeScrollingContents) {
  auto state = CreateScrollTranslationState(PropertyTreeState::Root(), -10, -15,
                                            gfx::Rect(20, 10, 40, 50),
                                            gfx::Size(2000, 2000));
  auto& scroll_translation = state.Transform();

  CullRect cull_rect(gfx::Rect(0, 0, 50, 100));
  if (RuntimeEnabledFeatures::CompositeScrollAfterPaintEnabled()) {
    // Same as ApplyScrollTranslationWholeScrollingContents.
    EXPECT_TRUE(ApplyScrollTranslation(cull_rect, scroll_translation));
    EXPECT_EQ(gfx::Rect(20, 10, 2000, 2000), cull_rect.Rect());
    cull_rect = CullRect::Infinite();
    EXPECT_TRUE(ApplyScrollTranslation(cull_rect, scroll_translation));
    EXPECT_EQ(gfx::Rect(20, 10, 2000, 2000), cull_rect.Rect());
  } else {
    EXPECT_FALSE(ApplyScrollTranslation(cull_rect, scroll_translation));

    // Clipped: (20, 10, 30, 50)
    // Inverse transformed: (30, 25, 30, 50)
    EXPECT_EQ(gfx::Rect(30, 25, 30, 50), cull_rect.Rect());

    cull_rect = CullRect::Infinite();
    EXPECT_FALSE(ApplyScrollTranslation(cull_rect, scroll_translation));
    // This result differs from the above result in height (40 vs 30)
    // because it's not clipped by the infinite input cull rect.
    EXPECT_EQ(gfx::Rect(30, 25, 40, 50), cull_rect.Rect());
  }
}

TEST_P(CullRectTest,
       ApplyScrollTranslationWholeScrollingContentsWithoutExpansion) {
  disable_expansion_ = true;
  auto state = CreateCompositedScrollTranslationState(
      PropertyTreeState::Root(), -10, -15, gfx::Rect(20, 10, 40, 50),
      gfx::Size(2000, 2000));
  auto& scroll_translation = state.Transform();

  CullRect cull_rect(gfx::Rect(0, 0, 50, 100));
  EXPECT_FALSE(ApplyScrollTranslation(cull_rect, scroll_translation));

  // Clipped: (20, 10, 30, 50)
  // Inverse transformed: (30, 25, 30, 50)
  EXPECT_EQ(gfx::Rect(30, 25, 30, 50), cull_rect.Rect());

  cull_rect = CullRect::Infinite();
  EXPECT_FALSE(ApplyScrollTranslation(cull_rect, scroll_translation));
  // This result differs from the above result in height (40 vs 30)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(gfx::Rect(30, 25, 40, 50), cull_rect.Rect());
}

TEST_P(CullRectTest, ChangedEnoughEmpty) {
  EXPECT_FALSE(ChangedEnough(gfx::Rect(), gfx::Rect()));
  EXPECT_FALSE(ChangedEnough(gfx::Rect(1, 1, 0, 0), gfx::Rect(2, 2, 0, 0)));
  EXPECT_TRUE(ChangedEnough(gfx::Rect(), gfx::Rect(0, 0, 1, 1)));
  EXPECT_FALSE(ChangedEnough(gfx::Rect(0, 0, 1, 1), gfx::Rect()));
}

TEST_P(CullRectTest, ChangedNotEnough) {
  gfx::Rect old_rect(100, 100, 100, 100);
  EXPECT_FALSE(ChangedEnough(old_rect, old_rect));
  EXPECT_FALSE(ChangedEnough(old_rect, gfx::Rect(100, 100, 90, 90)));
  EXPECT_FALSE(ChangedEnough(old_rect, gfx::Rect(100, 100, 100, 100)));
  EXPECT_FALSE(ChangedEnough(old_rect, gfx::Rect(1, 1, 200, 200)));
}

TEST_P(CullRectTest, ChangedEnoughOnMovement) {
  gfx::Rect old_rect(100, 100, 100, 100);
  gfx::Rect new_rect(old_rect);
  new_rect.Offset(500, 0);
  EXPECT_FALSE(ChangedEnough(old_rect, new_rect));
  new_rect.Offset(0, 500);
  EXPECT_FALSE(ChangedEnough(old_rect, new_rect));
  new_rect.Offset(50, 0);
  EXPECT_TRUE(ChangedEnough(old_rect, new_rect));
  new_rect.Offset(-50, 50);
  EXPECT_TRUE(ChangedEnough(old_rect, new_rect));
}

TEST_P(CullRectTest, ChangedEnoughNewRectTouchingEdge) {
  gfx::Rect bounds(0, 0, 500, 500);
  gfx::Rect old_rect(100, 100, 100, 100);
  // Top edge.
  EXPECT_FALSE(ChangedEnough(old_rect, gfx::Rect(100, 50, 100, 200), bounds));
  EXPECT_TRUE(ChangedEnough(old_rect, gfx::Rect(100, 0, 100, 200), bounds));
  // Left edge.
  EXPECT_FALSE(ChangedEnough(old_rect, gfx::Rect(50, 100, 200, 100), bounds));
  EXPECT_TRUE(ChangedEnough(old_rect, gfx::Rect(0, 100, 200, 100), bounds));
  // Bottom edge.
  EXPECT_FALSE(ChangedEnough(old_rect, gfx::Rect(100, 100, 100, 350), bounds));
  EXPECT_TRUE(ChangedEnough(old_rect, gfx::Rect(100, 100, 100, 400), bounds));
  // Right edge.
  EXPECT_FALSE(ChangedEnough(old_rect, gfx::Rect(100, 100, 350, 100), bounds));
  EXPECT_TRUE(ChangedEnough(old_rect, gfx::Rect(100, 100, 400, 100), bounds));

  // With offset.
  bounds.Offset(-100, 100);
  old_rect.Offset(-100, 100);
  // Top edge.
  EXPECT_FALSE(ChangedEnough(old_rect, gfx::Rect(0, 150, 100, 200), bounds));
  EXPECT_TRUE(ChangedEnough(old_rect, gfx::Rect(0, 100, 100, 200), bounds));
  // Left edge.
  EXPECT_FALSE(ChangedEnough(old_rect, gfx::Rect(-50, 200, 200, 100), bounds));
  EXPECT_TRUE(ChangedEnough(old_rect, gfx::Rect(-100, 200, 200, 100), bounds));
  // Bottom edge.
  EXPECT_FALSE(ChangedEnough(old_rect, gfx::Rect(0, 200, 100, 350), bounds));
  EXPECT_TRUE(ChangedEnough(old_rect, gfx::Rect(0, 200, 100, 400), bounds));
  // Right edge.
  EXPECT_FALSE(ChangedEnough(old_rect, gfx::Rect(0, 200, 350, 100), bounds));
  EXPECT_TRUE(ChangedEnough(old_rect, gfx::Rect(0, 200, 400, 100), bounds));
}

TEST_P(CullRectTest, ChangedEnoughOldRectTouchingEdge) {
  gfx::Rect bounds(0, 0, 500, 500);
  gfx::Rect new_rect(100, 100, 300, 300);
  // Top edge.
  EXPECT_FALSE(ChangedEnough(gfx::Rect(100, 0, 100, 100), new_rect, bounds));
  // Left edge.
  EXPECT_FALSE(ChangedEnough(gfx::Rect(0, 100, 100, 100), new_rect, bounds));
  // Bottom edge.
  EXPECT_FALSE(ChangedEnough(gfx::Rect(300, 400, 100, 100), new_rect, bounds));
  // Right edge.
  EXPECT_FALSE(ChangedEnough(gfx::Rect(400, 300, 100, 100), new_rect, bounds));

  // With offset.
  bounds.Offset(-100, 100);
  new_rect.Offset(-100, 100);
  // Top edge.
  EXPECT_FALSE(ChangedEnough(gfx::Rect(0, 100, 100, 100), new_rect, bounds));
  // Left edge.
  EXPECT_FALSE(ChangedEnough(gfx::Rect(-100, 0, 100, 100), new_rect, bounds));
  // Bottom edge.
  EXPECT_FALSE(ChangedEnough(gfx::Rect(200, 500, 100, 100), new_rect, bounds));
  // Right edge.
  EXPECT_FALSE(ChangedEnough(gfx::Rect(300, 400, 100, 100), new_rect, bounds));
}

TEST_P(CullRectTest, ApplyPaintPropertiesWithoutClipScroll) {
  auto t1 = CreateTransform(t0(), MakeTranslationMatrix(1, 2));
  auto t2 = CreateTransform(*t1, MakeTranslationMatrix(10, 20));
  PropertyTreeState root = PropertyTreeState::Root();
  PropertyTreeState state1(*t1, c0(), e0());
  PropertyTreeState state2(*t2, c0(), e0());

  CullRect cull_rect1(gfx::Rect(1, 1, 50, 50));
  EXPECT_FALSE(ApplyPaintProperties(cull_rect1, root, state1, state2));
  EXPECT_EQ(gfx::Rect(-9, -19, 50, 50), cull_rect1.Rect());

  CullRect cull_rect2(gfx::Rect(1, 1, 50, 50));
  EXPECT_FALSE(ApplyPaintProperties(cull_rect2, root, root, state2));
  EXPECT_EQ(gfx::Rect(-10, -21, 50, 50), cull_rect2.Rect());

  CullRect old_cull_rect = cull_rect2;
  old_cull_rect.Move(gfx::Vector2d(1, 1));
  CullRect cull_rect3(gfx::Rect(1, 1, 50, 50));
  // Should ignore old_cull_rect.
  EXPECT_FALSE(ApplyPaintProperties(cull_rect3, root, root, state2));
  EXPECT_EQ(cull_rect2, cull_rect3);

  CullRect infinite = CullRect::Infinite();
  EXPECT_FALSE(ApplyPaintProperties(infinite, root, root, state2));
  EXPECT_TRUE(infinite.IsInfinite());
}

TEST_P(CullRectTest, SingleScrollWholeCompsitedScrollingContents) {
  auto t1 = CreateTransform(t0(), MakeTranslationMatrix(1, 2));
  PropertyTreeState state1(*t1, c0(), e0());
  auto ref_scroll_translation_state = CreateCompositedScrollTranslationState(
      state1, -10, -15, gfx::Rect(20, 10, 40, 50), gfx::Size(2000, 2000));
  auto scroll_translation_state =
      ref_scroll_translation_state.GetPropertyTreeState();

  // Same as ApplyScrollTranslationWholeScrollingContents.
  CullRect cull_rect1(gfx::Rect(0, 0, 50, 100));
  EXPECT_TRUE(ApplyPaintProperties(cull_rect1, state1, state1,
                                   scroll_translation_state));
  EXPECT_EQ(gfx::Rect(20, 10, 2000, 2000), cull_rect1.Rect());

  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(gfx::Vector2d(1, 1));
  CullRect cull_rect2(gfx::Rect(0, 0, 50, 100));
  // Should ignore old_cull_rect.
  EXPECT_TRUE(ApplyPaintProperties(cull_rect2, state1, state1,
                                   scroll_translation_state, old_cull_rect));
  EXPECT_EQ(cull_rect1, cull_rect2);

  CullRect cull_rect3 = CullRect::Infinite();
  EXPECT_TRUE(ApplyPaintProperties(cull_rect3, state1, state1,
                                   scroll_translation_state));
  EXPECT_EQ(gfx::Rect(20, 10, 2000, 2000), cull_rect3.Rect());
}

TEST_P(CullRectTest, ApplyTransformsWithOrigin) {
  auto t1 = CreateTransform(t0(), MakeTranslationMatrix(1, 2));
  auto t2 =
      CreateTransform(*t1, MakeScaleMatrix(0.5), gfx::Point3F(50, 100, 0));
  PropertyTreeState root = PropertyTreeState::Root();
  PropertyTreeState state1(*t1, c0(), e0());
  PropertyTreeState state2(*t2, c0(), e0());
  CullRect cull_rect1(gfx::Rect(0, 0, 50, 200));
  EXPECT_FALSE(ApplyPaintProperties(cull_rect1, root, state1, state2));
  EXPECT_EQ(gfx::Rect(-50, -100, 100, 400), cull_rect1.Rect());
}

TEST_P(CullRectTest, SingleScrollPartialScrollingContents) {
  auto t1 = Create2DTranslation(t0(), 1, 2);
  PropertyTreeState state1(*t1, c0(), e0());

  auto ref_scroll_translation_state = CreateCompositedScrollTranslationState(
      state1, -3000, -5000, gfx::Rect(20, 10, 40, 50), gfx::Size(8000, 8000));
  auto scroll_translation_state =
      ref_scroll_translation_state.GetPropertyTreeState();

  // Same as ApplyScrollTranslationPartialScrollingContents.
  CullRect cull_rect1(gfx::Rect(0, 0, 50, 100));
  EXPECT_TRUE(ApplyPaintProperties(cull_rect1, state1, state1,
                                   scroll_translation_state));
  EXPECT_EQ(gfx::Rect(20, 1010, 7030, 7000), cull_rect1.Rect());

  CullRect old_cull_rect(gfx::Rect(20, 1100, 7050, 6910));
  CullRect cull_rect2(gfx::Rect(0, 0, 50, 100));
  // Use old_cull_rect if the new cull rect didn't change enough.
  EXPECT_TRUE(ApplyPaintProperties(cull_rect2, state1, state1,
                                   scroll_translation_state, old_cull_rect));
  EXPECT_EQ(old_cull_rect, cull_rect2);

  old_cull_rect.Move(gfx::Vector2d(1000, 1000));
  CullRect cull_rect3(gfx::Rect(0, 0, 50, 100));
  // Use the new cull rect if it changed enough.
  EXPECT_TRUE(ApplyPaintProperties(cull_rect3, state1, state1,
                                   scroll_translation_state, old_cull_rect));
  EXPECT_EQ(cull_rect1, cull_rect3);

  CullRect cull_rect4 = CullRect::Infinite();
  EXPECT_TRUE(ApplyPaintProperties(cull_rect4, state1, state1,
                                   scroll_translation_state));
  // This result differs from the first result in width (7030 vs 7040)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(gfx::Rect(20, 1010, 7040, 7000), cull_rect4.Rect());
}

TEST_P(CullRectTest, TransformUnderScrollTranslation) {
  auto t1 = Create2DTranslation(t0(), 1, 2);
  PropertyTreeState state1(*t1, c0(), e0());
  auto scroll_translation_state = CreateCompositedScrollTranslationState(
      state1, -3000, -5000, gfx::Rect(20, 10, 40, 50), gfx::Size(8000, 8000));
  auto t2 =
      Create2DTranslation(scroll_translation_state.Transform(), 2000, 3000);
  PropertyTreeState state2 = scroll_translation_state.GetPropertyTreeState();
  state2.SetTransform(*t2);

  // Cases below are the same as those in SingleScrollPartialScrollingContents,
  // except that the offset is adjusted with |t2|.
  CullRect cull_rect1(gfx::Rect(0, 0, 50, 100));
  EXPECT_TRUE(ApplyPaintProperties(cull_rect1, state1, state1, state2));
  EXPECT_EQ(gfx::Rect(-1980, -1990, 7030, 7000), cull_rect1.Rect());

  CullRect old_cull_rect(gfx::Rect(-1980, -1990, 7030, 7000));
  CullRect cull_rect2(gfx::Rect(0, 0, 50, 100));
  // Use old_cull_rect if the new cull rect didn't change enough.
  EXPECT_TRUE(
      ApplyPaintProperties(cull_rect2, state1, state1, state2, old_cull_rect));
  EXPECT_EQ(old_cull_rect, cull_rect2);

  old_cull_rect.Move(gfx::Vector2d(1000, 1000));
  CullRect cull_rect3(gfx::Rect(0, 0, 50, 100));
  // Use the new cull rect if it changed enough.
  EXPECT_TRUE(
      ApplyPaintProperties(cull_rect3, state1, state1, state2, old_cull_rect));
  EXPECT_EQ(cull_rect1, cull_rect3);

  CullRect cull_rect4 = CullRect::Infinite();
  EXPECT_TRUE(ApplyPaintProperties(cull_rect4, state1, state1, state2));
  // This result differs from the first result in height (7050 vs 7060)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(gfx::Rect(-1980, -1990, 7040, 7000), cull_rect4.Rect());
}

TEST_P(CullRectTest, TransformEscapingScroll) {
  PropertyTreeState root = PropertyTreeState::Root();
  auto t1 = CreateTransform(t0(), MakeTranslationMatrix(1, 2));
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(111, 222, 333, 444));
  PropertyTreeState state1(*t1, *c1, e0());

  auto ref_scroll_translation_state = CreateCompositedScrollTranslationState(
      state1, -3000, -5000, gfx::Rect(20, 10, 40, 50), gfx::Size(8000, 8000));
  auto scroll_translation_state =
      ref_scroll_translation_state.GetPropertyTreeState();

  auto t2 = CreateTransform(scroll_translation_state.Transform(),
                            MakeTranslationMatrix(100, 200));
  PropertyTreeState state2(*t2, scroll_translation_state.Clip(), e0());

  CullRect cull_rect1(gfx::Rect(0, 0, 50, 100));
  // Ignore the current cull rect, and apply paint properties from root to
  // state1 on infinite cull rect instead.
  EXPECT_FALSE(ApplyPaintProperties(cull_rect1, root, state2, state1));
  EXPECT_EQ(gfx::Rect(110, 220, 333, 444), cull_rect1.Rect());

  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(gfx::Vector2d(1, 1));
  CullRect cull_rect2(gfx::Rect(0, 0, 50, 100));
  // Should ignore old_cull_rect.
  EXPECT_FALSE(
      ApplyPaintProperties(cull_rect2, root, state2, state1, old_cull_rect));
  EXPECT_EQ(cull_rect1, cull_rect2);

  CullRect cull_rect3 = CullRect::Infinite();
  EXPECT_FALSE(ApplyPaintProperties(cull_rect3, root, state2, state1));
  EXPECT_EQ(cull_rect1, cull_rect3);
}

TEST_P(CullRectTest, SmallScrollContentsAfterBigScrollContents) {
  auto t1 = CreateTransform(t0(), MakeTranslationMatrix(1, 2));
  PropertyTreeState state1(*t1, c0(), e0());

  auto ref_scroll_translation_state1 = CreateCompositedScrollTranslationState(
      state1, -10, -15, gfx::Rect(20, 10, 40, 50), gfx::Size(8000, 8000));
  auto scroll_translation_state1 =
      ref_scroll_translation_state1.GetPropertyTreeState();

  auto t2 = CreateTransform(scroll_translation_state1.Transform(),
                            MakeTranslationMatrix(2000, 3000));
  PropertyTreeState state2(*t2, scroll_translation_state1.Clip(), e0());

  auto ref_scroll_translation_state2 = CreateCompositedScrollTranslationState(
      state2, -10, -15, gfx::Rect(30, 20, 100, 200), gfx::Size(200, 400));
  auto scroll_translation_state2 =
      ref_scroll_translation_state2.GetPropertyTreeState();

  CullRect cull_rect1(gfx::Rect(0, 0, 50, 100));
  EXPECT_TRUE(ApplyPaintProperties(cull_rect1, state1, state1,
                                   scroll_translation_state2));
  EXPECT_EQ(gfx::Rect(30, 20, 200, 400), cull_rect1.Rect());

  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(gfx::Vector2d(1, 1));
  CullRect cull_rect2(gfx::Rect(0, 0, 50, 100));
  // Should ignore old_cull_rect.
  EXPECT_TRUE(ApplyPaintProperties(cull_rect2, state1, state1,
                                   scroll_translation_state2, old_cull_rect));
  EXPECT_EQ(cull_rect1, cull_rect2);
}

TEST_P(CullRectTest, BigScrollContentsAfterSmallScrollContents) {
  auto t1 = CreateTransform(t0(), MakeTranslationMatrix(1, 2));
  PropertyTreeState state1(*t1, c0(), e0());

  auto ref_scroll_translation_state1 = CreateCompositedScrollTranslationState(
      state1, -10, -15, gfx::Rect(30, 20, 100, 200), gfx::Size(200, 400));
  auto scroll_translation_state1 =
      ref_scroll_translation_state1.GetPropertyTreeState();

  auto t2 = CreateTransform(scroll_translation_state1.Transform(),
                            MakeTranslationMatrix(10, 20));
  PropertyTreeState state2(*t2, scroll_translation_state1.Clip(), e0());

  auto ref_scroll_translation_state2 = CreateCompositedScrollTranslationState(
      state2, -3000, -5000, gfx::Rect(20, 10, 50, 100),
      gfx::Size(10000, 20000));
  auto scroll_translation_state2 =
      ref_scroll_translation_state2.GetPropertyTreeState();

  CullRect cull_rect1(gfx::Rect(0, 0, 100, 200));
  EXPECT_TRUE(ApplyPaintProperties(cull_rect1, state1, state1,
                                   scroll_translation_state2));
  // After the first scroll: (-3960, -3965, 8070, 8180)
  // After t2: (-3980, -3975, 8070, 8180)
  // Clipped by the container rect of the second scroll: (20, 10, 50, 100)
  // After the second scroll offset: (3020, 5010, 50, 100)
  // Expanded: (-980, 1010, 8050, 8100)
  // Then clipped by the contents rect.
  EXPECT_EQ(gfx::Rect(20, 1010, 7050, 8100), cull_rect1.Rect());

  CullRect old_cull_rect(gfx::Rect(20, 1100, 7050, 8100));
  CullRect cull_rect2(gfx::Rect(0, 0, 100, 200));
  // Use old_cull_rect if the new cull rect didn't change enough.
  EXPECT_TRUE(ApplyPaintProperties(cull_rect2, state1, state1,
                                   scroll_translation_state2, old_cull_rect));
  EXPECT_EQ(old_cull_rect, cull_rect2);

  old_cull_rect.Move(gfx::Vector2d(1000, 1000));
  CullRect cull_rect3(gfx::Rect(0, 0, 100, 200));
  // Use the new cull rect if it changed enough.
  EXPECT_TRUE(ApplyPaintProperties(cull_rect3, state1, state1,
                                   scroll_translation_state2, old_cull_rect));
  EXPECT_EQ(cull_rect1, cull_rect3);
}

TEST_P(CullRectTest, NonCompositedTransformUnderClip) {
  PropertyTreeState root = PropertyTreeState::Root();
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(100, 200, 300, 400));
  auto t1 = CreateTransform(t0(), MakeTranslationMatrix(10, 20));
  PropertyTreeState state1(*t1, *c1, e0());

  CullRect cull_rect1(gfx::Rect(0, 0, 300, 500));
  EXPECT_FALSE(ApplyPaintProperties(cull_rect1, root, root, state1));
  // Clip by c1, then transformed by t1.
  EXPECT_EQ(gfx::Rect(90, 180, 200, 300), cull_rect1.Rect());

  CullRect cull_rect2(gfx::Rect(0, 0, 300, 500));
  CullRect old_cull_rect(gfx::Rect(133, 244, 333, 444));
  // Should ignore old_cull_rect.
  EXPECT_FALSE(
      ApplyPaintProperties(cull_rect2, root, root, state1, old_cull_rect));
  EXPECT_EQ(cull_rect1, cull_rect2);

  CullRect cull_rect3 = CullRect::Infinite();
  EXPECT_FALSE(ApplyPaintProperties(cull_rect3, root, root, state1));
  EXPECT_EQ(gfx::Rect(90, 180, 300, 400), cull_rect3.Rect());

  CullRect cull_rect4;
  EXPECT_FALSE(ApplyPaintProperties(cull_rect4, root, root, state1));
  EXPECT_EQ(gfx::Rect(), cull_rect4.Rect());
}

TEST_P(CullRectTest, CompositedTranslationUnderClip) {
  PropertyTreeState root = PropertyTreeState::Root();
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(100, 200, 300, 400));
  auto transform = MakeTranslationMatrix(10, 20);
  transform.Scale3d(2, 4, 1);
  auto t1 = CreateTransform(t0(), transform, gfx::Point3F(),
                            CompositingReason::kWillChangeTransform);
  PropertyTreeState state1(*t1, *c1, e0());

  CullRect cull_rect1(gfx::Rect(0, 0, 300, 500));
  EXPECT_TRUE(ApplyPaintProperties(cull_rect1, root, root, state1));
  // The result in NonCompositedTransformUnderClip expanded by 2000 (scaled by
  // minimum of 1/2 and 1/4).
  EXPECT_EQ(gfx::Rect(-955, -955, 2100, 2075), cull_rect1.Rect());

  CullRect cull_rect2(gfx::Rect(0, 0, 300, 500));
  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(gfx::Vector2d(200, 200));
  // Use old_cull_rect if the new cull rect didn't change enough.
  EXPECT_TRUE(
      ApplyPaintProperties(cull_rect2, root, root, state1, old_cull_rect));
  EXPECT_EQ(old_cull_rect, cull_rect2);

  CullRect cull_rect3(gfx::Rect(0, 0, 300, 500));
  old_cull_rect.Move(gfx::Vector2d(1000, 1000));
  // Use the new cull rect if it changed enough.
  EXPECT_TRUE(
      ApplyPaintProperties(cull_rect3, root, root, state1, old_cull_rect));
  EXPECT_EQ(cull_rect1, cull_rect3);

  CullRect cull_rect4 = CullRect::Infinite();
  EXPECT_TRUE(ApplyPaintProperties(cull_rect4, root, root, state1));
  EXPECT_EQ(gfx::Rect(-955, -955, 2150, 2100), cull_rect4.Rect());

  CullRect cull_rect5;
  EXPECT_TRUE(ApplyPaintProperties(cull_rect4, root, root, state1));
  EXPECT_EQ(gfx::Rect(), cull_rect5.Rect());
}

TEST_P(CullRectTest, CompositedTransformUnderClipWithoutExpansion) {
  disable_expansion_ = true;
  PropertyTreeState root = PropertyTreeState::Root();
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(100, 200, 300, 400));
  auto t1 = CreateTransform(t0(), MakeTranslationMatrix(10, 20), gfx::Point3F(),
                            CompositingReason::kWillChangeTransform);
  PropertyTreeState state1(*t1, *c1, e0());

  CullRect cull_rect1(gfx::Rect(0, 0, 300, 500));
  EXPECT_FALSE(ApplyPaintProperties(cull_rect1, root, root, state1));
  // Clip by c1, then transformed by t1.
  EXPECT_EQ(gfx::Rect(90, 180, 200, 300), cull_rect1.Rect());

  CullRect cull_rect2(gfx::Rect(0, 0, 300, 500));
  CullRect old_cull_rect(gfx::Rect(133, 244, 333, 444));
  // Should ignore old_cull_rect.
  EXPECT_FALSE(
      ApplyPaintProperties(cull_rect2, root, root, state1, old_cull_rect));
  EXPECT_EQ(cull_rect1, cull_rect2);

  CullRect cull_rect3 = CullRect::Infinite();
  EXPECT_FALSE(ApplyPaintProperties(cull_rect3, root, root, state1));
  EXPECT_EQ(gfx::Rect(90, 180, 300, 400), cull_rect3.Rect());

  CullRect cull_rect4;
  EXPECT_FALSE(ApplyPaintProperties(cull_rect4, root, root, state1));
  EXPECT_EQ(gfx::Rect(), cull_rect4.Rect());
}

TEST_P(CullRectTest, ClipAndCompositedScrollAndClip) {
  auto root = PropertyTreeState::Root();
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0, 10000, 100, 100));
  auto t1 = Create2DTranslation(t0(), 0, 10000);
  auto scroll_state = CreateCompositedScrollTranslationState(
      PropertyTreeState(*t1, *c1, e0()), 0, 0, gfx::Rect(0, 0, 120, 120),
      gfx::Size(10000, 5000));
  auto& scroll_clip = scroll_state.Clip();
  auto& scroll_translation = scroll_state.Transform();
  auto c2a = CreateClip(scroll_clip, scroll_translation,
                        FloatRoundedRect(0, 300, 100, 100));
  auto c2b = CreateClip(scroll_clip, scroll_translation,
                        FloatRoundedRect(0, 8000, 100, 100));
  auto t2 =
      CreateTransform(scroll_translation, gfx::Transform(), gfx::Point3F(),
                      CompositingReason::kWillChangeTransform);

  // c2a is out of view, but in the expansion area of the composited scroll.
  CullRect cull_rect = CullRect::Infinite();
  EXPECT_TRUE(
      ApplyPaintProperties(cull_rect, root, root,
                           PropertyTreeState(scroll_translation, *c2a, e0())));
  EXPECT_EQ(gfx::Rect(0, 300, 100, 100), cull_rect.Rect());
  // Composited case. The cull rect should be expanded.
  cull_rect = CullRect::Infinite();
  EXPECT_TRUE(ApplyPaintProperties(cull_rect, root, root,
                                   PropertyTreeState(*t2, *c2a, e0())));
  EXPECT_EQ(gfx::Rect(-4000, -3700, 8100, 8100), cull_rect.Rect());

  // Using c2a with old cull rect.
  cull_rect = CullRect::Infinite();
  EXPECT_TRUE(ApplyPaintProperties(
      cull_rect, root, root, PropertyTreeState(scroll_translation, *c2a, e0()),
      CullRect(gfx::Rect(0, 310, 100, 100))));
  // The new cull rect touches the left edge of the clipped expanded scrolling
  // contents bounds, so the old cull rect is not used.
  EXPECT_EQ(gfx::Rect(0, 300, 100, 100), cull_rect.Rect());
  // Composited case. The cull rect should be expanded.
  cull_rect = CullRect::Infinite();
  EXPECT_TRUE(ApplyPaintProperties(
      cull_rect, root, root, PropertyTreeState(*t2, *c2a, e0()),
      CullRect(gfx::Rect(-3900, -3700, 8100, 8100))));
  // The new cull rect touches the left edge of the clipped expanded scrolling
  // contents bounds, so the old cull rect is not used.
  EXPECT_EQ(gfx::Rect(-4000, -3700, 8100, 8100), cull_rect.Rect());

  // c2b is out of the expansion area of the composited scroll.
  cull_rect = CullRect::Infinite();
  EXPECT_FALSE(
      ApplyPaintProperties(cull_rect, root, root,
                           PropertyTreeState(scroll_translation, *c2b, e0())));
  EXPECT_EQ(gfx::Rect(), cull_rect.Rect());
  // Composited case. The cull rect should be still empty.
  cull_rect = CullRect::Infinite();
  EXPECT_FALSE(ApplyPaintProperties(cull_rect, root, root,
                                    PropertyTreeState(*t2, *c2b, e0())));
  EXPECT_EQ(gfx::Rect(), cull_rect.Rect());
}

// Test for multiple clips (e.g., overflow clip and inner border radius)
// associated with the same scroll translation.
TEST_P(CullRectTest, MultipleClips) {
  auto t1 = Create2DTranslation(t0(), 0, 0);
  auto scroll_state = CreateCompositedScrollTranslationState(
      PropertyTreeState(*t1, c0(), e0()), 0, 0, gfx::Rect(0, 0, 100, 100),
      gfx::Size(100, 2000));
  auto border_radius_clip =
      CreateClip(c0(), *t1, FloatRoundedRect(0, 0, 100, 100));
  auto scroll_clip =
      CreateClip(*border_radius_clip, *t1, FloatRoundedRect(0, 0, 100, 100));

  PropertyTreeState root = PropertyTreeState::Root();
  PropertyTreeState source(*t1, c0(), e0());
  PropertyTreeState destination = scroll_state.GetPropertyTreeState();
  CullRect cull_rect(gfx::Rect(0, 0, 800, 600));
  EXPECT_TRUE(ApplyPaintProperties(cull_rect, root, source, destination));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 2000), cull_rect.Rect());
}

TEST_P(CullRectTest, ClipWithNonIntegralOffsetAndZeroSize) {
  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(0.4, 0.6, 0, 0));
  PropertyTreeState source = PropertyTreeState::Root();
  PropertyTreeState destination(t0(), *clip, e0());
  CullRect cull_rect(gfx::Rect(0, 0, 800, 600));
  EXPECT_FALSE(ApplyPaintProperties(cull_rect, source, source, destination));
  EXPECT_TRUE(cull_rect.Rect().IsEmpty());
}

TEST_P(CullRectTest, IntersectsVerticalRange) {
  CullRect cull_rect(gfx::Rect(0, 0, 50, 100));

  EXPECT_TRUE(cull_rect.IntersectsVerticalRange(LayoutUnit(), LayoutUnit(1)));
  EXPECT_FALSE(
      cull_rect.IntersectsVerticalRange(LayoutUnit(100), LayoutUnit(101)));
}

TEST_P(CullRectTest, IntersectsHorizontalRange) {
  CullRect cull_rect(gfx::Rect(0, 0, 50, 100));

  EXPECT_TRUE(cull_rect.IntersectsHorizontalRange(LayoutUnit(), LayoutUnit(1)));
  EXPECT_FALSE(
      cull_rect.IntersectsHorizontalRange(LayoutUnit(50), LayoutUnit(51)));
}

}  // namespace blink
