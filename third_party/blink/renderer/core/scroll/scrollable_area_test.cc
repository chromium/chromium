// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_test_suite.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mock.h"
#include "third_party/blink/renderer/core/style/scroll_start_data.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

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
                           public PaintTestConfigurations {
 private:
  test::TaskEnvironment task_environment_;
};

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

TEST_P(ScrollableAreaTest, ScrollbarBackgroundAndThumbRepaint) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  ScrollbarThemeWithMockInvalidation theme;
  MockScrollableArea* scrollable_area =
      MockScrollableArea::Create(ScrollOffset(0, 100));
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      scrollable_area, kHorizontalScrollbar, &theme);

  EXPECT_CALL(theme, ShouldRepaintAllPartsOnInvalidation())
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(scrollbar->TrackAndButtonsNeedRepaint());
  EXPECT_TRUE(scrollbar->ThumbNeedsRepaint());
  scrollbar->SetNeedsPaintInvalidation(kNoPart);
  EXPECT_TRUE(scrollbar->TrackAndButtonsNeedRepaint());
  EXPECT_TRUE(scrollbar->ThumbNeedsRepaint());

  scrollbar->ClearTrackAndButtonsNeedRepaint();
  scrollbar->ClearThumbNeedsRepaint();
  EXPECT_FALSE(scrollbar->TrackAndButtonsNeedRepaint());
  EXPECT_FALSE(scrollbar->ThumbNeedsRepaint());
  scrollbar->SetNeedsPaintInvalidation(kThumbPart);
  EXPECT_TRUE(scrollbar->TrackAndButtonsNeedRepaint());
  EXPECT_TRUE(scrollbar->ThumbNeedsRepaint());

  // When not all parts are repainted on invalidation,
  // setNeedsPaintInvalidation sets repaint bits only on the requested parts.
  EXPECT_CALL(theme, ShouldRepaintAllPartsOnInvalidation())
      .WillRepeatedly(Return(false));
  scrollbar->ClearTrackAndButtonsNeedRepaint();
  scrollbar->ClearThumbNeedsRepaint();
  EXPECT_FALSE(scrollbar->TrackAndButtonsNeedRepaint());
  EXPECT_FALSE(scrollbar->ThumbNeedsRepaint());
  scrollbar->SetNeedsPaintInvalidation(kThumbPart);
  EXPECT_FALSE(scrollbar->TrackAndButtonsNeedRepaint());
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
      ScrollableArea::ScrollCallback(WTF::BindOnce(
          [](bool* finished, ScrollableArea::ScrollCompletionMode) {
            *finished = true;
          },
          WTF::Unretained(&finished))));
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
      ScrollableArea::ScrollCallback(WTF::BindOnce(
          [](bool* finished, ScrollableArea::ScrollCompletionMode) {
            *finished = true;
          },
          WTF::Unretained(&finished))));
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
      ScrollableArea::ScrollCallback(WTF::BindOnce(
          [](bool* finished, ScrollableArea::ScrollCompletionMode) {
            *finished = true;
          },
          WTF::Unretained(&finished))));
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
      ScrollableArea::ScrollCallback(WTF::BindOnce(
          [](bool* finished, ScrollableArea::ScrollCompletionMode) {
            *finished = true;
          },
          WTF::Unretained(&finished))));
  scrollable_area->SetScrollOffset(ScrollOffset(0, 0),
                                   mojom::blink::ScrollType::kProgrammatic,
                                   mojom::blink::ScrollBehavior::kSmooth);
  scrollable_area->UpdateCompositorScrollAnimations();
  scrollable_area->ServiceScrollAnimations(1);
  scrollable_area->ServiceScrollAnimations(1000000);
  EXPECT_EQ(0, scrollable_area->GetScrollOffset().y());
  EXPECT_TRUE(finished);
}

