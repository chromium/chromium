/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_VIEW_H_

#include <memory>
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/hit_test_cache.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_state.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/overlay_scrollbar_clip_behavior.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class LayoutQuote;
class LocalFrameView;
class NamedPagesMapper;
class PaintLayerCompositor;
class ViewFragmentationContext;

// LayoutView is the root of the layout tree and the Document's LayoutObject.
//
// It corresponds to the CSS concept of 'initial containing block' (or ICB).
// http://www.w3.org/TR/CSS2/visudet.html#containing-block-details
//
// Its dimensions match that of the layout viewport. This viewport is used to
// size elements, in particular fixed positioned elements.
// LayoutView is always at position (0,0) relative to the document (and so isn't
// necessarily in view).
// See
// https://www.chromium.org/developers/design-documents/blink-coordinate-spaces
// about the different viewports.
//
// Because there is one LayoutView per rooted layout tree (or Frame), this class
// is used to add members shared by this tree (e.g. m_layoutState or
// m_layoutQuoteHead).
class CORE_EXPORT LayoutView final : public LayoutBlockFlow {
 public:
  explicit LayoutView(Document*);
  ~LayoutView() override;

  void WillBeDestroyed() override;

  // Called when the Document is shutdown, to have the compositor clean up
  // during frame detach, while pointers remain valid.
  void CleanUpCompositor();

  // hitTest() will update layout, style and compositing first while
  // hitTestNoLifecycleUpdate() does not.
  bool HitTest(const HitTestLocation& location, HitTestResult&);
  bool HitTestNoLifecycleUpdate(const HitTestLocation& location,
                                HitTestResult&);

  // Returns the total count of calls to HitTest, for testing.
  unsigned HitTestCount() const {
    NOT_DESTROYED();
    return hit_test_count_;
  }
  unsigned HitTestCacheHits() const {
    NOT_DESTROYED();
    return hit_test_cache_hits_;
  }

