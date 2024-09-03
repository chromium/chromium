// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"

#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mock.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace blink {

class ScrollableAreaStub : public GarbageCollected<ScrollableAreaStub>,
                           public ScrollableArea {
 public:
  ScrollableAreaStub(const gfx::Size& viewport_size,
                     const gfx::Size& contents_size)
      : ScrollableArea(blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
        user_input_scrollable_x_(true),
        user_input_scrollable_y_(true),
        viewport_size_(viewport_size),
        contents_size_(contents_size),
        timer_task_runner_(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()) {}

  void SetViewportSize(const gfx::Size& viewport_size) {
    viewport_size_ = viewport_size;
  }

  gfx::Size ViewportSize() const { return viewport_size_; }

  // ScrollableArea Impl
  int ScrollSize(ScrollbarOrientation orientation) const override {
    gfx::Vector2d scroll_dimensions =
        MaximumScrollOffsetInt() - MinimumScrollOffsetInt();

    return (orientation == kHorizontalScrollbar) ? scroll_dimensions.x()
                                                 : scroll_dimensions.y();
  }

  void SetUserInputScrollable(bool x, bool y) {
    user_input_scrollable_x_ = x;
    user_input_scrollable_y_ = y;
  }

  gfx::Vector2d ScrollOffsetInt() const override {
    return SnapScrollOffsetToPhysicalPixels(scroll_offset_);
  }
  ScrollOffset GetScrollOffset() const override { return scroll_offset_; }
  gfx::Vector2d MinimumScrollOffsetInt() const override {
    return gfx::Vector2d();
  }
  ScrollOffset MinimumScrollOffset() const override { return ScrollOffset(); }
  gfx::Vector2d MaximumScrollOffsetInt() const override {
    return gfx::ToFlooredVector2d(MaximumScrollOffset());
  }

  gfx::Rect VisibleContentRect(
      IncludeScrollbarsInRect = kExcludeScrollbars) const override {
    return gfx::Rect(
        gfx::ToFlooredPoint(gfx::PointAtOffsetFromOrigin(scroll_offset_)),
        viewport_size_);
  }

  gfx::Size ContentsSize() const override { return contents_size_; }
  void SetContentSize(const gfx::Size& contents_size) {
    contents_size_ = contents_size;
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTimerTaskRunner() const final {
    return timer_task_runner_;
  }

  ScrollbarTheme& GetPageScrollbarTheme() const override {
    DEFINE_STATIC_LOCAL(ScrollbarThemeOverlayMock, theme, ());
    return theme;
  }
  bool ScrollAnimatorEnabled() const override { return true; }

  void Trace(Visitor* visitor) const override {
    ScrollableArea::Trace(visitor);
  }

 protected:
  CompositorElementId GetScrollElementId() const override {
    return CompositorElementId();
  }
  void UpdateScrollOffset(const ScrollOffset& offset,
                          mojom::blink::ScrollType) override {
    scroll_offset_ = offset;
  }
  bool ShouldUseIntegerScrollOffset() const override { return true; }
  bool IsThrottled() const override { return false; }
  bool IsActive() const override { return true; }
  bool IsScrollCornerVisible() const override { return true; }
  gfx::Rect ScrollCornerRect() const override { return gfx::Rect(); }
  bool ScrollbarsCanBeActive() const override { return true; }
  bool ShouldPlaceVerticalScrollbarOnLeft() const override { return true; }
  void ScrollControlWasSetNeedsPaintInvalidation() override {}
  bool UsesCompositedScrolling() const override { NOTREACHED(); }
  bool UserInputScrollable(ScrollbarOrientation orientation) const override {
    return orientation == kHorizontalScrollbar ? user_input_scrollable_x_
                                               : user_input_scrollable_y_;
  }
  bool ScheduleAnimation() override { return true; }
  mojom::blink::ColorScheme UsedColorSchemeScrollbars() const override {
    return mojom::blink::ColorScheme::kLight;
  }

  ScrollOffset ClampedScrollOffset(const ScrollOffset& offset) {
    ScrollOffset min_offset = MinimumScrollOffset();
    ScrollOffset max_offset = MaximumScrollOffset();
    float width =
        std::min(std::max(offset.x(), min_offset.x()), max_offset.x());
    float height =
        std::min(std::max(offset.y(), min_offset.y()), max_offset.y());
    return ScrollOffset(width, height);
  }

  bool user_input_scrollable_x_;
  bool user_input_scrollable_y_;
  ScrollOffset scroll_offset_;
  gfx::Size viewport_size_;
  gfx::Size contents_size_;
  scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner_;
};

class RootLayoutViewportStub : public ScrollableAreaStub {
 public:
  RootLayoutViewportStub(const gfx::Size& viewport_size,
                         const gfx::Size& contents_size)
      : ScrollableAreaStub(viewport_size, contents_size) {}

  ScrollOffset MaximumScrollOffset() const override {
    gfx::Size diff = ContentsSize() - ViewportSize();
    return ScrollOffset(diff.width(), diff.height());
  }

  PhysicalRect DocumentToFrame(const PhysicalRect& rect) const {
    PhysicalRect ret = rect;
    ret.Move(-PhysicalOffset::FromVector2dFRound(GetScrollOffset()));
    return ret;
  }

  PhysicalOffset LocalToScrollOriginOffset() const override { return {}; }

 private:
  int VisibleWidth() const override { return viewport_size_.width(); }
  int VisibleHeight() const override { return viewport_size_.height(); }
};

class VisualViewportStub : public ScrollableAreaStub {
 public:
  VisualViewportStub(const gfx::Size& viewport_size,
                     const gfx::Size& contents_size)
      : ScrollableAreaStub(viewport_size, contents_size), scale_(1) {}

  ScrollOffset MaximumScrollOffset() const override {
    gfx::Size diff =
        ContentsSize() - gfx::ScaleToFlooredSize(ViewportSize(), 1 / scale_);
    return ScrollOffset(diff.width(), diff.height());
  }

  PhysicalOffset LocalToScrollOriginOffset() const override { return {}; }

  void SetScale(float scale) { scale_ = scale; }

 private:
  int VisibleWidth() const override { return viewport_size_.width() / scale_; }
  int VisibleHeight() const override {
    return viewport_size_.height() / scale_;
  }
  gfx::Rect VisibleContentRect(IncludeScrollbarsInRect) const override {
    return gfx::Rect(gfx::ToFlooredPoint(ScrollPosition()),
                     gfx::ToCeiledSize(gfx::ScaleSize(
                         gfx::SizeF(viewport_size_), 1 / scale_)));
  }

  float scale_;
};

class RootFrameViewportTest : public testing::Test {
 public:
  RootFrameViewportTest() = default;

 protected:
  void SetUp() override {}

 private:
  test::TaskEnvironment task_environment_;
};

// Tests that scrolling the viewport when the layout viewport is
// !userInputScrollable (as happens when overflow:hidden is set) works
// correctly, that is, the visual viewport can scroll, but not the layout.
TEST_F(RootFrameViewportTest, UserInputScrollable) {
  gfx::Size viewport_size(100, 150);
  auto* layout_viewport = MakeGarbageCollected<RootLayoutViewportStub>(
      viewport_size, gfx::Size(200, 300));
  auto* visual_viewport =
      MakeGarbageCollected<VisualViewportStub>(viewport_size, viewport_size);

  auto* root_frame_viewport = MakeGarbageCollected<RootFrameViewport>(
      *visual_viewport, *layout_viewport);

  visual_viewport->SetScale(2);

  // Disable just the layout viewport's horizontal scrolling, the
  // RootFrameViewport should remain scrollable overall.
  layout_viewport->SetUserInputScrollable(false, true);
  visual_viewport->SetUserInputScrollable(true, true);

  EXPECT_TRUE(root_frame_viewport->UserInputScrollable(kHorizontalScrollbar));
  EXPECT_TRUE(root_frame_viewport->UserInputScrollable(kVerticalScrollbar));

  // Layout viewport shouldn't scroll since it's not horizontally scrollable,
  // but visual viewport should.
  root_frame_viewport->UserScroll(ui::ScrollGranularity::kScrollByPrecisePixel,
                                  ScrollOffset(300, 0),
                                  ScrollableArea::ScrollCallback());
  EXPECT_EQ(ScrollOffset(0, 0), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 0), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 0), root_frame_viewport->GetScrollOffset());

  // Vertical scrolling should be unaffected.
  root_frame_viewport->UserScroll(ui::ScrollGranularity::kScrollByPrecisePixel,
                                  ScrollOffset(0, 300),
                                  ScrollableArea::ScrollCallback());
  EXPECT_EQ(ScrollOffset(0, 150), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 75), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 225), root_frame_viewport->GetScrollOffset());

  // Try the same checks as above but for the vertical direction.
  // ===============================================

  root_frame_viewport->SetScrollOffset(
      ScrollOffset(), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant, ScrollableArea::ScrollCallback());

  // Disable just the layout viewport's vertical scrolling, the
  // RootFrameViewport should remain scrollable overall.
  layout_viewport->SetUserInputScrollable(true, false);
  visual_viewport->SetUserInputScrollable(true, true);

  EXPECT_TRUE(root_frame_viewport->UserInputScrollable(kHorizontalScrollbar));
  EXPECT_TRUE(root_frame_viewport->UserInputScrollable(kVerticalScrollbar));

  // Layout viewport shouldn't scroll since it's not vertically scrollable,
  // but visual viewport should.
  root_frame_viewport->UserScroll(ui::ScrollGranularity::kScrollByPrecisePixel,
                                  ScrollOffset(0, 300),
                                  ScrollableArea::ScrollCallback());
  EXPECT_EQ(ScrollOffset(0, 0), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 75), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 75), root_frame_viewport->GetScrollOffset());

  // Horizontal scrolling should be unaffected.
  root_frame_viewport->UserScroll(ui::ScrollGranularity::kScrollByPrecisePixel,
                                  ScrollOffset(300, 0),
                                  ScrollableArea::ScrollCallback());
  EXPECT_EQ(ScrollOffset(100, 0), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 75), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(150, 75), root_frame_viewport->GetScrollOffset());
}

