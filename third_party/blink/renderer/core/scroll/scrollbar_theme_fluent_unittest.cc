// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_fluent.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_test_suite.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

using ::testing::NiceMock;

namespace {

// Fluent scrollbar parts' dimensions.
constexpr int kScrollbarButtonLength = 18;
constexpr int kScrollbarThumbOffset = 4;
constexpr int kScrollbarThumbThickness = 6;
constexpr int kScrollbarTrackThickness = 14;

// Const values for unit tests.
constexpr int kThumbLengthForTests = 30;
constexpr int kScrollbarLengthForTests = 200;
constexpr int kOffsetFromViewport = 100;

}  // namespace

class ScrollbarThemeFluentMock final : public ScrollbarThemeFluent {
 public:
  ScrollbarThemeFluentMock() {
    scrollbar_button_length_ = kScrollbarButtonLength;
    scrollbar_thumb_thickness_ = kScrollbarThumbThickness;
    scrollbar_track_thickness_ = kScrollbarTrackThickness;
  }
  int ThumbLength(const Scrollbar& scrollbar) override {
    return kThumbLengthForTests;
  }

 protected:
  FRIEND_TEST_ALL_PREFIXES(ScrollbarThemeFluentTest,
                           VerticalScrollbarPartsSizes);
  FRIEND_TEST_ALL_PREFIXES(ScrollbarThemeFluentTest,
                           HorizontalScrollbarPartsSizes);
};

class ScrollbarThemeFluentTest : public ::testing::Test,
                                 public ::testing::WithParamInterface<float> {
 protected:
  void SetUp() override {
    ScrollbarThemeSettings::SetFluentScrollbarsEnabled(true);
    mock_scrollable_area_ =
        MakeGarbageCollected<NiceMock<MockScrollableArea>>();
    mock_scrollable_area_->SetScaleFromDIP(GetParam());
  }

  int ButtonLength() const {
    return static_cast<int>(kScrollbarButtonLength * ScaleFromDIP());
  }

  int TrackLength() const {
    return kScrollbarLengthForTests - 2 * ButtonLength();
  }

  int TrackThickness() const {
    return static_cast<int>(kScrollbarTrackThickness * ScaleFromDIP());
  }

  int ThumbThickness() const {
    return static_cast<int>(kScrollbarThumbThickness * ScaleFromDIP());
  }

  int ThumbOffset() const {
    return static_cast<int>(kScrollbarThumbOffset * ScaleFromDIP());
  }

  float ScaleFromDIP() const { return GetParam(); }

  Persistent<NiceMock<MockScrollableArea>> mock_scrollable_area_;
};

TEST_P(ScrollbarThemeFluentTest, VerticalScrollbarPartsSizes) {
  ScrollbarThemeFluentMock theme;
  Scrollbar* vetical_scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area_, kVerticalScrollbar, &theme);
  vetical_scrollbar->SetFrameRect(gfx::Rect(
      kOffsetFromViewport, 0,
      theme.ScrollbarThickness(ScaleFromDIP(), EScrollbarWidth::kAuto),
      kScrollbarLengthForTests));

  EXPECT_EQ(ThumbThickness() + 2 * ThumbOffset(), TrackThickness());

  const gfx::Rect track_rect = theme.TrackRect(*vetical_scrollbar);
  EXPECT_EQ(track_rect, gfx::Rect(kOffsetFromViewport, ButtonLength(),
                                  TrackThickness(), TrackLength()));

  const gfx::Rect thumb_rect = theme.ThumbRect(*vetical_scrollbar);
  EXPECT_EQ(thumb_rect,
            gfx::Rect(kOffsetFromViewport + ThumbOffset(), ButtonLength(),
                      ThumbThickness(), kThumbLengthForTests));

  const gfx::Size button_size = theme.ButtonSize(*vetical_scrollbar);
  EXPECT_EQ(button_size, gfx::Size(TrackThickness(), ButtonLength()));
}

TEST_P(ScrollbarThemeFluentTest, HorizontalScrollbarPartsSizes) {
  ScrollbarThemeFluentMock theme;
  Scrollbar* horizontal_scrollbar = Scrollbar::CreateForTesting(
      mock_scrollable_area_, kHorizontalScrollbar, &theme);
  horizontal_scrollbar->SetFrameRect(gfx::Rect(
      0, kOffsetFromViewport, kScrollbarLengthForTests,
      theme.ScrollbarThickness(ScaleFromDIP(), EScrollbarWidth::kAuto)));

  EXPECT_EQ(ThumbThickness() + 2 * ThumbOffset(), TrackThickness());

  const gfx::Rect track_rect = theme.TrackRect(*horizontal_scrollbar);
  EXPECT_EQ(track_rect, gfx::Rect(ButtonLength(), kOffsetFromViewport,
                                  TrackLength(), TrackThickness()));

  const gfx::Rect thumb_rect = theme.ThumbRect(*horizontal_scrollbar);
  EXPECT_EQ(thumb_rect,
            gfx::Rect(ButtonLength(), kOffsetFromViewport + ThumbOffset(),
                      kThumbLengthForTests, ThumbThickness()));

  const gfx::Size button_size = theme.ButtonSize(*horizontal_scrollbar);
  EXPECT_EQ(button_size, gfx::Size(ButtonLength(), TrackThickness()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ScrollbarThemeFluentTest,
                         ::testing::Values(1.f, 1.25f, 1.5f, 1.75f, 2.f));

}  // namespace blink
