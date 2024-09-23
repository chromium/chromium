// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay.h"

#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_test_suite.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {

using testing::NiceMock;
using testing::Return;

class ScrollbarThemeOverlayTest : public testing::Test {
 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(ScrollbarThemeOverlayTest, PaintInvalidation) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  NiceMock<MockScrollableArea>* mock_scrollable_area =
      MakeGarbageCollected<NiceMock<MockScrollableArea>>(
          ScrollOffset(100, 100));
  ScrollbarThemeOverlay theme(14, 0, 14, 0);

  Scrollbar* vertical_scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area, kVerticalScrollbar, &theme);
  Scrollbar* horizontal_scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area, kHorizontalScrollbar, &theme);
  ON_CALL(*mock_scrollable_area, VerticalScrollbar())
      .WillByDefault(Return(vertical_scrollbar));
  ON_CALL(*mock_scrollable_area, HorizontalScrollbar())
      .WillByDefault(Return(horizontal_scrollbar));

  gfx::Rect vertical_rect(1010, 0, 14, 768);
  gfx::Rect horizontal_rect(0, 754, 1024, 14);
  vertical_scrollbar->SetFrameRect(vertical_rect);
  horizontal_scrollbar->SetFrameRect(horizontal_rect);

  ASSERT_EQ(vertical_scrollbar, mock_scrollable_area->VerticalScrollbar());
  ASSERT_EQ(horizontal_scrollbar, mock_scrollable_area->HorizontalScrollbar());

  vertical_scrollbar->ClearTrackAndButtonsNeedRepaint();
  vertical_scrollbar->ClearThumbNeedsRepaint();
  horizontal_scrollbar->ClearTrackAndButtonsNeedRepaint();
  horizontal_scrollbar->ClearThumbNeedsRepaint();
  mock_scrollable_area->ClearNeedsPaintInvalidationForScrollControls();

  ASSERT_FALSE(vertical_scrollbar->ThumbNeedsRepaint());
  ASSERT_FALSE(vertical_scrollbar->TrackAndButtonsNeedRepaint());
  ASSERT_FALSE(mock_scrollable_area->VerticalScrollbarNeedsPaintInvalidation());
  ASSERT_FALSE(horizontal_scrollbar->ThumbNeedsRepaint());
  ASSERT_FALSE(horizontal_scrollbar->TrackAndButtonsNeedRepaint());
  ASSERT_FALSE(
      mock_scrollable_area->HorizontalScrollbarNeedsPaintInvalidation());

  // Changing the scroll offset shouldn't invalid the thumb nor background, but
  // it should cause a "general" invalidation for non-composited scrollbars.
  // Ensure the horizontal scrollbar is unaffected.
  mock_scrollable_area->UpdateScrollOffset(ScrollOffset(0, 5),
                                           mojom::blink::ScrollType::kUser);
  vertical_scrollbar->OffsetDidChange(mojom::blink::ScrollType::kUser);
  horizontal_scrollbar->OffsetDidChange(mojom::blink::ScrollType::kUser);
  EXPECT_FALSE(vertical_scrollbar->ThumbNeedsRepaint());
  EXPECT_FALSE(vertical_scrollbar->TrackAndButtonsNeedRepaint());
  EXPECT_TRUE(mock_scrollable_area->VerticalScrollbarNeedsPaintInvalidation());
  EXPECT_FALSE(horizontal_scrollbar->ThumbNeedsRepaint());
  EXPECT_FALSE(horizontal_scrollbar->TrackAndButtonsNeedRepaint());
  EXPECT_FALSE(
      mock_scrollable_area->HorizontalScrollbarNeedsPaintInvalidation());

  // Try the horizontal scrollbar.
  mock_scrollable_area->ClearNeedsPaintInvalidationForScrollControls();
  mock_scrollable_area->UpdateScrollOffset(ScrollOffset(5, 5),
                                           mojom::blink::ScrollType::kUser);
  horizontal_scrollbar->OffsetDidChange(mojom::blink::ScrollType::kUser);
  vertical_scrollbar->OffsetDidChange(mojom::blink::ScrollType::kUser);
  EXPECT_FALSE(vertical_scrollbar->ThumbNeedsRepaint());
  EXPECT_FALSE(vertical_scrollbar->TrackAndButtonsNeedRepaint());
  EXPECT_FALSE(mock_scrollable_area->VerticalScrollbarNeedsPaintInvalidation());
  EXPECT_FALSE(horizontal_scrollbar->ThumbNeedsRepaint());
  EXPECT_FALSE(horizontal_scrollbar->TrackAndButtonsNeedRepaint());
  EXPECT_TRUE(
      mock_scrollable_area->HorizontalScrollbarNeedsPaintInvalidation());

  mock_scrollable_area->ClearNeedsPaintInvalidationForScrollControls();

  // Move the mouse over the vertical scrollbar's thumb. Ensure the thumb is
  // invalidated as its state is changed to hover.
  vertical_scrollbar->SetHoveredPart(kThumbPart);
  EXPECT_TRUE(vertical_scrollbar->ThumbNeedsRepaint());
  EXPECT_TRUE(mock_scrollable_area->VerticalScrollbarNeedsPaintInvalidation());

  vertical_scrollbar->ClearThumbNeedsRepaint();
  mock_scrollable_area->ClearNeedsPaintInvalidationForScrollControls();

  // Pressing down should also cause an invalidation.
  vertical_scrollbar->SetPressedPart(kThumbPart,
                                     WebInputEvent::Type::kMouseDown);
  EXPECT_TRUE(vertical_scrollbar->ThumbNeedsRepaint());
  EXPECT_TRUE(mock_scrollable_area->VerticalScrollbarNeedsPaintInvalidation());

  vertical_scrollbar->ClearThumbNeedsRepaint();
  mock_scrollable_area->ClearNeedsPaintInvalidationForScrollControls();

  // Release should cause invalidation.
  vertical_scrollbar->SetPressedPart(kNoPart, WebInputEvent::Type::kMouseDown);
  EXPECT_TRUE(vertical_scrollbar->ThumbNeedsRepaint());
  EXPECT_TRUE(mock_scrollable_area->VerticalScrollbarNeedsPaintInvalidation());

  vertical_scrollbar->ClearThumbNeedsRepaint();
  mock_scrollable_area->ClearNeedsPaintInvalidationForScrollControls();

  // Move off should cause invalidation
  vertical_scrollbar->SetHoveredPart(kNoPart);
  EXPECT_TRUE(vertical_scrollbar->ThumbNeedsRepaint());
  EXPECT_TRUE(mock_scrollable_area->VerticalScrollbarNeedsPaintInvalidation());

  vertical_scrollbar->ClearThumbNeedsRepaint();
  mock_scrollable_area->ClearNeedsPaintInvalidationForScrollControls();

  // Hiding the scrollbar should invalidate the layer (InvalidateAll) but not
  // trigger repaint of the thumb resource, since the compositor will give the
  // entire layer opacity 0.
  EXPECT_CALL(*mock_scrollable_area, ScrollbarsHiddenIfOverlay())
      .WillOnce(Return(true));
  vertical_scrollbar->SetEnabled(false);
  EXPECT_FALSE(vertical_scrollbar->ThumbNeedsRepaint());
  EXPECT_TRUE(mock_scrollable_area->VerticalScrollbarNeedsPaintInvalidation());

  mock_scrollable_area->ClearNeedsPaintInvalidationForScrollControls();

  // Showing the scrollbar needs to repaint the thumb resource, since it may
  // have been repainted in the disabled state while hidden (e.g. from
  // SetProportion on bounds changes).
  EXPECT_CALL(*mock_scrollable_area, ScrollbarsHiddenIfOverlay())
      .WillOnce(Return(false));
  vertical_scrollbar->SetEnabled(true);
  EXPECT_TRUE(vertical_scrollbar->ThumbNeedsRepaint());
  EXPECT_TRUE(mock_scrollable_area->VerticalScrollbarNeedsPaintInvalidation());

  ThreadState::Current()->CollectAllGarbageForTesting();
}

}  // namespace blink
