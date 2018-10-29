// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"

#include "base/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_mock.h"
#include "third_party/blink/renderer/platform/geometry/double_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"

namespace {
blink::ScrollbarThemeMock scrollbar_theme_;
}

namespace blink {

class ScrollableAreaStub : public GarbageCollectedFinalized<ScrollableAreaStub>,
                           public ScrollableArea {
  USING_GARBAGE_COLLECTED_MIXIN(ScrollableAreaStub);

 public:
  static ScrollableAreaStub* Create(const IntSize& viewport_size,
                                    const IntSize& contents_size) {
    return new ScrollableAreaStub(viewport_size, contents_size);
  }

  void SetViewportSize(const IntSize& viewport_size) {
    viewport_size_ = viewport_size;
  }

  IntSize ViewportSize() const { return viewport_size_; }

  // ScrollableArea Impl
  int ScrollSize(ScrollbarOrientation orientation) const override {
    IntSize scroll_dimensions =
        MaximumScrollOffsetInt() - MinimumScrollOffsetInt();

    return (orientation == kHorizontalScrollbar) ? scroll_dimensions.Width()
                                                 : scroll_dimensions.Height();
  }

  void SetUserInputScrollable(bool x, bool y) {
    user_input_scrollable_x_ = x;
    user_input_scrollable_y_ = y;
  }

  IntSize ScrollOffsetInt() const override {
    return FlooredIntSize(scroll_offset_);
  }
  ScrollOffset GetScrollOffset() const override { return scroll_offset_; }
  IntSize MinimumScrollOffsetInt() const override { return IntSize(); }
  ScrollOffset MinimumScrollOffset() const override { return ScrollOffset(); }
  IntSize MaximumScrollOffsetInt() const override {
    return FlooredIntSize(MaximumScrollOffset());
  }

  IntRect VisibleContentRect(
      IncludeScrollbarsInRect = kExcludeScrollbars) const override {
    return IntRect(IntPoint(FlooredIntSize(scroll_offset_)), viewport_size_);
  }

  IntSize ContentsSize() const override { return contents_size_; }
  void SetContentSize(const IntSize& contents_size) {
    contents_size_ = contents_size;
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTimerTaskRunner() const final {
    return timer_task_runner_;
  }

  ScrollbarTheme& GetPageScrollbarTheme() const override {
    return scrollbar_theme_;
  }

  void Trace(blink::Visitor* visitor) override {
    ScrollableArea::Trace(visitor);
  }

 protected:
  ScrollableAreaStub(const IntSize& viewport_size, const IntSize& contents_size)
      : user_input_scrollable_x_(true),
        user_input_scrollable_y_(true),
        viewport_size_(viewport_size),
        contents_size_(contents_size),
        timer_task_runner_(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()) {}

  CompositorElementId GetCompositorElementId() const override {
    return CompositorElementId();
  }
  void UpdateScrollOffset(const ScrollOffset& offset, ScrollType) override {
    scroll_offset_ = offset;
  }
  bool ShouldUseIntegerScrollOffset() const override { return true; }
  bool IsThrottled() const override { return false; }
  bool IsActive() const override { return true; }
  bool IsScrollCornerVisible() const override { return true; }
  IntRect ScrollCornerRect() const override { return IntRect(); }
  bool ScrollbarsCanBeActive() const override { return true; }
  IntRect ScrollableAreaBoundingBox() const override { return IntRect(); }
  bool ShouldPlaceVerticalScrollbarOnLeft() const override { return true; }
  void ScrollControlWasSetNeedsPaintInvalidation() override {}
  GraphicsLayer* LayerForContainer() const override { return nullptr; }
  GraphicsLayer* LayerForScrolling() const override { return nullptr; }
  GraphicsLayer* LayerForHorizontalScrollbar() const override {
    return nullptr;
  }
  GraphicsLayer* LayerForVerticalScrollbar() const override { return nullptr; }
  bool UserInputScrollable(ScrollbarOrientation orientation) const override {
    return orientation == kHorizontalScrollbar ? user_input_scrollable_x_
                                               : user_input_scrollable_y_;
  }

  ScrollOffset ClampedScrollOffset(const ScrollOffset& offset) {
    ScrollOffset min_offset = MinimumScrollOffset();
    ScrollOffset max_offset = MaximumScrollOffset();
    float width = std::min(std::max(offset.Width(), min_offset.Width()),
                           max_offset.Width());
    float height = std::min(std::max(offset.Height(), min_offset.Height()),
                            max_offset.Height());
    return ScrollOffset(width, height);
  }

  bool user_input_scrollable_x_;
  bool user_input_scrollable_y_;
  ScrollOffset scroll_offset_;
  IntSize viewport_size_;
  IntSize contents_size_;
  scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner_;
};

class RootLayoutViewportStub : public ScrollableAreaStub {
 public:
  static RootLayoutViewportStub* Create(const IntSize& viewport_size,
                                        const IntSize& contents_size) {
    return new RootLayoutViewportStub(viewport_size, contents_size);
  }

  ScrollOffset MaximumScrollOffset() const override {
    return ScrollOffset(ContentsSize() - ViewportSize());
  }

  LayoutRect DocumentToFrame(const LayoutRect& rect) const {
    LayoutRect ret = rect;
    ret.Move(LayoutSize(-GetScrollOffset()));
    return ret;
  }

 private:
  RootLayoutViewportStub(const IntSize& viewport_size,
                         const IntSize& contents_size)
      : ScrollableAreaStub(viewport_size, contents_size) {}

  int VisibleWidth() const override { return viewport_size_.Width(); }
  int VisibleHeight() const override { return viewport_size_.Height(); }
};

class VisualViewportStub : public ScrollableAreaStub {
 public:
  static VisualViewportStub* Create(const IntSize& viewport_size,
                                    const IntSize& contents_size) {
    return new VisualViewportStub(viewport_size, contents_size);
  }

  ScrollOffset MaximumScrollOffset() const override {
    ScrollOffset visible_viewport(ViewportSize());
    visible_viewport.Scale(1 / scale_);

    ScrollOffset max_offset = ScrollOffset(ContentsSize()) - visible_viewport;
    return ScrollOffset(max_offset);
  }

  void SetScale(float scale) { scale_ = scale; }

 private:
  VisualViewportStub(const IntSize& viewport_size, const IntSize& contents_size)
      : ScrollableAreaStub(viewport_size, contents_size), scale_(1) {}

  int VisibleWidth() const override { return viewport_size_.Width() / scale_; }
  int VisibleHeight() const override {
    return viewport_size_.Height() / scale_;
  }
  IntRect VisibleContentRect(IncludeScrollbarsInRect) const override {
    FloatSize size(viewport_size_);
    size.Scale(1 / scale_);
    return IntRect(IntPoint(FlooredIntSize(GetScrollOffset())),
                   ExpandedIntSize(size));
  }

  float scale_;
};

class RootFrameViewportTest : public testing::Test {
 public:
  RootFrameViewportTest() = default;

 protected:
  void SetUp() override {}
};

// Tests that scrolling the viewport when the layout viewport is
// !userInputScrollable (as happens when overflow:hidden is set) works
// correctly, that is, the visual viewport can scroll, but not the layout.
TEST_F(RootFrameViewportTest, UserInputScrollable) {
  IntSize viewport_size(100, 150);
  RootLayoutViewportStub* layout_viewport =
      RootLayoutViewportStub::Create(viewport_size, IntSize(200, 300));
  VisualViewportStub* visual_viewport =
      VisualViewportStub::Create(viewport_size, viewport_size);

  ScrollableArea* root_frame_viewport =
      RootFrameViewport::Create(*visual_viewport, *layout_viewport);

  visual_viewport->SetScale(2);

  // Disable just the layout viewport's horizontal scrolling, the
  // RootFrameViewport should remain scrollable overall.
  layout_viewport->SetUserInputScrollable(false, true);
  visual_viewport->SetUserInputScrollable(true, true);

  EXPECT_TRUE(root_frame_viewport->UserInputScrollable(kHorizontalScrollbar));
  EXPECT_TRUE(root_frame_viewport->UserInputScrollable(kVerticalScrollbar));

  // Layout viewport shouldn't scroll since it's not horizontally scrollable,
  // but visual viewport should.
  root_frame_viewport->UserScroll(kScrollByPixel, FloatSize(300, 0));
  EXPECT_EQ(ScrollOffset(0, 0), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 0), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 0), root_frame_viewport->GetScrollOffset());