// Make sure scrolls using the scroll animator (scroll(), setScrollOffset())
// work correctly when one of the subviewports is explicitly scrolled without
// using the // RootFrameViewport interface.
TEST_F(RootFrameViewportTest, TestScrollAnimatorUpdatedBeforeScroll) {
  gfx::Size viewport_size(100, 150);
  auto* layout_viewport = MakeGarbageCollected<RootLayoutViewportStub>(
      viewport_size, gfx::Size(200, 300));
  auto* visual_viewport =
      MakeGarbageCollected<VisualViewportStub>(viewport_size, viewport_size);

  auto* root_frame_viewport = MakeGarbageCollected<RootFrameViewport>(
      *visual_viewport, *layout_viewport);

  visual_viewport->SetScale(2);

  visual_viewport->SetScrollOffset(ScrollOffset(50, 75),
                                   mojom::blink::ScrollType::kProgrammatic);
  EXPECT_EQ(ScrollOffset(50, 75), root_frame_viewport->GetScrollOffset());

  // If the scroll animator doesn't update, it will still think it's at (0, 0)
  // and so it may early exit.
  root_frame_viewport->SetScrollOffset(
      ScrollOffset(0, 0), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant, ScrollableArea::ScrollCallback());
  EXPECT_EQ(ScrollOffset(0, 0), root_frame_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 0), visual_viewport->GetScrollOffset());

  // Try again for userScroll()
  visual_viewport->SetScrollOffset(ScrollOffset(50, 75),
                                   mojom::blink::ScrollType::kProgrammatic);
  EXPECT_EQ(ScrollOffset(50, 75), root_frame_viewport->GetScrollOffset());

  root_frame_viewport->UserScroll(ui::ScrollGranularity::kScrollByPrecisePixel,
                                  ScrollOffset(-50, 0),
                                  ScrollableArea::ScrollCallback());
  EXPECT_EQ(ScrollOffset(0, 75), root_frame_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 75), visual_viewport->GetScrollOffset());

  // Make sure the layout viewport is also accounted for.
  root_frame_viewport->SetScrollOffset(
      ScrollOffset(0, 0), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant, ScrollableArea::ScrollCallback());
  layout_viewport->SetScrollOffset(ScrollOffset(100, 150),
                                   mojom::blink::ScrollType::kProgrammatic);
  EXPECT_EQ(ScrollOffset(100, 150), root_frame_viewport->GetScrollOffset());

  root_frame_viewport->UserScroll(ui::ScrollGranularity::kScrollByPrecisePixel,
                                  ScrollOffset(-100, 0),
                                  ScrollableArea::ScrollCallback());
  EXPECT_EQ(ScrollOffset(0, 150), root_frame_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 150), layout_viewport->GetScrollOffset());
}

