/*
   Copyright (C) 1997 Martin Jones (mjones@kde.org)
             (C) 1998 Waldo Bastian (bastian@kde.org)
             (C) 1998, 1999 Torben Weis (weis@kde.org)
             (C) 1999 Lars Knoll (knoll@kde.org)
             (C) 1999 Antti Koivisto (koivisto@kde.org)
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
   reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_VIEW_H_

#include <memory>

#include "base/callback_forward.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/frame/frame_view.h"
#include "third_party/blink/renderer/core/frame/layout_subtree_root_list.h"
#include "third_party/blink/renderer/core/frame/overlay_interstitial_ad_detector.h"
#include "third_party/blink/renderer/core/frame/sticky_ad_detector.h"
#include "third_party/blink/renderer/core/layout/depth_ordered_layout_object_list.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_update_type.h"
#include "third_party/blink/renderer/core/paint/layout_object_counter.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/graphics/subtree_paint_property_update_reason.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace cc {
class AnimationHost;
class Layer;
class PaintOpBuffer;
enum class PaintHoldingCommitTrigger;

using PaintRecord = PaintOpBuffer;
}

namespace ui {
class Cursor;
}

namespace blink {
class AXObjectCache;
class ChromeClient;
class CompositorAnimationTimeline;
class DocumentLifecycle;
class FloatRect;
class FloatSize;
class FragmentAnchor;
class Frame;
class FrameViewAutoSizeInfo;
class GraphicsLayer;
class HTMLVideoElement;
class JSONObject;
class KURL;
class LayoutAnalyzer;
class LayoutBox;
class LayoutEmbeddedObject;
class LayoutObject;
class LayoutShiftTracker;
class LayoutSVGRoot;
class LayoutView;
class LocalFrame;
class Page;
class PaintArtifactCompositor;
class PaintController;
class PaintLayer;
class PaintLayerScrollableArea;
class PaintTimingDetector;
class RootFrameViewport;
class ScrollableArea;
class Scrollbar;
class ScrollingCoordinator;
class ScrollingCoordinatorContext;
class TracedValue;
class TransformState;
class LocalFrameUkmAggregator;
class WebPluginContainerImpl;
struct AnnotatedRegionValue;
struct IntrinsicSizingInfo;
struct PhysicalOffset;
struct PhysicalRect;

typedef uint64_t DOMTimeStamp;
using LayerTreeFlags = unsigned;
using MainThreadScrollingReasons = uint32_t;

struct LifecycleData {
  LifecycleData() {}
  LifecycleData(base::TimeTicks start_time_arg, int count_arg)
      : start_time(start_time_arg), count(count_arg) {}
  base::TimeTicks start_time;
  // The number of lifecycles that have occcurred since the first one,
  // inclusive, on a given LocalFrameRoot.
  unsigned count = 0;
};

class CORE_EXPORT LocalFrameView final
    : public GarbageCollected<LocalFrameView>,
      public FrameView {
  friend class PaintControllerPaintTestBase;
  friend class Internals;

 public:
  class CORE_EXPORT LifecycleNotificationObserver
      : public GarbageCollectedMixin {
   public:
    // These are called when the lifecycle updates start/finish.
    virtual void WillStartLifecycleUpdate(const LocalFrameView&) {}
    virtual void DidFinishLifecycleUpdate(const LocalFrameView&) {}
  };

  explicit LocalFrameView(LocalFrame&);
  LocalFrameView(LocalFrame&, const IntSize& initial_size);
  ~LocalFrameView() override;

  void Invalidate() { InvalidateRect(IntRect(0, 0, Width(), Height())); }
  void InvalidateRect(const IntRect&);

  LocalFrame& GetFrame() const {
    DCHECK(frame_);
    return *frame_;
  }

  Page* GetPage() const;

  LayoutView* GetLayoutView() const;

  void SetCanHaveScrollbars(bool can_have_scrollbars) {
    can_have_scrollbars_ = can_have_scrollbars;
  }
  bool CanHaveScrollbars() const { return can_have_scrollbars_; }
  bool VisualViewportSuppliesScrollbars();

  void SetLayoutOverflowSize(const IntSize&);

  void UpdateLayout();
  bool DidFirstLayout() const;
  bool LifecycleUpdatesActive() const;
  void SetLifecycleUpdatesThrottledForTesting(bool throttled = true);
  void ScheduleRelayout();
  void ScheduleRelayoutOfSubtree(LayoutObject*);
  bool LayoutPending() const;
  bool IsInPerformLayout() const;

  // Methods to capture forced layout metrics.
  void WillStartForcedLayout();
  void DidFinishForcedLayout(DocumentUpdateReason);

  void ClearLayoutSubtreeRoot(const LayoutObject&);
  void AddOrthogonalWritingModeRoot(LayoutBox&);
  void RemoveOrthogonalWritingModeRoot(LayoutBox&);
  bool HasOrthogonalWritingModeRoots() const;
  void LayoutOrthogonalWritingModeRoots();
  void ScheduleOrthogonalWritingModeRootsForLayout();
  void MarkOrthogonalWritingModeRootsForLayout();

  unsigned LayoutCountForTesting() const { return layout_count_for_testing_; }
  unsigned LifecycleUpdateCountForTesting() const {
    return lifecycle_update_count_for_testing_;
  }

  void CountObjectsNeedingLayout(unsigned& needs_layout_objects,
                                 unsigned& total_objects,
                                 bool& is_partial);

  bool NeedsLayout() const;
  bool CheckDoesNotNeedLayout() const;
  void SetNeedsLayout();

  void SetNeedsUpdateGeometries() { needs_update_geometries_ = true; }
  void UpdateGeometry() override;

  // Marks this frame, and ancestor frames, as needing one intersection
  // observervation. This overrides throttling for one frame, up to
  // kLayoutClean. The order of these enums is important - they must proceed
  // from "least required to most required".
  enum IntersectionObservationState {
    // The next painting frame does not need an intersection observation.
    kNotNeeded = 0,
    // The next painting frame needs an intersection observation.
    kDesired = 1,
    // The next painting frame must be generated up to intersection observation
    // (even if frame is throttled).
    kRequired = 2
  };

  // Sets the internal IntersectionObservationState to the max of the
  // current value and the provided one.
  void SetIntersectionObservationState(IntersectionObservationState);
  IntersectionObservationState GetIntersectionObservationStateForTesting()
      const {
    return intersection_observation_state_;
  }

  // Get the InstersectionObservation::ComputeFlags for target elements in this
  // view.
  unsigned GetIntersectionObservationFlags(unsigned parent_flags) const;

  void ForceUpdateViewportIntersections();

  void SetPaintArtifactCompositorNeedsUpdate();
  void SetForeignLayerListNeedsUpdate();

  // Marks this frame, and ancestor frames, as needing a mandatory compositing
  // update. This overrides throttling for one frame, up to kCompositingClean.
  void SetNeedsForcedCompositingUpdate();
  void ResetNeedsForcedCompositingUpdate() {
    needs_forced_compositing_update_ = false;
  }

  // Methods for getting/setting the size Blink should use to layout the
  // contents.
  IntSize GetLayoutSize() const { return layout_size_; }
  void SetLayoutSize(const IntSize&);

  // If this is set to false, the layout size will need to be explicitly set by
  // the owner.  E.g. WebViewImpl sets its mainFrame's layout size manually
  void SetLayoutSizeFixedToFrameSize(bool);
  bool LayoutSizeFixedToFrameSize() { return layout_size_fixed_to_frame_size_; }

  void SetInitialViewportSize(const IntSize&);
  int InitialViewportWidth() const;
  int InitialViewportHeight() const;

  bool GetIntrinsicSizingInfo(IntrinsicSizingInfo&) const override;
  bool HasIntrinsicSizingInfo() const override;

  void UpdateCountersAfterStyleChange();

  void Dispose() override;
  void PropagateFrameRects() override;
  void InvalidateAllCustomScrollbarsOnActiveChanged();

  Color BaseBackgroundColor() const;
  void SetBaseBackgroundColor(const Color&);
  void UpdateBaseBackgroundColorRecursively(const Color&);

  enum class UseColorAdjustBackground {
    // Use the base background color set on this view.
    kNo,
    // Use the color-adjust background from StyleEngine instead of the base
    // background color.
    kYes,
    // Use the color-adjust background from StyleEngine, but only if the base
    // background is not transparent.
    kIfBaseNotTransparent,
  };

  void SetUseColorAdjustBackground(UseColorAdjustBackground use,
                                   bool color_scheme_changed);
  bool ShouldPaintBaseBackgroundColor() const;

  void AdjustViewSize();
  void AdjustViewSizeAndLayout();

  // Scale used to convert incoming input events.
  float InputEventsScaleFactor() const;

  void DidChangeScrollOffset();

  void ViewportSizeChanged(bool width_changed, bool height_changed);
  void MarkViewportConstrainedObjectsForLayout(bool width_changed,
                                               bool height_changed);

  AtomicString MediaType() const;
  void SetMediaType(const AtomicString&);
  void AdjustMediaTypeForPrinting(bool printing);

  // For any viewport-constrained object, we need to know if it's due to fixed
  // or sticky so that we can support HasStickyViewportConstrainedObject().
  enum ViewportConstrainedType { kFixed = 0, kSticky = 1 };
  // Fixed-position and viewport-constrained sticky-position objects.
  typedef HashSet<LayoutObject*> ObjectSet;
  void AddViewportConstrainedObject(LayoutObject&, ViewportConstrainedType);
  void RemoveViewportConstrainedObject(LayoutObject&, ViewportConstrainedType);
  const ObjectSet* ViewportConstrainedObjects() const {
    return viewport_constrained_objects_.get();
  }
  bool HasViewportConstrainedObjects() const {
    return viewport_constrained_objects_ &&
           viewport_constrained_objects_->size() > 0;
  }
  // Returns true if any of the objects in viewport_constrained_objects_ are
  // sticky position.
  bool HasStickyViewportConstrainedObject() const {
    DCHECK(!sticky_position_object_count_ || HasViewportConstrainedObjects());
    return sticky_position_object_count_ > 0;
  }

  // Objects with background-attachment:fixed.
  void AddBackgroundAttachmentFixedObject(LayoutObject*);
  void RemoveBackgroundAttachmentFixedObject(LayoutObject*);
  bool RequiresMainThreadScrollingForBackgroundAttachmentFixed() const;
  const ObjectSet& BackgroundAttachmentFixedObjects() const {
    return background_attachment_fixed_objects_;
  }
  void InvalidateBackgroundAttachmentFixedDescendantsOnScroll(
      const LayoutObject& scrolled_object);

  void HandleLoadCompleted();

  void UpdateDocumentAnnotatedRegions() const;

  void DidAttachDocument();

  bool SafeToPropagateScrollToParent() const {
    return safe_to_propagate_scroll_to_parent_;
  }
  void SetSafeToPropagateScrollToParent(bool is_safe) {
    safe_to_propagate_scroll_to_parent_ = is_safe;
  }

  void AddPartToUpdate(LayoutEmbeddedObject&);

  Color DocumentBackgroundColor() const;

  // Called when this view is going to be removed from its owning
  // LocalFrame.
  void WillBeRemovedFromFrame();

  bool IsUpdatingLifecycle() {
    return current_update_lifecycle_phases_target_state_ !=
           DocumentLifecycle::kUninitialized;
  }

  // Run all needed lifecycle stages. After calling this method, all frames will
  // be in the lifecycle state PaintClean.  If lifecycle throttling is allowed
  // (see DocumentLifecycle::AllowThrottlingScope), some frames may skip the
  // lifecycle update (e.g., based on visibility) and will not end up being
  // PaintClean. Set |reason| to indicate the reason for this update,
  // for metrics purposes.
  // Returns whether the lifecycle was successfully updated to PaintClean.
  bool UpdateAllLifecyclePhases(DocumentUpdateReason reason);

  // Computes the style, layout, compositing and pre-paint lifecycle stages
  // if needed.
  // After calling this method, all frames will be in a lifecycle
  // state >= PrePaintClean, unless the frame was throttled or inactive.
  // Returns whether the lifecycle was successfully updated to the
  // desired state.
  bool UpdateAllLifecyclePhasesExceptPaint(DocumentUpdateReason reason);

  // Printing needs everything up-to-date except paint (which will be done
  // specially). We may also print a detached frame or a descendant of a
  // detached frame and need special handling of the frame.
  void UpdateLifecyclePhasesForPrinting();

  bool UpdateLifecycleToPrePaintClean(DocumentUpdateReason reason);

  // After calling this method, all frames will be in a lifecycle
  // state >= CompositingClean, and scrolling has been updated (unless
  // throttling is allowed), unless the frame was throttled or inactive.
  // Returns whether the lifecycle was successfully updated to the
  // desired state.
  bool UpdateLifecycleToCompositingCleanPlusScrolling(
      DocumentUpdateReason reason);

  // Computes the style, layout, and compositing inputs lifecycle stages if
  // needed. After calling this method, all frames will be in a lifecycle state
  // >= CompositingInputsClean, unless the frame was throttled or inactive.
  // Returns whether the lifecycle was successfully updated to the
  // desired state.
  bool UpdateLifecycleToCompositingInputsClean(DocumentUpdateReason reason);

  // Computes only the style and layout lifecycle stages.
  // After calling this method, all frames will be in a lifecycle
  // state >= LayoutClean, unless the frame was throttled or inactive.
  // Returns whether the lifecycle was successfully updated to the
  // desired state.
  bool UpdateLifecycleToLayoutClean(DocumentUpdateReason reason);

  void SetInLifecycleUpdateForTest(bool val) { in_lifecycle_update_ = val; }

  // This for doing work that needs to run synchronously at the end of lifecyle
  // updates, but needs to happen outside of the lifecycle code. It's OK to
  // schedule another animation frame here, but the layout tree should not be
  // invalidated.
  void RunPostLifecycleSteps();

  void ScheduleVisualUpdateForPaintInvalidationIfNeeded();

  bool InvalidateViewportConstrainedObjects();

  void IncrementLayoutObjectCount() { layout_object_counter_.Increment(); }
  void IncrementVisuallyNonEmptyCharacterCount(unsigned);
  void IncrementVisuallyNonEmptyPixelCount(const IntSize&);
  bool IsVisuallyNonEmpty() const { return is_visually_non_empty_; }
  void SetIsVisuallyNonEmpty() { is_visually_non_empty_ = true; }
  void EnableAutoSizeMode(const IntSize& min_size, const IntSize& max_size);
  void DisableAutoSizeMode();

  void ForceLayoutForPagination(const FloatSize& page_size,
                                const FloatSize& original_page_size,
                                float maximum_shrink_factor);

  // Updates the fragment anchor element based on URL's fragment identifier.
  // Updates corresponding ':target' CSS pseudo class on the anchor element.
  // If |should_scroll| is passed it can be used to prevent scrolling/focusing
  // while still performing all related side-effects like setting :target (used
  // for e.g. in history restoration to override the scroll offset). The scroll
  // offset is maintained during the frame loading process.
  void ProcessUrlFragment(const KURL&,
                          bool same_document_navigation,
                          bool should_scroll = true);
  FragmentAnchor* GetFragmentAnchor() { return fragment_anchor_; }
  void InvokeFragmentAnchor();
  void DismissFragmentAnchor();

  bool ShouldSetCursor() const;

  void SetCursor(const ui::Cursor&);

  // FIXME: Remove this method once plugin loading is decoupled from layout.
  void FlushAnyPendingPostLayoutTasks();

  // These methods are for testing.
  void SetTracksRasterInvalidations(bool);
  bool IsTrackingRasterInvalidations() const {
    return is_tracking_raster_invalidations_;
  }

  using ScrollableAreaSet = HeapHashSet<Member<PaintLayerScrollableArea>>;
  void AddScrollableArea(PaintLayerScrollableArea*);
  void RemoveScrollableArea(PaintLayerScrollableArea*);
  const ScrollableAreaSet* ScrollableAreas() const {
    return scrollable_areas_.Get();
  }

  void AddAnimatingScrollableArea(PaintLayerScrollableArea*);
  void RemoveAnimatingScrollableArea(PaintLayerScrollableArea*);
  const ScrollableAreaSet* AnimatingScrollableAreas() const {
    return animating_scrollable_areas_.Get();
  }

  void ScheduleAnimation(base::TimeDelta = base::TimeDelta());

  // FIXME: This should probably be renamed as the 'inSubtreeLayout' parameter
  // passed around the LocalFrameView layout methods can be true while this
  // returns false.
  bool IsSubtreeLayout() const { return !layout_subtree_root_list_.IsEmpty(); }

  // The window that hosts the LocalFrameView. The LocalFrameView will
  // communicate scrolls and repaints to the host window in the window's
  // coordinate space.
  ChromeClient* GetChromeClient() const;

  LocalFrameView* ParentFrameView() const override;
  LayoutEmbeddedContent* GetLayoutEmbeddedContent() const override;
  void AttachToLayout() override;
  void DetachFromLayout() override;
  using PluginSet = HeapHashSet<Member<WebPluginContainerImpl>>;
  const PluginSet& Plugins() const { return plugins_; }
  void AddPlugin(WebPluginContainerImpl*);
  void RemovePlugin(WebPluginContainerImpl*);
  // Custom scrollbars in PaintLayerScrollableArea need to be called with
  // StyleChanged whenever window focus is changed.
  void RemoveScrollbar(Scrollbar*);
  void AddScrollbar(Scrollbar*);

  // Clips the provided rect to the visible content area. For this purpose, we
  // also query the chrome client for any active overrides to the visible area
  // (e.g. DevTool's viewport override).
  void ClipPaintRect(FloatRect*) const;

  // Indicates the root layer's scroll offset changed since the last frame
  void SetRootLayerDidScroll() { root_layer_did_scroll_ = true; }

  // Methods for converting between this frame and other coordinate spaces.
  // For definitions and an explanation of the varous spaces, please see:
  // http://www.chromium.org/developers/design-documents/blink-coordinate-spaces
  IntRect ViewportToFrame(const IntRect&) const;
  IntRect FrameToViewport(const IntRect&) const;
  IntPoint FrameToViewport(const IntPoint&) const;
  IntPoint ViewportToFrame(const IntPoint&) const;
  FloatPoint ViewportToFrame(const FloatPoint&) const;
  PhysicalOffset ViewportToFrame(const PhysicalOffset&) const;

  // FIXME: Some external callers expect to get back a rect that's positioned
  // in viewport space, but sized in CSS pixels. This is an artifact of the
  // old pinch-zoom path. These callers should be converted to expect a rect
  // fully in viewport space. crbug.com/459591.
  IntPoint SoonToBeRemovedUnscaledViewportToContents(const IntPoint&) const;

  // Functions for converting to screen coordinates.
  IntRect FrameToScreen(const IntRect&) const;

  // Converts from/to local frame coordinates to the root frame coordinates.
  IntRect ConvertToRootFrame(const IntRect&) const;
  IntPoint ConvertToRootFrame(const IntPoint&) const;
  PhysicalOffset ConvertToRootFrame(const PhysicalOffset&) const;
  FloatPoint ConvertToRootFrame(const FloatPoint&) const;
  PhysicalRect ConvertToRootFrame(const PhysicalRect&) const;
  IntRect ConvertFromRootFrame(const IntRect&) const;
  IntPoint ConvertFromRootFrame(const IntPoint&) const;
  FloatPoint ConvertFromRootFrame(const FloatPoint&) const;
  PhysicalOffset ConvertFromRootFrame(const PhysicalOffset&) const;

  IntRect RootFrameToDocument(const IntRect&);
  IntPoint RootFrameToDocument(const IntPoint&);
  FloatPoint RootFrameToDocument(const FloatPoint&);
  IntPoint DocumentToFrame(const IntPoint&) const;
  FloatPoint DocumentToFrame(const FloatPoint&) const;
  DoublePoint DocumentToFrame(const DoublePoint&) const;
  PhysicalOffset DocumentToFrame(const PhysicalOffset&) const;
  IntRect DocumentToFrame(const IntRect&) const;
  PhysicalRect DocumentToFrame(const PhysicalRect&) const;
  IntPoint FrameToDocument(const IntPoint&) const;
  PhysicalOffset FrameToDocument(const PhysicalOffset&) const;
  IntRect FrameToDocument(const IntRect&) const;
  PhysicalRect FrameToDocument(const PhysicalRect&) const;

  // Normally a LocalFrameView synchronously paints during full lifecycle
  // updates, into the local frame root's PaintController (CompositeAfterPaint)
  // or the PaintControllers of GraphicsLayers (pre-CompositeAfterPaint)
  // However, in some cases (e.g. when printing) we need to paint the frame view
  // into a PaintController other than the default one(s). The following
  // functions are provided for these cases. This frame view must be in
  // PrePaintClean or PaintClean state when these functions are called.
  void PaintOutsideOfLifecycle(
      GraphicsContext&,
      const GlobalPaintFlags,
      const CullRect& cull_rect = CullRect::Infinite());
  void PaintContentsOutsideOfLifecycle(GraphicsContext&,
                                       const GlobalPaintFlags,
                                       const CullRect&);

  // Get the PaintRecord based on the cached paint artifact generated during
  // the last paint in lifecycle update. For CompositeAfterPaint only.
  sk_sp<cc::PaintRecord> GetPaintRecord() const;

  void Show() override;
  void Hide() override;

  bool IsLocalFrameView() const override { return true; }
  bool ShouldReportMainFrameIntersection() const override { return true; }

  void Trace(Visitor*) const override;
  void NotifyPageThatContentAreaWillPaint() const;

  // Returns the scrollable area for the frame. For the root frame, this will
  // be the RootFrameViewport, which adds pinch-zoom semantics to scrolling.
  // For non-root frames, this will be the ScrollableArea of the LayoutView.
  ScrollableArea* GetScrollableArea();

  // Returns the ScrollableArea of the LayoutView, a.k.a. the layout viewport.
  // In the root frame, this is the "outer" viewport in the pinch-zoom dual
  // viewport model.  Callers that need awareness of both inner and outer
  // viewports should use GetScrollableArea() instead.
  PaintLayerScrollableArea* LayoutViewport() const;

  // If this is the main frame, this will return the RootFrameViewport used
  // to scroll the main frame. Otherwise returns nullptr. Unless you need a
  // unique method on RootFrameViewport, you should probably use
  // getScrollableArea.
  RootFrameViewport* GetRootFrameViewport();

  int ViewportWidth() const;

  LayoutAnalyzer* GetLayoutAnalyzer() { return analyzer_.get(); }

  // Returns true if this frame should not render or schedule visual updates.
  bool ShouldThrottleRendering() const;

  bool CanThrottleRendering() const override;
  void UpdateRenderThrottlingStatus(bool hidden_for_throttling,
                                    bool subtree_throttled,
                                    bool recurse = false) override;

  void BeginLifecycleUpdates();

  // Records a timestamp in PaintTiming when the frame is first not
  // render-throttled (since it last was throttled if applicable).
  void MarkFirstEligibleToPaint();

  // Resets the optional timestamp in PaintTiming to null to indicate
  // that the frame is now render-throttled, unless the frame already has
  // a first contentful paint. This is a necessary workaround, as when
  // constructing the frame, HTMLConstructionSite::InsertHTMLBodyElement
  // initiates a call via Document::WillInsertBody to begin lifecycle
  // updates, and hence |lifecycle_updates_throttled_| is set to false, which
  // can cause the frame to be briefly unthrottled and receive a paint
  // eligibility timestamp, even if the frame is throttled shortly thereafter
  // and not actually painted.
  void MarkIneligibleToPaint();

  // Shorthands of LayoutView's corresponding methods.
  void SetNeedsPaintPropertyUpdate();

  // Viewport size that should be used for viewport units (i.e. 'vh'/'vw').
  // May include the size of browser controls. See implementation for further
  // documentation.
  FloatSize ViewportSizeForViewportUnits() const;

  // Initial containing block size for evaluating viewport-dependent media
  // queries.
  FloatSize ViewportSizeForMediaQueries() const;

  void EnqueueScrollAnchoringAdjustment(ScrollableArea*);
  void DequeueScrollAnchoringAdjustment(ScrollableArea*);
  void PerformScrollAnchoringAdjustments();

  void SetNeedsEnqueueScrollEvent(PaintLayerScrollableArea*);

  // Only for CompositeAfterPaint.
  std::unique_ptr<JSONObject> CompositedLayersAsJSON(LayerTreeFlags);

  String MainThreadScrollingReasonsAsText();
  // Main thread scrolling reasons including reasons from ancestors.
  MainThreadScrollingReasons GetMainThreadScrollingReasons() const;
  // Main thread scrolling reasons for this object only. For all reasons,
  // see: mainThreadScrollingReasons().
  MainThreadScrollingReasons MainThreadScrollingReasonsPerFrame() const;

  bool HasVisibleSlowRepaintViewportConstrainedObjects() const;

  bool MapToVisualRectInRemoteRootFrame(PhysicalRect& rect,
                                        bool apply_overflow_clip = true);

  void MapLocalToRemoteMainFrame(TransformState&);

  void CrossOriginToMainFrameChanged();
  void CrossOriginToParentFrameChanged();

  // The visual viewport can supply scrollbars.
  void VisualViewportScrollbarsChanged();

  void SetVisualViewportNeedsRepaint() {
    visual_viewport_needs_repaint_ = true;
  }
  bool VisualViewportNeedsRepaint() const {
    return visual_viewport_needs_repaint_;
  }

  LayoutUnit CaretWidth() const;

  size_t PaintFrameCount() const { return paint_frame_count_; }

  // Return the ScrollableArea in a FrameView with the given ElementId, if any.
  // This is not recursive and will only return ScrollableAreas owned by this
  // LocalFrameView (or possibly the LocalFrameView itself).
  ScrollableArea* ScrollableAreaWithElementId(const CompositorElementId&);

  // When the frame is a local root and not a main frame, any recursive
  // scrolling should continue in the parent process.
  void ScrollRectToVisibleInRemoteParent(const PhysicalRect&,
                                         mojom::blink::ScrollIntoViewParamsPtr);

  PaintArtifactCompositor* GetPaintArtifactCompositor() const;

  const cc::Layer* RootCcLayer() const;

  // Should be called whenever this LocalFrameView adds or removes a
  // scrollable area, or gains/loses a composited layer.
  void ScrollableAreasDidChange();

  ScrollingCoordinatorContext* GetScrollingContext() const;
  cc::AnimationHost* GetCompositorAnimationHost() const;
  CompositorAnimationTimeline* GetCompositorAnimationTimeline() const;

  LayoutShiftTracker& GetLayoutShiftTracker() { return *layout_shift_tracker_; }
  PaintTimingDetector& GetPaintTimingDetector() const {
    return *paint_timing_detector_;
  }

  // Return the UKM aggregator for this frame, creating it if necessary.
  LocalFrameUkmAggregator& EnsureUkmAggregator();

  // Report the First Contentful Paint signal to the LocalFrameView.
  // This causes Deferred Commits to be restarted and tells the UKM
  // aggregator that FCP has been reached.
  void OnFirstContentfulPaint();

#if DCHECK_IS_ON()
  void SetIsUpdatingDescendantDependentFlags(bool val) {
    is_updating_descendant_dependent_flags_ = val;
  }
  bool IsUpdatingDescendantDependentFlags() const {
    return is_updating_descendant_dependent_flags_;
  }
#endif

  void RegisterForLifecycleNotifications(LifecycleNotificationObserver*);
  void UnregisterFromLifecycleNotifications(LifecycleNotificationObserver*);

  // Enqueue tasks to be run at the start of the next lifecycle. These tasks
  // will run right after `WillStartLifecycleUpdate()` on the lifecycle
  // notification observers.
  void EnqueueStartOfLifecycleTask(base::OnceClosure);

  // For testing way to steal the start-of-lifecycle tasks.
  WTF::Vector<base::OnceClosure> TakeStartOfLifecycleTasksForTest() {
    return std::move(start_of_lifecycle_tasks_);
  }

  // Called when the "dominant visible" status has changed for a
  // HTMLVideoElement in the page. "dominant visible" means the element is
  // mostly filling the viewport.
  void NotifyVideoIsDominantVisibleStatus(HTMLVideoElement* element,
                                          bool is_dominant);

  bool HasDominantVideoElement() const;

  PaintLayer* GetFullScreenOverlayLayer() const;

 protected:
  void FrameRectsChanged(const IntRect&) override;
  void SelfVisibleChanged() override;
  void ParentVisibleChanged() override;
  void NotifyFrameRectsChangedIfNeeded();
  void SetViewportIntersection(
      const ViewportIntersectionState& intersection_state) override {}
  void VisibilityForThrottlingChanged() override;
  bool LifecycleUpdatesThrottled() const override {
    return lifecycle_updates_throttled_;
  }
  void VisibilityChanged(blink::mojom::FrameVisibility visibility) override;

  void EnqueueScrollEvents();

 private:
  LocalFrameView(LocalFrame&, IntRect);

#if DCHECK_IS_ON()
  class DisallowLayoutInvalidationScope {
    STACK_ALLOCATED();

   public:
    explicit DisallowLayoutInvalidationScope(LocalFrameView* view);
    ~DisallowLayoutInvalidationScope();

   private:
    LocalFrameView* local_frame_view_;
  };
#endif

  // A paint preview is a copy of the visual contents of a webpage recorded as
  // a set of SkPictures. This sends an IPC to the browser to trigger a
  // recording of this frame as a separate SkPicture. An ID is added to the
  // canvas of |context| at |paint_offset| to track the correct position of
  // this frame relative to its parent. Returns true on successfully creating
  // a placeholder and sending an IPC to the browser.
  bool CapturePaintPreview(GraphicsContext& context,
                           const IntSize& paint_offset) const;

  // EmbeddedContentView implementation
  void Paint(GraphicsContext&,
             const GlobalPaintFlags,
             const CullRect&,
             const IntSize& = IntSize()) const final;

  void PaintInternal(GraphicsContext&,
                     const GlobalPaintFlags,
                     const CullRect&) const;

  LayoutSVGRoot* EmbeddedReplacedContent() const;

  // Returns whether the lifecycle was successfully updated to the
  // target state.
  bool UpdateLifecyclePhases(DocumentLifecycle::LifecycleState target_state,
                             DocumentUpdateReason reason);
  // The internal version that does the work after the proper context and checks
  // have passed in the above function call.
  void UpdateLifecyclePhasesInternal(
      DocumentLifecycle::LifecycleState target_state);
  // Four lifecycle phases helper functions corresponding to StyleAndLayout,
  // Compositing, PrePaint, and Paint phases. If the return value is true, it
  // means further lifecycle phases need to be run. This is used to abort
  // earlier if we don't need to run future lifecycle phases.
  bool RunStyleAndLayoutLifecyclePhases(
      DocumentLifecycle::LifecycleState target_state);
  bool RunAccessibilityLifecyclePhase(
      DocumentLifecycle::LifecycleState target_state);
  bool RunCompositingInputsLifecyclePhase(
      DocumentLifecycle::LifecycleState target_state);
  bool RunCompositingAssignmentsLifecyclePhase(
      DocumentLifecycle::LifecycleState target_state);
  bool RunPrePaintLifecyclePhase(
      DocumentLifecycle::LifecycleState target_state);
  void RunPaintLifecyclePhase();

  void PaintTree(HashSet<const GraphicsLayer*>& repainted_layers);
  void UpdateStyleAndLayoutIfNeededRecursive();

  void PushPaintArtifactToCompositor(
      const HashSet<const GraphicsLayer*>& repainted_layers);

  void ClearLayoutSubtreeRootsAndMarkContainingBlocks();

  void PerformPreLayoutTasks();
  void PerformLayout(bool in_subtree_layout);
  void PerformPostLayoutTasks();

  DocumentLifecycle& Lifecycle() const;

  void RunIntersectionObserverSteps();
  void RenderThrottlingStatusChanged();

  // Methods to do point conversion via layoutObjects, in order to take
  // transforms into account.
  IntRect ConvertToContainingEmbeddedContentView(const IntRect&) const;
  IntPoint ConvertToContainingEmbeddedContentView(const IntPoint&) const;
  PhysicalOffset ConvertToContainingEmbeddedContentView(
      const PhysicalOffset&) const;
  FloatPoint ConvertToContainingEmbeddedContentView(const FloatPoint&) const;
  IntRect ConvertFromContainingEmbeddedContentView(const IntRect&) const;
  IntPoint ConvertFromContainingEmbeddedContentView(const IntPoint&) const;
  PhysicalOffset ConvertFromContainingEmbeddedContentView(
      const PhysicalOffset&) const;
  FloatPoint ConvertFromContainingEmbeddedContentView(const FloatPoint&) const;
  DoublePoint ConvertFromContainingEmbeddedContentView(
      const DoublePoint&) const;

  void InvalidateForThrottlingChange();

  void UpdateGeometriesIfNeeded();
  bool WasViewportResized();
  void SendResizeEventIfNeeded();

  void ScheduleUpdatePluginsIfNecessary();
  void UpdatePluginsTimerFired(TimerBase*);
  bool UpdatePlugins();

  void UpdateCompositedSelectionIfNeeded();
  void SetNeedsCompositingUpdate(CompositingUpdateType);

  AXObjectCache* ExistingAXObjectCache() const;

  void SetLayoutSizeInternal(const IntSize&);

  ScrollingCoordinator* GetScrollingCoordinator() const;

  void PrepareLayoutAnalyzer();
  std::unique_ptr<TracedValue> AnalyzerCounters();

  void CollectAnnotatedRegions(LayoutObject&,
                               Vector<AnnotatedRegionValue>&) const;

  template <typename Function>
  void ForAllChildViewsAndPlugins(const Function&);

  template <typename Function>
  void ForAllChildLocalFrameViews(const Function&);

  template <typename Function>
  void ForAllNonThrottledLocalFrameViews(const Function&);

  template <typename Function>
  void ForAllThrottledLocalFrameViews(const Function&);

  void ForAllThrottledLocalFrameViewsForTesting(
      base::RepeatingCallback<void(LocalFrameView&)>);

  template <typename Function>
  void ForAllRemoteFrameViews(const Function&);

  bool UpdateViewportIntersectionsForSubtree(unsigned parent_flags) override;
  void DeliverSynchronousIntersectionObservations();

  bool NotifyResizeObservers(DocumentLifecycle::LifecycleState target_state);
  bool RunResizeObserverSteps(DocumentLifecycle::LifecycleState target_state);
  void ClearResizeObserverLimit();

  bool CheckLayoutInvalidationIsAllowed() const;

  // This runs the intersection observer steps for observations that need to
  // happen in post-layout. These results are also delivered (if needed) in the
  // same call. Returns true if the lifecycle should process style and layout
  // again before proceeding.
  bool RunPostLayoutIntersectionObserverSteps();
  // This is a recursive helper for determining intersection observations which
  // need to happen in post-layout.
  void ComputePostLayoutIntersections(unsigned parent_flags);

  PaintController* GetPaintController() { return paint_controller_.get(); }

  // Returns true if the root object was laid out. Returns false if the layout
  // was prevented (e.g. by ancestor display-lock) or not needed.
  bool LayoutFromRootObject(LayoutObject& root);

  void UpdateLayerDebugInfoEnabled();

  // Return the interstitial-ad detector for this frame, creating it if
  // necessary.
  OverlayInterstitialAdDetector& EnsureOverlayInterstitialAdDetector();

  WTF::Vector<const TransformPaintPropertyNode*> GetScrollTranslationNodes();

  // Return the sticky-ad detector for this frame, creating it if necessary.
  StickyAdDetector& EnsureStickyAdDetector();

  // Returns true if we should paint the color adjust background from the
  // StyleEngine instead of the base background color.
  bool ShouldUseColorAdjustBackground() const;

  LayoutSize size_;

  typedef HashSet<scoped_refptr<LayoutEmbeddedObject>> EmbeddedObjectSet;
  EmbeddedObjectSet part_update_set_;

  Member<LocalFrame> frame_;

  bool can_have_scrollbars_;

  bool has_pending_layout_;
  LayoutSubtreeRootList layout_subtree_root_list_;
  DepthOrderedLayoutObjectList orthogonal_writing_mode_root_list_;

  bool layout_scheduling_enabled_;
  unsigned layout_count_for_testing_;
  unsigned lifecycle_update_count_for_testing_;
  unsigned nested_layout_count_;
  TaskRunnerTimer<LocalFrameView> update_plugins_timer_;

  bool first_layout_;
  UseColorAdjustBackground use_color_adjust_background_{
      UseColorAdjustBackground::kNo};
  Color base_background_color_;
  IntSize last_viewport_size_;
  float last_zoom_factor_;

  AtomicString media_type_;
  AtomicString media_type_when_not_printing_;

  bool safe_to_propagate_scroll_to_parent_;

  unsigned visually_non_empty_character_count_;
  uint64_t visually_non_empty_pixel_count_;
  bool is_visually_non_empty_;
  LayoutObjectCounter layout_object_counter_;

  Member<FragmentAnchor> fragment_anchor_;

  Member<ScrollableAreaSet> scrollable_areas_;
  Member<ScrollableAreaSet> animating_scrollable_areas_;
  std::unique_ptr<ObjectSet> viewport_constrained_objects_;
  // Number of entries in viewport_constrained_objects_ that are sticky.
  unsigned sticky_position_object_count_;
  ObjectSet background_attachment_fixed_objects_;
  Member<FrameViewAutoSizeInfo> auto_size_info_;

  IntSize layout_size_;
  IntSize initial_viewport_size_;
  bool layout_size_fixed_to_frame_size_;

  bool needs_update_geometries_;

#if DCHECK_IS_ON()
  // Verified when finalizing.
  bool has_been_disposed_ = false;
#endif

  PluginSet plugins_;
  HeapHashSet<Member<Scrollbar>> scrollbars_;

  // TODO(bokan): This is unneeded when root-layer-scrolls is turned on.
  // crbug.com/417782.
  IntSize layout_overflow_size_;

  bool root_layer_did_scroll_;

  std::unique_ptr<LayoutAnalyzer> analyzer_;

  // Mark if something has changed in the mapping from Frame to GraphicsLayer
  // and the Frame Timing regions should be recalculated.
  bool frame_timing_requests_dirty_;

  // Exists only on root frame.
  Member<RootFrameViewport> viewport_scrollable_area_;

  // Non-top-level frames a throttled until they are ready to run lifecycle
  // updates (after render-blocking resources have loaded).
  bool lifecycle_updates_throttled_;

  // This is set on the local root frame view only.
  DocumentLifecycle::LifecycleState
      current_update_lifecycle_phases_target_state_;
  bool past_layout_lifecycle_update_;

  using AnchoringAdjustmentQueue =
      HeapLinkedHashSet<WeakMember<ScrollableArea>>;
  AnchoringAdjustmentQueue anchoring_adjustment_queue_;

  HeapLinkedHashSet<WeakMember<PaintLayerScrollableArea>> scroll_event_queue_;

  bool suppress_adjust_view_size_;
#if DCHECK_IS_ON()
  // In DCHECK on builds, this is set to false when we're running lifecycle
  // phases past layout to ensure that phases after layout don't dirty layout.
  bool allows_layout_invalidation_after_layout_clean_ = true;
#endif
  IntersectionObservationState intersection_observation_state_;
  bool needs_forced_compositing_update_;

  bool needs_focus_on_fragment_;
  bool in_lifecycle_update_;

  // True if the frame has deferred commits at least once per document load.
  // We won't defer again for the same document.
  bool have_deferred_commits_ = false;

  bool visual_viewport_needs_repaint_ = false;

  // Whether to collect layer debug information for debugging, tracing,
  // inspection, etc. Applies to local root only.
  bool layer_debug_info_enabled_ = DCHECK_IS_ON();

  LifecycleData lifecycle_data_;

  // Lazily created, but should only be created on a local frame root's view.
  mutable std::unique_ptr<ScrollingCoordinatorContext> scrolling_context_;

  // For testing.
  bool is_tracking_raster_invalidations_ = false;

  // Currently used in PushPaintArtifactToCompositor() to collect composited
  // layers as foreign layers. It's transient, but may live across frame updates
  // until SetForeignLayerListNeedsUpdate() is called.
  // For CompositeAfterPaint, we use it in PaintTree() for all paintings of the
  // frame tree in PaintTree(). It caches display items and subsequences across
  // frame updates and repaints.
  std::unique_ptr<PaintController> paint_controller_;
  std::unique_ptr<PaintArtifactCompositor> paint_artifact_compositor_;

  MainThreadScrollingReasons main_thread_scrolling_reasons_;

  scoped_refptr<LocalFrameUkmAggregator> ukm_aggregator_;
  unsigned forced_layout_stack_depth_;
  base::TimeTicks forced_layout_start_time_;

  // From the beginning of the document, how many frames have painted.
  size_t paint_frame_count_;

  UniqueObjectId unique_id_;
  Member<LayoutShiftTracker> layout_shift_tracker_;
  Member<PaintTimingDetector> paint_timing_detector_;

  HeapHashSet<WeakMember<LifecycleNotificationObserver>> lifecycle_observers_;

  HeapHashSet<WeakMember<HTMLVideoElement>> fullscreen_video_elements_;

  // If set, this indicates that the rendering throttling status for the local
  // root frame has changed. In this scenario, if we have become unthrottled,
  // this is a no-op since we run paint anyway. However, if we have become
  // throttled, this will force the lifecycle to reach the paint phase so that
  // it can clear the painted output.
  bool need_paint_phase_after_throttling_ = false;

  std::unique_ptr<OverlayInterstitialAdDetector>
      overlay_interstitial_ad_detector_;

  std::unique_ptr<StickyAdDetector> sticky_ad_detector_;

  // These tasks will be run at the beginning of the next lifecycle.
  WTF::Vector<base::OnceClosure> start_of_lifecycle_tasks_;

#if DCHECK_IS_ON()
  bool is_updating_descendant_dependent_flags_;
#endif

  FRIEND_TEST_ALL_PREFIXES(WebViewTest, DeviceEmulationResetScrollbars);
  FRIEND_TEST_ALL_PREFIXES(FrameThrottlingTest, GraphicsLayerCollection);
  FRIEND_TEST_ALL_PREFIXES(FrameThrottlingTest, ForAllThrottledLocalFrameViews);
};

inline void LocalFrameView::IncrementVisuallyNonEmptyCharacterCount(
    unsigned count) {
  if (is_visually_non_empty_)
    return;
  visually_non_empty_character_count_ += count;
  // Use a threshold value to prevent very small amounts of visible content from
  // triggering didMeaningfulLayout.  The first few hundred characters rarely
  // contain the interesting content of the page.
  static const unsigned kVisualCharacterThreshold = 200;
  if (visually_non_empty_character_count_ > kVisualCharacterThreshold)
    SetIsVisuallyNonEmpty();
}

inline void LocalFrameView::IncrementVisuallyNonEmptyPixelCount(
    const IntSize& size) {
  if (is_visually_non_empty_)
    return;
  visually_non_empty_pixel_count_ += size.Area();
  // Use a threshold value to prevent very small amounts of visible content from
  // triggering didMeaningfulLayout.
  static const unsigned kVisualPixelThreshold = 32 * 32;
  if (visually_non_empty_pixel_count_ > kVisualPixelThreshold)
    SetIsVisuallyNonEmpty();
}

template <>
struct DowncastTraits<LocalFrameView> {
  static bool AllowFrom(const EmbeddedContentView& embedded_content_view) {
    return embedded_content_view.IsLocalFrameView();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_VIEW_H_
