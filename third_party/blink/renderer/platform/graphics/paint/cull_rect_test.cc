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

  CullRect::ApplyTransformResult ApplyTransform(
      CullRect& cull_rect,
      const TransformPaintPropertyNode& t) {
    return cull_rect.ApplyTransformInternal(t);
  }

  bool ChangedEnough(const IntRect& old_rect, const IntRect& new_rect) {
    return CullRect(new_rect).ChangedEnough(CullRect(old_rect));
  }
};

TEST_F(CullRectTest, IntersectsIntRect) {
  CullRect cull_rect(IntRect(0, 0, 50, 50));

  EXPECT_TRUE(cull_rect.Intersects(IntRect(0, 0, 1, 1)));
  EXPECT_FALSE(cull_rect.Intersects(IntRect(51, 51, 1, 1)));
}

TEST_F(CullRectTest, IntersectsLayoutRect) {
  CullRect cull_rect(IntRect(0, 0, 50, 50));

  EXPECT_TRUE(cull_rect.Intersects(LayoutRect(0, 0, 1, 1)));
  EXPECT_TRUE(cull_rect.Intersects(LayoutRect(
      LayoutUnit(0.1), LayoutUnit(0.1), LayoutUnit(0.1), LayoutUnit(0.1))));
}

TEST_F(CullRectTest, IntersectsTransformed) {
  CullRect cull_rect(IntRect(0, 0, 50, 50));
  AffineTransform transform;
  transform.Translate(-2, -2);

  EXPECT_TRUE(
      cull_rect.IntersectsTransformed(transform, FloatRect(51, 51, 1, 1)));
  EXPECT_FALSE(cull_rect.Intersects(IntRect(52, 52, 1, 1)));
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
  EXPECT_EQ(kNotExpanded, ApplyTransform(cull_rect, *transform));

  EXPECT_EQ(IntRect(0, 0, 50, 50), cull_rect.Rect());
}

TEST_F(CullRectTest, ApplyTransformInfinite) {
  CullRect cull_rect = CullRect::Infinite();
  auto transform =
      CreateTransform(t0(), TransformationMatrix().Translate(1, 1));
  EXPECT_EQ(kNotExpanded, ApplyTransform(cull_rect, *transform));

  EXPECT_TRUE(cull_rect.IsInfinite());
}