// Test that the scrollIntoView correctly scrolls the main frame
// and visual viewport such that the given rect is centered in the viewport.
TEST_F(RootFrameViewportTest, ScrollIntoView) {
  gfx::Size viewport_size(100, 150);
  auto* layout_viewport = MakeGarbageCollected<RootLayoutViewportStub>(
      viewport_size, gfx::Size(200, 300));
  auto* visual_viewport =
      MakeGarbageCollected<VisualViewportStub>(viewport_size, viewport_size);

  auto* root_frame_viewport = MakeGarbageCollected<RootFrameViewport>(
      *visual_viewport, *layout_viewport);

  // Test that the visual viewport is scrolled if the viewport has been
  // resized (as is the case when the ChromeOS keyboard comes up) but not
  // scaled.
  visual_viewport->SetViewportSize(gfx::Size(100, 100));
  root_frame_viewport->ScrollIntoView(
      layout_viewport->DocumentToFrame(PhysicalRect(100, 250, 50, 50)),
      PhysicalBoxStrut(),
      scroll_into_view_util::CreateScrollIntoViewParams(
          ScrollAlignment::ToEdgeIfNeeded(), ScrollAlignment::ToEdgeIfNeeded(),
          mojom::blink::ScrollType::kProgrammatic, true,
          mojom::blink::ScrollBehavior::kInstant));
  EXPECT_EQ(ScrollOffset(50, 150), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 50), visual_viewport->GetScrollOffset());

  root_frame_viewport->ScrollIntoView(
      layout_viewport->DocumentToFrame(PhysicalRect(25, 75, 50, 50)),
      PhysicalBoxStrut(),
      scroll_into_view_util::CreateScrollIntoViewParams(
          ScrollAlignment::ToEdgeIfNeeded(), ScrollAlignment::ToEdgeIfNeeded(),
          mojom::blink::ScrollType::kProgrammatic, true,
          mojom::blink::ScrollBehavior::kInstant));
  EXPECT_EQ(ScrollOffset(25, 75), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 0), visual_viewport->GetScrollOffset());

  // Reset the visual viewport's size, scale the page, and repeat the test
  visual_viewport->SetViewportSize(gfx::Size(100, 150));
  visual_viewport->SetScale(2);
  root_frame_viewport->SetScrollOffset(
      ScrollOffset(), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant, ScrollableArea::ScrollCallback());

  root_frame_viewport->ScrollIntoView(
      layout_viewport->DocumentToFrame(PhysicalRect(50, 75, 50, 75)),
      PhysicalBoxStrut(),
      scroll_into_view_util::CreateScrollIntoViewParams(
          ScrollAlignment::ToEdgeIfNeeded(), ScrollAlignment::ToEdgeIfNeeded(),
          mojom::blink::ScrollType::kProgrammatic, true,
          mojom::blink::ScrollBehavior::kInstant));
  EXPECT_EQ(ScrollOffset(0, 0), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 75), visual_viewport->GetScrollOffset());

  root_frame_viewport->ScrollIntoView(
      layout_viewport->DocumentToFrame(PhysicalRect(190, 290, 10, 10)),
      PhysicalBoxStrut(),
      scroll_into_view_util::CreateScrollIntoViewParams(
          ScrollAlignment::ToEdgeIfNeeded(), ScrollAlignment::ToEdgeIfNeeded(),
          mojom::blink::ScrollType::kProgrammatic, true,
          mojom::blink::ScrollBehavior::kInstant));
  EXPECT_EQ(ScrollOffset(100, 150), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 75), visual_viewport->GetScrollOffset());

  // Scrolling into view the viewport rect itself should be a no-op.
  visual_viewport->SetViewportSize(gfx::Size(100, 100));
  visual_viewport->SetScale(1.5f);
  visual_viewport->SetScrollOffset(ScrollOffset(0, 10),
                                   mojom::blink::ScrollType::kProgrammatic);
  layout_viewport->SetScrollOffset(ScrollOffset(50, 50),
                                   mojom::blink::ScrollType::kProgrammatic);
  root_frame_viewport->SetScrollOffset(root_frame_viewport->GetScrollOffset(),
                                       mojom::blink::ScrollType::kProgrammatic,
                                       mojom::blink::ScrollBehavior::kInstant,
                                       ScrollableArea::ScrollCallback());

  root_frame_viewport->ScrollIntoView(
      layout_viewport->DocumentToFrame(PhysicalRect(
          root_frame_viewport->VisibleContentRect(kExcludeScrollbars))),
      PhysicalBoxStrut(),
      scroll_into_view_util::CreateScrollIntoViewParams(
          ScrollAlignment::ToEdgeIfNeeded(), ScrollAlignment::ToEdgeIfNeeded(),
          mojom::blink::ScrollType::kProgrammatic, true,
          mojom::blink::ScrollBehavior::kInstant));
  EXPECT_EQ(ScrollOffset(50, 50), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 10), visual_viewport->GetScrollOffset());

  root_frame_viewport->ScrollIntoView(
      layout_viewport->DocumentToFrame(PhysicalRect(
          root_frame_viewport->VisibleContentRect(kExcludeScrollbars))),
      PhysicalBoxStrut(),
      scroll_into_view_util::CreateScrollIntoViewParams(
          ScrollAlignment::CenterAlways(), ScrollAlignment::CenterAlways(),
          mojom::blink::ScrollType::kProgrammatic, true,
          mojom::blink::ScrollBehavior::kInstant));
  EXPECT_EQ(ScrollOffset(50, 50), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 10), visual_viewport->GetScrollOffset());

  root_frame_viewport->ScrollIntoView(
      layout_viewport->DocumentToFrame(PhysicalRect(
          root_frame_viewport->VisibleContentRect(kExcludeScrollbars))),
      PhysicalBoxStrut(),
      scroll_into_view_util::CreateScrollIntoViewParams(
          ScrollAlignment::TopAlways(), ScrollAlignment::TopAlways(),
          mojom::blink::ScrollType::kProgrammatic, true,
          mojom::blink::ScrollBehavior::kInstant));
  EXPECT_EQ(ScrollOffset(50, 50), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 10), visual_viewport->GetScrollOffset());
}

