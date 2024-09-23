// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_fluent.h"

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_test_suite.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/native_theme/native_theme_features.h"

namespace blink {

using ::testing::Return;

namespace {

// Const values for unit tests.
constexpr int kScrollbarLength = 200;
constexpr int kOffsetFromViewport = 100;

}  // namespace

class ScrollbarThemeFluentMock : public ScrollbarThemeFluent {
 public:
  int ButtonLength(const Scrollbar& scrollbar) const {
    gfx::Size size = ButtonSize(scrollbar);
    return scrollbar.Orientation() == kVerticalScrollbar ? size.height()
                                                         : size.width();
  }

  // Margin between the thumb and the edge of the scrollbars.
  int ThumbOffset(float scale_from_dip) const {
    int scrollbar_thumb_offset =
        (scrollbar_track_thickness() - scrollbar_thumb_thickness()) / 2;
    return base::ClampRound(scrollbar_thumb_offset * scale_from_dip);
  }

  int scrollbar_thumb_thickness() const { return scrollbar_thumb_thickness_; }
  int scrollbar_track_thickness() const { return scrollbar_track_thickness_; }
  int scrollbar_track_inset() const { return scrollbar_track_inset_; }

  using ScrollbarThemeFluent::ButtonSize;
  using ScrollbarThemeFluent::InsetButtonRect;
  using ScrollbarThemeFluent::InsetTrackRect;
  using ScrollbarThemeFluent::ScrollbarTrackInsetPx;
  using ScrollbarThemeFluent::ThumbRect;
  using ScrollbarThemeFluent::ThumbThickness;
  using ScrollbarThemeFluent::TrackRect;
};

class ScrollbarThemeFluentTest : public ::testing::TestWithParam<float> {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(::features::kFluentScrollbar);
    ScrollbarThemeSettings::SetFluentScrollbarsEnabled(true);
    mock_scrollable_area_ = MakeGarbageCollected<MockScrollableArea>(
        /*maximum_scroll_offset=*/ScrollOffset(0, 1000));
    mock_scrollable_area_->SetScaleFromDIP(GetParam());
    // ScrollbarThemeFluent Needs to be instantiated after feature flag and
    // scrollbar settings have been set.
    theme_ = std::make_unique<ScrollbarThemeFluentMock>();
  }

  void TearDown() override { theme_.reset(); }

  int TrackLength(const Scrollbar& scrollbar) const {
    return kScrollbarLength - 2 * theme_->ButtonLength(scrollbar);
  }

  int ScrollbarThickness() const {
    return theme_->ScrollbarThickness(ScaleFromDIP(), EScrollbarWidth::kAuto);
  }
  int ThumbThickness() const {
    return theme_->ThumbThickness(ScaleFromDIP(), EScrollbarWidth::kAuto);
  }
  int ThumbOffset() const { return theme_->ThumbOffset(ScaleFromDIP()); }
  int ScrollbarTrackInsetPx() const {
    return theme_->ScrollbarTrackInsetPx(ScaleFromDIP());
  }
  float ScaleFromDIP() const { return GetParam(); }

  Persistent<MockScrollableArea> mock_scrollable_area() const {
    return mock_scrollable_area_;
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<ScrollbarThemeFluentMock> theme_;

 private:
  base::test::ScopedFeatureList feature_list_;
  Persistent<MockScrollableArea> mock_scrollable_area_;
};

class OverlayScrollbarThemeFluentTest : public ScrollbarThemeFluentTest {
 protected:
  void SetUp() override {
    ScrollbarThemeFluentTest::SetUp();
    feature_list_.InitAndEnableFeature(::features::kFluentOverlayScrollbar);
    // Re-instantiate ScrollbarThemeFluent with the overlay scrollbar flag on.
    theme_ = std::make_unique<ScrollbarThemeFluentMock>();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedMockOverlayScrollbars mock_overlay_scrollbar_;
};

// Test that the scrollbar's thickness scales appropriately with the thumb's
// thickness and always maintains proportion with the DIP scale.
TEST_P(ScrollbarThemeFluentTest, ScrollbarThicknessScalesProperly) {
  int scrollbar_thickness = ScrollbarThickness();
  int thumb_thickness = ThumbThickness();
  EXPECT_EQ((scrollbar_thickness - thumb_thickness) % 2, 0);
  EXPECT_EQ(theme_->scrollbar_track_thickness(),
            base::ClampRound(scrollbar_thickness / ScaleFromDIP()));
}

// Test that Scrollbar objects are correctly sized with Fluent theme parts.
TEST_P(ScrollbarThemeFluentTest, VerticalScrollbarPartsSizes) {
  Scrollbar* vertical_scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area(), kVerticalScrollbar, &(theme_->GetInstance()));
  int scrollbar_thickness = ScrollbarThickness();
  vertical_scrollbar->SetFrameRect(
      gfx::Rect(kOffsetFromViewport, 0, scrollbar_thickness, kScrollbarLength));

