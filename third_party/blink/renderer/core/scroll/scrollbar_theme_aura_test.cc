// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_aura.h"

#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_test_suite.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {

using testing::Return;

namespace {

class ScrollbarThemeAuraButtonOverride final : public ScrollbarThemeAura {
 public:
  ScrollbarThemeAuraButtonOverride() : has_scrollbar_buttons_(true) {}

  void SetHasScrollbarButtons(bool value) { has_scrollbar_buttons_ = value; }

  bool HasScrollbarButtons(ScrollbarOrientation unused) const override {
    return has_scrollbar_buttons_;
  }

  int MinimumThumbLength(const Scrollbar& scrollbar) const override {
    return ScrollbarThickness(scrollbar.ScaleFromDIP(),
                              scrollbar.CSSScrollbarWidth());
  }

  using ScrollbarThemeAura::NinePatchTrackAndButtonsAperture;
  using ScrollbarThemeAura::NinePatchTrackAndButtonsCanvasSize;
  using ScrollbarThemeAura::UsesNinePatchTrackAndButtonsResource;

 private:
  bool has_scrollbar_buttons_;
};

}  // namespace

class ScrollbarThemeAuraTest : public ::testing::TestWithParam<float> {
 protected:
  MockScrollableArea* CreateMockScrollableArea() {
    MockScrollableArea* scrollable_area =
        MockScrollableArea::Create(ScrollOffset(0, 1000));
    scrollable_area->SetScaleFromDIP(GetParam());
    return scrollable_area;
  }

  void TestSetFrameRect(Scrollbar& scrollbar,
                        const gfx::Rect& rect,
                        bool thumb_expectation,
                        bool track_and_buttons_expectation) {
    scrollbar.SetFrameRect(rect);
    EXPECT_EQ(scrollbar.TrackAndButtonsNeedRepaint(),
              track_and_buttons_expectation);
    EXPECT_EQ(scrollbar.ThumbNeedsRepaint(), thumb_expectation);
    scrollbar.ClearTrackAndButtonsNeedRepaint();
    scrollbar.ClearThumbNeedsRepaint();
  }

  void TestSetProportion(Scrollbar& scrollbar,
                         int proportion,
                         bool thumb_expectation,
                         bool track_and_buttons_expectation) {
    scrollbar.SetProportion(proportion, proportion);
    EXPECT_EQ(scrollbar.TrackAndButtonsNeedRepaint(),
              track_and_buttons_expectation);
    EXPECT_EQ(scrollbar.ThumbNeedsRepaint(), thumb_expectation);
    scrollbar.ClearTrackAndButtonsNeedRepaint();
    scrollbar.ClearThumbNeedsRepaint();
  }

  test::TaskEnvironment task_environment_;
};

// Note that this helper only sends mouse events that are already handled on the
// compositor thread, to the scrollbar (i.e they will have the event modifier
// "kScrollbarManipulationHandledOnCompositorThread" set). The point of this
// exercise is to validate that the scrollbar parts invalidate as expected
// (since we still rely on the main thread for invalidation).
void SendEvent(Scrollbar* scrollbar,
               blink::WebInputEvent::Type type,
               gfx::PointF point) {
  const blink::WebMouseEvent web_mouse_event(
      type, point, point, blink::WebPointerProperties::Button::kLeft, 0,
      blink::WebInputEvent::kScrollbarManipulationHandledOnCompositorThread,
      base::TimeTicks::Now());
  switch (type) {
    case blink::WebInputEvent::Type::kMouseDown:
      scrollbar->MouseDown(web_mouse_event);
      break;
    case blink::WebInputEvent::Type::kMouseMove:
      scrollbar->MouseMoved(web_mouse_event);
      break;
    case blink::WebInputEvent::Type::kMouseUp:
      scrollbar->MouseUp(web_mouse_event);
      break;
    default:
      // The rest are unhandled. Let the called know that this helper has not
      // yet implemented them.
      NOTIMPLEMENTED();
  }
}

