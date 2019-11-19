// Copyright 2015 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/fake_graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/fake_graphics_layer_client.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {

namespace {

using testing::_;
using testing::Return;

class ScrollbarThemeWithMockInvalidation : public ScrollbarThemeOverlayMock {
 public:
  MOCK_CONST_METHOD0(ShouldRepaintAllPartsOnInvalidation, bool());
  MOCK_CONST_METHOD3(PartsToInvalidateOnThumbPositionChange,
                     ScrollbarPart(const Scrollbar&, float, float));
};

}  // namespace

class ScrollableAreaTest : public testing::Test {};

TEST_F(ScrollableAreaTest, ScrollAnimatorCurrentPositionShouldBeSync) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  scrollable_area->SetScrollOffset(ScrollOffset(0, 10000), kCompositorScroll);
  EXPECT_EQ(100.0,
            scrollable_area->GetScrollAnimator().CurrentOffset().Height());
}

TEST_F(ScrollableAreaTest, ScrollbarTrackAndThumbRepaint) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  ScrollbarThemeWithMockInvalidation theme;
  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      scrollable_area, kHorizontalScrollbar, kRegularScrollbar, &theme);

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

TEST_F(ScrollableAreaTest, ScrollbarLayerInvalidation) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  ScopedMockOverlayScrollbars mock_overlay_scrollbars;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  layer->SetIsDrawable(true);
  layer->SetBounds(gfx::Size(111, 222));

  EXPECT_CALL(*scrollable_area, LayerForHorizontalScrollbar())
      .WillRepeatedly(Return(layer.get()));

  auto* scrollbar =
      MakeGarbageCollected<Scrollbar>(scrollable_area, kHorizontalScrollbar,
                                      kRegularScrollbar, nullptr, nullptr);
  EXPECT_TRUE(layer->update_rect().IsEmpty());
  scrollbar->SetNeedsPaintInvalidation(kNoPart);
  EXPECT_FALSE(layer->update_rect().IsEmpty());

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::Current()->CollectAllGarbageForTesting();
}

TEST_F(ScrollableAreaTest, InvalidatesNonCompositedScrollbarsWhenThumbMoves) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  ScrollbarThemeWithMockInvalidation theme;
  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(100, 100));
  Scrollbar* horizontal_scrollbar = Scrollbar::CreateForTesting(
      scrollable_area, kHorizontalScrollbar, kRegularScrollbar, &theme);
  Scrollbar* vertical_scrollbar = Scrollbar::CreateForTesting(
      scrollable_area, kVerticalScrollbar, kRegularScrollbar, &theme);
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
  scrollable_area->SetScrollOffset(ScrollOffset(0, 50), kProgrammaticScroll);
  EXPECT_FALSE(scrollable_area->HorizontalScrollbarNeedsPaintInvalidation());
  EXPECT_TRUE(scrollable_area->VerticalScrollbarNeedsPaintInvalidation());
  scrollable_area->ClearNeedsPaintInvalidationForScrollControls();
  scrollable_area->SetScrollOffset(ScrollOffset(50, 50), kProgrammaticScroll);
  EXPECT_TRUE(scrollable_area->HorizontalScrollbarNeedsPaintInvalidation());
  EXPECT_FALSE(scrollable_area->VerticalScrollbarNeedsPaintInvalidation());
  scrollable_area->ClearNeedsPaintInvalidationForScrollControls();

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::Current()->CollectAllGarbageForTesting();
}