  // Check that ThumbOffset() calculation is correct.
  EXPECT_EQ(ThumbThickness() + 2 * ThumbOffset(), scrollbar_thickness);

  const gfx::Rect track_rect = theme_->TrackRect(*vertical_scrollbar);
  EXPECT_EQ(
      track_rect,
      gfx::Rect(kOffsetFromViewport, theme_->ButtonLength(*vertical_scrollbar),
                scrollbar_thickness, TrackLength(*vertical_scrollbar)));

  const gfx::Rect thumb_rect = theme_->ThumbRect(*vertical_scrollbar);
  EXPECT_EQ(thumb_rect, gfx::Rect(kOffsetFromViewport + ThumbOffset(),
                                  theme_->ButtonLength(*vertical_scrollbar),
                                  ThumbThickness(),
                                  theme_->ThumbLength(*vertical_scrollbar)));

  const gfx::Size button_size = theme_->ButtonSize(*vertical_scrollbar);
  EXPECT_EQ(button_size, gfx::Size(scrollbar_thickness,
                                   theme_->ButtonLength(*vertical_scrollbar)));
}

// Test that Scrollbar objects are correctly sized with Fluent theme parts.
TEST_P(ScrollbarThemeFluentTest, HorizontalScrollbarPartsSizes) {
  Scrollbar* horizontal_scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area(), kHorizontalScrollbar, &(theme_->GetInstance()));
  int scrollbar_thickness = ScrollbarThickness();
  horizontal_scrollbar->SetFrameRect(
      gfx::Rect(0, kOffsetFromViewport, kScrollbarLength, scrollbar_thickness));

  // Check that ThumbOffset() calculation is correct.
  EXPECT_EQ(ThumbThickness() + 2 * ThumbOffset(), scrollbar_thickness);

  const gfx::Rect track_rect = theme_->TrackRect(*horizontal_scrollbar);
  EXPECT_EQ(track_rect,
            gfx::Rect(theme_->ButtonLength(*horizontal_scrollbar),
                      kOffsetFromViewport, TrackLength(*horizontal_scrollbar),
                      scrollbar_thickness));

  const gfx::Rect thumb_rect = theme_->ThumbRect(*horizontal_scrollbar);
  EXPECT_EQ(thumb_rect, gfx::Rect(theme_->ButtonLength(*horizontal_scrollbar),
                                  kOffsetFromViewport + ThumbOffset(),
                                  theme_->ThumbLength(*horizontal_scrollbar),
                                  ThumbThickness()));

  const gfx::Size button_size = theme_->ButtonSize(*horizontal_scrollbar);
  EXPECT_EQ(button_size, gfx::Size(theme_->ButtonLength(*horizontal_scrollbar),
                                   scrollbar_thickness));
}