TEST_P(ScrollbarThemeAuraTest, ButtonSizeHorizontal) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* mock_scrollable_area = CreateMockScrollableArea();
  ScrollbarThemeAuraButtonOverride theme;
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area, kHorizontalScrollbar, &theme);

  gfx::Rect scrollbar_size_normal_dimensions(11, 22, 444, 66);
  scrollbar->SetFrameRect(scrollbar_size_normal_dimensions);
  gfx::Size size1 = theme.ButtonSize(*scrollbar);
  EXPECT_EQ(66, size1.width());
  EXPECT_EQ(66, size1.height());

  gfx::Rect scrollbar_size_squashed_dimensions(11, 22, 444, 666);
  scrollbar->SetFrameRect(scrollbar_size_squashed_dimensions);
  gfx::Size size2 = theme.ButtonSize(*scrollbar);
  EXPECT_EQ(222, size2.width());
  EXPECT_EQ(666, size2.height());

  ThreadState::Current()->CollectAllGarbageForTesting();
}

TEST_P(ScrollbarThemeAuraTest, ButtonSizeVertical) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* mock_scrollable_area = CreateMockScrollableArea();
  ScrollbarThemeAuraButtonOverride theme;
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area, kVerticalScrollbar, &theme);

  gfx::Rect scrollbar_size_normal_dimensions(11, 22, 44, 666);
  scrollbar->SetFrameRect(scrollbar_size_normal_dimensions);
  gfx::Size size1 = theme.ButtonSize(*scrollbar);
  EXPECT_EQ(44, size1.width());
  EXPECT_EQ(44, size1.height());

  gfx::Rect scrollbar_size_squashed_dimensions(11, 22, 444, 666);
  scrollbar->SetFrameRect(scrollbar_size_squashed_dimensions);
  gfx::Size size2 = theme.ButtonSize(*scrollbar);
  EXPECT_EQ(444, size2.width());
  EXPECT_EQ(333, size2.height());

  ThreadState::Current()->CollectAllGarbageForTesting();
}

TEST_P(ScrollbarThemeAuraTest, NoButtonsReturnsSize0) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* mock_scrollable_area = CreateMockScrollableArea();
  ScrollbarThemeAuraButtonOverride theme;
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area, kVerticalScrollbar, &theme);
  theme.SetHasScrollbarButtons(false);

  scrollbar->SetFrameRect(gfx::Rect(1, 2, 3, 4));
  gfx::Size size = theme.ButtonSize(*scrollbar);
  EXPECT_EQ(0, size.width());
  EXPECT_EQ(0, size.height());

  ThreadState::Current()->CollectAllGarbageForTesting();
}

