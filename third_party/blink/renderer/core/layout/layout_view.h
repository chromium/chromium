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

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/hit_test_cache.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_quote.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/overlay_scrollbar_clip_behavior.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class LayoutQuote;
class LayoutViewTransitionRoot;
class LocalFrameView;
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
class CORE_EXPORT LayoutView : public LayoutBlockFlow {
 public:
  explicit LayoutView(ContainerNode* document);
  ~LayoutView() override;
  void Trace(Visitor*) const override;

  void WillBeDestroyed() override;

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
    return type == kLayoutObjectView || LayoutBlockFlow::IsOfType(type);
  }

  PaintLayerType LayerTypeRequired() const override {
    NOT_DESTROYED();
    return kNormalPaintLayer;
  }

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  void UpdateLayout() override {
    NOT_DESTROYED();
    NOTREACHED_NORETURN();
  }
  void ComputeLogicalHeight(LayoutUnit logical_height,
                            LayoutUnit logical_top,
                            LogicalExtentComputedValues&) const override;
  LayoutUnit ComputeMinimumWidth();

  // Based on LocalFrameView::LayoutSize, but:
  // - checks for null LocalFrameView
  // - Replaces logical height with PageLogicalHeight() if using printing layout
  // - scrollbar exclusion is compatible with root layer scrolling
  gfx::Size GetLayoutSize(IncludeScrollbarsInRect = kExcludeScrollbars) const;

  int ViewHeight(
      IncludeScrollbarsInRect scrollbar_inclusion = kExcludeScrollbars) const {
    NOT_DESTROYED();
    return GetLayoutSize(scrollbar_inclusion).height();
  }
  int ViewWidth(
      IncludeScrollbarsInRect scrollbar_inclusion = kExcludeScrollbars) const {
    NOT_DESTROYED();
    return GetLayoutSize(scrollbar_inclusion).width();
  }

  int ViewLogicalWidth(IncludeScrollbarsInRect = kExcludeScrollbars) const;
  int ViewLogicalHeight(IncludeScrollbarsInRect = kExcludeScrollbars) const;

  LayoutUnit ViewLogicalHeightForPercentages() const;

  LocalFrameView* GetFrameView() const {
    NOT_DESTROYED();
    return frame_view_;
  }
  const LayoutBox& RootBox() const;

  void UpdateAfterLayout() override;

  // See comments for the equivalent method on LayoutObject.
  // |ancestor| can be nullptr, which will map the rect to the main frame's
  // space, even if the main frame is remote (or has intermediate remote
  // frames in the chain).
  bool MapToVisualRectInAncestorSpaceInternal(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      VisualRectFlags = kDefaultVisualRectFlags) const override;

  PhysicalOffset OffsetForFixedPosition() const;
  PhysicalOffset PixelSnappedOffsetForFixedPosition() const;

  void Paint(const PaintInfo&) const override;
  void PaintBoxDecorationBackground(
      const PaintInfo&,
      const PhysicalOffset& paint_offset) const override;

  void CommitPendingSelection();

  void AbsoluteQuads(Vector<gfx::QuadF>&,
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

  bool CanHaveAdditionalCompositingReasons() const override {
    NOT_DESTROYED();
    return true;
  }
  CompositingReasons AdditionalCompositingReasons() const override;

  void UpdateHitTestResult(HitTestResult&,
                           const PhysicalOffset&) const override;

  ViewFragmentationContext* FragmentationContext() const {
    NOT_DESTROYED();
    return fragmentation_context_;
  }

  LayoutUnit PageLogicalHeight() const {
    NOT_DESTROYED();
    return IsHorizontalWritingMode() ? page_size_.height : page_size_.width;
  }
  void SetPageSize(PhysicalSize size) {
    NOT_DESTROYED();
    page_size_ = size;
  }
  PhysicalSize PageSize() const {
    NOT_DESTROYED();
    return page_size_;
  }

  // TODO(1229581): Make non-virtual.
  virtual AtomicString NamedPageAtIndex(wtf_size_t page_index) const = 0;

  PhysicalRect DocumentRect() const;

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
  // needed. Long term we should rewrite the counter code.
  // TODO(xiaochengh): Or do we keep it as is?
  void AddLayoutCounter() {
    NOT_DESTROYED();
    layout_counter_count_++;
    SetNeedsMarkerOrCounterUpdate();
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
  void AddLayoutListItem() {
    NOT_DESTROYED();
    layout_list_item_count_++;
    // No need to traverse and update markers at this point. We need it only
    // when @counter-style rules are changed.
  }
  void RemoveLayoutListItem() {
    NOT_DESTROYED();
    DCHECK_GT(layout_list_item_count_, 0u);
    layout_list_item_count_--;
  }
  bool HasLayoutListItems() {
    NOT_DESTROYED();
    return layout_list_item_count_;
  }
  void SetNeedsMarkerOrCounterUpdate() {
    NOT_DESTROYED();
    needs_marker_counter_update_ = true;
  }

  // Return true if laying out with a new initial containing block size. lala.
  bool AffectedByResizedInitialContainingBlock(const NGLayoutResult&);

  // Update generated markers and counters after style and layout tree update.
  // container - The container for container queries, otherwise nullptr.
  void UpdateMarkersAndCountersAfterStyleChange(
      LayoutObject* container = nullptr);

  bool BackgroundIsKnownToBeOpaqueInRect(
      const PhysicalRect& local_rect) const override;

  // Returns the viewport size in (CSS pixels) that vh and vw units are
  // calculated from.
  gfx::SizeF ViewportSizeForViewportUnits() const;
  // https://drafts.csswg.org/css-values-4/#small-viewport-size
  gfx::SizeF SmallViewportSizeForViewportUnits() const;
  // https://drafts.csswg.org/css-values-4/#large-viewport-size
  gfx::SizeF LargeViewportSizeForViewportUnits() const;
  // https://drafts.csswg.org/css-values-4/#dynamic-viewport-size
  gfx::SizeF DynamicViewportSizeForViewportUnits() const;

  PhysicalRect LocalVisualRectIgnoringVisibility() const override;

  // Invalidates paint for the entire view, including composited descendants,
  // but not including child frames.
  // It is very likely you do not want to call this method.
  void InvalidatePaintForViewAndDescendants();

  bool ShouldPlaceBlockDirectionScrollbarOnLogicalLeft() const override;

  PhysicalRect DebugRect() const override;

  // Returns the coordinates of find-in-page scrollbar tickmarks.  These come
  // from DocumentMarkerController.
  Vector<gfx::Rect> GetTickmarks() const;
  bool HasTickmarks() const;

  RecalcLayoutOverflowResult RecalcLayoutOverflow() override;

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

  static bool ShouldUsePrintingLayout(const Document&);
  bool ShouldUsePrintingLayout() const {
    NOT_DESTROYED();
    return ShouldUsePrintingLayout(GetDocument());
  }

  void MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                          TransformState&,
                          MapCoordinatesFlags) const override;

  LogicalSize InitialContainingBlockSize() const;

  TrackedDescendantsMap& SvgTextDescendantsMap();

  LayoutViewTransitionRoot* GetViewTransitionRoot() const;

 protected:
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  int ViewLogicalWidthForBoxSizing() const {
    NOT_DESTROYED();
    return ViewLogicalWidth(kIncludeScrollbars);
  }
  int ViewLogicalHeightForBoxSizing() const {
    NOT_DESTROYED();
    return ViewLogicalHeight(kIncludeScrollbars);
  }

  // Set if laying out with a new initial containing block size, and populated
  // as we handle nodes that may have been affected by that.
  Member<HeapHashSet<Member<const LayoutObject>>>
      initial_containing_block_resize_handled_list_;

 private:
  bool CanHaveChildren() const override;
  void UpdateFromStyle() override;

  // The CompositeBackgroundAttachmentFixed optimization doesn't apply to
  // LayoutView which paints background specially.
  bool ComputeCanCompositeBackgroundAttachmentFixed() const override {
    NOT_DESTROYED();
    return false;
  }

 protected:
  // The page size.
  // This is only used during printing to split the content into pages.
  // Outside of printing, this is 0x0.
  PhysicalSize page_size_;

  Member<ViewFragmentationContext> fragmentation_context_;

 private:
  Member<LocalFrameView> frame_view_;
  Member<LayoutQuote> layout_quote_head_;
  unsigned layout_counter_count_ = 0;
  unsigned layout_list_item_count_ = 0;
  bool needs_marker_counter_update_ = false;

  // This map keeps track of SVG <text> descendants.
  // LayoutNGSVGText needs to do re-layout on transform changes of any ancestor
  // because LayoutNGSVGText's layout result depends on scaling factors
  // computed with ancestor transforms.
  Member<TrackedDescendantsMap> svg_text_descendants_;

  unsigned hit_test_count_;
  unsigned hit_test_cache_hits_;
  Member<HitTestCache> hit_test_cache_;

  // FrameViewAutoSizeInfo controls scrollbar appearance manually rather than
  // relying on layout. These members are used to override the ScrollbarModes
  // calculated from style. kScrollbarAuto disables the override.
  mojom::blink::ScrollbarMode autosize_h_scrollbar_mode_;
  mojom::blink::ScrollbarMode autosize_v_scrollbar_mode_;

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