// The test verifies that the background paint is not invalidated when
// the thumb position changes. Aura scrollbars change arrow buttons color
// when the scroll offset changes from and to the min/max scroll offset.
// Fluent scrollbars do not change the arrow buttons color in this case.
TEST_P(ScrollbarThemeFluentTest, ScrollbarBackgroundInvalidationTest) {
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area(), kVerticalScrollbar, &(theme_->GetInstance()));
  ON_CALL(*mock_scrollable_area(), VerticalScrollbar())
      .WillByDefault(Return(scrollbar));

  scrollbar->SetFrameRect(
      gfx::Rect(0, 0, ScrollbarThickness(), kScrollbarLength));
  scrollbar->ClearTrackAndButtonsNeedRepaint();

  // Verifies that when the thumb position changes from min offset, the
  // background invalidation is not triggered.
  mock_scrollable_area()->SetScrollOffset(
      ScrollOffset(0, 10), mojom::blink::ScrollType::kCompositor);
  EXPECT_FALSE(scrollbar->TrackAndButtonsNeedRepaint());

  // Verifies that when the thumb position changes from a non-zero offset,
  // the background invalidation is not triggered.
  mock_scrollable_area()->SetScrollOffset(
      ScrollOffset(0, 20), mojom::blink::ScrollType::kCompositor);
  EXPECT_FALSE(scrollbar->TrackAndButtonsNeedRepaint());

  // Verifies that when the thumb position changes back to 0 (min) offset,
  // the background invalidation is not triggered.
  mock_scrollable_area()->SetScrollOffset(
      ScrollOffset(0, 0), mojom::blink::ScrollType::kCompositor);
  EXPECT_FALSE(scrollbar->TrackAndButtonsNeedRepaint());
}

// Test that Scrollbar objects are correctly sized with Overlay Fluent theme
// parts.
TEST_P(OverlayScrollbarThemeFluentTest, OverlaySetsCorrectTrackAndInsetSize) {
  // Some OSes keep fluent scrollbars disabled even if the feature flag is set
  // to enable them.
  if (!ui::IsFluentScrollbarEnabled()) {
    EXPECT_FALSE(theme_->UsesOverlayScrollbars());
    return;
  }

  EXPECT_TRUE(theme_->UsesOverlayScrollbars());
  Scrollbar* horizontal_scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area(), kHorizontalScrollbar, &(theme_->GetInstance()));
  int scrollbar_thickness = ScrollbarThickness();
  horizontal_scrollbar->SetFrameRect(
      gfx::Rect(0, kOffsetFromViewport, kScrollbarLength, scrollbar_thickness));

  // Check that ThumbOffset() calculation is correct.
  EXPECT_EQ(ThumbThickness() + 2 * ThumbOffset(), scrollbar_thickness);

  const gfx::Rect track_rect = theme_->TrackRect(*horizontal_scrollbar);
  EXPECT_EQ(track_rect,
            gfx::Rect(theme_->ButtonLength(*horizontal_scrollbar),
                      kOffsetFromViewport, TrackLength(*horizontal_scrollbar),
                      scrollbar_thickness));
}

// Same as ScrollbarThemeFluentTest.ScrollbarThicknessScalesProperly, but for
// Overlay Scrollbars.
TEST_P(OverlayScrollbarThemeFluentTest, ScrollbarThicknessScalesProperly) {
  int scrollbar_thickness = ScrollbarThickness();
  int thumb_thickness = ThumbThickness();
  EXPECT_EQ((scrollbar_thickness - thumb_thickness) % 2, 0);
  EXPECT_EQ(theme_->scrollbar_track_thickness(),
            base::ClampRound(scrollbar_thickness / ScaleFromDIP()));
}

TEST_P(OverlayScrollbarThemeFluentTest, TestVerticalInsetTrackRect) {
  int scrollbar_thickness = ScrollbarThickness();
  Scrollbar* vertical_scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area(), kVerticalScrollbar, &(theme_->GetInstance()));
  vertical_scrollbar->SetFrameRect(
      gfx::Rect(kOffsetFromViewport, 0, scrollbar_thickness, kScrollbarLength));
  gfx::Rect track_rect(kOffsetFromViewport, 0, scrollbar_thickness,
                       kScrollbarLength);

  // Vertical scrollbars should be inset from the left and right.
  gfx::Rect expected_rect(kOffsetFromViewport + ScrollbarTrackInsetPx(), 0,
                          scrollbar_thickness - 2 * ScrollbarTrackInsetPx(),
                          kScrollbarLength);
  EXPECT_EQ(expected_rect,
            theme_->InsetTrackRect(*vertical_scrollbar, track_rect));
}

