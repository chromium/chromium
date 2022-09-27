// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_test_suite.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mock.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {

namespace {

using testing::_;
using testing::Mock;
using testing::Return;

class MockAnimatingScrollableArea : public MockScrollableArea {
 public:
  static MockAnimatingScrollableArea* Create() {
    return MakeGarbageCollected<MockAnimatingScrollableArea>();
  }
  static MockAnimatingScrollableArea* Create(
      const ScrollOffset& maximum_scroll_offset) {
    MockAnimatingScrollableArea* mock = Create();
    mock->SetMaximumScrollOffset(maximum_scroll_offset);
    return mock;
  }
  Scrollbar* HorizontalScrollbar() const override { return nullptr; }
  Scrollbar* VerticalScrollbar() const override { return nullptr; }
  MOCK_CONST_METHOD0(ScrollAnimatorEnabled, bool());
  MOCK_METHOD0(ScheduleAnimation, bool());
};

class ScrollbarThemeWithMockInvalidation : public ScrollbarThemeOverlayMock {
 public:
  MOCK_CONST_METHOD0(ShouldRepaintAllPartsOnInvalidation, bool());
  MOCK_CONST_METHOD3(PartsToInvalidateOnThumbPositionChange,
                     ScrollbarPart(const Scrollbar&, float, float));
};

}  // namespace

class ScrollableAreaTest : public testing::Test,
                           public PaintTestConfigurations {};

INSTANTIATE_PAINT_TEST_SUITE_P(ScrollableAreaTest);

TEST_P(ScrollableAreaTest, ScrollAnimatorCurrentPositionShouldBeSync) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  scrollable_area->SetScrollOffset(ScrollOffset(0, 10000),
                                   mojom::blink::ScrollType::kCompositor);
  EXPECT_EQ(100.0, scrollable_area->GetScrollAnimator().CurrentOffset().y());
}

TEST_P(ScrollableAreaTest, ScrollbarTrackAndThumbRepaint) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  ScrollbarThemeWithMockInvalidation theme;
  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      scrollable_area, kHorizontalScrollbar, &theme);

  EXPECT_CALL(theme, ShouldRepaintAllPartsOnInvalidation())
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(scrollbar->TrackNeedsRepaint());
  EXPECT_TRUE(scrollbar->ThumbNeedsRepaint());
  scrollbar->SetNeedsPaintInvalidation(kNoPart);
  EXPECT_TRUE(scrollbar->TrackNeedsRepaint());
  EXPECT_TRUE(scrollbar->ThumbNeedsRepaint());

  scrollbar->ClearTrackNeedsRepaint();
  scrollbar->ClearThumbNeedsRepaint();
  EXPECT_FALSE(scrollbar->TrackNeedsRepaint());
  EXPECT_FALSE(scrollbar->ThumbNeedsRepaint());
  scrollbar->SetNeedsPaintInvalidation(kThumbPart);
  EXPECT_TRUE(scrollbar->TrackNeedsRepaint());
  EXPECT_TRUE(scrollbar->ThumbNeedsRepaint());

  // When not all parts are repainted on invalidation,
  // setNeedsPaintInvalidation sets repaint bits only on the requested parts.
  EXPECT_CALL(theme, ShouldRepaintAllPartsOnInvalidation())
      .WillRepeatedly(Return(false));
  scrollbar->ClearTrackNeedsRepaint();
  scrollbar->ClearThumbNeedsRepaint();
  EXPECT_FALSE(scrollbar->TrackNeedsRepaint());
  EXPECT_FALSE(scrollbar->ThumbNeedsRepaint());
  scrollbar->SetNeedsPaintInvalidation(kThumbPart);
  EXPECT_FALSE(scrollbar->TrackNeedsRepaint());
  EXPECT_TRUE(scrollbar->ThumbNeedsRepaint());

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::Current()->CollectAllGarbageForTesting();
}