// Tests that the setScrollOffset method works correctly with both viewports.
TEST_F(RootFrameViewportTest, SetScrollOffset) {
  gfx::Size viewport_size(500, 500);
  auto* layout_viewport = MakeGarbageCollected<RootLayoutViewportStub>(
      viewport_size, gfx::Size(1000, 2000));
  auto* visual_viewport =
      MakeGarbageCollected<VisualViewportStub>(viewport_size, viewport_size);

  auto* root_frame_viewport = MakeGarbageCollected<RootFrameViewport>(
      *visual_viewport, *layout_viewport);

  visual_viewport->SetScale(2);

  // Ensure that the visual viewport scrolls first.
  root_frame_viewport->SetScrollOffset(
      ScrollOffset(100, 100), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant, ScrollableArea::ScrollCallback());
  EXPECT_EQ(ScrollOffset(100, 100), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 0), layout_viewport->GetScrollOffset());

  // Scroll to the visual viewport's extent, the layout viewport should scroll
  // the remainder.
  root_frame_viewport->SetScrollOffset(
      ScrollOffset(300, 400), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant, ScrollableArea::ScrollCallback());
  EXPECT_EQ(ScrollOffset(250, 250), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 150), layout_viewport->GetScrollOffset());

  // Only the layout viewport should scroll further. Make sure it doesn't scroll
  // out of bounds.
  root_frame_viewport->SetScrollOffset(
      ScrollOffset(780, 1780), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant, ScrollableArea::ScrollCallback());
  EXPECT_EQ(ScrollOffset(250, 250), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(500, 1500), layout_viewport->GetScrollOffset());

  // Scroll all the way back.
  root_frame_viewport->SetScrollOffset(
      ScrollOffset(0, 0), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant, ScrollableArea::ScrollCallback());
  EXPECT_EQ(ScrollOffset(0, 0), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 0), layout_viewport->GetScrollOffset());
}

