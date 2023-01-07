// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_paint_aggregator.h"

#include "ppapi/tests/testing_instance.h"
#include "ppapi/utility/graphics/paint_aggregator.h"

REGISTER_TEST_CASE(PaintAggregator);

bool TestPaintAggregator::Init() {
  return true;
}

void TestPaintAggregator::RunTests(const std::string& filter) {
  RUN_TEST(InitialState, filter);
  RUN_TEST(SingleInvalidation, filter);
  RUN_TEST(DoubleDisjointInvalidation, filter);
  RUN_TEST(SingleScroll, filter);
  RUN_TEST(DoubleOverlappingScroll, filter);
  RUN_TEST(NegatingScroll, filter);
  RUN_TEST(DiagonalScroll, filter);
  RUN_TEST(ContainedPaintAfterScroll, filter);
  RUN_TEST(ContainedPaintBeforeScroll, filter);
  RUN_TEST(ContainedPaintsBeforeAndAfterScroll, filter);
  RUN_TEST(LargeContainedPaintAfterScroll, filter);
  RUN_TEST(LargeContainedPaintBeforeScroll, filter);
  RUN_TEST(OverlappingPaintBeforeScroll, filter);
  RUN_TEST(OverlappingPaintAfterScroll, filter);
  RUN_TEST(DisjointPaintBeforeScroll, filter);
  RUN_TEST(DisjointPaintAfterScroll, filter);
  RUN_TEST(ContainedPaintTrimmedByScroll, filter);
  RUN_TEST(ContainedPaintEliminatedByScroll, filter);
  RUN_TEST(ContainedPaintAfterScrollTrimmedByScrollDamage, filter);
  RUN_TEST(ContainedPaintAfterScrollEliminatedByScrollDamage, filter);
}

std::string TestPaintAggregator::TestInitialState() {
  pp::PaintAggregator greg;
  if (greg.HasPendingUpdate())
    return "Pending update invalid";
  PASS();
}