  void ClearHitTestCache();

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutView";
  }

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectLayoutView || LayoutBlockFlow::IsOfType(type);
  }

  PaintLayerType LayerTypeRequired() const override {
    NOT_DESTROYED();
    return kNormalPaintLayer;
  }

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  void UpdateLayout() override;
  void UpdateLogicalWidth() override;
  void ComputeLogicalHeight(LayoutUnit logical_height,
                            LayoutUnit logical_top,
                            LogicalExtentComputedValues&) const override;

  // Based on LocalFrameView::LayoutSize, but:
  // - checks for null LocalFrameView
  // - Replaces logical height with PageLogicalHeight() if using printing layout
  // - scrollbar exclusion is compatible with root layer scrolling
  IntSize GetLayoutSize(IncludeScrollbarsInRect = kExcludeScrollbars) const;

  int ViewHeight(
      IncludeScrollbarsInRect scrollbar_inclusion = kExcludeScrollbars) const {
    NOT_DESTROYED();
    return GetLayoutSize(scrollbar_inclusion).Height();
  }
  int ViewWidth(
      IncludeScrollbarsInRect scrollbar_inclusion = kExcludeScrollbars) const {
    NOT_DESTROYED();
    return GetLayoutSize(scrollbar_inclusion).Width();
  }

  int ViewLogicalWidth(IncludeScrollbarsInRect = kExcludeScrollbars) const;
  int ViewLogicalHeight(IncludeScrollbarsInRect = kExcludeScrollbars) const;

  LayoutUnit ViewLogicalHeightForPercentages() const;

  float ZoomFactor() const;

  LocalFrameView* GetFrameView() const {
    NOT_DESTROYED();
    return frame_view_;
  }
  const LayoutBox& RootBox() const;

  void UpdateAfterLayout() override;

  // See comments for the equivalent method on LayoutObject.
  bool MapToVisualRectInAncestorSpace(const LayoutBoxModelObject* ancestor,
                                      PhysicalRect&,
                                      MapCoordinatesFlags mode,
                                      VisualRectFlags) const;

  // |ancestor| can be nullptr, which will map the rect to the main frame's
  // space, even if the main frame is remote (or has intermediate remote
  // frames in the chain).
  bool MapToVisualRectInAncestorSpaceInternal(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      MapCoordinatesFlags,
      VisualRectFlags) const;

  bool MapToVisualRectInAncestorSpaceInternal(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      VisualRectFlags = kDefaultVisualRectFlags) const override;
  PhysicalOffset OffsetForFixedPosition() const;
  PhysicalOffset PixelSnappedOffsetForFixedPosition() const;

  void InvalidatePaintForViewAndCompositedLayers();

  void Paint(const PaintInfo&) const override;
  void PaintBoxDecorationBackground(
      const PaintInfo&,
      const PhysicalOffset& paint_offset) const override;

  void CommitPendingSelection();

  void AbsoluteQuads(Vector<FloatQuad>&,
                     MapCoordinatesFlags mode = 0) const override;

  PhysicalRect ViewRect() const override;
  PhysicalRect OverflowClipRect(const PhysicalOffset& location,
                                OverlayScrollbarClipBehavior =
                                    kIgnoreOverlayScrollbarSize) const override;

  // If either direction has a non-auto mode, the other must as well.
  void SetAutosizeScrollbarModes(mojom::blink::ScrollbarMode h_mode,
                                 mojom::blink::ScrollbarMode v_mode);
  mojom::blink::ScrollbarMode AutosizeHorizontalScrollbarMode() const {
    NOT_DESTROYED();
    return autosize_h_scrollbar_mode_;
  }
  mojom::blink::ScrollbarMode AutosizeVerticalScrollbarMode() const {
    NOT_DESTROYED();
    return autosize_v_scrollbar_mode_;
  }

  void CalculateScrollbarModes(mojom::blink::ScrollbarMode& h_mode,
                               mojom::blink::ScrollbarMode& v_mode) const;

  LayoutState* GetLayoutState() const {
    NOT_DESTROYED();
    return layout_state_;
  }

  bool CanHaveAdditionalCompositingReasons() const override {
    NOT_DESTROYED();
    return true;
  }
  CompositingReasons AdditionalCompositingReasons() const override;

  void UpdateHitTestResult(HitTestResult&,
                           const PhysicalOffset&) const override;

  ViewFragmentationContext* FragmentationContext() const {
    NOT_DESTROYED();
    return fragmentation_context_.get();
  }

  LayoutUnit PageLogicalHeight() const {
    NOT_DESTROYED();
    return page_logical_height_;
  }
  void SetPageLogicalHeight(LayoutUnit height) {
    NOT_DESTROYED();
    page_logical_height_ = height;
  }

  NamedPagesMapper* GetNamedPagesMapper() const {
    NOT_DESTROYED();
    return named_pages_mapper_.get();
  }

  PaintLayerCompositor* Compositor();
  bool UsesCompositing() const;

  PhysicalRect DocumentRect() const;

  IntervalArena* GetIntervalArena();

  void SetLayoutQuoteHead(LayoutQuote* head) {
    NOT_DESTROYED();
    layout_quote_head_ = head;
  }
  LayoutQuote* LayoutQuoteHead() const {
    NOT_DESTROYED();
    return layout_quote_head_;
  }

  // FIXME: This is a work around because the current implementation of counters
  // requires walking the entire tree repeatedly and most pages don't actually
  // use either feature so we shouldn't take the performance hit when not
  // needed. Long term we should rewrite the counter and quotes code.
  void AddLayoutCounter() {
    NOT_DESTROYED();
    layout_counter_count_++;
    SetNeedsCounterUpdate();
  }
  void RemoveLayoutCounter() {
    NOT_DESTROYED();
    DCHECK_GT(layout_counter_count_, 0u);
    layout_counter_count_--;
  }
  bool HasLayoutCounters() {
    NOT_DESTROYED();
    return layout_counter_count_;
  }
  void SetNeedsCounterUpdate() {
    NOT_DESTROYED();
    needs_counter_update_ = true;
  }
  void UpdateCounters();

  bool BackgroundIsKnownToBeOpaqueInRect(
      const PhysicalRect& local_rect) const override;

  // Returns the viewport size in (CSS pixels) that vh and vw units are
  // calculated from.
  FloatSize ViewportSizeForViewportUnits() const;

  void PushLayoutState(LayoutState& layout_state) {
    NOT_DESTROYED();
    layout_state_ = &layout_state;
  }
  void PopLayoutState() {
    NOT_DESTROYED();
    DCHECK(layout_state_);
    layout_state_ = layout_state_->Next();
  }

  PhysicalRect LocalVisualRectIgnoringVisibility() const override;

  // Invalidates paint for the entire view, including composited descendants,
  // but not including child frames.
  // It is very likely you do not want to call this method.
  void SetShouldDoFullPaintInvalidationForViewAndAllDescendants();

  bool ShouldPlaceBlockDirectionScrollbarOnLogicalLeft() const override;

  PhysicalRect DebugRect() const override;

  // Returns the coordinates of find-in-page scrollbar tickmarks.  These come
  // from DocumentMarkerController, unless overridden by OverrideTickmarks().
  Vector<IntRect> GetTickmarks() const;
  bool HasTickmarks() const;

  // Sets the coordinates of find-in-page scrollbar tickmarks, bypassing
  // DocumentMarkerController.  This is used by the PDF plugin.
  void OverrideTickmarks(const Vector<IntRect>&);

  // Issues a paint invalidation on the layout viewport's vertical scrollbar
  // (which is responsible for painting the tickmarks).
  void InvalidatePaintForTickmarks();

  RecalcLayoutOverflowResult RecalcLayoutOverflow() final;

  // The visible background area, in the local coordinates. The view background
  // will be painted in this rect. It's also the positioning area of fixed-
  // attachment backgrounds.
  PhysicalRect BackgroundRect() const {
    NOT_DESTROYED();
    return OverflowClipRect(PhysicalOffset());
  }

  // The previous BackgroundRect after the previous paint invalidation.
  PhysicalRect PreviousBackgroundRect() const {
    NOT_DESTROYED();
    DCHECK_EQ(GetDocument().Lifecycle().GetState(),
              DocumentLifecycle::kInPrePaint);
    return previous_background_rect_;
  }
  void SetPreviousBackgroundRect(const PhysicalRect& r) const {
    NOT_DESTROYED();
    DCHECK_EQ(GetDocument().Lifecycle().GetState(),
              DocumentLifecycle::kInPrePaint);
    previous_background_rect_ = r;
  }

  void MapAncestorToLocal(const LayoutBoxModelObject*,
                          TransformState&,
                          MapCoordinatesFlags) const override;

  bool ShouldUsePrintingLayout() const;

  void MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                          TransformState&,
                          MapCoordinatesFlags) const override;

 private:
  const LayoutObject* PushMappingToContainer(
      const LayoutBoxModelObject* ancestor_to_stop_at,
      LayoutGeometryMap&) const override;
  bool CanHaveChildren() const override;

  void UpdateBlockLayout(bool relayout_children) override;

