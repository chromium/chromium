// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ROOT_FRAME_VIEWPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ROOT_FRAME_VIEWPORT_H_

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/scroll_types.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class LocalFrameView;
struct PhysicalRect;
struct WebScrollIntoViewParams;

// ScrollableArea for the root frame's viewport. This class ties together the
// concepts of layout and visual viewports, used in pinch-to-zoom. This class
// takes two ScrollableAreas, one for the visual viewport and one for the
// layout viewport, and delegates and composes the ScrollableArea API as needed
// between them. For most scrolling APIs, this class will split the scroll up
// between the two viewports in accord with the pinch-zoom semantics. For other
// APIs that don't make sense on the combined viewport, the call is delegated to
// the layout viewport. Thus, we could say this class is a decorator on the
// LocalFrameView scrollable area that adds pinch-zoom semantics to scrolling.
class CORE_EXPORT RootFrameViewport final
    : public GarbageCollected<RootFrameViewport>,
      public ScrollableArea {
  USING_GARBAGE_COLLECTED_MIXIN(RootFrameViewport);

 public:
  RootFrameViewport(ScrollableArea& visual_viewport,
                    ScrollableArea& layout_viewport);

  void Trace(blink::Visitor*) override;

  void SetLayoutViewport(ScrollableArea&);
  ScrollableArea& LayoutViewport() const;

  // Convert from the root content document's coordinate space, into the
  // coordinate space of the layout viewport's content. In the normal case,
  // this will be a no-op since the root LocalFrameView is the layout viewport
  // and so the root content is the layout viewport's content but if the page
  // sets a custom root scroller via document.rootScroller, another element
  // may be the layout viewport.
  PhysicalRect RootContentsToLayoutViewportContents(
      LocalFrameView& root_frame_view,
      const PhysicalRect&) const;

  void RestoreToAnchor(const ScrollOffset&);

  // Callback whenever the visual viewport changes scroll position or scale.
  void DidUpdateVisualViewport();

  // ScrollableArea Implementation
  bool IsRootFrameViewport() const override { return true; }
  void SetScrollOffset(const ScrollOffset&,
                       ScrollType,
                       ScrollBehavior,
                       ScrollCallback on_finish) override;
  PhysicalRect ScrollIntoView(const PhysicalRect&,
                              const WebScrollIntoViewParams&) override;
  IntRect VisibleContentRect(
      IncludeScrollbarsInRect = kExcludeScrollbars) const override;
  PhysicalRect VisibleScrollSnapportRect(
      IncludeScrollbarsInRect = kExcludeScrollbars) const override;
  bool ShouldUseIntegerScrollOffset() const override;
  bool IsThrottled() const override {
    // RootFrameViewport is always in the main frame, so the frame does not get
    // throttled.
    return false;
  }
  bool IsActive() const override;
  int ScrollSize(ScrollbarOrientation) const override;
  bool IsScrollCornerVisible() const override;
  IntRect ScrollCornerRect() const override;
  void UpdateScrollOffset(const ScrollOffset&, ScrollType) override;
  IntSize ScrollOffsetInt() const override;
  ScrollOffset GetScrollOffset() const override;
  IntSize MinimumScrollOffsetInt() const override;
  IntSize MaximumScrollOffsetInt() const override;
  ScrollOffset MaximumScrollOffset() const override;
  IntSize ClampScrollOffset(const IntSize&) const override;
  ScrollOffset ClampScrollOffset(const ScrollOffset&) const override;
  IntSize ContentsSize() const override;
  bool ShouldScrollOnMainThread() const override;
  bool ScrollbarsCanBeActive() const override;
  bool UserInputScrollable(ScrollbarOrientation) const override;
  bool ShouldPlaceVerticalScrollbarOnLeft() const override;
  void ScrollControlWasSetNeedsPaintInvalidation() override;
  cc::Layer* LayerForScrolling() const override;
  cc::Layer* LayerForHorizontalScrollbar() const override;
  cc::Layer* LayerForVerticalScrollbar() const override;
  cc::Layer* LayerForScrollCorner() const override;
  int HorizontalScrollbarHeight(
      OverlayScrollbarClipBehavior =
          kIgnorePlatformOverlayScrollbarSize) const override;
  int VerticalScrollbarWidth(
      OverlayScrollbarClipBehavior =
          kIgnorePlatformOverlayScrollbarSize) const override;
  ScrollResult UserScroll(ScrollGranularity,
                          const FloatSize&,
                          ScrollableArea::ScrollCallback on_finish) override;
  CompositorElementId GetCompositorElementId() const override;
  CompositorElementId GetScrollbarElementId(
      ScrollbarOrientation orientation) override;
  bool ScrollAnimatorEnabled() const override;
  ChromeClient* GetChromeClient() const override;
  SmoothScrollSequencer* GetSmoothScrollSequencer() const override;
  void ServiceScrollAnimations(double) override;
  void UpdateCompositorScrollAnimations() override;
  void CancelProgrammaticScrollAnimation() override;
  ScrollBehavior ScrollBehaviorStyle() const override;
  WebColorScheme UsedColorScheme() const override;
  void ClearScrollableArea() override;
  LayoutBox* GetLayoutBox() const override;
  FloatQuad LocalToVisibleContentQuad(const FloatQuad&,
                                      const LayoutObject*,
                                      unsigned = 0) const final;
  scoped_refptr<base::SingleThreadTaskRunner> GetTimerTaskRunner() const final;
  ScrollbarTheme& GetPageScrollbarTheme() const override;
  const cc::SnapContainerData* GetSnapContainerData() const override;
  void SetSnapContainerData(base::Optional<cc::SnapContainerData>) override;
  base::Optional<FloatPoint> GetSnapPositionAndSetTarget(
      const cc::SnapSelectionStrategy& strategy) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(RootFrameViewportTest, DistributeScrollOrder);

  enum ViewportToScrollFirst { kVisualViewport, kLayoutViewport };

  ScrollOffset ScrollOffsetFromScrollAnimators() const;

  void DistributeScrollBetweenViewports(
      const ScrollOffset&,
      ScrollType,
      ScrollBehavior,
      ViewportToScrollFirst,
      ScrollCallback on_finish = ScrollCallback());

  // If either of the layout or visual viewports are scrolled explicitly (i.e.
  // not through this class), their updated offset will not be reflected in this
  // class' animator so use this method to pull updated values when necessary.
  void UpdateScrollAnimator();

  ScrollableArea& VisualViewport() const {
    DCHECK(visual_viewport_);
    return *visual_viewport_;
  }

  ScrollOffset ClampToUserScrollableOffset(const ScrollOffset&) const;

  Member<ScrollableArea> visual_viewport_;
  Member<ScrollableArea> layout_viewport_;
};

template <>
struct DowncastTraits<RootFrameViewport> {
  static bool AllowFrom(const ScrollableArea& scrollable_area) {
    return scrollable_area.IsRootFrameViewport();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ROOT_FRAME_VIEWPORT_H_