// Tests that the visible rect (i.e. visual viewport rect) is correctly
// calculated, taking into account both viewports and page scale.
TEST_F(RootFrameViewportTest, VisibleContentRect) {
  gfx::Size viewport_size(500, 401);
  auto* layout_viewport = MakeGarbageCollected<RootLayoutViewportStub>(
      viewport_size, gfx::Size(1000, 2000));
  auto* visual_viewport =
      MakeGarbageCollected<VisualViewportStub>(viewport_size, viewport_size);

  auto* root_frame_viewport = MakeGarbageCollected<RootFrameViewport>(
      *visual_viewport, *layout_viewport);

  root_frame_viewport->SetScrollOffset(
      ScrollOffset(100, 75), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant, ScrollableArea::ScrollCallback());

  EXPECT_EQ(gfx::Point(100, 75),
            root_frame_viewport->VisibleContentRect().origin());
  EXPECT_EQ(gfx::Size(500, 401),
            root_frame_viewport->VisibleContentRect().size());

  visual_viewport->SetScale(2);

  EXPECT_EQ(gfx::Point(100, 75),
            root_frame_viewport->VisibleContentRect().origin());
  EXPECT_EQ(gfx::Size(250, 201),
            root_frame_viewport->VisibleContentRect().size());
}

// Tests that scrolls on the root frame scroll the visual viewport before
// trying to scroll the layout viewport.
TEST_F(RootFrameViewportTest, ViewportScrollOrder) {
  gfx::Size viewport_size(100, 100);
  auto* layout_viewport = MakeGarbageCollected<RootLayoutViewportStub>(
      viewport_size, gfx::Size(200, 300));
  auto* visual_viewport =
      MakeGarbageCollected<VisualViewportStub>(viewport_size, viewport_size);

  auto* root_frame_viewport = MakeGarbageCollected<RootFrameViewport>(
      *visual_viewport, *layout_viewport);

  visual_viewport->SetScale(2);

  root_frame_viewport->SetScrollOffset(
      ScrollOffset(40, 40), mojom::blink::ScrollType::kUser,
      mojom::blink::ScrollBehavior::kInstant,
      ScrollableArea::ScrollCallback(WTF::BindOnce(
          [](ScrollableArea* visual_viewport, ScrollableArea* layout_viewport,
             ScrollableArea::ScrollCompletionMode) {
            EXPECT_EQ(ScrollOffset(40, 40), visual_viewport->GetScrollOffset());
            EXPECT_EQ(ScrollOffset(0, 0), layout_viewport->GetScrollOffset());
          },
          WrapWeakPersistent(visual_viewport),
          WrapWeakPersistent(layout_viewport))));
  EXPECT_EQ(ScrollOffset(40, 40), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 0), layout_viewport->GetScrollOffset());

  root_frame_viewport->SetScrollOffset(
      ScrollOffset(60, 60), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant,
      ScrollableArea::ScrollCallback(WTF::BindOnce(
          [](ScrollableArea* visual_viewport, ScrollableArea* layout_viewport,
             ScrollableArea::ScrollCompletionMode) {
            EXPECT_EQ(ScrollOffset(50, 50), visual_viewport->GetScrollOffset());
            EXPECT_EQ(ScrollOffset(10, 10), layout_viewport->GetScrollOffset());
          },
          WrapWeakPersistent(visual_viewport),
          WrapWeakPersistent(layout_viewport))));
  EXPECT_EQ(ScrollOffset(50, 50), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(10, 10), layout_viewport->GetScrollOffset());
}