  // Vertical scrolling should be unaffected.
  root_frame_viewport->UserScroll(kScrollByPixel, FloatSize(0, 300));
  EXPECT_EQ(ScrollOffset(0, 150), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 75), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 225), root_frame_viewport->GetScrollOffset());

  // Try the same checks as above but for the vertical direction.
  // ===============================================

  root_frame_viewport->SetScrollOffset(ScrollOffset(), kProgrammaticScroll);

  // Disable just the layout viewport's vertical scrolling, the
  // RootFrameViewport should remain scrollable overall.
  layout_viewport->SetUserInputScrollable(true, false);
  visual_viewport->SetUserInputScrollable(true, true);

  EXPECT_TRUE(root_frame_viewport->UserInputScrollable(kHorizontalScrollbar));
  EXPECT_TRUE(root_frame_viewport->UserInputScrollable(kVerticalScrollbar));

  // Layout viewport shouldn't scroll since it's not vertically scrollable,
  // but visual viewport should.
  root_frame_viewport->UserScroll(kScrollByPixel, FloatSize(0, 300));
  EXPECT_EQ(ScrollOffset(0, 0), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 75), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 75), root_frame_viewport->GetScrollOffset());

  // Horizontal scrolling should be unaffected.
  root_frame_viewport->UserScroll(kScrollByPixel, FloatSize(300, 0));
  EXPECT_EQ(ScrollOffset(100, 0), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 75), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(150, 75), root_frame_viewport->GetScrollOffset());
}