TEST_F(ScrollableAreaTest, InvalidatesCompositedScrollbarsIfPartsNeedRepaint) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  ScrollbarThemeWithMockInvalidation theme;
  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(100, 100));
  Scrollbar* horizontal_scrollbar = Scrollbar::CreateForTesting(
      scrollable_area, kHorizontalScrollbar, kRegularScrollbar, &theme);
  horizontal_scrollbar->ClearTrackNeedsRepaint();
  horizontal_scrollbar->ClearThumbNeedsRepaint();
  Scrollbar* vertical_scrollbar = Scrollbar::CreateForTesting(
      scrollable_area, kVerticalScrollbar, kRegularScrollbar, &theme);
  vertical_scrollbar->ClearTrackNeedsRepaint();
  vertical_scrollbar->ClearThumbNeedsRepaint();
  EXPECT_CALL(*scrollable_area, HorizontalScrollbar())
      .WillRepeatedly(Return(horizontal_scrollbar));
  EXPECT_CALL(*scrollable_area, VerticalScrollbar())
      .WillRepeatedly(Return(vertical_scrollbar));

  // Composited scrollbars only need repainting when parts become invalid
  // (e.g. if the track changes appearance when the thumb reaches the end).
  scoped_refptr<cc::Layer> layer_for_horizontal_scrollbar = cc::Layer::Create();
  layer_for_horizontal_scrollbar->SetIsDrawable(true);
  layer_for_horizontal_scrollbar->SetBounds(gfx::Size(10, 10));
  scoped_refptr<cc::Layer> layer_for_vertical_scrollbar = cc::Layer::Create();
  layer_for_vertical_scrollbar->SetIsDrawable(true);
  layer_for_vertical_scrollbar->SetBounds(gfx::Size(10, 10));
  EXPECT_CALL(*scrollable_area, LayerForHorizontalScrollbar())
      .WillRepeatedly(Return(layer_for_horizontal_scrollbar.get()));
  EXPECT_CALL(*scrollable_area, LayerForVerticalScrollbar())
      .WillRepeatedly(Return(layer_for_vertical_scrollbar.get()));
  ASSERT_TRUE(scrollable_area->HasLayerForHorizontalScrollbar());
  ASSERT_TRUE(scrollable_area->HasLayerForVerticalScrollbar());
  EXPECT_CALL(theme, ShouldRepaintAllPartsOnInvalidation())
      .WillRepeatedly(Return(false));

  EXPECT_TRUE(layer_for_horizontal_scrollbar->update_rect().IsEmpty());
  EXPECT_TRUE(layer_for_vertical_scrollbar->update_rect().IsEmpty());

  // First, we'll scroll horizontally, and the theme will require repainting
  // the back button (i.e. the track).
  EXPECT_CALL(theme, PartsToInvalidateOnThumbPositionChange(_, _, _))
      .WillOnce(Return(kBackButtonStartPart));
  scrollable_area->SetScrollOffset(ScrollOffset(50, 0), kProgrammaticScroll);
  EXPECT_FALSE(layer_for_horizontal_scrollbar->update_rect().IsEmpty());
  EXPECT_TRUE(layer_for_vertical_scrollbar->update_rect().IsEmpty());
  EXPECT_TRUE(horizontal_scrollbar->TrackNeedsRepaint());
  EXPECT_FALSE(horizontal_scrollbar->ThumbNeedsRepaint());
  layer_for_horizontal_scrollbar->ResetUpdateRectForTesting();
  horizontal_scrollbar->ClearTrackNeedsRepaint();

  // Next, we'll scroll vertically, but invalidate the thumb.
  EXPECT_CALL(theme, PartsToInvalidateOnThumbPositionChange(_, _, _))
      .WillOnce(Return(kThumbPart));
  scrollable_area->SetScrollOffset(ScrollOffset(50, 50), kProgrammaticScroll);
  EXPECT_TRUE(layer_for_horizontal_scrollbar->update_rect().IsEmpty());
  EXPECT_FALSE(layer_for_vertical_scrollbar->update_rect().IsEmpty());
  EXPECT_FALSE(vertical_scrollbar->TrackNeedsRepaint());
  EXPECT_TRUE(vertical_scrollbar->ThumbNeedsRepaint());
  layer_for_vertical_scrollbar->ResetUpdateRectForTesting();
  vertical_scrollbar->ClearThumbNeedsRepaint();

  // Next we'll scroll in both, but the thumb position moving requires no
  // invalidations. Nonetheless the GraphicsLayer should be invalidated,
  // because we still need to update the underlying layer (though no
  // rasterization will be required).
  EXPECT_CALL(theme, PartsToInvalidateOnThumbPositionChange(_, _, _))
      .Times(2)
      .WillRepeatedly(Return(kNoPart));
  scrollable_area->SetScrollOffset(ScrollOffset(70, 70), kProgrammaticScroll);
  EXPECT_FALSE(layer_for_horizontal_scrollbar->update_rect().IsEmpty());
  EXPECT_FALSE(layer_for_vertical_scrollbar->update_rect().IsEmpty());
  EXPECT_FALSE(horizontal_scrollbar->TrackNeedsRepaint());
  EXPECT_FALSE(horizontal_scrollbar->ThumbNeedsRepaint());
  EXPECT_FALSE(vertical_scrollbar->TrackNeedsRepaint());
  EXPECT_FALSE(vertical_scrollbar->ThumbNeedsRepaint());

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::Current()->CollectAllGarbageForTesting();
}