TEST_P(ScrollableAreaTest, InvalidatesNonCompositedScrollbarsWhenThumbMoves) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  ScrollbarThemeWithMockInvalidation theme;
  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(100, 100));
  Scrollbar* horizontal_scrollbar = Scrollbar::CreateForTesting(
      scrollable_area, kHorizontalScrollbar, &theme);
  Scrollbar* vertical_scrollbar =
      Scrollbar::CreateForTesting(scrollable_area, kVerticalScrollbar, &theme);
  EXPECT_CALL(*scrollable_area, HorizontalScrollbar())
      .WillRepeatedly(Return(horizontal_scrollbar));
  EXPECT_CALL(*scrollable_area, VerticalScrollbar())
      .WillRepeatedly(Return(vertical_scrollbar));

  // Regardless of whether the theme invalidates any parts, non-composited
  // scrollbars have to be repainted if the thumb moves.
  EXPECT_CALL(*scrollable_area, LayerForHorizontalScrollbar())
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*scrollable_area, LayerForVerticalScrollbar())
      .WillRepeatedly(Return(nullptr));
  ASSERT_FALSE(scrollable_area->HasLayerForVerticalScrollbar());
  ASSERT_FALSE(scrollable_area->HasLayerForHorizontalScrollbar());
  EXPECT_CALL(theme, ShouldRepaintAllPartsOnInvalidation())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(theme, PartsToInvalidateOnThumbPositionChange(_, _, _))
      .WillRepeatedly(Return(kNoPart));

  // A scroll in each direction should only invalidate one scrollbar.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 50),
                                   mojom::blink::ScrollType::kProgrammatic);
  EXPECT_FALSE(scrollable_area->HorizontalScrollbarNeedsPaintInvalidation());
  EXPECT_TRUE(scrollable_area->VerticalScrollbarNeedsPaintInvalidation());
  scrollable_area->ClearNeedsPaintInvalidationForScrollControls();
  scrollable_area->SetScrollOffset(ScrollOffset(50, 50),
                                   mojom::blink::ScrollType::kProgrammatic);
  EXPECT_TRUE(scrollable_area->HorizontalScrollbarNeedsPaintInvalidation());
  EXPECT_FALSE(scrollable_area->VerticalScrollbarNeedsPaintInvalidation());
  scrollable_area->ClearNeedsPaintInvalidationForScrollControls();

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::Current()->CollectAllGarbageForTesting();
}

TEST_P(ScrollableAreaTest, ScrollableAreaDidScroll) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(100, 100));
  scrollable_area->DidCompositorScroll(gfx::PointF(40, 51));

  EXPECT_EQ(40, scrollable_area->ScrollOffsetInt().x());
  EXPECT_EQ(51, scrollable_area->ScrollOffsetInt().y());
}

TEST_P(ScrollableAreaTest, ProgrammaticScrollRespectAnimatorEnabled) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;
  MockAnimatingScrollableArea* scrollable_area =
      MockAnimatingScrollableArea::Create(ScrollOffset(0, 100));
  // Disable animations. Make sure an explicitly smooth programmatic scroll is
  // instantly scrolled.
  {
    EXPECT_CALL(*scrollable_area, ScrollAnimatorEnabled())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*scrollable_area, ScheduleAnimation()).Times(0);
    scrollable_area->SetScrollOffset(ScrollOffset(0, 100),
                                     mojom::blink::ScrollType::kProgrammatic,
                                     mojom::blink::ScrollBehavior::kSmooth);
    EXPECT_EQ(100, scrollable_area->GetScrollOffset().y());
  }
  Mock::VerifyAndClearExpectations(scrollable_area);
  // Enable animations. A smooth programmatic scroll should now schedule an
  // animation rather than immediately mutating the offset.
  {
    EXPECT_CALL(*scrollable_area, ScrollAnimatorEnabled())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*scrollable_area, ScheduleAnimation()).WillOnce(Return(true));
    scrollable_area->SetScrollOffset(ScrollOffset(0, 50),
                                     mojom::blink::ScrollType::kProgrammatic,
                                     mojom::blink::ScrollBehavior::kSmooth);
    // Offset is unchanged.
    EXPECT_EQ(100, scrollable_area->GetScrollOffset().y());
  }
}