// Make sure scrolls using the scroll animator (scroll(), setScrollOffset())
// work correctly when one of the subviewports is explicitly scrolled without
// using the // RootFrameViewport interface.
TEST_F(RootFrameViewportTest, TestScrollAnimatorUpdatedBeforeScroll) {
  IntSize viewport_size(100, 150);
  RootLayoutViewportStub* layout_viewport =
      RootLayoutViewportStub::Create(viewport_size, IntSize(200, 300));
  VisualViewportStub* visual_viewport =
      VisualViewportStub::Create(viewport_size, viewport_size);

  ScrollableArea* root_frame_viewport =
      RootFrameViewport::Create(*visual_viewport, *layout_viewport);

  visual_viewport->SetScale(2);

  visual_viewport->SetScrollOffset(ScrollOffset(50, 75), kProgrammaticScroll);
  EXPECT_EQ(ScrollOffset(50, 75), root_frame_viewport->GetScrollOffset());

  // If the scroll animator doesn't update, it will still think it's at (0, 0)
  // and so it may early exit.
  root_frame_viewport->SetScrollOffset(ScrollOffset(0, 0), kProgrammaticScroll);
  EXPECT_EQ(ScrollOffset(0, 0), root_frame_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 0), visual_viewport->GetScrollOffset());

  // Try again for userScroll()
  visual_viewport->SetScrollOffset(ScrollOffset(50, 75), kProgrammaticScroll);
  EXPECT_EQ(ScrollOffset(50, 75), root_frame_viewport->GetScrollOffset());

  root_frame_viewport->UserScroll(kScrollByPixel, FloatSize(-50, 0));
  EXPECT_EQ(ScrollOffset(0, 75), root_frame_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 75), visual_viewport->GetScrollOffset());

  // Make sure the layout viewport is also accounted for.
  root_frame_viewport->SetScrollOffset(ScrollOffset(0, 0), kProgrammaticScroll);
  layout_viewport->SetScrollOffset(ScrollOffset(100, 150), kProgrammaticScroll);
  EXPECT_EQ(ScrollOffset(100, 150), root_frame_viewport->GetScrollOffset());

  root_frame_viewport->UserScroll(kScrollByPixel, FloatSize(-100, 0));
  EXPECT_EQ(ScrollOffset(0, 150), root_frame_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 150), layout_viewport->GetScrollOffset());
}