TEST_P(OverlayScrollbarThemeFluentTest, TestHorizontalInsetTrackRect) {
  int scrollbar_thickness = ScrollbarThickness();
  Scrollbar* horizontal_scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area(), kHorizontalScrollbar, &(theme_->GetInstance()));
  horizontal_scrollbar->SetFrameRect(
      gfx::Rect(0, kOffsetFromViewport, kScrollbarLength, scrollbar_thickness));
  gfx::Rect track_rect(0, kOffsetFromViewport, kScrollbarLength,
                       scrollbar_thickness);

  // Horizontal scrollbars should be inset from the top and the bottom.
  gfx::Rect expected_rect(0, kOffsetFromViewport + ScrollbarTrackInsetPx(),
                          kScrollbarLength,
                          scrollbar_thickness - 2 * ScrollbarTrackInsetPx());
  EXPECT_EQ(expected_rect,
            theme_->InsetTrackRect(*horizontal_scrollbar, track_rect));
}

TEST_P(OverlayScrollbarThemeFluentTest, TestVerticalInsetButtonRect) {
  int scrollbar_thickness = ScrollbarThickness();
  Scrollbar* vertical_scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area(), kVerticalScrollbar, &(theme_->GetInstance()));
  vertical_scrollbar->SetFrameRect(
      gfx::Rect(kOffsetFromViewport, 0, scrollbar_thickness, kScrollbarLength));
  int inset = ScrollbarTrackInsetPx();
  int button_length = theme_->ButtonLength(*vertical_scrollbar);
  gfx::Rect button_rect(0, 0, scrollbar_thickness, button_length);

  // Up arrow button should be inset from every part except the bottom.
  gfx::Rect expected_up_rect(inset, inset, scrollbar_thickness - inset * 2,
                             button_length - inset);
  EXPECT_EQ(expected_up_rect,
            theme_->InsetButtonRect(*vertical_scrollbar, button_rect,
                                    kBackButtonStartPart));
  // Down arrow button should be inset from every part except the top.
  gfx::Rect expected_down_rect(inset, 0, scrollbar_thickness - inset * 2,
                               button_length - inset);
  EXPECT_EQ(expected_down_rect,
            theme_->InsetButtonRect(*vertical_scrollbar, button_rect,
                                    kForwardButtonStartPart));
}

TEST_P(OverlayScrollbarThemeFluentTest, TestHorizontalInsetButtonRect) {
  int scrollbar_thickness = ScrollbarThickness();
  Scrollbar* horizontal_scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area(), kHorizontalScrollbar, &(theme_->GetInstance()));
  horizontal_scrollbar->SetFrameRect(
      gfx::Rect(0, kOffsetFromViewport, kScrollbarLength, scrollbar_thickness));
  int inset = ScrollbarTrackInsetPx();
  int button_length = theme_->ButtonLength(*horizontal_scrollbar);
  gfx::Rect button_rect(0, 0, button_length, scrollbar_thickness);

  // Left arrow button should be inset from every part except the right.
  gfx::Rect expected_left_rect(inset, inset, button_length - inset,
                               scrollbar_thickness - inset * 2);
  EXPECT_EQ(expected_left_rect,
            theme_->InsetButtonRect(*horizontal_scrollbar, button_rect,
                                    kBackButtonStartPart));
  // Right arrow button should be inset from every part except the left.
  gfx::Rect expected_right_rect(0, inset, button_length - inset,
                                scrollbar_thickness - inset * 2);
  EXPECT_EQ(expected_right_rect,
            theme_->InsetButtonRect(*horizontal_scrollbar, button_rect,
                                    kForwardButtonStartPart));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ScrollbarThemeFluentTest,
                         ::testing::Values(1.f, 1.25f, 1.5f, 1.75f, 2.f));
INSTANTIATE_TEST_SUITE_P(All,
                         OverlayScrollbarThemeFluentTest,
                         ::testing::Values(1.f, 1.25f, 1.5f, 1.75f, 2.f));

}  // namespace blink