#if DCHECK_IS_ON()
  void CheckLayoutState();
#endif

  void UpdateFromStyle() override;

  int ViewLogicalWidthForBoxSizing() const {
    NOT_DESTROYED();
    return ViewLogicalWidth(kIncludeScrollbars);
  }
  int ViewLogicalHeightForBoxSizing() const {
    NOT_DESTROYED();
    return ViewLogicalHeight(kIncludeScrollbars);
  }

  bool UpdateLogicalWidthAndColumnWidth() override;

  UntracedMember<LocalFrameView> frame_view_;

  // The page logical height.
  // This is only used during printing to split the content into pages.
  // Outside of printing, this is 0.
  LayoutUnit page_logical_height_;

  // LayoutState is an optimization used during layout.
  // |m_layoutState| will be nullptr outside of layout.
  //
  // See the class comment for more details.
  LayoutState* layout_state_;

  std::unique_ptr<ViewFragmentationContext> fragmentation_context_;
  std::unique_ptr<NamedPagesMapper> named_pages_mapper_;
  std::unique_ptr<PaintLayerCompositor> compositor_;
  scoped_refptr<IntervalArena> interval_arena_;

  LayoutQuote* layout_quote_head_;
  unsigned layout_counter_count_;
  bool needs_counter_update_ = false;

  unsigned hit_test_count_;
  unsigned hit_test_cache_hits_;
  Persistent<HitTestCache> hit_test_cache_;

  // FrameViewAutoSizeInfo controls scrollbar appearance manually rather than
  // relying on layout. These members are used to override the ScrollbarModes
  // calculated from style. kScrollbarAuto disables the override.
  mojom::blink::ScrollbarMode autosize_h_scrollbar_mode_;
  mojom::blink::ScrollbarMode autosize_v_scrollbar_mode_;

  Vector<IntRect> tickmarks_override_;

  mutable PhysicalRect previous_background_rect_;
};

template <>
struct DowncastTraits<LayoutView> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutView();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_VIEW_H_