// Test that the scrollIntoView correctly scrolls the main frame
// and visual viewport such that the given rect is centered in the viewport.
TEST_F(RootFrameViewportTest, ScrollIntoView) {
  IntSize viewport_size(100, 150);
  RootLayoutViewportStub* layout_viewport =
      RootLayoutViewportStub::Create(viewport_size, IntSize(200, 300));
  VisualViewportStub* visual_viewport =
      VisualViewportStub::Create(viewport_size, viewport_size);

  ScrollableArea* root_frame_viewport =
      RootFrameViewport::Create(*visual_viewport, *layout_viewport);

  // Test that the visual viewport is scrolled if the viewport has been
  // resized (as is the case when the ChromeOS keyboard comes up) but not
  // scaled.
  visual_viewport->SetViewportSize(IntSize(100, 100));
  root_frame_viewport->ScrollIntoView(
      layout_viewport->DocumentToFrame(LayoutRect(100, 250, 50, 50)),
      WebScrollIntoViewParams(ScrollAlignment::kAlignToEdgeIfNeeded,
                              ScrollAlignment::kAlignToEdgeIfNeeded,
                              kProgrammaticScroll, true,
                              kScrollBehaviorInstant));
  EXPECT_EQ(ScrollOffset(50, 150), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 50), visual_viewport->GetScrollOffset());

  root_frame_viewport->ScrollIntoView(
      layout_viewport->DocumentToFrame(LayoutRect(25, 75, 50, 50)),
      WebScrollIntoViewParams(ScrollAlignment::kAlignToEdgeIfNeeded,
                              ScrollAlignment::kAlignToEdgeIfNeeded,
                              kProgrammaticScroll, true,
                              kScrollBehaviorInstant));
  EXPECT_EQ(ScrollOffset(25, 75), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 0), visual_viewport->GetScrollOffset());

  // Reset the visual viewport's size, scale the page, and repeat the test
  visual_viewport->SetViewportSize(IntSize(100, 150));
  visual_viewport->SetScale(2);
  root_frame_viewport->SetScrollOffset(ScrollOffset(), kProgrammaticScroll);

  root_frame_viewport->ScrollIntoView(
      layout_viewport->DocumentToFrame(LayoutRect(50, 75, 50, 75)),
      WebScrollIntoViewParams(ScrollAlignment::kAlignToEdgeIfNeeded,
                              ScrollAlignment::kAlignToEdgeIfNeeded,
                              kProgrammaticScroll, true,
                              kScrollBehaviorInstant));
  EXPECT_EQ(ScrollOffset(0, 0), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 75), visual_viewport->GetScrollOffset());

  root_frame_viewport->ScrollIntoView(
      layout_viewport->DocumentToFrame(LayoutRect(190, 290, 10, 10)),
      WebScrollIntoViewParams(ScrollAlignment::kAlignToEdgeIfNeeded,
                              ScrollAlignment::kAlignToEdgeIfNeeded,
                              kProgrammaticScroll, true,
                              kScrollBehaviorInstant));
  EXPECT_EQ(ScrollOffset(100, 150), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 75), visual_viewport->GetScrollOffset());

  // Scrolling into view the viewport rect itself should be a no-op.
  visual_viewport->SetViewportSize(IntSize(100, 100));
  visual_viewport->SetScale(1.5f);
  visual_viewport->SetScrollOffset(ScrollOffset(0, 10), kProgrammaticScroll);
  layout_viewport->SetScrollOffset(ScrollOffset(50, 50), kProgrammaticScroll);
  root_frame_viewport->SetScrollOffset(root_frame_viewport->GetScrollOffset(),
                                       kProgrammaticScroll);

  root_frame_viewport->ScrollIntoView(
      layout_viewport->DocumentToFrame(LayoutRect(
          root_frame_viewport->VisibleContentRect(kExcludeScrollbars))),
      WebScrollIntoViewParams(ScrollAlignment::kAlignToEdgeIfNeeded,
                              ScrollAlignment::kAlignToEdgeIfNeeded,
                              kProgrammaticScroll, true,
                              kScrollBehaviorInstant));
  EXPECT_EQ(ScrollOffset(50, 50), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 10), visual_viewport->GetScrollOffset());

  root_frame_viewport->ScrollIntoView(
      layout_viewport->DocumentToFrame(LayoutRect(
          root_frame_viewport->VisibleContentRect(kExcludeScrollbars))),
      WebScrollIntoViewParams(ScrollAlignment::kAlignCenterAlways,
                              ScrollAlignment::kAlignCenterAlways,
                              kProgrammaticScroll, true,
                              kScrollBehaviorInstant));
  EXPECT_EQ(ScrollOffset(50, 50), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 10), visual_viewport->GetScrollOffset());

  root_frame_viewport->ScrollIntoView(
      layout_viewport->DocumentToFrame(LayoutRect(
          root_frame_viewport->VisibleContentRect(kExcludeScrollbars))),
      WebScrollIntoViewParams(
          ScrollAlignment::kAlignTopAlways, ScrollAlignment::kAlignTopAlways,
          kProgrammaticScroll, true, kScrollBehaviorInstant));
  EXPECT_EQ(ScrollOffset(50, 50), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 10), visual_viewport->GetScrollOffset());
}

