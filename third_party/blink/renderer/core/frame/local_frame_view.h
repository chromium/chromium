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
#include <utility>

#include "third_party/blink/public/common/manifest/web_display_mode.h"
#include "third_party/blink/public/platform/shape_properties.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/frame_view.h"
#include "third_party/blink/renderer/core/frame/layout_subtree_root_list.h"
#include "third_party/blink/renderer/core/layout/depth_ordered_layout_object_list.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/layout_object_counter.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/graphics/subtree_paint_property_update_reason.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class AXObjectCache;
class ChromeClient;
class CompositorAnimationHost;
class CompositorAnimationTimeline;
class Cursor;
class DisplayItemClient;
class Document;
class DocumentLifecycle;
class Element;
class ElementVisibilityObserver;
class FloatRect;
class FloatSize;
class Frame;
class FrameViewAutoSizeInfo;
class JSONObject;
class JankTracker;
class KURL;
class LayoutAnalyzer;
class LayoutBox;
class LayoutEmbeddedContent;
class LayoutEmbeddedObject;
class LayoutObject;
class LayoutRect;
class LayoutSVGRoot;
class LayoutView;
class LocalFrame;
class Node;
class Page;
class PaintArtifactCompositor;
class PaintController;
class PaintLayerScrollableArea;
class PaintTracker;
class PrintContext;
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
struct WebScrollIntoViewParams;

typedef unsigned long long DOMTimeStamp;
using LayerTreeFlags = unsigned;
using MainThreadScrollingReasons = uint32_t;