// Scrollbars in popups shouldn't fade out since they aren't composited and thus
// they don't appear on hover so users without a wheel can't scroll if they fade
// out.
TEST_P(ScrollableAreaTest, PopupOverlayScrollbarShouldNotFadeOut) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  ScopedMockOverlayScrollbars mock_overlay_scrollbars;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  EXPECT_CALL(*scrollable_area, UsesCompositedScrolling())
      .WillRepeatedly(Return(false));
  scrollable_area->SetIsPopup();

  ScrollbarThemeOverlayMock& theme =
      (ScrollbarThemeOverlayMock&)scrollable_area->GetPageScrollbarTheme();
  theme.SetOverlayScrollbarFadeOutDelay(base::Seconds(1));
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      scrollable_area, kHorizontalScrollbar, &theme);

  DCHECK(scrollbar->IsOverlayScrollbar());
  DCHECK(scrollbar->Enabled());

  scrollable_area->ShowNonMacOverlayScrollbars();

  // No fade out animation should be posted.
  EXPECT_FALSE(scrollable_area->fade_overlay_scrollbars_timer_);

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::Current()->CollectAllGarbageForTesting();
}

TEST_P(ScrollableAreaTest, ScrollAnimatorCallbackFiresOnAnimationCancel) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  EXPECT_CALL(*scrollable_area, ScheduleAnimation())
      .WillRepeatedly(Return(true));
  bool finished = false;
  scrollable_area->SetScrollOffset(
      ScrollOffset(0, 10000), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kSmooth,
      ScrollableArea::ScrollCallback(
          base::BindOnce([](bool* finished) { *finished = true; }, &finished)));
  EXPECT_EQ(0.0, scrollable_area->GetScrollAnimator().CurrentOffset().y());
  EXPECT_FALSE(finished);
  scrollable_area->CancelProgrammaticScrollAnimation();
  EXPECT_EQ(0.0, scrollable_area->GetScrollAnimator().CurrentOffset().y());
  EXPECT_TRUE(finished);
}

TEST_P(ScrollableAreaTest, ScrollAnimatorCallbackFiresOnInstantScroll) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  EXPECT_CALL(*scrollable_area, ScheduleAnimation())
      .WillRepeatedly(Return(true));
  bool finished = false;
  scrollable_area->SetScrollOffset(
      ScrollOffset(0, 10000), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant,
      ScrollableArea::ScrollCallback(
          base::BindOnce([](bool* finished) { *finished = true; }, &finished)));
  EXPECT_EQ(100, scrollable_area->GetScrollAnimator().CurrentOffset().y());
  EXPECT_TRUE(finished);
}

TEST_P(ScrollableAreaTest, ScrollAnimatorCallbackFiresOnAnimationFinish) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  EXPECT_CALL(*scrollable_area, ScheduleAnimation())
      .WillRepeatedly(Return(true));
  bool finished = false;
  scrollable_area->SetScrollOffset(
      ScrollOffset(0, 9), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kSmooth,
      ScrollableArea::ScrollCallback(
          base::BindOnce([](bool* finished) { *finished = true; }, &finished)));
  EXPECT_EQ(0.0, scrollable_area->GetScrollAnimator().CurrentOffset().y());
  EXPECT_FALSE(finished);
  scrollable_area->UpdateCompositorScrollAnimations();
  scrollable_area->ServiceScrollAnimations(1);
  EXPECT_EQ(0.0, scrollable_area->GetScrollAnimator().CurrentOffset().y());
  EXPECT_FALSE(finished);
  scrollable_area->ServiceScrollAnimations(1000000);
  EXPECT_EQ(9.0, scrollable_area->GetScrollAnimator().CurrentOffset().y());
  EXPECT_TRUE(finished);
}

TEST_P(ScrollableAreaTest, ScrollBackToInitialPosition) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  EXPECT_CALL(*scrollable_area, ScheduleAnimation())
      .WillRepeatedly(Return(true));
  bool finished = false;
  scrollable_area->SetScrollOffset(
      ScrollOffset(0, 50), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kSmooth,
      ScrollableArea::ScrollCallback(
          base::BindOnce([](bool* finished) { *finished = true; }, &finished)));
  scrollable_area->SetScrollOffset(ScrollOffset(0, 0),
                                   mojom::blink::ScrollType::kProgrammatic,
                                   mojom::blink::ScrollBehavior::kSmooth);
  scrollable_area->UpdateCompositorScrollAnimations();
  scrollable_area->ServiceScrollAnimations(1);
  scrollable_area->ServiceScrollAnimations(1000000);
  EXPECT_EQ(0, scrollable_area->GetScrollOffset().y());
  EXPECT_TRUE(finished);
}

}  // namespace blink