// Tests that setting an alternate layout viewport scrolls the alternate
// instead of the original.
TEST_F(RootFrameViewportTest, SetAlternateLayoutViewport) {
  gfx::Size viewport_size(100, 100);
  auto* layout_viewport = MakeGarbageCollected<RootLayoutViewportStub>(
      viewport_size, gfx::Size(200, 300));
  auto* visual_viewport =
      MakeGarbageCollected<VisualViewportStub>(viewport_size, viewport_size);

  auto* alternate_scroller = MakeGarbageCollected<RootLayoutViewportStub>(
      viewport_size, gfx::Size(600, 500));

  auto* root_frame_viewport = MakeGarbageCollected<RootFrameViewport>(
      *visual_viewport, *layout_viewport);

  visual_viewport->SetScale(2);

  root_frame_viewport->SetScrollOffset(
      ScrollOffset(100, 100), mojom::blink::ScrollType::kUser,
      mojom::blink::ScrollBehavior::kInstant, ScrollableArea::ScrollCallback());
  EXPECT_EQ(ScrollOffset(50, 50), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 50), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(100, 100), root_frame_viewport->GetScrollOffset());

  root_frame_viewport->SetLayoutViewport(*alternate_scroller);
  EXPECT_EQ(ScrollOffset(50, 50), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 0), alternate_scroller->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 50), root_frame_viewport->GetScrollOffset());

  root_frame_viewport->SetScrollOffset(
      ScrollOffset(200, 200), mojom::blink::ScrollType::kUser,
      mojom::blink::ScrollBehavior::kInstant, ScrollableArea::ScrollCallback());
  EXPECT_EQ(ScrollOffset(50, 50), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(150, 150), alternate_scroller->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(200, 200), root_frame_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 50), layout_viewport->GetScrollOffset());

  EXPECT_EQ(ScrollOffset(550, 450), root_frame_viewport->MaximumScrollOffset());
}