// Tests that the setScrollOffset method works correctly with both viewports.
TEST_F(RootFrameViewportTest, SetScrollOffset) {
  IntSize viewport_size(500, 500);
  RootLayoutViewportStub* layout_viewport =
      RootLayoutViewportStub::Create(viewport_size, IntSize(1000, 2000));
  VisualViewportStub* visual_viewport =
      VisualViewportStub::Create(viewport_size, viewport_size);

  ScrollableArea* root_frame_viewport =
      RootFrameViewport::Create(*visual_viewport, *layout_viewport);

  visual_viewport->SetScale(2);

  // Ensure that the visual viewport scrolls first.
  root_frame_viewport->SetScrollOffset(ScrollOffset(100, 100),
                                       kProgrammaticScroll);
  EXPECT_EQ(ScrollOffset(100, 100), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 0), layout_viewport->GetScrollOffset());

  // Scroll to the visual viewport's extent, the layout viewport should scroll
  // the remainder.
  root_frame_viewport->SetScrollOffset(ScrollOffset(300, 400),
                                       kProgrammaticScroll);
  EXPECT_EQ(ScrollOffset(250, 250), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 150), layout_viewport->GetScrollOffset());

  // Only the layout viewport should scroll further. Make sure it doesn't scroll
  // out of bounds.
  root_frame_viewport->SetScrollOffset(ScrollOffset(780, 1780),
                                       kProgrammaticScroll);
  EXPECT_EQ(ScrollOffset(250, 250), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(500, 1500), layout_viewport->GetScrollOffset());

  // Scroll all the way back.
  root_frame_viewport->SetScrollOffset(ScrollOffset(0, 0), kProgrammaticScroll);
  EXPECT_EQ(ScrollOffset(0, 0), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 0), layout_viewport->GetScrollOffset());
}