TEST_P(ScrollbarThemeAuraTest, ScrollbarPartsInvalidationTest) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  MockScrollableArea* mock_scrollable_area = CreateMockScrollableArea();
  ScrollbarThemeAuraButtonOverride theme;
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area, kVerticalScrollbar, &theme);
  ON_CALL(*mock_scrollable_area, VerticalScrollbar())
      .WillByDefault(Return(scrollbar));

  gfx::Rect vertical_rect(1010, 0, 14, 768);
  scrollbar->SetFrameRect(vertical_rect);
  scrollbar->ClearThumbNeedsRepaint();
  scrollbar->ClearTrackAndButtonsNeedRepaint();

  // Tests that mousedown on the thumb causes an invalidation.
  SendEvent(scrollbar, blink::WebInputEvent::Type::kMouseMove,
            gfx::PointF(10, 20));
  SendEvent(scrollbar, blink::WebInputEvent::Type::kMouseDown,
            gfx::PointF(10, 20));
  EXPECT_TRUE(scrollbar->ThumbNeedsRepaint());

  // Tests that mouseup on the thumb causes an invalidation.
  scrollbar->ClearThumbNeedsRepaint();
  SendEvent(scrollbar, blink::WebInputEvent::Type::kMouseUp,
            gfx::PointF(10, 20));
  EXPECT_TRUE(scrollbar->ThumbNeedsRepaint());

  // Note that, since these tests run with the assumption that the compositor
  // thread has already handled scrolling, a "scroll" will be simulated by
  // calling SetScrollOffset. To check if the arrow was invalidated,
  // TrackAndButtonsNeedRepaint needs to be used. The following verifies that
  // when the offset changes from 0 to a value > 0, an invalidation gets
  // triggered. At (0, 0) there is no upwards scroll available, so the arrow is
  // disabled. When we change the offset, it must be repainted to show available
  // scroll extent.
  EXPECT_FALSE(scrollbar->TrackAndButtonsNeedRepaint());
  mock_scrollable_area->SetScrollOffset(ScrollOffset(0, 10),
                                        mojom::blink::ScrollType::kCompositor);
  EXPECT_TRUE(scrollbar->TrackAndButtonsNeedRepaint());

  // Tests that when the scroll offset changes from a value greater than 0 to a
  // value less than the max scroll offset, a track-and-buttons invalidation is
  // *not* triggered.
  scrollbar->ClearTrackAndButtonsNeedRepaint();
  mock_scrollable_area->SetScrollOffset(ScrollOffset(0, 20),
                                        mojom::blink::ScrollType::kCompositor);
  EXPECT_FALSE(scrollbar->TrackAndButtonsNeedRepaint());

  // Tests that when the scroll offset changes to 0, a track-and-buttons
  // invalidation gets triggered (for the arrow).
  scrollbar->ClearTrackAndButtonsNeedRepaint();
  mock_scrollable_area->SetScrollOffset(ScrollOffset(0, 0),
                                        mojom::blink::ScrollType::kCompositor);
  EXPECT_TRUE(scrollbar->TrackAndButtonsNeedRepaint());

  // Tests that mousedown on the arrow causes an invalidation.
  scrollbar->ClearTrackAndButtonsNeedRepaint();
  SendEvent(scrollbar, blink::WebInputEvent::Type::kMouseMove,
            gfx::PointF(10, 760));
  SendEvent(scrollbar, blink::WebInputEvent::Type::kMouseDown,
            gfx::PointF(10, 760));
  EXPECT_TRUE(scrollbar->TrackAndButtonsNeedRepaint());

  // Tests that mouseup on the arrow causes an invalidation.
  scrollbar->ClearTrackAndButtonsNeedRepaint();
  SendEvent(scrollbar, blink::WebInputEvent::Type::kMouseUp,
            gfx::PointF(10, 760));
  EXPECT_TRUE(scrollbar->TrackAndButtonsNeedRepaint());

  ThreadState::Current()->CollectAllGarbageForTesting();
}

// Verify that the NinePatchCanvas function returns the correct minimal image
// size when the scrollbars are larger and smaller than the minimal size (enough
// space for two buttons and a pixel in the middle).
TEST_P(ScrollbarThemeAuraTest, NinePatchCanvas) {
  if (!RuntimeEnabledFeatures::AuraScrollbarUsesNinePatchTrackEnabled()) {
    GTEST_SKIP();
  }

  ScrollbarThemeAuraButtonOverride theme;
  ASSERT_TRUE(theme.UsesNinePatchTrackAndButtonsResource());
  MockScrollableArea* mock_scrollable_area = CreateMockScrollableArea();
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area, kVerticalScrollbar, &theme);

  // Test that a scrollbar larger than the minimal size is properly shrunk
  // when asked for the aperture.
  scrollbar->SetFrameRect(
      gfx::Rect(0, 0, scrollbar->Width(), scrollbar->Width() * 3));
  gfx::Size expected_size(scrollbar->Width(), scrollbar->Width() * 2 + 1);
  EXPECT_EQ(expected_size,
            theme.NinePatchTrackAndButtonsCanvasSize(*scrollbar));

  // Test that a scrollbar smaller than the minimal size is not shrunk when
  // asked for the aperture.
  scrollbar->SetFrameRect(
      gfx::Rect(0, 0, scrollbar->Width(), scrollbar->Width() / 3));
  expected_size = scrollbar->Size();
  EXPECT_EQ(expected_size,
            theme.NinePatchTrackAndButtonsCanvasSize(*scrollbar));
}