void VerifyOffsetFromScrollStart(ScrollableArea* scrollable_area,
                                 ScrollStartValueType y_type,
                                 ScrollStartValueType x_type,
                                 const Length& y_length,
                                 const Length& x_length,
                                 const ScrollOffset& offset) {
  switch (y_type) {
    case blink::ScrollStartValueType::kAuto:
    case blink::ScrollStartValueType::kStart:
    case blink::ScrollStartValueType::kTop:
    case blink::ScrollStartValueType::kLeft: {
      EXPECT_EQ(offset.y(), scrollable_area->MinimumScrollOffset().y());
      break;
    }
    case blink::ScrollStartValueType::kCenter: {
      EXPECT_EQ(offset.y(),
                scrollable_area->MinimumScrollOffset().y() +
                    0.5 * scrollable_area->ScrollSize(kVerticalScrollbar));
      break;
    }
    case blink::ScrollStartValueType::kEnd:
    case blink::ScrollStartValueType::kBottom: {
      EXPECT_EQ(offset.y(), scrollable_area->MaximumScrollOffset().y());
      break;
    }
    case blink::ScrollStartValueType::kRight: {
      EXPECT_EQ(offset.y(), scrollable_area->MinimumScrollOffset().y());
      break;
    }
    case blink::ScrollStartValueType::kLengthOrPercentage: {
      float expected_offset =
          scrollable_area->MinimumScrollOffset().y() +
          FloatValueForLength(y_length,
                              scrollable_area->ScrollSize(kVerticalScrollbar));
      EXPECT_EQ(offset.y(), expected_offset);
      break;
    }
  }

  switch (x_type) {
    case blink::ScrollStartValueType::kAuto:
    case blink::ScrollStartValueType::kStart:
    case blink::ScrollStartValueType::kTop:
    case blink::ScrollStartValueType::kLeft: {
      EXPECT_EQ(offset.x(), scrollable_area->MinimumScrollOffset().x());
      break;
    }
    case blink::ScrollStartValueType::kCenter: {
      EXPECT_EQ(offset.x(),
                scrollable_area->MinimumScrollOffset().x() +
                    0.5 * scrollable_area->ScrollSize(kHorizontalScrollbar));
      break;
    }
    case blink::ScrollStartValueType::kEnd:
    case blink::ScrollStartValueType::kRight: {
      EXPECT_EQ(offset.x(), scrollable_area->MaximumScrollOffset().x());
      break;
    }
    case blink::ScrollStartValueType::kBottom: {
      EXPECT_EQ(offset.x(), scrollable_area->MinimumScrollOffset().x());
      break;
    }
    case blink::ScrollStartValueType::kLengthOrPercentage: {
      float expected_offset =
          scrollable_area->MinimumScrollOffset().x() +
          FloatValueForLength(
              x_length, scrollable_area->ScrollSize(kHorizontalScrollbar));
      EXPECT_EQ(offset.x(), expected_offset);
      break;
    }
  }
}

void test_scroll_start_combination(ScrollableArea* scrollable_area,
                                   ScrollStartValueType y_type,
                                   ScrollStartValueType x_type,
                                   const Length& y_length,
                                   const Length& x_length) {
  ScrollStartData y_data;
  ScrollStartData x_data;

  y_data.value_type = y_type;
  y_data.value = y_length;
  x_data.value_type = x_type;
  x_data.value = x_length;

  ScrollOffset offset =
      scrollable_area->ScrollOffsetFromScrollStartData(y_data, x_data);
  VerifyOffsetFromScrollStart(scrollable_area, y_type, x_type, y_length,
                              x_length, offset);
}

TEST_P(ScrollableAreaTest, ScrollOffsetFromScrollStartDataAllCombinations) {
  const Vector<ScrollStartValueType> scroll_start_values = {
      ScrollStartValueType::kAuto,   ScrollStartValueType::kLengthOrPercentage,
      ScrollStartValueType::kStart,  ScrollStartValueType::kCenter,
      ScrollStartValueType::kEnd,    ScrollStartValueType::kTop,
      ScrollStartValueType::kBottom, ScrollStartValueType::kLeft,
      ScrollStartValueType::kRight};
  const int max_horizontal_scroll_offset = 500;
  const int max_vertical_scroll_offset = 500;
  MockScrollableArea* scrollable_area = MockScrollableArea::Create(
      ScrollOffset(max_horizontal_scroll_offset, max_vertical_scroll_offset));
  ON_CALL(*scrollable_area, ScrollSize(kHorizontalScrollbar))
      .WillByDefault(Return(max_horizontal_scroll_offset));
  ON_CALL(*scrollable_area, ScrollSize(kVerticalScrollbar))
      .WillByDefault(Return(max_vertical_scroll_offset));

  for (auto y_type : scroll_start_values) {
    Length y_length = y_type == ScrollStartValueType::kLengthOrPercentage
                          ? Length(100, Length::Type::kFixed)
                          : Length();
    for (auto x_type : scroll_start_values) {
      Length x_length = x_type == ScrollStartValueType::kLengthOrPercentage
                            ? Length(100, Length::Type::kFixed)
                            : Length();
      test_scroll_start_combination(scrollable_area, y_type, x_type, y_length,
                                    x_length);
    }
  }
}