// Tests that scrolls on the root frame scroll the visual viewport before
// trying to scroll the layout viewport when using
// DistributeScrollBetweenViewports directly.
TEST_F(RootFrameViewportTest, DistributeScrollOrder) {
  gfx::Size viewport_size(100, 100);
  auto* layout_viewport = MakeGarbageCollected<RootLayoutViewportStub>(
      viewport_size, gfx::Size(200, 300));
  auto* visual_viewport =
      MakeGarbageCollected<VisualViewportStub>(viewport_size, viewport_size);

  auto* root_frame_viewport = MakeGarbageCollected<RootFrameViewport>(
      *visual_viewport, *layout_viewport);

  visual_viewport->SetScale(2);

  root_frame_viewport->DistributeScrollBetweenViewports(
      ScrollOffset(60, 60), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kSmooth, RootFrameViewport::kVisualViewport,
      ScrollableArea::ScrollCallback(WTF::BindOnce(
          [](ScrollableArea* visual_viewport, ScrollableArea* layout_viewport,
             ScrollableArea::ScrollCompletionMode) {
            EXPECT_EQ(ScrollOffset(50, 50), visual_viewport->GetScrollOffset());
            EXPECT_EQ(ScrollOffset(10, 10), layout_viewport->GetScrollOffset());
          },
          WrapWeakPersistent(visual_viewport),
          WrapWeakPersistent(layout_viewport))));
  root_frame_viewport->UpdateCompositorScrollAnimations();
  root_frame_viewport->ServiceScrollAnimations(1);
  EXPECT_EQ(ScrollOffset(0, 0), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 0), layout_viewport->GetScrollOffset());
  root_frame_viewport->ServiceScrollAnimations(1000000);
  EXPECT_EQ(ScrollOffset(50, 50), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(10, 10), layout_viewport->GetScrollOffset());
}

class RootFrameViewportRenderTest : public RenderingTest {
 public:
  RootFrameViewportRenderTest()
      : RenderingTest(MakeGarbageCollected<EmptyLocalFrameClient>()) {}
};

TEST_F(RootFrameViewportRenderTest,
       ApplyPendingHistoryRestoreScrollOffsetTwice) {
  HistoryItem::ViewState view_state;
  view_state.page_scale_factor_ = 1.5;
  RootFrameViewport* root_frame_viewport = static_cast<RootFrameViewport*>(
      GetDocument().View()->GetScrollableArea());
  root_frame_viewport->SetPendingHistoryRestoreScrollOffset(
      view_state, false, mojom::blink::ScrollBehavior::kAuto);
  root_frame_viewport->ApplyPendingHistoryRestoreScrollOffset();

  // Override the 1.5 scale with 1.0.
  GetDocument().GetPage()->GetVisualViewport().SetScale(1.0f);

  // The second call to ApplyPendingHistoryRestoreScrollOffset should
  // do nothing, since the history was already restored.
  root_frame_viewport->ApplyPendingHistoryRestoreScrollOffset();
  EXPECT_EQ(1.0f, GetDocument().GetPage()->GetVisualViewport().Scale());
}

}  // namespace blink