TEST_F(CullRectTest, ApplyScrollTranslationPartialScrollingContents) {
  ScopedCompositeAfterPaintForTest cap(true);

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(20, 10, 40, 50);
  scroll_state.contents_size = IntSize(8000, 8000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation =
      CreateCompositedScrollTranslation(t0(), -3000, -5000, *scroll);

  CullRect cull_rect(IntRect(0, 0, 50, 100));
  EXPECT_EQ(kExpandedForPartialScrollingContents,
            ApplyTransform(cull_rect, *scroll_translation));

  // Clipped: (20, 10, 30, 50)
  // Inverse transformed: (3020, 5010, 30, 50)
  // Expanded: (-980, 1010, 8030, 8050)
  EXPECT_EQ(IntRect(-980, 1010, 8030, 8050), cull_rect.Rect());

  cull_rect = CullRect::Infinite();
  EXPECT_EQ(kExpandedForPartialScrollingContents,
            ApplyTransform(cull_rect, *scroll_translation));
  // This result differs from the above result in height (8040 vs 8030)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(IntRect(-980, 1010, 8040, 8050), cull_rect.Rect());
}

TEST_F(CullRectTest,
       ApplyNonCompositedScrollTranslationPartialScrollingContents) {
  ScopedCompositeAfterPaintForTest cap(true);

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(20, 10, 40, 50);
  scroll_state.contents_size = IntSize(8000, 8000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation =
      CreateScrollTranslation(t0(), -3000, -5000, *scroll);

  CullRect cull_rect(IntRect(0, 0, 50, 100));
  EXPECT_EQ(kNotExpanded, ApplyTransform(cull_rect, *scroll_translation));

  // Clipped: (20, 10, 30, 50)
  // Inverse transformed: (3020, 5010, 30, 50)
  EXPECT_EQ(IntRect(3020, 5010, 30, 50), cull_rect.Rect());

  cull_rect = CullRect::Infinite();
  EXPECT_EQ(kNotExpanded, ApplyTransform(cull_rect, *scroll_translation));
  // This result differs from the above result in height (40 vs 30)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(IntRect(3020, 5010, 40, 50), cull_rect.Rect());
}

TEST_F(CullRectTest, ApplyScrollTranslationNoIntersectionWithContainerRect) {
  ScopedCompositeAfterPaintForTest cap(true);

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(200, 100, 40, 50);
  scroll_state.contents_size = IntSize(2000, 2000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation =
      CreateCompositedScrollTranslation(t0(), -10, -15, *scroll);

  CullRect cull_rect(IntRect(0, 0, 50, 100));
  EXPECT_EQ(kNotExpanded, ApplyTransform(cull_rect, *scroll_translation));
  EXPECT_TRUE(cull_rect.Rect().IsEmpty());
}

TEST_F(CullRectTest,
       ApplyNonCompositedScrollTranslationNoIntersectionWithContainerRect) {
  ScopedCompositeAfterPaintForTest cap(true);

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(200, 100, 40, 50);
  scroll_state.contents_size = IntSize(2000, 2000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation = CreateScrollTranslation(t0(), -10, -15, *scroll);

  CullRect cull_rect(IntRect(0, 0, 50, 100));
  EXPECT_EQ(kNotExpanded, ApplyTransform(cull_rect, *scroll_translation));
  EXPECT_TRUE(cull_rect.Rect().IsEmpty());
}

TEST_F(CullRectTest, ApplyScrollTranslationWholeScrollingContents) {
  ScopedCompositeAfterPaintForTest cap(true);

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(20, 10, 40, 50);
  scroll_state.contents_size = IntSize(2000, 2000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation =
      CreateCompositedScrollTranslation(t0(), -10, -15, *scroll);

  CullRect cull_rect(IntRect(0, 0, 50, 100));
  EXPECT_EQ(kExpandedForWholeScrollingContents,
            ApplyTransform(cull_rect, *scroll_translation));

  // Clipped: (20, 10, 30, 50)
  // Inverse transformed: (30, 25, 30, 50)
  // Expanded: (-3970, -3975, 8030, 8050)
  EXPECT_EQ(IntRect(-3970, -3975, 8030, 8050), cull_rect.Rect());

  cull_rect = CullRect::Infinite();
  EXPECT_EQ(kExpandedForWholeScrollingContents,
            ApplyTransform(cull_rect, *scroll_translation));
  // This result differs from the above result in height (8040 vs 8030)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(IntRect(-3970, -3975, 8040, 8050), cull_rect.Rect());
}

TEST_F(CullRectTest,
       ApplyNonCompositedScrollTranslationWholeScrollingContents) {
  ScopedCompositeAfterPaintForTest cap(true);

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(20, 10, 40, 50);
  scroll_state.contents_size = IntSize(2000, 2000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation = CreateScrollTranslation(t0(), -10, -15, *scroll);

  CullRect cull_rect(IntRect(0, 0, 50, 100));
  EXPECT_EQ(kNotExpanded, ApplyTransform(cull_rect, *scroll_translation));

  // Clipped: (20, 10, 30, 50)
  // Inverse transformed: (30, 25, 30, 50)
  EXPECT_EQ(IntRect(30, 25, 30, 50), cull_rect.Rect());

  cull_rect = CullRect::Infinite();
  EXPECT_EQ(kNotExpanded, ApplyTransform(cull_rect, *scroll_translation));
  // This result differs from the above result in height (40 vs 30)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(IntRect(30, 25, 40, 50), cull_rect.Rect());
}

TEST_F(CullRectTest, ChangedEnoughEmpty) {
  ScopedCompositeAfterPaintForTest cap(true);
  EXPECT_FALSE(ChangedEnough(IntRect(), IntRect()));
  EXPECT_FALSE(ChangedEnough(IntRect(1, 1, 0, 0), IntRect(2, 2, 0, 0)));
  EXPECT_TRUE(ChangedEnough(IntRect(), IntRect(0, 0, 1, 1)));
  EXPECT_FALSE(ChangedEnough(IntRect(0, 0, 1, 1), IntRect()));
}

TEST_F(CullRectTest, ChangedNotEnough) {
  ScopedCompositeAfterPaintForTest cap(true);
  IntRect old_rect(100, 100, 100, 100);
  EXPECT_FALSE(ChangedEnough(old_rect, old_rect));
  EXPECT_FALSE(ChangedEnough(old_rect, IntRect(100, 100, 90, 90)));
  EXPECT_FALSE(ChangedEnough(old_rect, IntRect(100, 100, 100, 100)));
  EXPECT_FALSE(ChangedEnough(old_rect, IntRect(1, 1, 200, 200)));
}

TEST_F(CullRectTest, ChangedEnoughScrollScenarios) {
  ScopedCompositeAfterPaintForTest cap(true);
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

TEST_F(CullRectTest, ApplyTransformsSameTransform) {
  ScopedCompositeAfterPaintForTest cap(true);
  auto transform =
      CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  CullRect cull_rect1(IntRect(1, 1, 50, 50));
  cull_rect1.ApplyTransforms(*transform, *transform, base::nullopt);
  EXPECT_EQ(IntRect(1, 1, 50, 50), cull_rect1.Rect());

  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(IntSize(1, 1));
  CullRect cull_rect2(IntRect(1, 1, 50, 50));
  // Should ignore old_cull_rect.
  cull_rect2.ApplyTransforms(*transform, *transform, old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect2);

  CullRect infinite = CullRect::Infinite();
  infinite.ApplyTransforms(*transform, *transform, base::nullopt);
  EXPECT_TRUE(infinite.IsInfinite());
}

TEST_F(CullRectTest, ApplyTransformsWithoutScroll) {
  ScopedCompositeAfterPaintForTest cap(true);
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  auto t2 = CreateTransform(*t1, TransformationMatrix().Translate(10, 20));

  CullRect cull_rect1(IntRect(1, 1, 50, 50));
  cull_rect1.ApplyTransforms(*t1, *t2, base::nullopt);
  EXPECT_EQ(IntRect(-9, -19, 50, 50), cull_rect1.Rect());

  CullRect cull_rect2(IntRect(1, 1, 50, 50));
  cull_rect2.ApplyTransforms(t0(), *t2, base::nullopt);
  EXPECT_EQ(IntRect(-10, -21, 50, 50), cull_rect2.Rect());

  CullRect old_cull_rect = cull_rect2;
  old_cull_rect.Move(IntSize(1, 1));
  CullRect cull_rect3(IntRect(1, 1, 50, 50));
  // Should ignore old_cull_rect.
  cull_rect3.ApplyTransforms(t0(), *t2, old_cull_rect);
  EXPECT_EQ(cull_rect2, cull_rect3);

  CullRect infinite = CullRect::Infinite();
  infinite.ApplyTransforms(t0(), *t2, base::nullopt);
  EXPECT_TRUE(infinite.IsInfinite());
}

TEST_F(CullRectTest, ApplyTransformsSingleScrollWholeScrollingContents) {
  ScopedCompositeAfterPaintForTest cap(true);
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(20, 10, 40, 50);
  scroll_state.contents_size = IntSize(2000, 2000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation =
      CreateCompositedScrollTranslation(*t1, -10, -15, *scroll);

  // Same as ApplyScrollTranslationWholeScrollingContents.
  CullRect cull_rect1(IntRect(0, 0, 50, 100));
  cull_rect1.ApplyTransforms(*t1, *scroll_translation, base::nullopt);
  EXPECT_EQ(IntRect(-3970, -3975, 8030, 8050), cull_rect1.Rect());

  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(IntSize(1, 1));
  CullRect cull_rect2(IntRect(0, 0, 50, 100));
  // Should ignore old_cull_rect.
  cull_rect2.ApplyTransforms(*t1, *scroll_translation, old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect2);

  CullRect cull_rect3 = CullRect::Infinite();
  cull_rect3.ApplyTransforms(*t1, *scroll_translation, base::nullopt);
  // This result differs from the first result in height (8040 vs 8030)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(IntRect(-3970, -3975, 8040, 8050), cull_rect3.Rect());
}

TEST_F(CullRectTest, ApplyTransformsWithOrigin) {
  ScopedCompositeAfterPaintForTest cap(true);
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));
  auto t2 = CreateTransform(*t1, TransformationMatrix().Scale(0.5),
                            FloatPoint3D(50, 100, 0));
  CullRect cull_rect1(IntRect(0, 0, 50, 200));
  cull_rect1.ApplyTransforms(*t1, *t2, base::nullopt);
  EXPECT_EQ(IntRect(-50, -100, 100, 400), cull_rect1.Rect());
}

TEST_F(CullRectTest, ApplyTransformsSingleScrollPartialScrollingContents) {
  ScopedCompositeAfterPaintForTest cap(true);
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(20, 10, 40, 50);
  scroll_state.contents_size = IntSize(8000, 8000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation =
      CreateCompositedScrollTranslation(*t1, -3000, -5000, *scroll);

  // Same as ApplyScrollTranslationPartialScrollingContents.
  CullRect cull_rect1(IntRect(0, 0, 50, 100));
  cull_rect1.ApplyTransforms(*t1, *scroll_translation, base::nullopt);
  EXPECT_EQ(IntRect(-980, 1010, 8030, 8050), cull_rect1.Rect());

  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(IntSize(1, 1));
  CullRect cull_rect2(IntRect(0, 0, 50, 100));
  // Use old_cull_rect if the new cull rect didn't change enough.
  cull_rect2.ApplyTransforms(*t1, *scroll_translation, old_cull_rect);
  EXPECT_EQ(old_cull_rect, cull_rect2);

  old_cull_rect.Move(IntSize(1000, 1000));
  CullRect cull_rect3(IntRect(0, 0, 50, 100));
  // Use the new cull rect if it changed enough.
  cull_rect3.ApplyTransforms(*t1, *scroll_translation, old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect3);

  CullRect cull_rect4 = CullRect::Infinite();
  cull_rect4.ApplyTransforms(*t1, *scroll_translation, base::nullopt);
  // This result differs from the first result in height (8040 vs 8030)
  // because it's not clipped by the infinite input cull rect.
  EXPECT_EQ(IntRect(-980, 1010, 8040, 8050), cull_rect4.Rect());
}

TEST_F(CullRectTest, ApplyTransformsEscapingScroll) {
  ScopedCompositeAfterPaintForTest cap(true);
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));

  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = IntRect(20, 10, 40, 50);
  scroll_state.contents_size = IntSize(8000, 8000);
  auto scroll = ScrollPaintPropertyNode::Create(ScrollPaintPropertyNode::Root(),
                                                std::move(scroll_state));
  auto scroll_translation =
      CreateCompositedScrollTranslation(*t1, -3000, -5000, *scroll);
  auto t2 = CreateTransform(*scroll_translation,
                            TransformationMatrix().Translate(100, 200));

  CullRect cull_rect1(IntRect(0, 0, 50, 100));
  // Just apply tranforms without clipping and expansion for scroll translation.
  cull_rect1.ApplyTransforms(*t2, *t1, base::nullopt);
  EXPECT_EQ(IntRect(-2900, -4800, 50, 100), cull_rect1.Rect());

  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(IntSize(1, 1));
  CullRect cull_rect2(IntRect(0, 0, 50, 100));
  // Should ignore old_cull_rect.
  cull_rect2.ApplyTransforms(*t2, *t1, old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect2);

  CullRect infinite = CullRect::Infinite();
  infinite.ApplyTransforms(*t2, *t1, base::nullopt);
  EXPECT_TRUE(infinite.IsInfinite());
}

TEST_F(CullRectTest, ApplyTransformsSmallScrollContentsAfterBigScrollContents) {
  ScopedCompositeAfterPaintForTest cap(true);
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));

  ScrollPaintPropertyNode::State scroll_state1;
  scroll_state1.container_rect = IntRect(20, 10, 40, 50);
  scroll_state1.contents_size = IntSize(8000, 8000);
  auto scroll1 = ScrollPaintPropertyNode::Create(
      ScrollPaintPropertyNode::Root(), std::move(scroll_state1));
  auto scroll_translation1 =
      CreateCompositedScrollTranslation(*t1, -10, -15, *scroll1);

  auto t2 = CreateTransform(*scroll_translation1,
                            TransformationMatrix().Translate(2000, 3000));

  ScrollPaintPropertyNode::State scroll_state2;
  scroll_state2.container_rect = IntRect(30, 20, 100, 200);
  scroll_state2.contents_size = IntSize(200, 400);
  auto scroll2 = ScrollPaintPropertyNode::Create(
      ScrollPaintPropertyNode::Root(), std::move(scroll_state2));
  auto scroll_translation2 =
      CreateCompositedScrollTranslation(*t2, -10, -15, *scroll2);

  CullRect cull_rect1(IntRect(0, 0, 50, 100));
  cull_rect1.ApplyTransforms(*t1, *scroll_translation2, base::nullopt);
  EXPECT_EQ(IntRect(-3960, -3965, 8100, 8200), cull_rect1.Rect());

  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(IntSize(1, 1));
  CullRect cull_rect2(IntRect(0, 0, 50, 100));
  // Should ignore old_cull_rect.
  cull_rect2.ApplyTransforms(*t1, *scroll_translation2, old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect2);
}

TEST_F(CullRectTest, ApplyTransformsBigScrollContentsAfterSmallScrollContents) {
  ScopedCompositeAfterPaintForTest cap(true);
  auto t1 = CreateTransform(t0(), TransformationMatrix().Translate(1, 2));

  ScrollPaintPropertyNode::State scroll_state1;
  scroll_state1.container_rect = IntRect(30, 20, 100, 200);
  scroll_state1.contents_size = IntSize(200, 400);
  auto scroll1 = ScrollPaintPropertyNode::Create(
      ScrollPaintPropertyNode::Root(), std::move(scroll_state1));
  auto scroll_translation1 =
      CreateCompositedScrollTranslation(*t1, -10, -15, *scroll1);

  auto t2 = CreateTransform(*scroll_translation1,
                            TransformationMatrix().Translate(10, 20));

  ScrollPaintPropertyNode::State scroll_state2;
  scroll_state2.container_rect = IntRect(20, 10, 50, 100);
  scroll_state2.contents_size = IntSize(10000, 20000);
  auto scroll2 = ScrollPaintPropertyNode::Create(
      ScrollPaintPropertyNode::Root(), std::move(scroll_state2));
  auto scroll_translation2 =
      CreateCompositedScrollTranslation(*t2, -3000, -5000, *scroll2);

  CullRect cull_rect1(IntRect(0, 0, 100, 200));
  cull_rect1.ApplyTransforms(*t1, *scroll_translation2, base::nullopt);
  // After the first scroll: (-3960, -3965, 8070, 8180)
  // After t2: (-3980, -3975, 8070, 8180)
  // Clipped by the container rect of the second scroll: (20, 10, 50, 100)
  // After the second scroll offset: (3020, 5010, 50, 100)
  // Expanded: (-980, 1010, 8050, 8100)
  EXPECT_EQ(IntRect(-980, 1010, 8050, 8100), cull_rect1.Rect());

  CullRect old_cull_rect = cull_rect1;
  old_cull_rect.Move(IntSize(1, 1));
  CullRect cull_rect2(IntRect(0, 0, 100, 200));
  // Use old_cull_rect if the new cull rect didn't change enough.
  cull_rect2.ApplyTransforms(*t1, *scroll_translation2, old_cull_rect);
  EXPECT_EQ(old_cull_rect, cull_rect2);

  old_cull_rect.Move(IntSize(1000, 1000));
  CullRect cull_rect3(IntRect(0, 0, 100, 200));
  // Use the new cull rect if it changed enough.
  cull_rect3.ApplyTransforms(*t1, *scroll_translation2, old_cull_rect);
  EXPECT_EQ(cull_rect1, cull_rect3);
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