TEST_F(ScrollableAreaTest, RecalculatesScrollbarOverlayIfBackgroundChanges) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));

  EXPECT_EQ(kScrollbarOverlayColorThemeDark,
            scrollable_area->GetScrollbarOverlayColorTheme());
  scrollable_area->RecalculateScrollbarOverlayColorTheme(Color(34, 85, 51));
  EXPECT_EQ(kScrollbarOverlayColorThemeLight,
            scrollable_area->GetScrollbarOverlayColorTheme());
  scrollable_area->RecalculateScrollbarOverlayColorTheme(Color(236, 143, 185));
  EXPECT_EQ(kScrollbarOverlayColorThemeDark,
            scrollable_area->GetScrollbarOverlayColorTheme());
}

TEST_F(ScrollableAreaTest, ScrollableAreaDidScroll) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(100, 100));
  scrollable_area->DidScroll(FloatPoint(40, 51));

  EXPECT_EQ(40, scrollable_area->ScrollOffsetInt().Width());
  EXPECT_EQ(51, scrollable_area->ScrollOffsetInt().Height());
}

// Scrollbars in popups shouldn't fade out since they aren't composited and thus
// they don't appear on hover so users without a wheel can't scroll if they fade
// out.
TEST_F(ScrollableAreaTest, PopupOverlayScrollbarShouldNotFadeOut) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  ScopedMockOverlayScrollbars mock_overlay_scrollbars;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  scrollable_area->SetIsPopup();

  ScrollbarThemeOverlayMock& theme =
      (ScrollbarThemeOverlayMock&)scrollable_area->GetPageScrollbarTheme();
  theme.SetOverlayScrollbarFadeOutDelay(base::TimeDelta::FromSeconds(1));
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      scrollable_area, kHorizontalScrollbar, kRegularScrollbar, &theme);

  DCHECK(scrollbar->IsOverlayScrollbar());
  DCHECK(scrollbar->Enabled());

  scrollable_area->ShowOverlayScrollbars();

  // No fade out animation should be posted.
  EXPECT_FALSE(scrollable_area->fade_overlay_scrollbars_timer_);

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::Current()->CollectAllGarbageForTesting();
}

TEST_F(ScrollableAreaTest, ScrollAnimatorCallbackFiresOnAnimationCancel) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  EXPECT_CALL(*scrollable_area, ScheduleAnimation())
      .WillRepeatedly(Return(true));
  bool finished = false;
  scrollable_area->SetScrollOffset(
      ScrollOffset(0, 10000), kProgrammaticScroll, kScrollBehaviorSmooth,
      ScrollableArea::ScrollCallback(
          base::BindOnce([](bool* finished) { *finished = true; }, &finished)));
  EXPECT_EQ(0.0, scrollable_area->GetScrollAnimator().CurrentOffset().Height());
  EXPECT_FALSE(finished);
  scrollable_area->CancelProgrammaticScrollAnimation();
  EXPECT_EQ(0.0, scrollable_area->GetScrollAnimator().CurrentOffset().Height());
  EXPECT_TRUE(finished);
}

TEST_F(ScrollableAreaTest, ScrollAnimatorCallbackFiresOnInstantScroll) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  EXPECT_CALL(*scrollable_area, ScheduleAnimation())
      .WillRepeatedly(Return(true));
  bool finished = false;
  scrollable_area->SetScrollOffset(
      ScrollOffset(0, 10000), kProgrammaticScroll, kScrollBehaviorInstant,
      ScrollableArea::ScrollCallback(
          base::BindOnce([](bool* finished) { *finished = true; }, &finished)));
  EXPECT_EQ(100, scrollable_area->GetScrollAnimator().CurrentOffset().Height());
  EXPECT_TRUE(finished);
}

TEST_F(ScrollableAreaTest, ScrollAnimatorCallbackFiresOnAnimationFinish) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  EXPECT_CALL(*scrollable_area, ScheduleAnimation())
      .WillRepeatedly(Return(true));
  bool finished = false;
  scrollable_area->SetScrollOffset(
      ScrollOffset(0, 9), kProgrammaticScroll, kScrollBehaviorSmooth,
      ScrollableArea::ScrollCallback(
          base::BindOnce([](bool* finished) { *finished = true; }, &finished)));
  EXPECT_EQ(0.0, scrollable_area->GetScrollAnimator().CurrentOffset().Height());
  EXPECT_FALSE(finished);
  scrollable_area->UpdateCompositorScrollAnimations();
  scrollable_area->ServiceScrollAnimations(1);
  EXPECT_EQ(0.0, scrollable_area->GetScrollAnimator().CurrentOffset().Height());
  EXPECT_FALSE(finished);
  scrollable_area->ServiceScrollAnimations(1000000);
  EXPECT_EQ(9.0, scrollable_area->GetScrollAnimator().CurrentOffset().Height());
  EXPECT_TRUE(finished);
}

}  // namespace blink
