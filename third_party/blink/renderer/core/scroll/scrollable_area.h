/*
 * Copyright (C) 2008, 2011 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLABLE_AREA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLABLE_AREA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class CompositorAnimationHost;
class CompositorAnimationTimeline;
class GraphicsLayer;
class LayoutBox;
class LayoutObject;
class PaintLayer;
class ChromeClient;
class ProgrammaticScrollAnimator;
class ScrollAnchor;
class ScrollAnimatorBase;
struct SerializedAnchor;
class SmoothScrollSequencer;
class CompositorAnimationTimeline;
struct WebScrollIntoViewParams;

enum IncludeScrollbarsInRect {
  kExcludeScrollbars,
  kIncludeScrollbars,
};

class CORE_EXPORT ScrollableArea : public GarbageCollectedMixin {
  DISALLOW_COPY_AND_ASSIGN(ScrollableArea);

 public:
  static int PixelsPerLineStep(ChromeClient*);
  static float MinFractionToStepWhenPaging();
  int MaxOverlapBetweenPages() const;

  // Convert a non-finite scroll value (Infinity, -Infinity, NaN) to 0 as
  // per https://drafts.csswg.org/cssom-view/#normalize-non-finite-values.
  static float NormalizeNonFiniteScroll(float value) {
    return std::isfinite(value) ? value : 0.0;
  }

  virtual ChromeClient* GetChromeClient() const { return nullptr; }

  virtual SmoothScrollSequencer* GetSmoothScrollSequencer() const {
    return nullptr;
  }

  virtual ScrollResult UserScroll(ScrollGranularity, const ScrollOffset&);

  virtual void SetScrollOffset(const ScrollOffset&,
                               ScrollType,
                               ScrollBehavior = kScrollBehaviorInstant);
  virtual void ScrollBy(const ScrollOffset&,
                        ScrollType,
                        ScrollBehavior = kScrollBehaviorInstant);
  void SetScrollOffsetSingleAxis(ScrollbarOrientation,
                                 float,
                                 ScrollType,
                                 ScrollBehavior = kScrollBehaviorInstant);

  // Scrolls the area so that the given rect, given in absolute coordinates,
  // such that it's visible in the area. Returns the new location of the input
  // rect in absolute coordinates.
  virtual LayoutRect ScrollIntoView(const LayoutRect&,
                                    const WebScrollIntoViewParams&);

  static bool ScrollBehaviorFromString(const String&, ScrollBehavior&);

  void ContentAreaWillPaint() const;
  void MouseEnteredContentArea() const;
  void MouseExitedContentArea() const;
  void MouseMovedInContentArea() const;
  void MouseEnteredScrollbar(Scrollbar&);
  void MouseExitedScrollbar(Scrollbar&);
  void MouseCapturedScrollbar();
  void MouseReleasedScrollbar();
  void ContentAreaDidShow() const;
  void ContentAreaDidHide() const;

  virtual void SnapAfterScrollbarScrolling(ScrollbarOrientation) {}

  void FinishCurrentScrollAnimations() const;

  virtual void DidAddScrollbar(Scrollbar&, ScrollbarOrientation);
  virtual void WillRemoveScrollbar(Scrollbar&, ScrollbarOrientation);

  // Called when this ScrollableArea becomes or unbecomes the global root
  // scroller.
  virtual void DidChangeGlobalRootScroller() {}

  virtual void ContentsResized();

  bool HasOverlayScrollbars() const;
  void SetScrollbarOverlayColorTheme(ScrollbarOverlayColorTheme);
  void RecalculateScrollbarOverlayColorTheme(Color);
  ScrollbarOverlayColorTheme GetScrollbarOverlayColorTheme() const {
    return static_cast<ScrollbarOverlayColorTheme>(
        scrollbar_overlay_color_theme_);
  }

  // This getter will create a ScrollAnimatorBase if it doesn't already exist.
  ScrollAnimatorBase& GetScrollAnimator() const;

  // This getter will return null if the ScrollAnimatorBase hasn't been created
  // yet.
  ScrollAnimatorBase* ExistingScrollAnimator() const {
    return scroll_animator_;
  }

  ProgrammaticScrollAnimator& GetProgrammaticScrollAnimator() const;
  ProgrammaticScrollAnimator* ExistingProgrammaticScrollAnimator() const {
    return programmatic_scroll_animator_;
  }

  virtual CompositorAnimationHost* GetCompositorAnimationHost() const {
    return nullptr;
  }
  virtual CompositorAnimationTimeline* GetCompositorAnimationTimeline() const {
    return nullptr;
  }

  // This is used to determine whether the incoming fractional scroll offset
  // should be truncated to integer. Current rule is that if
  // preferCompositingToLCDTextEnabled() is disabled (which is true on low-dpi
  // device by default) we should do the truncation.  The justification is that
  // non-composited elements using fractional scroll offsets is causing too much
  // nasty bugs but does not add too benefit on low-dpi devices.
  // TODO(szager): Now that scroll offsets are floats everywhere, can we get rid
  // of this?
  virtual bool ShouldUseIntegerScrollOffset() const {
    return !RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled();
  }

  virtual bool IsActive() const = 0;
  // Returns true if the frame this ScrollableArea is attached to is being
  // throttled for lifecycle updates. In this case it should also not be
  // painted.
  virtual bool IsThrottled() const = 0;
  virtual int ScrollSize(ScrollbarOrientation) const = 0;
  void SetScrollbarNeedsPaintInvalidation(ScrollbarOrientation);
  virtual bool IsScrollCornerVisible() const = 0;
  virtual IntRect ScrollCornerRect() const = 0;
  void SetScrollCornerNeedsPaintInvalidation();
  virtual void GetTickmarks(Vector<IntRect>&) const {}

  // Convert points and rects between the scrollbar and its containing
  // EmbeddedContentView. The client needs to implement these in order to be
  // aware of layout effects like CSS transforms.
  virtual IntRect ConvertFromScrollbarToContainingEmbeddedContentView(
      const Scrollbar& scrollbar,
      const IntRect& scrollbar_rect) const {
    IntRect local_rect = scrollbar_rect;
    local_rect.MoveBy(scrollbar.Location());
    return local_rect;
  }
  virtual IntPoint ConvertFromContainingEmbeddedContentViewToScrollbar(
      const Scrollbar& scrollbar,
      const IntPoint& parent_point) const {
    NOTREACHED();
    return parent_point;
  }
  virtual IntPoint ConvertFromScrollbarToContainingEmbeddedContentView(
      const Scrollbar& scrollbar,
      const IntPoint& scrollbar_point) const {
    NOTREACHED();
    return scrollbar_point;
  }
  virtual IntPoint ConvertFromRootFrame(
      const IntPoint& point_in_root_frame) const {
    NOTREACHED();
    return point_in_root_frame;
  }

  virtual Scrollbar* HorizontalScrollbar() const { return nullptr; }
  virtual Scrollbar* VerticalScrollbar() const { return nullptr; }
  virtual Scrollbar* CreateScrollbar(ScrollbarOrientation) { return nullptr; }

  virtual PaintLayer* Layer() const { return nullptr; }

  // "Scroll offset" is in content-flow-aware coordinates, "Scroll position" is
  // in physical (i.e., not flow-aware) coordinates. Among ScrollableArea
  // sub-classes, only PaintLayerScrollableArea has a real distinction between
  // the two. For a more detailed explanation of scrollPosition, scrollOffset,
  // and scrollOrigin, see core/layout/README.md.
  virtual FloatPoint ScrollPosition() const {
    return FloatPoint(GetScrollOffset());
  }
  virtual FloatPoint ScrollOffsetToPosition(const ScrollOffset& offset) const {
    return FloatPoint(offset);
  }
  virtual ScrollOffset ScrollPositionToOffset(
      const FloatPoint& position) const {
    return ToScrollOffset(position);
  }
  virtual IntSize ScrollOffsetInt() const = 0;
  virtual ScrollOffset GetScrollOffset() const {
    return ScrollOffset(ScrollOffsetInt());
  }
  virtual IntSize MinimumScrollOffsetInt() const = 0;
  virtual ScrollOffset MinimumScrollOffset() const {
    return ScrollOffset(MinimumScrollOffsetInt());
  }
  virtual IntSize MaximumScrollOffsetInt() const = 0;
  virtual ScrollOffset MaximumScrollOffset() const {
    return ScrollOffset(MaximumScrollOffsetInt());
  }

  virtual IntRect VisibleContentRect(
      IncludeScrollbarsInRect = kExcludeScrollbars) const = 0;
  virtual int VisibleHeight() const { return VisibleContentRect().Height(); }
  virtual int VisibleWidth() const { return VisibleContentRect().Width(); }
  virtual IntSize ContentsSize() const = 0;

  // scroll snapport is the area of the scrollport that is used as the alignment
  // container for the scroll snap areas when calculating snap positions. It's
  // the box's scrollport contracted by its scroll-padding.
  // https://drafts.csswg.org/css-scroll-snap-1/#scroll-padding
  virtual LayoutRect VisibleScrollSnapportRect(
      IncludeScrollbarsInRect scrollbar_inclusion = kExcludeScrollbars) const {
    return LayoutRect(VisibleContentRect(scrollbar_inclusion));
  }

  virtual IntPoint LastKnownMousePosition() const { return IntPoint(); }

  virtual bool ShouldSuspendScrollAnimations() const { return true; }
  virtual void ScrollbarStyleChanged() {}
  virtual bool ScrollbarsCanBeActive() const = 0;

  // Returns the bounding box of this scrollable area, in the coordinate system
  // of the top-level FrameView.
  virtual IntRect ScrollableAreaBoundingBox() const = 0;

  virtual CompositorElementId GetCompositorElementId() const = 0;

  virtual CompositorElementId GetScrollbarElementId(
      ScrollbarOrientation orientation);

  virtual bool ScrollAnimatorEnabled() const { return false; }

  // NOTE: Only called from Internals for testing.
  void UpdateScrollOffsetFromInternals(const IntSize&);

  virtual IntSize ClampScrollOffset(const IntSize&) const;
  virtual ScrollOffset ClampScrollOffset(const ScrollOffset&) const;

  // Let subclasses provide a way of asking for and servicing scroll
  // animations.
  virtual bool ScheduleAnimation() { return false; }
  virtual void ServiceScrollAnimations(double monotonic_time);
  virtual void UpdateCompositorScrollAnimations();
  virtual void RegisterForAnimation() {}
  virtual void DeregisterForAnimation() {}

  bool UsesCompositedScrolling() const { return uses_composited_scrolling_; }
  void SetUsesCompositedScrolling(bool uses_composited_scrolling) {
    uses_composited_scrolling_ = uses_composited_scrolling;
  }
  virtual bool ShouldScrollOnMainThread() const;

  // Overlay scrollbars can "fade-out" when inactive.
  virtual bool ScrollbarsHiddenIfOverlay() const;
  virtual void SetScrollbarsHiddenIfOverlay(bool);

  virtual bool UserInputScrollable(ScrollbarOrientation) const = 0;
  virtual bool ShouldPlaceVerticalScrollbarOnLeft() const = 0;

  // Convenience functions
  float MinimumScrollOffset(ScrollbarOrientation orientation) {
    return orientation == kHorizontalScrollbar ? MinimumScrollOffset().Width()
                                               : MinimumScrollOffset().Height();
  }
  float MaximumScrollOffset(ScrollbarOrientation orientation) {
    return orientation == kHorizontalScrollbar ? MaximumScrollOffset().Width()
                                               : MaximumScrollOffset().Height();
  }
  float ClampScrollOffset(ScrollbarOrientation orientation, float offset) {
    return clampTo(offset, MinimumScrollOffset(orientation),
                   MaximumScrollOffset(orientation));
  }

  virtual GraphicsLayer* LayerForContainer() const;
  virtual GraphicsLayer* LayerForScrolling() const { return nullptr; }
  virtual GraphicsLayer* LayerForHorizontalScrollbar() const { return nullptr; }
  virtual GraphicsLayer* LayerForVerticalScrollbar() const { return nullptr; }
  virtual GraphicsLayer* LayerForScrollCorner() const { return nullptr; }
  bool HasLayerForHorizontalScrollbar() const;
  bool HasLayerForVerticalScrollbar() const;
  bool HasLayerForScrollCorner() const;

  void LayerForScrollingDidChange(CompositorAnimationTimeline*);
  bool NeedsShowScrollbarLayers() const { return needs_show_scrollbar_layers_; }
  void DidShowScrollbarLayers() { needs_show_scrollbar_layers_ = false; }

  void CancelScrollAnimation();
  virtual void CancelProgrammaticScrollAnimation();

  virtual ~ScrollableArea();

  // Called when any of horizontal scrollbar, vertical scrollbar and scroll
  // corner is setNeedsPaintInvalidation.
  virtual void ScrollControlWasSetNeedsPaintInvalidation() = 0;

  // Returns the default scroll style this area should scroll with when not
  // explicitly specified. E.g. The scrolling behavior of an element can be
  // specified in CSS.
  virtual ScrollBehavior ScrollBehaviorStyle() const {
    return kScrollBehaviorInstant;
  }

  // Subtracts space occupied by this ScrollableArea's scrollbars.
  // Does nothing if overlay scrollbars are enabled.
  IntSize ExcludeScrollbars(const IntSize&) const;

  virtual int VerticalScrollbarWidth(
      OverlayScrollbarClipBehavior = kIgnorePlatformOverlayScrollbarSize) const;
  virtual int HorizontalScrollbarHeight(
      OverlayScrollbarClipBehavior = kIgnorePlatformOverlayScrollbarSize) const;

  virtual LayoutBox* GetLayoutBox() const { return nullptr; }

  // Maps a quad from the coordinate system of a LayoutObject contained by the
  // ScrollableArea to the coordinate system of the ScrollableArea's visible
  // content rect.  If the LayoutObject* argument is null, the argument quad is
  // considered to be in the coordinate space of the overflow rect.
  virtual FloatQuad LocalToVisibleContentQuad(const FloatQuad&,
                                              const LayoutObject*,
                                              unsigned = 0) const;

  virtual bool IsLocalFrameView() const { return false; }
  virtual bool IsPaintLayerScrollableArea() const { return false; }
  virtual bool IsRootFrameViewport() const { return false; }

  virtual bool VisualViewportSuppliesScrollbars() const { return false; }

  // Returns true if the scroller adjusts the scroll offset to compensate
  // for layout movements (bit.ly/scroll-anchoring).
  virtual bool ShouldPerformScrollAnchoring() const { return false; }

  // Need to promptly let go of owned animator objects.
  EAGERLY_FINALIZE();
  void Trace(blink::Visitor*) override;

  virtual void ClearScrollableArea();

  virtual bool RestoreScrollAnchor(const SerializedAnchor&) { return false; }
  virtual ScrollAnchor* GetScrollAnchor() { return nullptr; }

  virtual void DidScrollWithScrollbar(ScrollbarPart, ScrollbarOrientation) {}

  // Returns the task runner to be used for scrollable area timers.
  // Ideally a frame-specific throttled one can be used.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTimerTaskRunner()
      const = 0;

  // Callback for compositor-side scrolling.
  virtual void DidScroll(const FloatPoint&);

  virtual void ScrollbarFrameRectChanged() {}

  virtual ScrollbarTheme& GetPageScrollbarTheme() const = 0;

  float ScrollStep(ScrollGranularity, ScrollbarOrientation) const;

 protected:
  // Deduces the ScrollBehavior based on the element style and the parameter set
  // by programmatic scroll into either instant or smooth scroll.
  static ScrollBehavior DetermineScrollBehavior(
      ScrollBehavior behavior_from_style,
      ScrollBehavior behavior_from_param);

  ScrollableArea();

  ScrollbarOrientation ScrollbarOrientationFromDirection(
      ScrollDirectionPhysical) const;

  // Needed to let the animators call scrollOffsetChanged.
  friend class ScrollAnimatorCompositorCoordinator;
  void ScrollOffsetChanged(const ScrollOffset&, ScrollType);

  bool HorizontalScrollbarNeedsPaintInvalidation() const {
    return horizontal_scrollbar_needs_paint_invalidation_;
  }
  bool VerticalScrollbarNeedsPaintInvalidation() const {
    return vertical_scrollbar_needs_paint_invalidation_;
  }
  bool ScrollCornerNeedsPaintInvalidation() const {
    return scroll_corner_needs_paint_invalidation_;
  }
  void ClearNeedsPaintInvalidationForScrollControls() {
    horizontal_scrollbar_needs_paint_invalidation_ = false;
    vertical_scrollbar_needs_paint_invalidation_ = false;
    scroll_corner_needs_paint_invalidation_ = false;
  }
  void ShowOverlayScrollbars();

  // Called when scrollbar hides/shows for overlay scrollbars. This callback
  // shouldn't do any significant work as it can be called unexpectadly often
  // on Mac. This happens because painting code has to set alpha to 1, paint,
  // then reset to alpha, causing spurrious "visibilityChanged" calls.
  virtual void ScrollbarVisibilityChanged() {}

  virtual bool HasBeenDisposed() const { return false; }

 private:
  FRIEND_TEST_ALL_PREFIXES(ScrollableAreaTest,
                           PopupOverlayScrollbarShouldNotFadeOut);

  void ProgrammaticScrollHelper(const ScrollOffset&, ScrollBehavior, bool);
  void UserScrollHelper(const ScrollOffset&, ScrollBehavior);

  void FadeOverlayScrollbarsTimerFired(TimerBase*);

  // This function should be overriden by subclasses to perform the actual
  // scroll of the content.
  virtual void UpdateScrollOffset(const ScrollOffset&, ScrollType) = 0;

  virtual int LineStep(ScrollbarOrientation) const;
  virtual int PageStep(ScrollbarOrientation) const;
  virtual int DocumentStep(ScrollbarOrientation) const;
  virtual float PixelStep(ScrollbarOrientation) const;

  mutable Member<ScrollAnimatorBase> scroll_animator_;
  mutable Member<ProgrammaticScrollAnimator> programmatic_scroll_animator_;

  std::unique_ptr<TaskRunnerTimer<ScrollableArea>>
      fade_overlay_scrollbars_timer_;

  unsigned scrollbar_overlay_color_theme_ : 2;

  unsigned horizontal_scrollbar_needs_paint_invalidation_ : 1;
  unsigned vertical_scrollbar_needs_paint_invalidation_ : 1;
  unsigned scroll_corner_needs_paint_invalidation_ : 1;
  unsigned scrollbars_hidden_if_overlay_ : 1;
  unsigned scrollbar_captured_ : 1;
  unsigned mouse_over_scrollbar_ : 1;

  // Indicates that the next compositing update needs to call
  // cc::Layer::ShowScrollbars() on our scroll layer. Ignored if not composited.
  unsigned needs_show_scrollbar_layers_ : 1;
  unsigned uses_composited_scrolling_ : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLABLE_AREA_H_