std::string TestPaintAggregator::TestSingleInvalidation() {
  pp::PaintAggregator greg;

  pp::Rect rect(2, 4, 10, 16);
  greg.InvalidateRect(rect);

  ASSERT_TRUE(greg.HasPendingUpdate());
  ASSERT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(1U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(rect == greg.GetPendingUpdate().paint_rects[0]);

  PASS();
}

std::string TestPaintAggregator::TestDoubleDisjointInvalidation() {
  pp::PaintAggregator greg;

  pp::Rect r1(2, 4, 2, 4);
  pp::Rect r2(4, 2, 4, 2);

  greg.InvalidateRect(r1);
  greg.InvalidateRect(r2);

  pp::Rect expected_bounds = r1.Union(r2);

  ASSERT_TRUE(greg.HasPendingUpdate());
  ASSERT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(2U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(expected_bounds == greg.GetPendingUpdate().paint_bounds);
  PASS();
}

std::string TestPaintAggregator::TestSingleScroll() {
  pp::PaintAggregator greg;

  pp::Rect r1(2, 4, 2, 4);
  pp::Rect r2(4, 2, 4, 2);

  greg.InvalidateRect(r1);
  greg.InvalidateRect(r2);

  pp::Rect expected_bounds = r1.Union(r2);

  ASSERT_TRUE(greg.HasPendingUpdate());
  ASSERT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(2U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(expected_bounds == greg.GetPendingUpdate().paint_bounds);
  PASS();
}

std::string TestPaintAggregator::TestDoubleOverlappingScroll() {
  pp::PaintAggregator greg;

  pp::Rect rect(1, 2, 3, 4);
  pp::Point delta1(1, 0);
  pp::Point delta2(1, 0);
  greg.ScrollRect(rect, delta1);
  greg.ScrollRect(rect, delta2);

  ASSERT_TRUE(greg.HasPendingUpdate());
  ASSERT_TRUE(1U == greg.GetPendingUpdate().paint_rects.size());
  ASSERT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());

  ASSERT_TRUE(rect == greg.GetPendingUpdate().scroll_rect);

  pp::Point expected_delta(delta1.x() + delta2.x(),
                            delta1.y() + delta2.y());
  ASSERT_TRUE(expected_delta.x() == greg.GetPendingUpdate().scroll_delta.x());
  ASSERT_TRUE(expected_delta.y() == greg.GetPendingUpdate().scroll_delta.y());

  pp::Rect resulting_damage = greg.GetPendingUpdate().paint_rects[0];
  pp::Rect expected_damage(1, 2, 2, 4);
  ASSERT_TRUE(expected_damage == resulting_damage);
  PASS();
}

std::string TestPaintAggregator::TestNegatingScroll() {
  pp::PaintAggregator greg;

  // Scroll twice in opposite directions by equal amounts.  The result
  // should be no scrolling.

  pp::Rect rect(1, 2, 3, 4);
  pp::Point delta1(1, 0);
  pp::Point delta2(-1, 0);
  greg.ScrollRect(rect, delta1);
  greg.ScrollRect(rect, delta2);

  ASSERT_FALSE(greg.HasPendingUpdate());
  PASS();
}

std::string TestPaintAggregator::TestDiagonalScroll() {
  pp::PaintAggregator greg;

  // We don't support optimized diagonal scrolling, so this should result in
  // repainting.

  pp::Rect rect(1, 2, 3, 4);
  pp::Point delta(1, 1);
  greg.ScrollRect(rect, delta);

  ASSERT_TRUE(greg.HasPendingUpdate());
  ASSERT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(1U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(rect == greg.GetPendingUpdate().paint_rects[0]);
  PASS();
}

std::string TestPaintAggregator::TestContainedPaintAfterScroll() {
  pp::PaintAggregator greg;

  pp::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(scroll_rect, pp::Point(2, 0));

  pp::Rect paint_rect(4, 4, 2, 2);
  greg.InvalidateRect(paint_rect);

  ASSERT_TRUE(greg.HasPendingUpdate());

  // Expecting a paint rect inside the scroll rect. The last paint rect is the
  // scroll dirty rect.
  ASSERT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(2U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(scroll_rect == greg.GetPendingUpdate().scroll_rect);
  ASSERT_TRUE(paint_rect == greg.GetPendingUpdate().paint_rects[0]);
  PASS();
}

std::string TestPaintAggregator::TestContainedPaintBeforeScroll() {
  pp::PaintAggregator greg;

  pp::Rect paint_rect(4, 4, 2, 2);
  greg.InvalidateRect(paint_rect);

  pp::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(scroll_rect, pp::Point(2, 0));

  ASSERT_TRUE(greg.HasPendingUpdate());

  // Expecting a paint rect inside the scroll rect. The last paint rect is the
  // scroll dirty rect.
  ASSERT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(2U == greg.GetPendingUpdate().paint_rects.size());

  paint_rect.Offset(2, 0);

  ASSERT_TRUE(scroll_rect == greg.GetPendingUpdate().scroll_rect);
  ASSERT_TRUE(paint_rect == greg.GetPendingUpdate().paint_rects[0]);
  PASS();
}

std::string TestPaintAggregator::TestContainedPaintsBeforeAndAfterScroll() {
  pp::PaintAggregator greg;

  pp::Rect paint_rect1(4, 4, 2, 2);
  greg.InvalidateRect(paint_rect1);

  pp::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(scroll_rect, pp::Point(2, 0));

  pp::Rect paint_rect2(6, 4, 2, 2);
  greg.InvalidateRect(paint_rect2);

  pp::Rect expected_paint_rect = paint_rect2;

  ASSERT_TRUE(greg.HasPendingUpdate());

  // Expecting a paint rect inside the scroll rect
  ASSERT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(2U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(scroll_rect == greg.GetPendingUpdate().scroll_rect);
  ASSERT_TRUE(expected_paint_rect == greg.GetPendingUpdate().paint_rects[0]);
  PASS();
}

std::string TestPaintAggregator::TestLargeContainedPaintAfterScroll() {
  pp::PaintAggregator greg;

  pp::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(scroll_rect, pp::Point(0, 1));

  pp::Rect paint_rect(0, 0, 10, 9);  // Repaint 90%
  greg.InvalidateRect(paint_rect);

  ASSERT_TRUE(greg.HasPendingUpdate());

  ASSERT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(1U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(scroll_rect == greg.GetPendingUpdate().paint_rects[0]);
  PASS();
}

std::string TestPaintAggregator::TestLargeContainedPaintBeforeScroll() {
  pp::PaintAggregator greg;

  pp::Rect paint_rect(0, 0, 10, 9);  // Repaint 90%
  greg.InvalidateRect(paint_rect);

  pp::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(scroll_rect, pp::Point(0, 1));

  ASSERT_TRUE(greg.HasPendingUpdate());

  ASSERT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(1U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(scroll_rect == greg.GetPendingUpdate().paint_rects[0]);
  PASS();
}

std::string TestPaintAggregator::TestOverlappingPaintBeforeScroll() {
  pp::PaintAggregator greg;

  pp::Rect paint_rect(4, 4, 10, 2);
  greg.InvalidateRect(paint_rect);

  pp::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(scroll_rect, pp::Point(2, 0));

  pp::Rect expected_paint_rect = scroll_rect.Union(paint_rect);

  ASSERT_TRUE(greg.HasPendingUpdate());

  ASSERT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(1U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(expected_paint_rect == greg.GetPendingUpdate().paint_rects[0]);
  PASS();
}

std::string TestPaintAggregator::TestOverlappingPaintAfterScroll() {
  pp::PaintAggregator greg;

  pp::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(scroll_rect, pp::Point(2, 0));

  pp::Rect paint_rect(4, 4, 10, 2);
  greg.InvalidateRect(paint_rect);

  pp::Rect expected_paint_rect = scroll_rect.Union(paint_rect);

  ASSERT_TRUE(greg.HasPendingUpdate());

  ASSERT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(1U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(expected_paint_rect == greg.GetPendingUpdate().paint_rects[0]);
  PASS();
}

std::string TestPaintAggregator::TestDisjointPaintBeforeScroll() {
  pp::PaintAggregator greg;

  pp::Rect paint_rect(4, 4, 10, 2);
  greg.InvalidateRect(paint_rect);

  pp::Rect scroll_rect(0, 0, 2, 10);
  greg.ScrollRect(scroll_rect, pp::Point(2, 0));

  ASSERT_TRUE(greg.HasPendingUpdate());

  ASSERT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(2U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(paint_rect == greg.GetPendingUpdate().paint_rects[0]);
  ASSERT_TRUE(scroll_rect == greg.GetPendingUpdate().scroll_rect);
  PASS();
}

std::string TestPaintAggregator::TestDisjointPaintAfterScroll() {
  pp::PaintAggregator greg;

  pp::Rect scroll_rect(0, 0, 2, 10);
  greg.ScrollRect(scroll_rect, pp::Point(2, 0));

  pp::Rect paint_rect(4, 4, 10, 2);
  greg.InvalidateRect(paint_rect);

  ASSERT_TRUE(greg.HasPendingUpdate());

  ASSERT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(2U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(paint_rect == greg.GetPendingUpdate().paint_rects[0]);
  ASSERT_TRUE(scroll_rect == greg.GetPendingUpdate().scroll_rect);
  PASS();
}

std::string TestPaintAggregator::TestContainedPaintTrimmedByScroll() {
  pp::PaintAggregator greg;

  pp::Rect paint_rect(4, 4, 6, 6);
  greg.InvalidateRect(paint_rect);

  pp::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(scroll_rect, pp::Point(2, 0));

  // The paint rect should have become narrower.
  pp::Rect expected_paint_rect(6, 4, 4, 6);

  ASSERT_TRUE(greg.HasPendingUpdate());

  ASSERT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(2U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(expected_paint_rect == greg.GetPendingUpdate().paint_rects[0]);
  ASSERT_TRUE(scroll_rect == greg.GetPendingUpdate().scroll_rect);
  PASS();
}

std::string TestPaintAggregator::TestContainedPaintEliminatedByScroll() {
  pp::PaintAggregator greg;

  pp::Rect paint_rect(4, 4, 6, 6);
  greg.InvalidateRect(paint_rect);

  pp::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(scroll_rect, pp::Point(6, 0));

  ASSERT_TRUE(greg.HasPendingUpdate());

  ASSERT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(1U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(scroll_rect == greg.GetPendingUpdate().scroll_rect);
  PASS();
}

std::string
TestPaintAggregator::TestContainedPaintAfterScrollTrimmedByScrollDamage() {
  pp::PaintAggregator greg;

  pp::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(scroll_rect, pp::Point(4, 0));

  pp::Rect paint_rect(2, 0, 4, 10);
  greg.InvalidateRect(paint_rect);

  pp::Rect expected_scroll_damage(0, 0, 4, 10);
  pp::Rect expected_paint_rect(4, 0, 2, 10);

  ASSERT_TRUE(greg.HasPendingUpdate());

  ASSERT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(2U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(scroll_rect == greg.GetPendingUpdate().scroll_rect);
  ASSERT_TRUE(expected_scroll_damage == greg.GetPendingUpdate().paint_rects[1]);
  ASSERT_TRUE(expected_paint_rect == greg.GetPendingUpdate().paint_rects[0]);
  PASS();
}

std::string
TestPaintAggregator::TestContainedPaintAfterScrollEliminatedByScrollDamage() {
  pp::PaintAggregator greg;

  pp::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(scroll_rect, pp::Point(4, 0));

  pp::Rect paint_rect(2, 0, 2, 10);
  greg.InvalidateRect(paint_rect);

  pp::Rect expected_scroll_damage(0, 0, 4, 10);

  ASSERT_TRUE(greg.HasPendingUpdate());

  ASSERT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_TRUE(1U == greg.GetPendingUpdate().paint_rects.size());

  ASSERT_TRUE(scroll_rect == greg.GetPendingUpdate().scroll_rect);
  ASSERT_TRUE(expected_scroll_damage == greg.GetPendingUpdate().paint_rects[0]);
  PASS();
}