// Verify that the NinePatchAperture function returns the correct point in the
// middle of the canvas taking into consideration when the scrollbars' width is
// even to expand the width of the center-patch.
TEST_P(ScrollbarThemeAuraTest, NinePatchAperture) {
  if (!RuntimeEnabledFeatures::AuraScrollbarUsesNinePatchTrackEnabled()) {
    GTEST_SKIP();
  }

  ScrollbarThemeAuraButtonOverride theme;
  ASSERT_TRUE(theme.UsesNinePatchTrackAndButtonsResource());
  MockScrollableArea* mock_scrollable_area = CreateMockScrollableArea();
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area, kVerticalScrollbar, &theme);
  scrollbar->SetFrameRect(
      gfx::Rect(0, 0, scrollbar->Width(), scrollbar->Width() * 3));
  const gfx::Size canvas = theme.NinePatchTrackAndButtonsCanvasSize(*scrollbar);
  gfx::Rect expected_rect(canvas.width() / 2, canvas.height() / 2, 1, 1);
  if (canvas.width() % 2 == 0) {
    expected_rect.set_x(expected_rect.x() - 1);
    expected_rect.set_width(2);
  }
  EXPECT_EQ(expected_rect, theme.NinePatchTrackAndButtonsAperture(*scrollbar));
}

// Verifies that resizing the scrollbar doesn't generate unnecessary paint
// invalidations when the scrollbar uses nine-patch track and buttons
// resources.
TEST_P(ScrollbarThemeAuraTest, TestPaintInvalidationsWhenNinePatchScaled) {
  if (!RuntimeEnabledFeatures::AuraScrollbarUsesNinePatchTrackEnabled()) {
    GTEST_SKIP();
  }

  ScrollbarThemeAuraButtonOverride theme;
  ASSERT_TRUE(theme.UsesNinePatchTrackAndButtonsResource());
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      CreateMockScrollableArea(), kVerticalScrollbar, &theme);
  // Start the test with a scrollbar larger than the canvas size and clean
  // flags.
  scrollbar->SetFrameRect(
      gfx::Rect(0, 0, scrollbar->Width(), scrollbar->Width() * 5));
  scrollbar->ClearTrackAndButtonsNeedRepaint();
  scrollbar->ClearThumbNeedsRepaint();

  // Test that resizing the scrollbar's length while larger than the canvas
  // doesn't trigger a repaint.
  TestSetFrameRect(
      *scrollbar, gfx::Rect(0, 0, scrollbar->Width(), scrollbar->Width() * 4),
      /*thumb_expectation=*/false, /*track_and_buttons_expectation=*/false);
  TestSetProportion(*scrollbar, scrollbar->Width() * 4,
                    /*thumb_expectation=*/true,
                    /*track_and_buttons_expectation=*/false);

  // Test that changing the width the scrollbar triggers a repaint.
  TestSetFrameRect(
      *scrollbar, gfx::Rect(0, 0, scrollbar->Width() / 2, scrollbar->Height()),
      /*thumb_expectation=*/true, /*track_and_buttons_expectation=*/true);
  // Set width back to normal (thickening).
  TestSetFrameRect(
      *scrollbar, gfx::Rect(0, 0, scrollbar->Width() * 2, scrollbar->Height()),
      /*thumb_expectation=*/true, /*track_and_buttons_expectation=*/true);

  // Test that making the track/buttons smaller than the canvas size triggers a
  // repaint.
  TestSetFrameRect(
      *scrollbar, gfx::Rect(0, 0, scrollbar->Width(), scrollbar->Width() / 2),
      /*thumb_expectation=*/true, /*track_and_buttons_expectation=*/true);
  TestSetProportion(*scrollbar, scrollbar->Width() / 2,
                    /*thumb_expectation=*/true,
                    /*track_and_buttons_expectation=*/true);

  // Test that no paint invalidation is triggered when the dimensions stay the
  // same.
  TestSetFrameRect(*scrollbar, scrollbar->FrameRect(),
                   /*thumb_expectation=*/false,
                   /*track_and_buttons_expectation=*/false);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ScrollbarThemeAuraTest,
                         ::testing::Values(1.f, 1.25f, 1.5f, 1.75f, 2.f));

}  // namespace blink