class CORE_EXPORT LocalFrameView final
    : public GarbageCollectedFinalized<LocalFrameView>,
      public FrameView {
  USING_GARBAGE_COLLECTED_MIXIN(LocalFrameView);

  friend class PaintControllerPaintTestBase;
  friend class Internals;
  friend class LayoutEmbeddedContent;  // for invalidateTreeIfNeeded

 public:
  static LocalFrameView* Create(LocalFrame&);
  static LocalFrameView* Create(LocalFrame&, const IntSize& initial_size);

  ~LocalFrameView() override;

  void Invalidate() { InvalidateRect(IntRect(0, 0, Width(), Height())); }
  void InvalidateRect(const IntRect&);
  void SetFrameRect(const IntRect&) override;
  IntRect FrameRect() const override { return IntRect(Location(), Size()); }
  IntPoint Location() const;
  int X() const { return Location().X(); }
  int Y() const { return Location().Y(); }
  int Width() const { return Size().Width(); }
  int Height() const { return Size().Height(); }
  IntSize Size() const { return frame_rect_.Size(); }
  void Resize(int width, int height) { Resize(IntSize(width, height)); }
  void Resize(const IntSize& size) {
    SetFrameRect(IntRect(frame_rect_.Location(), size));
  }

  // Called when our frame rect changes (or the rect/scroll offset of an
  // ancestor changes).
  void FrameRectsChanged() override;

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
  void SetLifecycleUpdatesThrottledForTesting();
  void ScheduleRelayout();
  void ScheduleRelayoutOfSubtree(LayoutObject*);
  bool LayoutPending() const;
  bool IsInPerformLayout() const;

  // Methods to capture forced layout metrics.
  void WillStartForcedLayout();
  void DidFinishForcedLayout();

  void ClearLayoutSubtreeRoot(const LayoutObject&);
  void AddOrthogonalWritingModeRoot(LayoutBox&);
  void RemoveOrthogonalWritingModeRoot(LayoutBox&);
  bool HasOrthogonalWritingModeRoots() const;
  void LayoutOrthogonalWritingModeRoots();
  void ScheduleOrthogonalWritingModeRootsForLayout();

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

  // Get the InstersectionObservation::ComputeFlags for target elements in this
  // view.
  unsigned GetIntersectionObservationFlags() const;

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

  void UpdateAcceleratedCompositingSettings();

  void UpdateCountersAfterStyleChange();

  void Dispose() override;
  void InvalidateAllCustomScrollbarsOnActiveChanged();

  // True if the LocalFrameView's base background color is completely opaque.
  bool HasOpaqueBackground() const;

  Color BaseBackgroundColor() const;
  void SetBaseBackgroundColor(const Color&);
  void UpdateBaseBackgroundColorRecursively(const Color&);

  void AdjustViewSize();
  void AdjustViewSizeAndLayout();

  // Scale used to convert incoming input events.
  float InputEventsScaleFactor() const;

  // Scale used to convert incoming input events while emulating device metics.
  void SetInputEventsScaleForEmulation(float);

  void DidChangeScrollOffset();

  void ViewportSizeChanged(bool width_changed, bool height_changed);
  void MarkViewportConstrainedObjectsForLayout(bool width_changed,
                                               bool height_changed);

  AtomicString MediaType() const;
  void SetMediaType(const AtomicString&);
  void AdjustMediaTypeForPrinting(bool printing);

  WebDisplayMode DisplayMode() { return display_mode_; }
  void SetDisplayMode(WebDisplayMode);

  DisplayShape GetDisplayShape() { return display_shape_; }
  void SetDisplayShape(DisplayShape);

  // Fixed-position objects.
  typedef HashSet<LayoutObject*> ViewportConstrainedObjectSet;
  void AddViewportConstrainedObject(LayoutObject&);
  void RemoveViewportConstrainedObject(LayoutObject&);
  const ViewportConstrainedObjectSet* ViewportConstrainedObjects() const {
    return viewport_constrained_objects_.get();
  }
  bool HasViewportConstrainedObjects() const {
    return viewport_constrained_objects_ &&
           viewport_constrained_objects_->size() > 0;
  }

  // Objects with background-attachment:fixed.
  void AddBackgroundAttachmentFixedObject(LayoutObject*);
  void RemoveBackgroundAttachmentFixedObject(LayoutObject*);
  bool HasBackgroundAttachmentFixedObjects() const {
    return background_attachment_fixed_objects_.size();
  }
  bool HasBackgroundAttachmentFixedDescendants(const LayoutObject&) const;
  void InvalidateBackgroundAttachmentFixedDescendants(const LayoutObject&);

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

  // Run all needed lifecycle stages. After calling this method, all frames will
  // be in the lifecycle state PaintClean.  If lifecycle throttling is allowed
  // (see DocumentLifecycle::AllowThrottlingScope), some frames may skip the
  // lifecycle update (e.g., based on visibility) and will not end up being
  // PaintClean.
  void UpdateAllLifecyclePhases();

  // Computes the style, layout, compositing and pre-paint lifecycle stages
  // if needed.
  // After calling this method, all frames will be in a lifecycle
  // state >= PrePaintClean, unless the frame was throttled or inactive.
  // Returns whether the lifecycle was successfully updated to the
  // desired state.
  bool UpdateAllLifecyclePhasesExceptPaint();

  // Printing needs everything up-to-date except paint (which will be done
  // specially). We may also print a detached frame or a descendant of a
  // detached frame and need special handling of the frame.
  void UpdateLifecyclePhasesForPrinting();

  // After calling this method, all frames will be in a lifecycle
  // state >= CompositingClean, and scrolling has been updated (unless
  // throttling is allowed), unless the frame was throttled or inactive.
  // Returns whether the lifecycle was successfully updated to the
  // desired state.
  bool UpdateLifecycleToCompositingCleanPlusScrolling();

  // Computes the style, layout, and compositing inputs lifecycle stages if
  // needed. After calling this method, all frames will be in a lifecycle state
  // >= CompositingInputsClean, unless the frame was throttled or inactive.
  // Returns whether the lifecycle was successfully updated to the
  // desired state.
  bool UpdateLifecycleToCompositingInputsClean();

  // Computes only the style and layout lifecycle stages.
  // After calling this method, all frames will be in a lifecycle
  // state >= LayoutClean, unless the frame was throttled or inactive.
  // Returns whether the lifecycle was successfully updated to the
  // desired state.
  bool UpdateLifecycleToLayoutClean();

  // Record any UMA and UKM metrics that depend on the end of a main frame.
  void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time);

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

  enum UrlFragmentBehavior { kUrlFragmentScroll, kUrlFragmentDontScroll };
  // Updates the fragment anchor element based on URL's fragment identifier.
  // Updates corresponding ':target' CSS pseudo class on the anchor element.
  // If |UrlFragmentScroll| is passed it sets the anchor element so that it
  // will be focused and scrolled into view during layout. The scroll offset is
  // maintained during the frame loading process.
  void ProcessUrlFragment(const KURL&,
                          UrlFragmentBehavior = kUrlFragmentScroll);
  void ClearFragmentAnchor();

  // Methods to convert points and rects between the coordinate space of the
  // layoutObject, and this view.
  IntRect ConvertFromLayoutObject(const LayoutObject&, const IntRect&) const;
  IntRect ConvertToLayoutObject(const LayoutObject&, const IntRect&) const;
  IntPoint ConvertFromLayoutObject(const LayoutObject&, const IntPoint&) const;
  IntPoint ConvertToLayoutObject(const LayoutObject&, const IntPoint&) const;
  LayoutPoint ConvertFromLayoutObject(const LayoutObject&,
                                      const LayoutPoint&) const;
  LayoutPoint ConvertToLayoutObject(const LayoutObject&,
                                    const LayoutPoint&) const;
  FloatPoint ConvertToLayoutObject(const LayoutObject&,
                                   const FloatPoint&) const;

  bool ShouldSetCursor() const;

  void SetCursor(const Cursor&);

  // FIXME: Remove this method once plugin loading is decoupled from layout.
  void FlushAnyPendingPostLayoutTasks();

  static void SetInitialTracksPaintInvalidationsForTesting(bool);

  // These methods are for testing.
  void SetTracksPaintInvalidations(bool);
  bool IsTrackingPaintInvalidations() const {
    return tracked_object_paint_invalidations_.get();
  }
  void TrackObjectPaintInvalidation(const DisplayItemClient&,
                                    PaintInvalidationReason);
  struct ObjectPaintInvalidation {
    String name;
    PaintInvalidationReason reason;
  };
  Vector<ObjectPaintInvalidation>* TrackedObjectPaintInvalidations() const {
    return tracked_object_paint_invalidations_.get();
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

  // With CSS style "resize:" enabled, a little resizer handle will appear at
  // the bottom right of the object. We keep track of these resizer areas for
  // checking if touches (implemented using Scroll gesture) are targeting the
  // resizer.
  typedef HashSet<LayoutBox*> ResizerAreaSet;
  void AddResizerArea(LayoutBox&);
  void RemoveResizerArea(LayoutBox&);
  const ResizerAreaSet* ResizerAreas() const { return resizer_areas_.get(); }

  void ScheduleAnimation();

  // FIXME: This should probably be renamed as the 'inSubtreeLayout' parameter
  // passed around the LocalFrameView layout methods can be true while this
  // returns false.
  bool IsSubtreeLayout() const { return !layout_subtree_root_list_.IsEmpty(); }

  // The window that hosts the LocalFrameView. The LocalFrameView will
  // communicate scrolls and repaints to the host window in the window's
  // coordinate space.
  ChromeClient* GetChromeClient() const;

  // Functions for child manipulation and inspection.
  bool IsSelfVisible() const {
    return self_visible_;
  }  // Whether or not we have been explicitly marked as visible or not.
  bool IsParentVisible() const {
    return parent_visible_;
  }  // Whether or not our parent is visible.
  bool IsVisible() const {
    return self_visible_ && parent_visible_;
  }  // Whether or not we are actually visible.
  void SetParentVisible(bool) override;
  void SetSelfVisible(bool);
  void AttachToLayout() override;
  void DetachFromLayout() override;
  bool IsAttached() const override { return is_attached_; }
  using PluginSet = HeapHashSet<Member<WebPluginContainerImpl>>;
  const PluginSet& Plugins() const { return plugins_; }
  void AddPlugin(WebPluginContainerImpl*);
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
  LayoutPoint ViewportToFrame(const LayoutPoint&) const;

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
  LayoutPoint ConvertToRootFrame(const LayoutPoint&) const;
  FloatPoint ConvertToRootFrame(const FloatPoint&) const;
  LayoutRect ConvertToRootFrame(const LayoutRect&) const;
  IntRect ConvertFromRootFrame(const IntRect&) const;
  IntPoint ConvertFromRootFrame(const IntPoint&) const;
  FloatPoint ConvertFromRootFrame(const FloatPoint&) const;
  LayoutPoint ConvertFromRootFrame(const LayoutPoint&) const;
  IntPoint ConvertSelfToChild(const EmbeddedContentView&,
                              const IntPoint&) const;

  IntRect RootFrameToDocument(const IntRect&);
  IntPoint RootFrameToDocument(const IntPoint&);
  FloatPoint RootFrameToDocument(const FloatPoint&);
  DoublePoint DocumentToFrame(const DoublePoint&) const;
  FloatPoint DocumentToFrame(const FloatPoint&) const;
  LayoutPoint DocumentToFrame(const LayoutPoint&) const;
  LayoutRect DocumentToFrame(const LayoutRect&) const;
  LayoutPoint FrameToDocument(const LayoutPoint&) const;
  LayoutRect FrameToDocument(const LayoutRect&) const;

  // Handles painting of the contents of the view as well as the scrollbars.
  void Paint(GraphicsContext&,
             const GlobalPaintFlags,
             const CullRect&,
             const IntSize& paint_offset = IntSize()) const override;
  // Paints, and also updates the lifecycle to in-paint and paint clean
  // beforehand.  Call this for painting use-cases outside of the lifecycle.
  void PaintWithLifecycleUpdate(GraphicsContext&,
                                const GlobalPaintFlags,
                                const CullRect&);
  void PaintContents(GraphicsContext&,
                     const GlobalPaintFlags,
                     const IntRect& damage_rect);

  void Show() override;
  void Hide() override;

  bool IsLocalFrameView() const override { return true; }

  void Trace(blink::Visitor*) override;
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

  // Returns true if this frame could potentially skip rendering and avoid
  // scheduling visual updates.
  bool CanThrottleRendering() const;
  bool IsHiddenForThrottling() const { return hidden_for_throttling_; }
  void SetupRenderThrottling();

  // For testing, run pending intersection observer notifications for this
  // frame.
  void UpdateRenderThrottlingStatusForTesting();

  void BeginLifecycleUpdates();

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

  // Only for SPv2.
  std::unique_ptr<JSONObject> CompositedLayersAsJSON(LayerTreeFlags);

  // Recursively update frame tree. Each frame has its only
  // scroll on main reason. Given the following frame tree
  // .. A...
  // ../.\..
  // .B...C.
  // .|.....
  // .D.....
  // If B has fixed background-attachment but other frames
  // don't, both A and C should scroll on cc. Frame D should
  // scrolled on main thread as its ancestor B.
  void UpdateSubFrameScrollOnMainReason(const Frame&,
                                        MainThreadScrollingReasons);
  String MainThreadScrollingReasonsAsText();
  // Main thread scrolling reasons including reasons from ancestors.
  MainThreadScrollingReasons GetMainThreadScrollingReasons() const;
  // Main thread scrolling reasons for this object only. For all reasons,
  // see: mainThreadScrollingReasons().
  MainThreadScrollingReasons MainThreadScrollingReasonsPerFrame() const;

  bool HasVisibleSlowRepaintViewportConstrainedObjects() const;

  bool MapToVisualRectInTopFrameSpace(LayoutRect&);

  void ApplyTransformForTopFrameSpace(TransformState&);

  void CrossOriginStatusChanged();

  // The visual viewport can supply scrollbars.
  void VisualViewportScrollbarsChanged();

  LayoutUnit CaretWidth() const;

  size_t PaintFrameCount() const { return paint_frame_count_; };

  // Return the ScrollableArea in a FrameView with the given ElementId, if any.
  // This is not recursive and will only return ScrollableAreas owned by this
  // LocalFrameView (or possibly the LocalFrameView itself).
  ScrollableArea* ScrollableAreaWithElementId(const CompositorElementId&);

  // When the frame is a local root and not a main frame, any recursive
  // scrolling should continue in the parent process.
  void ScrollRectToVisibleInRemoteParent(const LayoutRect&,
                                         const WebScrollIntoViewParams&);

  PaintArtifactCompositor* GetPaintArtifactCompositorForTesting() {
    DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
           RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled());
    return paint_artifact_compositor_.get();
  }

  enum ForceThrottlingInvalidationBehavior {
    kDontForceThrottlingInvalidation,
    kForceThrottlingInvalidation
  };
  enum NotifyChildrenBehavior { kDontNotifyChildren, kNotifyChildren };
  void UpdateRenderThrottlingStatus(
      bool hidden,
      bool subtree_throttled,
      ForceThrottlingInvalidationBehavior = kDontForceThrottlingInvalidation,
      NotifyChildrenBehavior = kNotifyChildren);

  // Keeps track of whether the scrollable state for the LocalRoot has changed
  // since ScrollingCoordinator last checked. Only ScrollingCoordinator should
  // ever call the clearing function.
  bool FrameIsScrollableDidChange();
  void ClearFrameIsScrollableDidChange();

  // Should be called whenever this LocalFrameView adds or removes a
  // scrollable area, or gains/loses a composited layer.
  void ScrollableAreasDidChange();

  ScrollingCoordinatorContext* GetScrollingContext() const;
  CompositorAnimationHost* GetCompositorAnimationHost() const;
  CompositorAnimationTimeline* GetCompositorAnimationTimeline() const;

  void ScrollAndFocusFragmentAnchor();
  JankTracker& GetJankTracker() { return *jank_tracker_; }
  PaintTracker& GetPaintTracker() const { return *paint_tracker_; }

 protected:
  void NotifyFrameRectsChangedIfNeeded();

 private:
#if DCHECK_IS_ON()
  class DisallowLayoutInvalidationScope {
   public:
    DisallowLayoutInvalidationScope(LocalFrameView* view)
        : local_frame_view_(view) {
      local_frame_view_->allows_layout_invalidation_after_layout_clean_ = false;
      local_frame_view_->ForAllChildLocalFrameViews(
          [](LocalFrameView& frame_view) {
            if (!frame_view.ShouldThrottleRendering())
              frame_view.CheckDoesNotNeedLayout();
            frame_view.allows_layout_invalidation_after_layout_clean_ = false;
          });
    }
    ~DisallowLayoutInvalidationScope() {
      local_frame_view_->allows_layout_invalidation_after_layout_clean_ = true;
      local_frame_view_->ForAllChildLocalFrameViews(
          [](LocalFrameView& frame_view) {
            if (!frame_view.ShouldThrottleRendering())
              frame_view.CheckDoesNotNeedLayout();
            frame_view.allows_layout_invalidation_after_layout_clean_ = true;
          });
    }

   private:
    UntracedMember<LocalFrameView> local_frame_view_;
  };
#endif

  explicit LocalFrameView(LocalFrame&, IntRect);

  void PaintInternal(GraphicsContext&,
                     const GlobalPaintFlags,
                     const CullRect&) const;

  LocalFrameView* ParentFrameView() const;
  LayoutSVGRoot* EmbeddedReplacedContent() const;

  void DispatchEventsForPrintingOnAllFrames();

  void SetupPrintContext();
  void ClearPrintContext();

  // Returns whether the lifecycle was succesfully updated to the
  // target state.
  bool UpdateLifecyclePhases(DocumentLifecycle::LifecycleState target_state);
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
  bool RunCompositingLifecyclePhase(
      DocumentLifecycle::LifecycleState target_state);
  bool RunPrePaintLifecyclePhase(
      DocumentLifecycle::LifecycleState target_state);
  void RunPaintLifecyclePhase();

  void NotifyFrameRectsChangedIfNeededRecursive();
  void PrePaint();
  void PaintTree();
  void UpdateStyleAndLayoutIfNeededRecursive();

  void PushPaintArtifactToCompositor(
      CompositorElementIdSet& composited_element_ids);

  void ClearLayoutSubtreeRootsAndMarkContainingBlocks();

  void PerformPreLayoutTasks();
  void PerformLayout(bool in_subtree_layout);
  void PerformPostLayoutTasks();

  void RecordDeferredLoadingStats();

  DocumentLifecycle& Lifecycle() const;

  // Methods to do point conversion via layoutObjects, in order to take
  // transforms into account.
  IntRect ConvertToContainingEmbeddedContentView(const IntRect&) const;
  IntPoint ConvertToContainingEmbeddedContentView(const IntPoint&) const;
  LayoutPoint ConvertToContainingEmbeddedContentView(const LayoutPoint&) const;
  FloatPoint ConvertToContainingEmbeddedContentView(const FloatPoint&) const;
  IntRect ConvertFromContainingEmbeddedContentView(const IntRect&) const;
  IntPoint ConvertFromContainingEmbeddedContentView(const IntPoint&) const;
  LayoutPoint ConvertFromContainingEmbeddedContentView(
      const LayoutPoint&) const;
  FloatPoint ConvertFromContainingEmbeddedContentView(const FloatPoint&) const;
  DoublePoint ConvertFromContainingEmbeddedContentView(
      const DoublePoint&) const;

  void UpdateGeometriesIfNeeded();

  bool WasViewportResized();
  void SendResizeEventIfNeeded();

  void ScheduleUpdatePluginsIfNecessary();
  void UpdatePluginsTimerFired(TimerBase*);
  bool UpdatePlugins();

  bool ProcessUrlFragmentHelper(const String&, UrlFragmentBehavior);
  bool ParseCSSFragmentIdentifier(const String&, String*);
  Element* FindCSSFragmentAnchor(const AtomicString&, Document*);

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

  void UpdateViewportIntersectionsForSubtree() override;
  void UpdateThrottlingStatusForSubtree();

  void NotifyResizeObservers();

  bool CheckLayoutInvalidationIsAllowed() const;

  PaintController* GetPaintController() { return paint_controller_.get(); }

  void LayoutFromRootObject(LayoutObject& root);

  LocalFrameUkmAggregator& EnsureUkmAggregator();

  LayoutSize size_;

  typedef HashSet<scoped_refptr<LayoutEmbeddedObject>> EmbeddedObjectSet;
  EmbeddedObjectSet part_update_set_;

  Member<LocalFrame> frame_;
  Member<LocalFrameView> parent_;

  IntRect frame_rect_;
  bool is_attached_;
  bool self_visible_;
  bool parent_visible_;

  WebDisplayMode display_mode_;

  DisplayShape display_shape_;

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

  Member<Node> fragment_anchor_;

  Member<ScrollableAreaSet> scrollable_areas_;
  Member<ScrollableAreaSet> animating_scrollable_areas_;
  std::unique_ptr<ResizerAreaSet> resizer_areas_;
  std::unique_ptr<ViewportConstrainedObjectSet> viewport_constrained_objects_;
  unsigned sticky_position_object_count_;
  ViewportConstrainedObjectSet background_attachment_fixed_objects_;
  Member<FrameViewAutoSizeInfo> auto_size_info_;

  float input_events_scale_factor_for_emulation_;

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
  // TODO(bokan): crbug.com/484188. We should specialize LocalFrameView for the
  // main frame.
  Member<RootFrameViewport> viewport_scrollable_area_;

  // The following members control rendering pipeline throttling for this
  // frame. They are only updated in response to intersection observer
  // notifications, i.e., not in the middle of the lifecycle.
  bool hidden_for_throttling_;
  bool subtree_throttled_;
  bool lifecycle_updates_throttled_;

  // This is set on the local root frame view only.
  DocumentLifecycle::LifecycleState
      current_update_lifecycle_phases_target_state_;
  bool past_layout_lifecycle_update_;

  using AnchoringAdjustmentQueue =
      HeapLinkedHashSet<WeakMember<ScrollableArea>>;
  AnchoringAdjustmentQueue anchoring_adjustment_queue_;

  bool suppress_adjust_view_size_;