TEST_P(ScrollableAreaTest, ScrollOffsetFromScrollStartDataNonZeroMin) {
  const int max_horizontal_scroll_offset = 500;
  const int min_horizontal_scroll_offset = -10;
  const int max_vertical_scroll_offset = 500;
  const int min_vertical_scroll_offset = -10;
  const int horizontal_scroll_size =
      max_horizontal_scroll_offset - min_horizontal_scroll_offset;
  const int vertical_scroll_size =
      max_vertical_scroll_offset - min_vertical_scroll_offset;
  MockScrollableArea* scrollable_area = MockScrollableArea::Create(
      ScrollOffset(max_horizontal_scroll_offset, max_vertical_scroll_offset),
      ScrollOffset(min_horizontal_scroll_offset, min_vertical_scroll_offset));
  ScrollOffset offset;
  ScrollStartData y_data;
  ScrollStartData x_data;

  ON_CALL(*scrollable_area, ScrollSize(kHorizontalScrollbar))
      .WillByDefault(Return(horizontal_scroll_size));
  ON_CALL(*scrollable_area, ScrollSize(kVerticalScrollbar))
      .WillByDefault(Return(vertical_scroll_size));

  // Test that scroll-start greater than max scroll offset is clamped to max.
  y_data.value = Length(600, Length::Type::kFixed);
  y_data.value_type = ScrollStartValueType::kLengthOrPercentage;
  x_data.value = Length(600, Length::Type::kFixed);
  x_data.value_type = ScrollStartValueType::kLengthOrPercentage;
  offset = scrollable_area->ScrollOffsetFromScrollStartData(y_data, x_data);
  EXPECT_EQ(offset.y(), max_vertical_scroll_offset);
  EXPECT_EQ(offset.x(), max_horizontal_scroll_offset);

  // Test that scroll-start less than min scroll offset is clamped to min
  y_data.value = Length(0, Length::Type::kFixed);
  y_data.value_type = ScrollStartValueType::kLengthOrPercentage;
  x_data.value = Length(0, Length::Type::kFixed);
  x_data.value_type = ScrollStartValueType::kLengthOrPercentage;
  offset = scrollable_area->ScrollOffsetFromScrollStartData(y_data, x_data);
  EXPECT_EQ(offset.y(), min_vertical_scroll_offset);
  EXPECT_EQ(offset.x(), min_horizontal_scroll_offset);

  // Test that scroll-start: <percentage> is relative to ScrollSize().
  y_data.value = Length(50, Length::Type::kPercent);
  y_data.value_type = ScrollStartValueType::kLengthOrPercentage;
  x_data.value = Length(50, Length::Type::kPercent);
  x_data.value_type = ScrollStartValueType::kLengthOrPercentage;
  offset = scrollable_area->ScrollOffsetFromScrollStartData(y_data, x_data);
  EXPECT_EQ(offset.y(), scrollable_area->MinimumScrollOffset().y() +
                            0.5 * vertical_scroll_size);
  EXPECT_EQ(offset.x(), scrollable_area->MinimumScrollOffset().x() +
                            0.5 * horizontal_scroll_size);

  // Test that scroll-start: end scrolls to MaximumScrollOffset.
  y_data.value_type = ScrollStartValueType::kEnd;
  x_data.value_type = ScrollStartValueType::kEnd;
  offset = scrollable_area->ScrollOffsetFromScrollStartData(y_data, x_data);
  EXPECT_EQ(offset.y(), max_vertical_scroll_offset);
  EXPECT_EQ(offset.x(), max_horizontal_scroll_offset);
}

TEST_P(ScrollableAreaTest, FilterIncomingScrollDuringSmoothUserScroll) {
  using mojom::blink::ScrollType;
  MockScrollableArea* area =
      MockScrollableArea::Create(ScrollOffset(100, 100), ScrollOffset(0, 0));
  area->set_active_smooth_scroll_type_for_testing(ScrollType::kUser);
  const std::vector<mojom::blink::ScrollType> scroll_types = {
      ScrollType::kUser,       ScrollType::kProgrammatic,
      ScrollType::kClamping,   ScrollType::kCompositor,
      ScrollType::kAnchoring,  ScrollType::kSequenced,
      ScrollType::kScrollStart};

  // ScrollTypes which we do not filter even if there is an active
  // kUser smooth scroll.
  std::set<mojom::blink::ScrollType> exempted_types = {
      ScrollType::kUser,
      ScrollType::kCompositor,
      ScrollType::kClamping,
      ScrollType::kAnchoring,
  };

  for (const auto& incoming_type : scroll_types) {
    const bool should_filter = !exempted_types.contains(incoming_type);
    EXPECT_EQ(area->ShouldFilterIncomingScroll(incoming_type), should_filter);
  }
}

}  // namespace blink