// Tests that the visible rect (i.e. visual viewport rect) is correctly
// calculated, taking into account both viewports and page scale.
TEST_F(RootFrameViewportTest, VisibleContentRect) {
  IntSize viewport_size(500, 401);
  RootLayoutViewportStub* layout_viewport =
      RootLayoutViewportStub::Create(viewport_size, IntSize(1000, 2000));
  VisualViewportStub* visual_viewport =
      VisualViewportStub::Create(viewport_size, viewport_size);

  ScrollableArea* root_frame_viewport =
      RootFrameViewport::Create(*visual_viewport, *layout_viewport);

  root_frame_viewport->SetScrollOffset(ScrollOffset(100, 75),
                                       kProgrammaticScroll);

  EXPECT_EQ(IntPoint(100, 75),
            root_frame_viewport->VisibleContentRect().Location());
  EXPECT_EQ(ScrollOffset(500, 401),
            DoubleSize(root_frame_viewport->VisibleContentRect().Size()));

  visual_viewport->SetScale(2);

  EXPECT_EQ(IntPoint(100, 75),
            root_frame_viewport->VisibleContentRect().Location());
  EXPECT_EQ(ScrollOffset(250, 201),
            DoubleSize(root_frame_viewport->VisibleContentRect().Size()));
}

// Tests that scrolls on the root frame scroll the visual viewport before
// trying to scroll the layout viewport.
TEST_F(RootFrameViewportTest, ViewportScrollOrder) {
  IntSize viewport_size(100, 100);
  RootLayoutViewportStub* layout_viewport =
      RootLayoutViewportStub::Create(viewport_size, IntSize(200, 300));
  VisualViewportStub* visual_viewport =
      VisualViewportStub::Create(viewport_size, viewport_size);

  ScrollableArea* root_frame_viewport =
      RootFrameViewport::Create(*visual_viewport, *layout_viewport);

  visual_viewport->SetScale(2);

  root_frame_viewport->SetScrollOffset(ScrollOffset(40, 40), kUserScroll);
  EXPECT_EQ(ScrollOffset(40, 40), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 0), layout_viewport->GetScrollOffset());

  root_frame_viewport->SetScrollOffset(ScrollOffset(60, 60),
                                       kProgrammaticScroll);
  EXPECT_EQ(ScrollOffset(50, 50), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(10, 10), layout_viewport->GetScrollOffset());
}

// Tests that setting an alternate layout viewport scrolls the alternate
// instead of the original.
TEST_F(RootFrameViewportTest, SetAlternateLayoutViewport) {
  IntSize viewport_size(100, 100);
  RootLayoutViewportStub* layout_viewport =
      RootLayoutViewportStub::Create(viewport_size, IntSize(200, 300));
  VisualViewportStub* visual_viewport =
      VisualViewportStub::Create(viewport_size, viewport_size);

  RootLayoutViewportStub* alternate_scroller =
      RootLayoutViewportStub::Create(viewport_size, IntSize(600, 500));

  RootFrameViewport* root_frame_viewport =
      RootFrameViewport::Create(*visual_viewport, *layout_viewport);

  visual_viewport->SetScale(2);

  root_frame_viewport->SetScrollOffset(ScrollOffset(100, 100), kUserScroll);
  EXPECT_EQ(ScrollOffset(50, 50), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 50), layout_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(100, 100), root_frame_viewport->GetScrollOffset());

  root_frame_viewport->SetLayoutViewport(*alternate_scroller);
  EXPECT_EQ(ScrollOffset(50, 50), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 0), alternate_scroller->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 50), root_frame_viewport->GetScrollOffset());

  root_frame_viewport->SetScrollOffset(ScrollOffset(200, 200), kUserScroll);
  EXPECT_EQ(ScrollOffset(50, 50), visual_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(150, 150), alternate_scroller->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(200, 200), root_frame_viewport->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(50, 50), layout_viewport->GetScrollOffset());

  EXPECT_EQ(ScrollOffset(550, 450), root_frame_viewport->MaximumScrollOffset());
}

}  // namespace blink