#if DCHECK_IS_ON()
  // In DCHECK on builds, this is set to false when we're running lifecycle
  // phases past layout to ensure that phases after layout don't dirty layout.
  bool allows_layout_invalidation_after_layout_clean_ = true;
#endif
  IntersectionObservationState intersection_observation_state_;
  bool needs_forced_compositing_update_;

  bool needs_focus_on_fragment_;

  Member<ElementVisibilityObserver> visibility_observer_;

  IntRect remote_viewport_intersection_;

  // Lazily created, but should only be created on a local frame root's view.
  mutable std::unique_ptr<ScrollingCoordinatorContext> scrolling_context_;

  // For testing.
  std::unique_ptr<Vector<ObjectPaintInvalidation>>
      tracked_object_paint_invalidations_;

  // For Slimming Paint v2 only.
  std::unique_ptr<PaintController> paint_controller_;
  std::unique_ptr<PaintArtifactCompositor> paint_artifact_compositor_;

  MainThreadScrollingReasons main_thread_scrolling_reasons_;

  std::unique_ptr<LocalFrameUkmAggregator> ukm_aggregator_;
  unsigned forced_layout_stack_depth_;
  TimeTicks forced_layout_start_time_;

  Member<PrintContext> print_context_;

  // From the beginning of the document, how many frames have painted.
  size_t paint_frame_count_;

  UniqueObjectId unique_id_;
  std::unique_ptr<JankTracker> jank_tracker_;
  Member<PaintTracker> paint_tracker_;

  FRIEND_TEST_ALL_PREFIXES(WebViewTest, DeviceEmulationResetScrollbars);
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

inline bool operator==(const LocalFrameView::ObjectPaintInvalidation& a,
                       const LocalFrameView::ObjectPaintInvalidation& b) {
  return a.name == b.name && a.reason == b.reason;
}
inline bool operator!=(const LocalFrameView::ObjectPaintInvalidation& a,
                       const LocalFrameView::ObjectPaintInvalidation& b) {
  return !(a == b);
}
inline std::ostream& operator<<(
    std::ostream& os,
    const LocalFrameView::ObjectPaintInvalidation& info) {
  return os << info.name << " reason=" << info.reason;
}

DEFINE_TYPE_CASTS(LocalFrameView,
                  EmbeddedContentView,
                  embedded_content_view,
                  embedded_content_view->IsLocalFrameView(),
                  embedded_content_view.IsLocalFrameView());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_VIEW_H_
