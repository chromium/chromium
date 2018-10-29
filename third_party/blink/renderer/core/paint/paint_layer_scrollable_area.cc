/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@gmail.com>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

#include "base/numerics/checked_math.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/css/pseudo_style_request.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_scrollbar.h"
#include "third_party/blink/renderer/core/layout/layout_scrollbar_part.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_util.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/find_paint_offset_and_visual_rect_needing_update.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_fragment.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/scroll/scroll_alignment.h"

namespace blink {

namespace {

// Default value is set to 15 as the default
// minimum size used by firefox is 15x15.
static const int kDefaultMinimumWidthForResizing = 15;
static const int kDefaultMinimumHeightForResizing = 15;

}  // namespace

static LayoutRect LocalToAbsolute(const LayoutBox& box, LayoutRect rect) {
  return LayoutRect(
      box.LocalToAbsoluteQuad(FloatQuad(FloatRect(rect)), kUseTransforms)
          .BoundingBox());
}

static LayoutRect AbsoluteToLocal(const LayoutBox& box, LayoutRect rect) {
  return LayoutRect(
      box.AbsoluteToLocalQuad(FloatQuad(FloatRect(rect)), kUseTransforms)
          .BoundingBox());
}

PaintLayerScrollableAreaRareData::PaintLayerScrollableAreaRareData() = default;

const int kResizerControlExpandRatioForTouch = 2;

PaintLayerScrollableArea::PaintLayerScrollableArea(PaintLayer& layer)
    : layer_(&layer),
      in_resize_mode_(false),
      scrolls_overflow_(false),
      in_overflow_relayout_(false),
      allow_second_overflow_relayout_(false),
      needs_composited_scrolling_(false),
      rebuild_horizontal_scrollbar_layer_(false),
      rebuild_vertical_scrollbar_layer_(false),
      needs_scroll_offset_clamp_(false),
      needs_relayout_(false),
      had_horizontal_scrollbar_before_relayout_(false),
      had_vertical_scrollbar_before_relayout_(false),
      scroll_origin_changed_(false),
      scrollbar_manager_(*this),
      scroll_corner_(nullptr),
      resizer_(nullptr),
      scroll_anchor_(this),
      non_composited_main_thread_scrolling_reasons_(0),
      horizontal_scrollbar_previously_was_overlay_(false),
      vertical_scrollbar_previously_was_overlay_(false),
      scrolling_background_display_item_client_(*this) {
  Node* node = GetLayoutBox()->GetNode();
  if (node && node->IsElementNode()) {
    // We save and restore only the scrollOffset as the other scroll values are
    // recalculated.
    Element* element = ToElement(node);
    scroll_offset_ = element->SavedLayerScrollOffset();
    if (!scroll_offset_.IsZero())
      GetScrollAnimator().SetCurrentOffset(scroll_offset_);
    element->SetSavedLayerScrollOffset(ScrollOffset());
  }
  UpdateResizerAreaSet();
}

PaintLayerScrollableArea::~PaintLayerScrollableArea() {
  CHECK(HasBeenDisposed());
}

void PaintLayerScrollableArea::DidScroll(const FloatPoint& position) {
  ScrollableArea::DidScroll(position);
  // This should be alive if it receives composited scroll callbacks.
  CHECK(!HasBeenDisposed());
}

void PaintLayerScrollableArea::Dispose() {
  if (InResizeMode() && !GetLayoutBox()->DocumentBeingDestroyed()) {
    if (LocalFrame* frame = GetLayoutBox()->GetFrame())
      frame->GetEventHandler().ResizeScrollableAreaDestroyed();
  }

  if (LocalFrame* frame = GetLayoutBox()->GetFrame()) {
    if (LocalFrameView* frame_view = frame->View()) {
      frame_view->RemoveScrollableArea(this);
      frame_view->RemoveAnimatingScrollableArea(this);
    }
  }

  non_composited_main_thread_scrolling_reasons_ = 0;

  if (ScrollingCoordinator* scrolling_coordinator = GetScrollingCoordinator())
    scrolling_coordinator->WillDestroyScrollableArea(this);

  if (!GetLayoutBox()->DocumentBeingDestroyed()) {
    Node* node = GetLayoutBox()->GetNode();
    // FIXME: Make setSavedLayerScrollOffset take DoubleSize. crbug.com/414283.
    if (node && node->IsElementNode())
      ToElement(node)->SetSavedLayerScrollOffset(scroll_offset_);
  }

  if (LocalFrame* frame = GetLayoutBox()->GetFrame()) {
    if (LocalFrameView* frame_view = frame->View())
      frame_view->RemoveResizerArea(*GetLayoutBox());
  }

  // Note: it is not safe to call ScrollAnchor::clear if the document is being
  // destroyed, because LayoutObjectChildList::removeChildNode skips the call to
  // willBeRemovedFromTree,
  // leaving the ScrollAnchor with a stale LayoutObject pointer.
  scroll_anchor_.Dispose();

  GetLayoutBox()
      ->GetDocument()
      .GetPage()
      ->GlobalRootScrollerController()
      .DidDisposeScrollableArea(*this);

  scrollbar_manager_.Dispose();

  if (scroll_corner_)
    scroll_corner_->Destroy();
  if (resizer_)
    resizer_->Destroy();

  ClearScrollableArea();

  if (SmoothScrollSequencer* sequencer = GetSmoothScrollSequencer())
    sequencer->DidDisposeScrollableArea(*this);

  layer_ = nullptr;
}

bool PaintLayerScrollableArea::HasBeenDisposed() const {
  return !layer_;
}

void PaintLayerScrollableArea::Trace(blink::Visitor* visitor) {
  visitor->Trace(scrollbar_manager_);
  visitor->Trace(scroll_anchor_);
  visitor->Trace(scrolling_background_display_item_client_);
  ScrollableArea::Trace(visitor);
}

bool PaintLayerScrollableArea::IsThrottled() const {
  return GetLayoutBox()->GetFrame()->ShouldThrottleRendering();
}

ChromeClient* PaintLayerScrollableArea::GetChromeClient() const {
  if (HasBeenDisposed())
    return nullptr;
  if (Page* page = GetLayoutBox()->GetFrame()->GetPage())
    return &page->GetChromeClient();
  return nullptr;
}

SmoothScrollSequencer* PaintLayerScrollableArea::GetSmoothScrollSequencer()
    const {
  if (HasBeenDisposed())
    return nullptr;

  return &GetLayoutBox()->GetFrame()->GetSmoothScrollSequencer();
}

GraphicsLayer* PaintLayerScrollableArea::LayerForScrolling() const {
  return Layer()->HasCompositedLayerMapping()
             ? Layer()->GetCompositedLayerMapping()->ScrollingContentsLayer()
             : nullptr;
}

GraphicsLayer* PaintLayerScrollableArea::LayerForHorizontalScrollbar() const {
  // See crbug.com/343132.
  DisableCompositingQueryAsserts disabler;

  return Layer()->HasCompositedLayerMapping()
             ? Layer()
                   ->GetCompositedLayerMapping()
                   ->LayerForHorizontalScrollbar()
             : nullptr;
}

GraphicsLayer* PaintLayerScrollableArea::LayerForVerticalScrollbar() const {
  // See crbug.com/343132.
  DisableCompositingQueryAsserts disabler;

  return Layer()->HasCompositedLayerMapping()
             ? Layer()->GetCompositedLayerMapping()->LayerForVerticalScrollbar()
             : nullptr;
}

GraphicsLayer* PaintLayerScrollableArea::LayerForScrollCorner() const {
  // See crbug.com/343132.
  DisableCompositingQueryAsserts disabler;

  return Layer()->HasCompositedLayerMapping()
             ? Layer()->GetCompositedLayerMapping()->LayerForScrollCorner()
             : nullptr;
}

bool PaintLayerScrollableArea::IsActive() const {
  Page* page = GetLayoutBox()->GetFrame()->GetPage();
  return page && page->GetFocusController().IsActive();
}

bool PaintLayerScrollableArea::IsScrollCornerVisible() const {
  return !ScrollCornerRect().IsEmpty();
}

static int CornerStart(const LayoutBox& box,
                       int min_x,
                       int max_x,
                       int thickness) {
  if (box.ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    return min_x + box.StyleRef().BorderLeftWidth();
  return max_x - thickness - box.StyleRef().BorderRightWidth();
}

IntRect PaintLayerScrollableArea::PaintLayerScrollableArea::CornerRect(
    const IntRect& bounds) const {
  int horizontal_thickness;
  int vertical_thickness;
  if (!VerticalScrollbar() && !HorizontalScrollbar()) {
    // FIXME: This isn't right. We need to know the thickness of custom
    // scrollbars even when they don't exist in order to set the resizer square
    // size properly.
    horizontal_thickness = GetPageScrollbarTheme().ScrollbarThickness();
    vertical_thickness = horizontal_thickness;
  } else if (VerticalScrollbar() && !HorizontalScrollbar()) {
    horizontal_thickness = VerticalScrollbar()->ScrollbarThickness();
    vertical_thickness = horizontal_thickness;
  } else if (HorizontalScrollbar() && !VerticalScrollbar()) {
    vertical_thickness = HorizontalScrollbar()->ScrollbarThickness();
    horizontal_thickness = vertical_thickness;
  } else {
    horizontal_thickness = VerticalScrollbar()->ScrollbarThickness();
    vertical_thickness = HorizontalScrollbar()->ScrollbarThickness();
  }
  return IntRect(CornerStart(*GetLayoutBox(), bounds.X(), bounds.MaxX(),
                             horizontal_thickness),
                 bounds.MaxY() - vertical_thickness -
                     GetLayoutBox()->StyleRef().BorderBottomWidth(),
                 horizontal_thickness, vertical_thickness);
}

IntRect PaintLayerScrollableArea::ScrollCornerRect() const {
  // We have a scrollbar corner when a scrollbar is visible and not filling the
  // entire length of the box.
  // This happens when:
  // (a) A resizer is present and at least one scrollbar is present
  // (b) Both scrollbars are present.
  bool has_horizontal_bar = HorizontalScrollbar();
  bool has_vertical_bar = VerticalScrollbar();
  bool has_resizer = GetLayoutBox()->StyleRef().Resize() != EResize::kNone;
  if ((has_horizontal_bar && has_vertical_bar) ||
      (has_resizer && (has_horizontal_bar || has_vertical_bar))) {
    return CornerRect(GetLayoutBox()->PixelSnappedBorderBoxRect(
        Layer()->SubpixelAccumulation()));
  }
  return IntRect();
}

IntRect
PaintLayerScrollableArea::ConvertFromScrollbarToContainingEmbeddedContentView(
    const Scrollbar& scrollbar,
    const IntRect& scrollbar_rect) const {
  LayoutView* view = GetLayoutBox()->View();
  if (!view)
    return scrollbar_rect;

  IntRect rect = scrollbar_rect;
  rect.Move(ScrollbarOffset(scrollbar));

  return view->GetFrameView()->ConvertFromLayoutObject(*GetLayoutBox(), rect);
}

IntPoint
PaintLayerScrollableArea::ConvertFromScrollbarToContainingEmbeddedContentView(
    const Scrollbar& scrollbar,
    const IntPoint& scrollbar_point) const {
  LayoutView* view = GetLayoutBox()->View();
  if (!view)
    return scrollbar_point;

  IntPoint point = scrollbar_point;
  point.Move(ScrollbarOffset(scrollbar));
  return view->GetFrameView()->ConvertFromLayoutObject(*GetLayoutBox(), point);
}

IntPoint
PaintLayerScrollableArea::ConvertFromContainingEmbeddedContentViewToScrollbar(
    const Scrollbar& scrollbar,
    const IntPoint& parent_point) const {
  LayoutView* view = GetLayoutBox()->View();
  if (!view)
    return parent_point;

  IntPoint point = view->GetFrameView()->ConvertToLayoutObject(*GetLayoutBox(),
                                                               parent_point);

  point.Move(-ScrollbarOffset(scrollbar));
  return point;
}

IntPoint PaintLayerScrollableArea::ConvertFromRootFrame(
    const IntPoint& point_in_root_frame) const {
  LayoutView* view = GetLayoutBox()->View();
  if (!view)
    return point_in_root_frame;

  return view->GetFrameView()->ConvertFromRootFrame(point_in_root_frame);
}

int PaintLayerScrollableArea::ScrollSize(
    ScrollbarOrientation orientation) const {
  IntSize scroll_dimensions =
      MaximumScrollOffsetInt() - MinimumScrollOffsetInt();
  return (orientation == kHorizontalScrollbar) ? scroll_dimensions.Width()
                                               : scroll_dimensions.Height();
}

void PaintLayerScrollableArea::UpdateScrollOffset(
    const ScrollOffset& new_offset,
    ScrollType scroll_type) {
  if (HasBeenDisposed() || GetScrollOffset() == new_offset)
    return;

  bool offset_was_zero = scroll_offset_.IsZero();
  scroll_offset_ = new_offset;

  LocalFrame* frame = GetLayoutBox()->GetFrame();
  DCHECK(frame);

  LocalFrameView* frame_view = GetLayoutBox()->GetFrameView();
  bool is_root_layer = Layer()->IsRootLayer();

  TRACE_EVENT1("devtools.timeline", "ScrollLayer", "data",
               InspectorScrollLayerEvent::Data(GetLayoutBox()));

  // FIXME(420741): Resolve circular dependency between scroll offset and
  // compositing state, and remove this disabler.
  DisableCompositingQueryAsserts disabler;

  // Update the positions of our child layers (if needed as only fixed layers
  // should be impacted by a scroll).
  if (!frame_view->IsInPerformLayout()) {
    // If we're in the middle of layout, we'll just update layers once layout
    // has finished.
    Layer()->UpdateLayerPositionsAfterOverflowScroll();
    // Update regions, scrolling may change the clip of a particular region.
    frame_view->UpdateDocumentAnnotatedRegions();

    // As a performance optimization, the scroll offset of the root layer is
    // not included in EmbeddedContentView's stored frame rect, so there is no
    // reason to mark the FrameView as needing a geometry update here.
    if (is_root_layer)
      frame_view->SetRootLayerDidScroll();
    else
      frame_view->SetNeedsUpdateGeometries();
  }
  UpdateCompositingLayersAfterScroll();

  GetLayoutBox()->MayUpdateHoverWhenContentUnderMouseChanged(
      frame->GetEventHandler());

  if (scroll_type == kUserScroll || scroll_type == kCompositorScroll) {
    Page* page = frame->GetPage();
    if (page)
      page->GetChromeClient().ClearToolTip(*frame);
  }

  InvalidatePaintForScrollOffsetChange(offset_was_zero);

  // The scrollOffsetTranslation paint property depends on the scroll offset.
  // (see: PaintPropertyTreeBuilder::UpdateScrollAndScrollTranslation).
  GetLayoutBox()->SetNeedsPaintPropertyUpdate();

  // Schedule the scroll DOM event.
  if (GetLayoutBox()->GetNode()) {
    GetLayoutBox()->GetNode()->GetDocument().EnqueueScrollEventForNode(
        GetLayoutBox()->GetNode());
  }

  GetLayoutBox()->View()->ClearHitTestCache();

  // Inform the FrameLoader of the new scroll position, so it can be restored
  // when navigating back.
  if (is_root_layer) {
    frame_view->GetFrame().Loader().SaveScrollState();
    frame_view->DidChangeScrollOffset();
    if (scroll_type == kCompositorScroll || scroll_type == kUserScroll) {
      if (DocumentLoader* document_loader = frame->Loader().GetDocumentLoader())
        document_loader->GetInitialScrollState().was_scrolled_by_user = true;
    }
  }

  if (IsExplicitScrollType(scroll_type)) {
    if (scroll_type != kCompositorScroll)
      ShowOverlayScrollbars();
    frame_view->ClearFragmentAnchor();
    GetScrollAnchor()->Clear();
  }

  if (AXObjectCache* cache =
          GetLayoutBox()->GetDocument().ExistingAXObjectCache())
    cache->HandleScrollPositionChanged(GetLayoutBox());
}

void PaintLayerScrollableArea::InvalidatePaintForScrollOffsetChange(
    bool offset_was_zero) {
  InvalidatePaintForStickyDescendants();

  bool requires_paint_invalidation = false;

  // "background-attachment: local" causes the background of this element to
  // change position due to scroll so a paint invalidation is needed.
  // TODO(pdr): This invalidation can be removed if the local background
  // attachment is painted into the scrolling contents.
  if (ScrollsOverflow() &&
      GetLayoutBox()->StyleRef().BackgroundLayers().Attachment() ==
          EFillAttachment::kLocal) {
    if (!UsesCompositedScrolling())
      requires_paint_invalidation = true;

    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      GetLayoutBox()->SetShouldDoFullPaintInvalidation();
      return;
    }
  }

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    // TODO(pdr): If this is the root frame, descendants with fixed background
    // attachments need to be invalidated.

    // A scroll offset translation is still needed for overflow:hidden and there
    // is an optimization to only create this translation node when scroll
    // offset is non-zero (see: NeedsScrollOrScrollTranslation in
    // PaintPropertyTreeBuilder.cpp). Because of this optimization, gaining or
    // losing scroll offset can change whether a property exists and we have to
    // invalidate paint to ensure this property gets picked up in BlockPainter.
    bool needs_repaint_for_overflow_hidden =
        !ScrollsOverflow() && (offset_was_zero || GetScrollOffset().IsZero());
    // An invalidation is needed to ensure the interest rect is recalculated
    // so newly-scrolled-to items are repainted. We may want to set a flag on
    // PaintLayer to just check for interest rect changes instead of doing a
    // full repaint.
    bool needs_repaint_for_interest_rect = true;
    if (needs_repaint_for_overflow_hidden || needs_repaint_for_interest_rect)
      Layer()->SetNeedsRepaint();

    return;
  }

  LocalFrameView* frame_view = GetLayoutBox()->GetFrameView();
  bool is_root_layer = Layer()->IsRootLayer();
  frame_view->InvalidateBackgroundAttachmentFixedDescendants(*GetLayoutBox());

  if (is_root_layer && frame_view->HasViewportConstrainedObjects() &&
      !frame_view->InvalidateViewportConstrainedObjects()) {
    requires_paint_invalidation = true;
  }

  if (requires_paint_invalidation) {
    GetLayoutBox()->SetShouldDoFullPaintInvalidation();
    GetLayoutBox()->SetSubtreeShouldCheckForPaintInvalidation();
  } else if (!UsesCompositedScrolling()) {
    // If any scrolling content might have ben clipped by a cull rect, then
    // that cull rect could be affected by scroll offset. For composited
    // scrollers, this will be taken care of by the interest rect computation
    // in CompositedLayerMapping.
    // TODO(chrishtr): replace this shortcut with interest rects.
    Layer()->SetNeedsRepaint();
  }
}

IntSize PaintLayerScrollableArea::ScrollOffsetInt() const {
  return FlooredIntSize(scroll_offset_);
}

ScrollOffset PaintLayerScrollableArea::GetScrollOffset() const {
  return scroll_offset_;
}

IntSize PaintLayerScrollableArea::MinimumScrollOffsetInt() const {
  return ToIntSize(-ScrollOrigin());
}

IntSize PaintLayerScrollableArea::MaximumScrollOffsetInt() const {
  if (!GetLayoutBox()->HasOverflowClip())
    return ToIntSize(-ScrollOrigin());

  IntSize content_size = ContentsSize();

  Page* page = GetLayoutBox()->GetDocument().GetPage();
  DCHECK(page);
  TopDocumentRootScrollerController& controller =
      page->GlobalRootScrollerController();

  // The global root scroller should be clipped by the top LocalFrameView rather
  // than it's overflow clipping box. This is to ensure that content exposed by
  // hiding the URL bar at the bottom of the screen is visible.
  IntSize visible_size;
  if (this == controller.RootScrollerArea()) {
    visible_size = controller.RootScrollerVisibleArea();
  } else {
    visible_size =
        PixelSnappedIntRect(GetLayoutBox()->OverflowClipRect(
                                GetLayoutBox()->Location(),
                                kIgnorePlatformAndCSSOverlayScrollbarSize))
            .Size();
  }

  // TODO(skobes): We should really ASSERT that contentSize >= visibleSize
  // when we are not the root layer, but we can't because contentSize is
  // based on stale layout overflow data (http://crbug.com/576933).
  content_size = content_size.ExpandedTo(visible_size);

  return ToIntSize(-ScrollOrigin() + (content_size - visible_size));
}

void PaintLayerScrollableArea::VisibleSizeChanged() {
  ShowOverlayScrollbars();
}

LayoutRect PaintLayerScrollableArea::LayoutContentRect(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  // LayoutContentRect is conceptually the same as the box's client rect.
  LayoutSize layer_size(Layer()->Size());
  LayoutUnit border_width = GetLayoutBox()->BorderWidth();
  LayoutUnit border_height = GetLayoutBox()->BorderHeight();
  LayoutUnit horizontal_scrollbar_height, vertical_scrollbar_width;
  if (scrollbar_inclusion == kExcludeScrollbars) {
    horizontal_scrollbar_height = LayoutUnit(
        HorizontalScrollbar() && !HorizontalScrollbar()->IsOverlayScrollbar()
            ? HorizontalScrollbar()->ScrollbarThickness()
            : 0);
    vertical_scrollbar_width = LayoutUnit(
        VerticalScrollbar() && !VerticalScrollbar()->IsOverlayScrollbar()
            ? VerticalScrollbar()->ScrollbarThickness()
            : 0);
  }

  return LayoutRect(
      LayoutPoint(ScrollPosition()),
      LayoutSize(
          layer_size.Width() - border_width - vertical_scrollbar_width,
          layer_size.Height() - border_height - horizontal_scrollbar_height)
          .ExpandedTo(LayoutSize()));
}

IntRect PaintLayerScrollableArea::VisibleContentRect(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  LayoutRect layout_content_rect(LayoutContentRect(scrollbar_inclusion));
  // TODO(szager): It's not clear that Floor() is the right thing to do here;
  // what is the correct behavior for fractional scroll offsets?
  return IntRect(FlooredIntPoint(layout_content_rect.Location()),
                 PixelSnappedIntSize(layout_content_rect.Size(),
                                     GetLayoutBox()->Location()));
}

LayoutRect PaintLayerScrollableArea::VisibleScrollSnapportRect(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  const ComputedStyle* style = GetLayoutBox()->Style();
  LayoutRect layout_content_rect(LayoutContentRect(scrollbar_inclusion));
  layout_content_rect.MoveBy(LayoutPoint(-ScrollOrigin()));
  LayoutRectOutsets padding(MinimumValueForLength(style->ScrollPaddingTop(),
                                                  layout_content_rect.Height()),
                            MinimumValueForLength(style->ScrollPaddingRight(),
                                                  layout_content_rect.Width()),
                            MinimumValueForLength(style->ScrollPaddingBottom(),
                                                  layout_content_rect.Height()),
                            MinimumValueForLength(style->ScrollPaddingLeft(),
                                                  layout_content_rect.Width()));
  layout_content_rect.Contract(padding);
  return layout_content_rect;
}

IntSize PaintLayerScrollableArea::ContentsSize() const {
  LayoutPoint offset(
      GetLayoutBox()->ClientLeft() + GetLayoutBox()->Location().X(),
      GetLayoutBox()->ClientTop() + GetLayoutBox()->Location().Y());
  return PixelSnappedContentsSize(offset);
}

IntSize PaintLayerScrollableArea::PixelSnappedContentsSize(
    const LayoutPoint& paint_offset) const {
  return PixelSnappedIntSize(overflow_rect_.Size(), paint_offset);
}

void PaintLayerScrollableArea::ContentsResized() {
  ScrollableArea::ContentsResized();
  // Need to update the bounds of the scroll property.
  GetLayoutBox()->SetNeedsPaintPropertyUpdate();
  Layer()->SetNeedsCompositingInputsUpdate();
}

IntPoint PaintLayerScrollableArea::LastKnownMousePosition() const {
  return GetLayoutBox()->GetFrame() ? GetLayoutBox()
                                          ->GetFrame()
                                          ->GetEventHandler()
                                          .LastKnownMousePositionInRootFrame()
                                    : IntPoint();
}

bool PaintLayerScrollableArea::ScrollAnimatorEnabled() const {
  if (HasBeenDisposed())
    return false;
  if (Settings* settings = GetLayoutBox()->GetFrame()->GetSettings())
    return settings->GetScrollAnimatorEnabled();
  return false;
}

bool PaintLayerScrollableArea::ShouldSuspendScrollAnimations() const {
  if (HasBeenDisposed())
    return true;
  LayoutView* view = GetLayoutBox()->View();
  if (!view)
    return true;
  return !GetLayoutBox()->GetDocument().LoadEventFinished();
}

void PaintLayerScrollableArea::ScrollbarVisibilityChanged() {
  UpdateScrollbarEnabledState();

  // Paint properties need to be updated, because clip rects
  // are affected by overlay scrollbars.
  layer_->GetLayoutObject().SetNeedsPaintPropertyUpdate();

  // TODO(chrishr): this should be able to be removed.
  layer_->ClearClipRects();

  if (LayoutView* view = GetLayoutBox()->View())
    view->ClearHitTestCache();
}

void PaintLayerScrollableArea::ScrollbarFrameRectChanged() {
  // Size of non-overlay scrollbar affects overflow clip rect.
  if (!HasOverlayScrollbars())
    GetLayoutBox()->SetNeedsPaintPropertyUpdate();
}

bool PaintLayerScrollableArea::ScrollbarsCanBeActive() const {
  LayoutView* view = GetLayoutBox()->View();
  if (!view)
    return false;

  // TODO(szager): This conditional is weird and likely obsolete. Originally
  // added in commit eb0d49caaee2b275ff524d3945a74e8d9180eb7d.
  LocalFrameView* frame_view = view->GetFrameView();
  if (frame_view != frame_view->GetFrame().View())
    return false;

  return !!frame_view->GetFrame().GetDocument();
}

IntRect PaintLayerScrollableArea::ScrollableAreaBoundingBox() const {
  if (LocalFrame* frame = GetLayoutBox()->GetFrame()) {
    if (LocalFrameView* local_root = frame->LocalFrameRoot().View()) {
      return local_root->RootFrameToDocument(frame->View()->ConvertToRootFrame(
          GetLayoutBox()->AbsoluteBoundingBoxRect(0)));
    }
  }
  return IntRect();
}

void PaintLayerScrollableArea::RegisterForAnimation() {
  if (HasBeenDisposed())
    return;
  if (LocalFrame* frame = GetLayoutBox()->GetFrame()) {
    if (LocalFrameView* frame_view = frame->View())
      frame_view->AddAnimatingScrollableArea(this);
  }
}

void PaintLayerScrollableArea::DeregisterForAnimation() {
  if (HasBeenDisposed())
    return;
  if (LocalFrame* frame = GetLayoutBox()->GetFrame()) {
    if (LocalFrameView* frame_view = frame->View())
      frame_view->RemoveAnimatingScrollableArea(this);
  }
}

bool PaintLayerScrollableArea::UserInputScrollable(
    ScrollbarOrientation orientation) const {
  if (orientation == kVerticalScrollbar &&
      GetLayoutBox()->GetDocument().IsVerticalScrollEnforced()) {
    return false;
  }

  if (GetLayoutBox()->IsIntrinsicallyScrollable(orientation))
    return true;

  if (GetLayoutBox()->IsLayoutView()) {
    Document& document = GetLayoutBox()->GetDocument();
    Element* fullscreen_element = Fullscreen::FullscreenElementFrom(document);
    if (fullscreen_element && fullscreen_element != document.documentElement())
      return false;

    ScrollbarMode h_mode;
    ScrollbarMode v_mode;
    ToLayoutView(GetLayoutBox())->CalculateScrollbarModes(h_mode, v_mode);
    ScrollbarMode mode =
        (orientation == kHorizontalScrollbar) ? h_mode : v_mode;
    return mode == kScrollbarAuto || mode == kScrollbarAlwaysOn;
  }

  EOverflow overflow_style = (orientation == kHorizontalScrollbar)
                                 ? GetLayoutBox()->StyleRef().OverflowX()
                                 : GetLayoutBox()->StyleRef().OverflowY();
  return (overflow_style == EOverflow::kScroll ||
          overflow_style == EOverflow::kAuto ||
          overflow_style == EOverflow::kOverlay);
}

bool PaintLayerScrollableArea::ShouldPlaceVerticalScrollbarOnLeft() const {
  return GetLayoutBox()->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft();
}

int PaintLayerScrollableArea::PageStep(ScrollbarOrientation orientation) const {
  // Paging scroll operations should take scroll-padding into account [1]. So we
  // use the snapport rect to calculate the page step instead of the visible
  // rect.
  // [1] https://drafts.csswg.org/css-scroll-snap/#scroll-padding
  IntSize snapport_size = VisibleScrollSnapportRect().PixelSnappedSize();
  int length = (orientation == kHorizontalScrollbar) ? snapport_size.Width()
                                                     : snapport_size.Height();
  int min_page_step = static_cast<float>(length) *
                      ScrollableArea::MinFractionToStepWhenPaging();
  int page_step = max(min_page_step, length - MaxOverlapBetweenPages());
  return max(page_step, 1);
}

LayoutBox* PaintLayerScrollableArea::GetLayoutBox() const {
  return layer_->GetLayoutBox();
}

PaintLayer* PaintLayerScrollableArea::Layer() const {
  return layer_;
}

LayoutUnit PaintLayerScrollableArea::ScrollWidth() const {
  return overflow_rect_.Width();
}

LayoutUnit PaintLayerScrollableArea::ScrollHeight() const {
  return overflow_rect_.Height();
}

void PaintLayerScrollableArea::UpdateScrollOrigin() {
  // This should do nothing prior to first layout; the if-clause will catch
  // that.
  if (OverflowRect().IsEmpty())
    return;
  LayoutRect scrollable_overflow(overflow_rect_);
  scrollable_overflow.Move(-GetLayoutBox()->BorderLeft(),
                           -GetLayoutBox()->BorderTop());
  IntPoint new_origin(-scrollable_overflow.PixelSnappedLocation() +
                      GetLayoutBox()->OriginAdjustmentForScrollbars());
  if (new_origin != scroll_origin_)
    scroll_origin_changed_ = true;
  scroll_origin_ = new_origin;
}

void PaintLayerScrollableArea::UpdateScrollDimensions() {
  LayoutRect new_overflow_rect = GetLayoutBox()->LayoutOverflowRect();
  GetLayoutBox()->FlipForWritingMode(new_overflow_rect);

  // The layout viewport can be larger than the document's layout overflow when
  // top controls are hidden.  Expand the overflow here to ensure that our
  // contents size >= visible size.
  new_overflow_rect.Unite(
      LayoutRect(new_overflow_rect.Location(),
                 LayoutContentRect(kExcludeScrollbars).Size()));

  if (overflow_rect_.Size() != new_overflow_rect.Size())
    ContentsResized();
  overflow_rect_ = new_overflow_rect;
  UpdateScrollOrigin();
}

void PaintLayerScrollableArea::UpdateScrollbarEnabledState() {
  bool force_disable =
      GetPageScrollbarTheme().ShouldDisableInvisibleScrollbars() &&
      ScrollbarsHiddenIfOverlay();

  if (HorizontalScrollbar())
    HorizontalScrollbar()->SetEnabled(HasHorizontalOverflow() &&
                                      !force_disable);
  if (VerticalScrollbar())
    VerticalScrollbar()->SetEnabled(HasVerticalOverflow() && !force_disable);
}

void PaintLayerScrollableArea::UpdateScrollbarProportions() {
  if (Scrollbar* horizontal_scrollbar = HorizontalScrollbar())
    horizontal_scrollbar->SetProportion(VisibleWidth(), ContentsSize().Width());
  if (Scrollbar* vertical_scrollbar = VerticalScrollbar())
    vertical_scrollbar->SetProportion(VisibleHeight(), ContentsSize().Height());
}

void PaintLayerScrollableArea::SetScrollOffsetUnconditionally(
    const ScrollOffset& offset,
    ScrollType scroll_type) {
  CancelScrollAnimation();
  ScrollOffsetChanged(offset, scroll_type);
}

void PaintLayerScrollableArea::UpdateAfterLayout() {
  bool scrollbars_are_frozen =
      (in_overflow_relayout_ && !allow_second_overflow_relayout_) ||
      FreezeScrollbarsScope::ScrollbarsAreFrozen();
  allow_second_overflow_relayout_ = false;

  if (NeedsScrollbarReconstruction()) {
    SetHasHorizontalScrollbar(false);
    SetHasVerticalScrollbar(false);
  }

  UpdateScrollDimensions();

  bool had_horizontal_scrollbar = HasHorizontalScrollbar();
  bool had_vertical_scrollbar = HasVerticalScrollbar();

  bool needs_horizontal_scrollbar;
  bool needs_vertical_scrollbar;
  ComputeScrollbarExistence(needs_horizontal_scrollbar,
                            needs_vertical_scrollbar);

  // Removing auto scrollbars is a heuristic and can be incorrect if the content
  // size depends on the scrollbar size (e.g., sized with percentages). Removing
  // scrollbars can require two additional layout passes so this is only done on
  // the first layout (!in_overflow_layout).
  if (!in_overflow_relayout_ && !scrollbars_are_frozen &&
      TryRemovingAutoScrollbars(needs_horizontal_scrollbar,
                                needs_vertical_scrollbar)) {
    needs_horizontal_scrollbar = needs_vertical_scrollbar = false;
    allow_second_overflow_relayout_ = true;
  }

  bool horizontal_scrollbar_should_change =
      needs_horizontal_scrollbar != had_horizontal_scrollbar;
  bool vertical_scrollbar_should_change =
      needs_vertical_scrollbar != had_vertical_scrollbar;

  bool scrollbars_will_change =
      !scrollbars_are_frozen &&
      (horizontal_scrollbar_should_change || vertical_scrollbar_should_change);
  if (scrollbars_will_change) {
    SetHasHorizontalScrollbar(needs_horizontal_scrollbar);
    SetHasVerticalScrollbar(needs_vertical_scrollbar);

    // If we change scrollbars on the layout viewport, the visual viewport
    // needs to update paint properties to account for the correct
    // scrollbounds.
    if (LocalFrameView* frame_view = GetLayoutBox()->GetFrameView()) {
      if (this == frame_view->LayoutViewport()) {
        GetLayoutBox()
            ->GetFrame()
            ->GetPage()
            ->GetVisualViewport()
            .SetNeedsPaintPropertyUpdate();
      }
    }

    if (HasScrollbar())
      UpdateScrollCornerStyle();

    Layer()->UpdateSelfPaintingLayer();

    // Force an update since we know the scrollbars have changed things.
    if (GetLayoutBox()->GetDocument().HasAnnotatedRegions())
      GetLayoutBox()->GetDocument().SetAnnotatedRegionsDirty(true);

    // Our proprietary overflow: overlay value doesn't trigger a layout.
    if (((horizontal_scrollbar_should_change &&
          GetLayoutBox()->StyleRef().OverflowX() != EOverflow::kOverlay) ||
         (vertical_scrollbar_should_change &&
          GetLayoutBox()->StyleRef().OverflowY() != EOverflow::kOverlay))) {
      if ((vertical_scrollbar_should_change &&
           GetLayoutBox()->IsHorizontalWritingMode()) ||
          (horizontal_scrollbar_should_change &&
           !GetLayoutBox()->IsHorizontalWritingMode())) {
        GetLayoutBox()->SetPreferredLogicalWidthsDirty();
      }
      // If the box is managed by LayoutNG, don't go here. We don't want to
      // re-enter the NG layout algorithm for this box from here.
      if (!IsManagedByLayoutNG(*GetLayoutBox())) {
        if (PreventRelayoutScope::RelayoutIsPrevented()) {
          // We're not doing re-layout right now, but we still want to
          // add the scrollbar to the logical width now, to facilitate parent
          // layout.
          GetLayoutBox()->UpdateLogicalWidth();
          PreventRelayoutScope::SetBoxNeedsLayout(
              *this, had_horizontal_scrollbar, had_vertical_scrollbar);
        } else {
          in_overflow_relayout_ = true;
          SubtreeLayoutScope layout_scope(*GetLayoutBox());
          layout_scope.SetNeedsLayout(
              GetLayoutBox(), LayoutInvalidationReason::kScrollbarChanged);
          if (GetLayoutBox()->IsLayoutBlock()) {
            LayoutBlock* block = ToLayoutBlock(GetLayoutBox());
            block->ScrollbarsChanged(horizontal_scrollbar_should_change,
                                     vertical_scrollbar_should_change);
            block->UpdateBlockLayout(true);
          } else {
            GetLayoutBox()->UpdateLayout();
          }
          in_overflow_relayout_ = false;
          scrollbar_manager_.DestroyDetachedScrollbars();
        }
        LayoutObject* parent = GetLayoutBox()->Parent();
        if (parent && parent->IsFlexibleBox()) {
          ToLayoutFlexibleBox(parent)->ClearCachedMainSizeForChild(
              *GetLayoutBox());
        }
      }
    }
  }

  {
    // Hits in
    // compositing/overflow/automatically-opt-into-composited-scrolling-after-style-change.html.
    DisableCompositingQueryAsserts disabler;

    UpdateScrollbarEnabledState();

    UpdateScrollbarProportions();
  }

  if (!scrollbars_are_frozen && HasOverlayScrollbars()) {
    if (!ScrollSize(kHorizontalScrollbar))
      SetHasHorizontalScrollbar(false);
    if (!ScrollSize(kVerticalScrollbar))
      SetHasVerticalScrollbar(false);
  }

  ClampScrollOffsetAfterOverflowChange();

  if (!scrollbars_are_frozen) {
    UpdateScrollableAreaSet();
  }

  DisableCompositingQueryAsserts disabler;
  PositionOverflowControls();
}

void PaintLayerScrollableArea::ClampScrollOffsetAfterOverflowChange() {
  if (HasBeenDisposed())
    return;

  // If a vertical scrollbar was removed, the min/max scroll offsets may have
  // changed, so the scroll offsets needs to be clamped.  If the scroll offset
  // did not change, but the scroll origin *did* change, we still need to notify
  // the scrollbars to update their dimensions.

  if (DelayScrollOffsetClampScope::ClampingIsDelayed()) {
    DelayScrollOffsetClampScope::SetNeedsClamp(this);
    return;
  }

  UpdateScrollDimensions();
  if (ScrollOriginChanged())
    SetScrollOffsetUnconditionally(ClampScrollOffset(GetScrollOffset()));
  else
    ScrollableArea::SetScrollOffset(GetScrollOffset(), kClampingScroll);

  SetNeedsScrollOffsetClamp(false);
  ResetScrollOriginChanged();
  scrollbar_manager_.DestroyDetachedScrollbars();
}

void PaintLayerScrollableArea::DidChangeGlobalRootScroller() {
  // Being the global root scroller will affect clipping size due to browser
  // controls behavior so we need to update compositing based on updated clip
  // geometry.
  if (GetLayoutBox()->GetNode()->IsElementNode()) {
    ToElement(GetLayoutBox()->GetNode())->SetNeedsCompositingUpdate();
    GetLayoutBox()->SetNeedsPaintPropertyUpdate();
  }

  // On Android, where the VisualViewport supplies scrollbars, we need to
  // remove the PLSA's scrollbars if we become the global root scroller.
  // In general, this would be problematic as that can cause layout but this
  // should only ever apply with overlay scrollbars.
  if (GetLayoutBox()->GetFrame()->GetSettings() &&
      GetLayoutBox()->GetFrame()->GetSettings()->GetViewportEnabled()) {
    bool needs_horizontal_scrollbar;
    bool needs_vertical_scrollbar;
    ComputeScrollbarExistence(needs_horizontal_scrollbar,
                              needs_vertical_scrollbar);
    SetHasHorizontalScrollbar(needs_horizontal_scrollbar);
    SetHasVerticalScrollbar(needs_vertical_scrollbar);
  }
}

bool PaintLayerScrollableArea::ShouldPerformScrollAnchoring() const {
  return scroll_anchor_.HasScroller() && GetLayoutBox() &&
         GetLayoutBox()->StyleRef().OverflowAnchor() !=
             EOverflowAnchor::kNone &&
         !GetLayoutBox()->GetDocument().FinishingOrIsPrinting();
}

bool PaintLayerScrollableArea::RestoreScrollAnchor(
    const SerializedAnchor& serialized_anchor) {
  return ShouldPerformScrollAnchoring() &&
         scroll_anchor_.RestoreAnchor(serialized_anchor);
}

FloatQuad PaintLayerScrollableArea::LocalToVisibleContentQuad(
    const FloatQuad& quad,
    const LayoutObject* local_object,
    MapCoordinatesFlags flags) const {
  LayoutBox* box = GetLayoutBox();
  if (!box)
    return quad;
  DCHECK(local_object);
  return local_object->LocalToAncestorQuad(quad, box, flags);
}

scoped_refptr<base::SingleThreadTaskRunner>
PaintLayerScrollableArea::GetTimerTaskRunner() const {
  return GetLayoutBox()->GetFrame()->GetTaskRunner(TaskType::kInternalDefault);
}

ScrollBehavior PaintLayerScrollableArea::ScrollBehaviorStyle() const {
  return GetLayoutBox()->StyleRef().GetScrollBehavior();
}

bool PaintLayerScrollableArea::HasHorizontalOverflow() const {
  // TODO(szager): Make the algorithm for adding/subtracting overflow:auto
  // scrollbars memoryless (crbug.com/625300).  This client_width hack will
  // prevent the spurious horizontal scrollbar, but it can cause a converse
  // problem: it can leave a sliver of horizontal overflow hidden behind the
  // vertical scrollbar without creating a horizontal scrollbar.  This
  // converse problem seems to happen much less frequently in practice, so we
  // bias the logic towards preventing unwanted horizontal scrollbars, which
  // are more common and annoying.
  LayoutUnit client_width =
      LayoutContentRect(kIncludeScrollbars).Width() -
      VerticalScrollbarWidth(kIgnorePlatformAndCSSOverlayScrollbarSize);
  if (NeedsRelayout() && !HadVerticalScrollbarBeforeRelayout())
    client_width += VerticalScrollbarWidth();
  LayoutUnit scroll_width(ScrollWidth());
  LayoutUnit box_x = GetLayoutBox()->Location().X();
  return SnapSizeToPixel(scroll_width, box_x) >
         SnapSizeToPixel(client_width, box_x);
}

bool PaintLayerScrollableArea::HasVerticalOverflow() const {
  LayoutUnit client_height =
      LayoutContentRect(kIncludeScrollbars).Height() -
      HorizontalScrollbarHeight(kIgnorePlatformAndCSSOverlayScrollbarSize);
  LayoutUnit scroll_height(ScrollHeight());
  LayoutUnit box_y = GetLayoutBox()->Location().Y();
  return SnapSizeToPixel(scroll_height, box_y) >
         SnapSizeToPixel(client_height, box_y);
}

// This function returns true if the given box requires overflow scrollbars (as
// opposed to the 'viewport' scrollbars managed by the PaintLayerCompositor).
// FIXME: we should use the same scrolling machinery for both the viewport and
// overflow. Currently, we need to avoid producing scrollbars here if they'll be
// handled externally in the RLC.
static bool CanHaveOverflowScrollbars(const LayoutBox& box) {
  return box.GetDocument().ViewportDefiningElement() != box.GetNode();
}

void PaintLayerScrollableArea::UpdateAfterStyleChange(
    const ComputedStyle* old_style) {
  // Don't do this on first style recalc, before layout has ever happened.
  if (!OverflowRect().Size().IsZero()) {
    UpdateScrollableAreaSet();
  }

  // Whenever background changes on the scrollable element, the scroll bar
  // overlay style might need to be changed to have contrast against the
  // background.
  // Skip the need scrollbar check, because we dont know do we need a scrollbar
  // when this method get called.
  Color old_background;
  if (old_style) {
    old_background =
        old_style->VisitedDependentColor(GetCSSPropertyBackgroundColor());
  }
  Color new_background = GetLayoutBox()->StyleRef().VisitedDependentColor(
      GetCSSPropertyBackgroundColor());

  if (new_background != old_background) {
    RecalculateScrollbarOverlayColorTheme(new_background);
  }

  bool needs_horizontal_scrollbar;
  bool needs_vertical_scrollbar;
  // We add auto scrollbars only during layout to prevent spurious activations.
  ComputeScrollbarExistence(needs_horizontal_scrollbar,
                            needs_vertical_scrollbar, kForbidAddingAutoBars);

  // Avoid some unnecessary computation if there were and will be no scrollbars.
  if (!HasScrollbar() && !needs_horizontal_scrollbar &&
      !needs_vertical_scrollbar)
    return;

  bool horizontal_scrollbar_changed =
      SetHasHorizontalScrollbar(needs_horizontal_scrollbar);
  bool vertical_scrollbar_changed =
      SetHasVerticalScrollbar(needs_vertical_scrollbar);

  if (GetLayoutBox()->IsLayoutBlock() &&
      (horizontal_scrollbar_changed || vertical_scrollbar_changed)) {
    ToLayoutBlock(GetLayoutBox())
        ->ScrollbarsChanged(horizontal_scrollbar_changed,
                            vertical_scrollbar_changed,
                            LayoutBlock::ScrollbarChangeContext::kStyleChange);
  }

  // With overflow: scroll, scrollbars are always visible but may be disabled.
  // When switching to another value, we need to re-enable them (see bug 11985).
  if (HasHorizontalScrollbar() && old_style &&
      old_style->OverflowX() == EOverflow::kScroll &&
      GetLayoutBox()->StyleRef().OverflowX() != EOverflow::kScroll) {
    HorizontalScrollbar()->SetEnabled(true);
  }

  if (HasVerticalScrollbar() && old_style &&
      old_style->OverflowY() == EOverflow::kScroll &&
      GetLayoutBox()->StyleRef().OverflowY() != EOverflow::kScroll) {
    VerticalScrollbar()->SetEnabled(true);
  }

  // FIXME: Need to detect a swap from custom to native scrollbars (and vice
  // versa).
  if (HorizontalScrollbar())
    HorizontalScrollbar()->StyleChanged();
  if (VerticalScrollbar())
    VerticalScrollbar()->StyleChanged();

  UpdateScrollCornerStyle();
  UpdateResizerAreaSet();
  UpdateResizerStyle(old_style);
}

void PaintLayerScrollableArea::UpdateAfterOverflowRecalc() {
  UpdateScrollDimensions();
  UpdateScrollbarProportions();

  bool needs_horizontal_scrollbar;
  bool needs_vertical_scrollbar;
  ComputeScrollbarExistence(needs_horizontal_scrollbar,
                            needs_vertical_scrollbar);

  bool horizontal_scrollbar_should_change =
      needs_horizontal_scrollbar != HasHorizontalScrollbar();
  bool vertical_scrollbar_should_change =
      needs_vertical_scrollbar != HasVerticalScrollbar();

  if ((GetLayoutBox()->HasAutoHorizontalScrollbar() &&
       horizontal_scrollbar_should_change) ||
      (GetLayoutBox()->HasAutoVerticalScrollbar() &&
       vertical_scrollbar_should_change)) {
    GetLayoutBox()->SetNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::kUnknown);
  }

  ClampScrollOffsetAfterOverflowChange();
}

IntRect PaintLayerScrollableArea::RectForHorizontalScrollbar(
    const IntRect& border_box_rect) const {
  if (!HasHorizontalScrollbar())
    return IntRect();

  const IntRect& scroll_corner = ScrollCornerRect();

  return IntRect(
      HorizontalScrollbarStart(border_box_rect.X()),
      border_box_rect.MaxY() - GetLayoutBox()->BorderBottom().ToInt() -
          HorizontalScrollbar()->ScrollbarThickness(),
      border_box_rect.Width() -
          (GetLayoutBox()->BorderLeft() + GetLayoutBox()->BorderRight())
              .ToInt() -
          scroll_corner.Width(),
      HorizontalScrollbar()->ScrollbarThickness());
}

IntRect PaintLayerScrollableArea::RectForVerticalScrollbar(
    const IntRect& border_box_rect) const {
  if (!HasVerticalScrollbar())
    return IntRect();

  const IntRect& scroll_corner = ScrollCornerRect();

  return IntRect(
      VerticalScrollbarStart(border_box_rect.X(), border_box_rect.MaxX()),
      border_box_rect.Y() + GetLayoutBox()->BorderTop().ToInt(),
      VerticalScrollbar()->ScrollbarThickness(),
      border_box_rect.Height() -
          (GetLayoutBox()->BorderTop() + GetLayoutBox()->BorderBottom())
              .ToInt() -
          scroll_corner.Height());
}

int PaintLayerScrollableArea::VerticalScrollbarStart(int min_x,
                                                     int max_x) const {
  if (GetLayoutBox()->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    return min_x + GetLayoutBox()->BorderLeft().ToInt();
  return max_x - GetLayoutBox()->BorderRight().ToInt() -
         VerticalScrollbar()->ScrollbarThickness();
}

int PaintLayerScrollableArea::HorizontalScrollbarStart(int min_x) const {
  int x = min_x + GetLayoutBox()->BorderLeft().ToInt();
  if (GetLayoutBox()->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    x += HasVerticalScrollbar()
             ? VerticalScrollbar()->ScrollbarThickness()
             : ResizerCornerRect(GetLayoutBox()->PixelSnappedBorderBoxRect(
                                     Layer()->SubpixelAccumulation()),
                                 kResizerForPointer)
                   .Width();
  return x;
}

IntSize PaintLayerScrollableArea::ScrollbarOffset(
    const Scrollbar& scrollbar) const {
  // TODO(szager): Factor out vertical offset calculation into other methods,
  // for symmetry with *ScrollbarStart methods for horizontal offset.
  if (&scrollbar == VerticalScrollbar()) {
    return IntSize(
        VerticalScrollbarStart(0, Layer()->PixelSnappedSize().Width()),
        GetLayoutBox()->BorderTop().ToInt());
  }

  if (&scrollbar == HorizontalScrollbar()) {
    return IntSize(HorizontalScrollbarStart(0),
                   GetLayoutBox()->BorderTop().ToInt() +
                       VisibleContentRect(kIncludeScrollbars).Height() -
                       HorizontalScrollbar()->ScrollbarThickness());
  }

  NOTREACHED();
  return IntSize();
}

static inline const LayoutObject& ScrollbarStyleSource(
    const LayoutBox& layout_box) {
  if (layout_box.IsLayoutView()) {
    Document& doc = layout_box.GetDocument();
    if (Settings* settings = doc.GetSettings()) {
      if (!settings->GetAllowCustomScrollbarInMainFrame() &&
          layout_box.GetFrame() && layout_box.GetFrame()->IsMainFrame())
        return layout_box;
    }

    // Try the <body> element first as a scrollbar source, but only if the body
    // can scroll.
    Element* body = doc.body();
    if (body && body->GetLayoutObject() && body->GetLayoutObject()->IsBox() &&
        body->GetLayoutObject()->StyleRef().HasPseudoStyle(kPseudoIdScrollbar))
      return *body->GetLayoutObject();

    // If the <body> didn't have a custom style, then the root element might.
    Element* doc_element = doc.documentElement();
    if (doc_element && doc_element->GetLayoutObject() &&
        doc_element->GetLayoutObject()->StyleRef().HasPseudoStyle(
            kPseudoIdScrollbar))
      return *doc_element->GetLayoutObject();
  }

  return layout_box;
}

int PaintLayerScrollableArea::HypotheticalScrollbarThickness(
    ScrollbarOrientation orientation) const {
  Scrollbar* scrollbar = orientation == kHorizontalScrollbar
                             ? HorizontalScrollbar()
                             : VerticalScrollbar();
  if (scrollbar)
    return scrollbar->ScrollbarThickness();

  const LayoutObject& style_source = ScrollbarStyleSource(*GetLayoutBox());
  bool has_custom_scrollbar_style =
      style_source.StyleRef().HasPseudoStyle(kPseudoIdScrollbar);
  if (has_custom_scrollbar_style) {
    return LayoutScrollbar::HypotheticalScrollbarThickness(
        orientation, *GetLayoutBox(), style_source);
  }

  ScrollbarControlSize scrollbar_size = kRegularScrollbar;
  if (style_source.StyleRef().HasAppearance()) {
    scrollbar_size = LayoutTheme::GetTheme().ScrollbarControlSizeForPart(
        style_source.StyleRef().Appearance());
  }
  ScrollbarTheme& theme = GetPageScrollbarTheme();
  if (theme.UsesOverlayScrollbars())
    return 0;
  int thickness = theme.ScrollbarThickness(scrollbar_size);
  return GetLayoutBox()
      ->GetDocument()
      .GetPage()
      ->GetChromeClient()
      .WindowToViewportScalar(thickness);
}

bool PaintLayerScrollableArea::NeedsScrollbarReconstruction() const {
  if (!HasScrollbar())
    return false;

  const LayoutObject& style_source = ScrollbarStyleSource(*GetLayoutBox());
  bool needs_custom =
      style_source.IsBox() &&
      style_source.StyleRef().HasPseudoStyle(kPseudoIdScrollbar);

  Scrollbar* scrollbars[] = {HorizontalScrollbar(), VerticalScrollbar()};

  for (Scrollbar* scrollbar : scrollbars) {
    if (!scrollbar)
      continue;

    // We have a native scrollbar that should be custom, or vice versa.
    if (scrollbar->IsCustomScrollbar() != needs_custom)
      return true;

    if (needs_custom) {
      DCHECK(scrollbar->IsCustomScrollbar());
      // We have a custom scrollbar with a stale m_owner.
      if (ToLayoutScrollbar(scrollbar)->StyleSource()->GetLayoutObject() !=
          style_source) {
        return true;
      }

      // Should use custom scrollbar and nothing should change.
      continue;
    }

    // Check if native scrollbar should change.
    Page* page = GetLayoutBox()->GetFrame()->LocalFrameRoot().GetPage();
    DCHECK(page);
    ScrollbarTheme* current_theme = &page->GetScrollbarTheme();

    if (current_theme != &scrollbar->GetTheme())
      return true;
  }
  return false;
}

void PaintLayerScrollableArea::ComputeScrollbarExistence(
    bool& needs_horizontal_scrollbar,
    bool& needs_vertical_scrollbar,
    ComputeScrollbarExistenceOption option) const {
  // Scrollbars may be hidden or provided by visual viewport or frame instead.
  DCHECK(GetLayoutBox()->GetFrame()->GetSettings());
  if (VisualViewportSuppliesScrollbars() ||
      !CanHaveOverflowScrollbars(*GetLayoutBox()) ||
      GetLayoutBox()->GetFrame()->GetSettings()->GetHideScrollbars()) {
    needs_horizontal_scrollbar = false;
    needs_vertical_scrollbar = false;
    return;
  }

  needs_horizontal_scrollbar = GetLayoutBox()->ScrollsOverflowX();
  needs_vertical_scrollbar = GetLayoutBox()->ScrollsOverflowY();

  // Don't add auto scrollbars if the box contents aren't visible.
  if (GetLayoutBox()->HasAutoHorizontalScrollbar()) {
    if (option == kForbidAddingAutoBars)
      needs_horizontal_scrollbar &= HasHorizontalScrollbar();
    needs_horizontal_scrollbar &=
        GetLayoutBox()->IsRooted() && HasHorizontalOverflow() &&
        VisibleContentRect(kIncludeScrollbars).Height();
  }

  if (GetLayoutBox()->HasAutoVerticalScrollbar()) {
    if (option == kForbidAddingAutoBars)
      needs_vertical_scrollbar &= HasVerticalScrollbar();
    needs_vertical_scrollbar &= GetLayoutBox()->IsRooted() &&
                                HasVerticalOverflow() &&
                                VisibleContentRect(kIncludeScrollbars).Width();
  }

  if (GetLayoutBox()->IsLayoutView()) {
    ScrollbarMode h_mode;
    ScrollbarMode v_mode;
    ToLayoutView(GetLayoutBox())->CalculateScrollbarModes(h_mode, v_mode);

    // Look for the scrollbarModes and reset the needs Horizontal & vertical
    // Scrollbar values based on scrollbarModes, as during force style change
    // StyleResolver::styleForDocument returns documentStyle with no overflow
    // values, due to which we are destroying the scrollbars that were already
    // present.
    if (h_mode == kScrollbarAlwaysOn)
      needs_horizontal_scrollbar = true;
    else if (h_mode == kScrollbarAlwaysOff)
      needs_horizontal_scrollbar = false;
    if (v_mode == kScrollbarAlwaysOn)
      needs_vertical_scrollbar = true;
    else if (v_mode == kScrollbarAlwaysOff)
      needs_vertical_scrollbar = false;
  }
}

bool PaintLayerScrollableArea::TryRemovingAutoScrollbars(
    const bool& needs_horizontal_scrollbar,
    const bool& needs_vertical_scrollbar) {
  // If scrollbars are removed but the content size depends on the scrollbars,
  // additional layouts will be required to size the content. Therefore, only
  // remove auto scrollbars for the initial layout pass.
  DCHECK(!in_overflow_relayout_);

  if (!needs_horizontal_scrollbar && !needs_vertical_scrollbar)
    return false;

  if (GetLayoutBox()->IsLayoutView()) {
    ScrollbarMode h_mode;
    ScrollbarMode v_mode;
    ToLayoutView(GetLayoutBox())->CalculateScrollbarModes(h_mode, v_mode);
    if (h_mode != kScrollbarAuto || v_mode != kScrollbarAuto)
      return false;

    IntSize visible_size_with_scrollbars =
        VisibleContentRect(kIncludeScrollbars).Size();
    if (ScrollWidth() <= visible_size_with_scrollbars.Width() &&
        ScrollHeight() <= visible_size_with_scrollbars.Height()) {
      return true;
    }
  } else {
    if (!GetLayoutBox()->HasAutoVerticalScrollbar() ||
        !GetLayoutBox()->HasAutoHorizontalScrollbar())
      return false;

    LayoutSize client_size_with_scrollbars =
        LayoutContentRect(kIncludeScrollbars).Size();
    if (ScrollWidth() <= client_size_with_scrollbars.Width() &&
        ScrollHeight() <= client_size_with_scrollbars.Height()) {
      return true;
    }
  }

  return false;
}

bool PaintLayerScrollableArea::SetHasHorizontalScrollbar(bool has_scrollbar) {
  if (FreezeScrollbarsScope::ScrollbarsAreFrozen())
    return false;

  if (has_scrollbar == HasHorizontalScrollbar())
    return false;

  SetScrollbarNeedsPaintInvalidation(kHorizontalScrollbar);

  scrollbar_manager_.SetHasHorizontalScrollbar(has_scrollbar);

  UpdateScrollOrigin();

  // Destroying or creating one bar can cause our scrollbar corner to come and
  // go. We need to update the opposite scrollbar's style.
  if (HasHorizontalScrollbar())
    HorizontalScrollbar()->StyleChanged();
  if (HasVerticalScrollbar())
    VerticalScrollbar()->StyleChanged();

  SetScrollCornerNeedsPaintInvalidation();

  // Force an update since we know the scrollbars have changed things.
  if (GetLayoutBox()->GetDocument().HasAnnotatedRegions())
    GetLayoutBox()->GetDocument().SetAnnotatedRegionsDirty(true);
  return true;
}

bool PaintLayerScrollableArea::SetHasVerticalScrollbar(bool has_scrollbar) {
  if (FreezeScrollbarsScope::ScrollbarsAreFrozen())
    return false;

  if (has_scrollbar == HasVerticalScrollbar())
    return false;

  SetScrollbarNeedsPaintInvalidation(kVerticalScrollbar);

  scrollbar_manager_.SetHasVerticalScrollbar(has_scrollbar);

  UpdateScrollOrigin();

  // Destroying or creating one bar can cause our scrollbar corner to come and
  // go. We need to update the opposite scrollbar's style.
  if (HasHorizontalScrollbar())
    HorizontalScrollbar()->StyleChanged();
  if (HasVerticalScrollbar())
    VerticalScrollbar()->StyleChanged();

  SetScrollCornerNeedsPaintInvalidation();

  // Force an update since we know the scrollbars have changed things.
  if (GetLayoutBox()->GetDocument().HasAnnotatedRegions())
    GetLayoutBox()->GetDocument().SetAnnotatedRegionsDirty(true);
  return true;
}

int PaintLayerScrollableArea::VerticalScrollbarWidth(
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  if (!HasVerticalScrollbar())
    return 0;
  if (overlay_scrollbar_clip_behavior ==
          kIgnorePlatformAndCSSOverlayScrollbarSize &&
      GetLayoutBox()->StyleRef().OverflowY() == EOverflow::kOverlay) {
    return 0;
  }
  if ((overlay_scrollbar_clip_behavior == kIgnorePlatformOverlayScrollbarSize ||
       overlay_scrollbar_clip_behavior ==
           kIgnorePlatformAndCSSOverlayScrollbarSize ||
       !VerticalScrollbar()->ShouldParticipateInHitTesting()) &&
      VerticalScrollbar()->IsOverlayScrollbar()) {
    return 0;
  }
  return VerticalScrollbar()->ScrollbarThickness();
}

int PaintLayerScrollableArea::HorizontalScrollbarHeight(
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  if (!HasHorizontalScrollbar())
    return 0;
  if (overlay_scrollbar_clip_behavior ==
          kIgnorePlatformAndCSSOverlayScrollbarSize &&
      GetLayoutBox()->StyleRef().OverflowX() == EOverflow::kOverlay) {
    return 0;
  }
  if ((overlay_scrollbar_clip_behavior == kIgnorePlatformOverlayScrollbarSize ||
       overlay_scrollbar_clip_behavior ==
           kIgnorePlatformAndCSSOverlayScrollbarSize ||
       !HorizontalScrollbar()->ShouldParticipateInHitTesting()) &&
      HorizontalScrollbar()->IsOverlayScrollbar()) {
    return 0;
  }
  return HorizontalScrollbar()->ScrollbarThickness();
}

void PaintLayerScrollableArea::SnapAfterScrollbarScrolling(
    ScrollbarOrientation orientation) {
  SnapCoordinator* snap_coordinator =
      GetLayoutBox()->GetDocument().GetSnapCoordinator();
  if (!snap_coordinator)
    return;

  snap_coordinator->SnapForEndPosition(*GetLayoutBox(),
                                       orientation == kHorizontalScrollbar,
                                       orientation == kVerticalScrollbar);
}

void PaintLayerScrollableArea::PositionOverflowControls() {
  if (!HasScrollbar() && !GetLayoutBox()->CanResize())
    return;

  const IntRect border_box =
      GetLayoutBox()->PixelSnappedBorderBoxRect(layer_->SubpixelAccumulation());

  if (Scrollbar* vertical_scrollbar = VerticalScrollbar())
    vertical_scrollbar->SetFrameRect(RectForVerticalScrollbar(border_box));

  if (Scrollbar* horizontal_scrollbar = HorizontalScrollbar())
    horizontal_scrollbar->SetFrameRect(RectForHorizontalScrollbar(border_box));

  const IntRect& scroll_corner = ScrollCornerRect();
  if (scroll_corner_)
    scroll_corner_->SetFrameRect(LayoutRect(scroll_corner));

  if (resizer_)
    resizer_->SetFrameRect(
        LayoutRect(ResizerCornerRect(border_box, kResizerForPointer)));

  // FIXME, this should eventually be removed, once we are certain that
  // composited controls get correctly positioned on a compositor update. For
  // now, conservatively leaving this unchanged.
  if (Layer()->HasCompositedLayerMapping())
    Layer()->GetCompositedLayerMapping()->PositionOverflowControlsLayers();
}

void PaintLayerScrollableArea::UpdateScrollCornerStyle() {
  if (!scroll_corner_ && !HasScrollbar())
    return;
  if (!scroll_corner_ && HasOverlayScrollbars())
    return;

  const LayoutObject& style_source = ScrollbarStyleSource(*GetLayoutBox());
  scoped_refptr<ComputedStyle> corner =
      GetLayoutBox()->HasOverflowClip()
          ? style_source.GetUncachedPseudoStyle(
                PseudoStyleRequest(kPseudoIdScrollbarCorner),
                style_source.Style())
          : scoped_refptr<ComputedStyle>(nullptr);
  if (corner) {
    if (!scroll_corner_) {
      scroll_corner_ = LayoutScrollbarPart::CreateAnonymous(
          &GetLayoutBox()->GetDocument(), this);
      scroll_corner_->SetDangerousOneWayParent(GetLayoutBox());
    }
    scroll_corner_->SetStyleWithWritingModeOfParent(std::move(corner));
  } else if (scroll_corner_) {
    scroll_corner_->Destroy();
    scroll_corner_ = nullptr;
  }
}

bool PaintLayerScrollableArea::HitTestOverflowControls(
    HitTestResult& result,
    const IntPoint& local_point) {
  if (!HasScrollbar() && !GetLayoutBox()->CanResize())
    return false;

  IntRect resize_control_rect;
  if (GetLayoutBox()->StyleRef().Resize() != EResize::kNone) {
    resize_control_rect =
        ResizerCornerRect(GetLayoutBox()->PixelSnappedBorderBoxRect(
                              Layer()->SubpixelAccumulation()),
                          kResizerForPointer);
    if (resize_control_rect.Contains(local_point))
      return true;
  }
  int resize_control_size = max(resize_control_rect.Height(), 0);

  IntRect visible_rect = VisibleContentRect(kIncludeScrollbars);

  if (HasVerticalScrollbar() &&
      VerticalScrollbar()->ShouldParticipateInHitTesting()) {
    LayoutRect v_bar_rect(
        VerticalScrollbarStart(0, Layer()->PixelSnappedSize().Width()),
        GetLayoutBox()->BorderTop().ToInt(),
        VerticalScrollbar()->ScrollbarThickness(),
        visible_rect.Height() -
            (HasHorizontalScrollbar()
                 ? HorizontalScrollbar()->ScrollbarThickness()
                 : resize_control_size));
    if (v_bar_rect.Contains(local_point)) {
      result.SetScrollbar(VerticalScrollbar());
      return true;
    }
  }

  resize_control_size = max(resize_control_rect.Width(), 0);
  if (HasHorizontalScrollbar() &&
      HorizontalScrollbar()->ShouldParticipateInHitTesting()) {
    // TODO(crbug.com/638981): Are the conversions to int intentional?
    int h_scrollbar_thickness = HorizontalScrollbar()->ScrollbarThickness();
    LayoutRect h_bar_rect(
        HorizontalScrollbarStart(0),
        GetLayoutBox()->BorderTop().ToInt() + visible_rect.Height() -
            h_scrollbar_thickness,
        visible_rect.Width() - (HasVerticalScrollbar()
                                    ? VerticalScrollbar()->ScrollbarThickness()
                                    : resize_control_size),
        h_scrollbar_thickness);
    if (h_bar_rect.Contains(local_point)) {
      result.SetScrollbar(HorizontalScrollbar());
      return true;
    }
  }

  // FIXME: We should hit test the m_scrollCorner and pass it back through the
  // result.

  return false;
}

IntRect PaintLayerScrollableArea::ResizerCornerRect(
    const IntRect& bounds,
    ResizerHitTestType resizer_hit_test_type) const {
  if (GetLayoutBox()->StyleRef().Resize() == EResize::kNone)
    return IntRect();
  IntRect corner = CornerRect(bounds);

  if (resizer_hit_test_type == kResizerForTouch) {
    // We make the resizer virtually larger for touch hit testing. With the
    // expanding ratio k = ResizerControlExpandRatioForTouch, we first move
    // the resizer rect (of width w & height h), by (-w * (k-1), -h * (k-1)),
    // then expand the rect by new_w/h = w/h * k.
    int expand_ratio = kResizerControlExpandRatioForTouch - 1;
    corner.Move(-corner.Width() * expand_ratio,
                -corner.Height() * expand_ratio);
    corner.Expand(corner.Width() * expand_ratio,
                  corner.Height() * expand_ratio);
  }

  return corner;
}

IntRect PaintLayerScrollableArea::ScrollCornerAndResizerRect() const {
  IntRect scroll_corner_and_resizer = ScrollCornerRect();
  if (scroll_corner_and_resizer.IsEmpty()) {
    scroll_corner_and_resizer =
        ResizerCornerRect(GetLayoutBox()->PixelSnappedBorderBoxRect(
                              Layer()->SubpixelAccumulation()),
                          kResizerForPointer);
  }
  return scroll_corner_and_resizer;
}

bool PaintLayerScrollableArea::IsPointInResizeControl(
    const IntPoint& absolute_point,
    ResizerHitTestType resizer_hit_test_type) const {
  if (!GetLayoutBox()->CanResize())
    return false;

  IntPoint local_point = RoundedIntPoint(GetLayoutBox()->AbsoluteToLocal(
      FloatPoint(absolute_point), kUseTransforms));
  IntRect local_bounds(IntPoint(), Layer()->PixelSnappedSize());
  return ResizerCornerRect(local_bounds, resizer_hit_test_type)
      .Contains(local_point);
}

bool PaintLayerScrollableArea::HitTestResizerInFragments(
    const PaintLayerFragments& layer_fragments,
    const HitTestLocation& hit_test_location) const {
  if (!GetLayoutBox()->CanResize())
    return false;

  if (layer_fragments.IsEmpty())
    return false;

  for (int i = layer_fragments.size() - 1; i >= 0; --i) {
    const PaintLayerFragment& fragment = layer_fragments.at(i);
    if (fragment.background_rect.Intersects(hit_test_location) &&
        ResizerCornerRect(PixelSnappedIntRect(fragment.layer_bounds),
                          kResizerForPointer)
            .Contains(hit_test_location.RoundedPoint()))
      return true;
  }

  return false;
}

void PaintLayerScrollableArea::UpdateResizerAreaSet() {
  LocalFrame* frame = GetLayoutBox()->GetFrame();
  if (!frame)
    return;
  LocalFrameView* frame_view = frame->View();
  if (!frame_view)
    return;
  if (GetLayoutBox()->CanResize())
    frame_view->AddResizerArea(*GetLayoutBox());
  else
    frame_view->RemoveResizerArea(*GetLayoutBox());
}

void PaintLayerScrollableArea::UpdateResizerStyle(
    const ComputedStyle* old_style) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() && old_style &&
      old_style->Resize() != GetLayoutBox()->StyleRef().Resize()) {
    // Invalidate the composited scroll corner layer on resize style change.
    if (auto* graphics_layer = LayerForScrollCorner())
      graphics_layer->SetNeedsDisplay();
  }

  if (!resizer_ && !GetLayoutBox()->CanResize())
    return;

  const LayoutObject& style_source = ScrollbarStyleSource(*GetLayoutBox());
  scoped_refptr<ComputedStyle> resizer =
      GetLayoutBox()->HasOverflowClip()
          ? style_source.GetUncachedPseudoStyle(
                PseudoStyleRequest(kPseudoIdResizer), style_source.Style())
          : scoped_refptr<ComputedStyle>(nullptr);
  if (resizer) {
    if (!resizer_) {
      resizer_ = LayoutScrollbarPart::CreateAnonymous(
          &GetLayoutBox()->GetDocument(), this);
      resizer_->SetDangerousOneWayParent(GetLayoutBox());
    }
    resizer_->SetStyleWithWritingModeOfParent(std::move(resizer));
  } else if (resizer_) {
    resizer_->Destroy();
    resizer_ = nullptr;
  }
}

void PaintLayerScrollableArea::InvalidateAllStickyConstraints() {
  if (PaintLayerScrollableAreaRareData* d = RareData()) {
    for (PaintLayer* sticky_layer : d->sticky_constraints_map_.Keys()) {
      if (sticky_layer->GetLayoutObject().StyleRef().GetPosition() ==
          EPosition::kSticky) {
        sticky_layer->SetNeedsCompositingInputsUpdate();
        sticky_layer->GetLayoutObject().SetNeedsPaintPropertyUpdate();
      }
    }
    d->sticky_constraints_map_.clear();
  }
}

void PaintLayerScrollableArea::InvalidateStickyConstraintsFor(
    PaintLayer* layer,
    bool needs_compositing_update) {
  if (PaintLayerScrollableAreaRareData* d = RareData()) {
    d->sticky_constraints_map_.erase(layer);
    if (needs_compositing_update &&
        layer->GetLayoutObject().StyleRef().HasStickyConstrainedPosition()) {
      layer->SetNeedsCompositingInputsUpdate();
      layer->GetLayoutObject().SetNeedsPaintPropertyUpdate();
    }
  }
}

bool PaintLayerScrollableArea::HasStickyDescendants() const {
  if (const PaintLayerScrollableAreaRareData* d = RareData())
    return !d->sticky_constraints_map_.IsEmpty();
  return false;
}

bool PaintLayerScrollableArea::HasNonCompositedStickyDescendants() const {
  if (const PaintLayerScrollableAreaRareData* d = RareData()) {
    for (const PaintLayer* sticky_layer : d->sticky_constraints_map_.Keys()) {
      if (sticky_layer->GetLayoutObject().IsSlowRepaintConstrainedObject())
        return true;
    }
  }
  return false;
}

void PaintLayerScrollableArea::InvalidatePaintForStickyDescendants() {
  if (PaintLayerScrollableAreaRareData* d = RareData()) {
    for (PaintLayer* sticky_layer : d->sticky_constraints_map_.Keys())
      sticky_layer->GetLayoutObject().SetNeedsPaintPropertyUpdate();
  }
}

IntSize PaintLayerScrollableArea::OffsetFromResizeCorner(
    const IntPoint& absolute_point) const {
  // Currently the resize corner is either the bottom right corner or the bottom
  // left corner.
  // FIXME: This assumes the location is 0, 0. Is this guaranteed to always be
  // the case?
  IntSize element_size = Layer()->PixelSnappedSize();
  if (GetLayoutBox()->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    element_size.SetWidth(0);
  IntPoint resizer_point = IntPoint(element_size);
  IntPoint local_point = RoundedIntPoint(GetLayoutBox()->AbsoluteToLocal(
      FloatPoint(absolute_point), kUseTransforms));
  return local_point - resizer_point;
}

LayoutSize PaintLayerScrollableArea::MinimumSizeForResizing(float zoom_factor) {
  LayoutUnit min_width =
      MinimumValueForLength(GetLayoutBox()->StyleRef().MinWidth(),
                            GetLayoutBox()->ContainingBlock()->Size().Width());
  LayoutUnit min_height =
      MinimumValueForLength(GetLayoutBox()->StyleRef().MinHeight(),
                            GetLayoutBox()->ContainingBlock()->Size().Height());
  min_width = std::max(LayoutUnit(min_width / zoom_factor),
                       LayoutUnit(kDefaultMinimumWidthForResizing));
  min_height = std::max(LayoutUnit(min_height / zoom_factor),
                        LayoutUnit(kDefaultMinimumHeightForResizing));
  return LayoutSize(min_width, min_height);
}

void PaintLayerScrollableArea::Resize(const IntPoint& pos,
                                      const LayoutSize& old_offset) {
  // FIXME: This should be possible on generated content but is not right now.
  if (!InResizeMode() || !GetLayoutBox()->CanResize() ||
      !GetLayoutBox()->GetNode())
    return;

  DCHECK(GetLayoutBox()->GetNode()->IsElementNode());
  Element* element = ToElement(GetLayoutBox()->GetNode());

  Document& document = element->GetDocument();

  float zoom_factor = GetLayoutBox()->StyleRef().EffectiveZoom();

  IntSize new_offset =
      OffsetFromResizeCorner(document.View()->ConvertFromRootFrame(pos));
  new_offset.SetWidth(new_offset.Width() / zoom_factor);
  new_offset.SetHeight(new_offset.Height() / zoom_factor);

  LayoutSize current_size = GetLayoutBox()->Size();
  current_size.Scale(1 / zoom_factor);

  LayoutSize adjusted_old_offset = LayoutSize(
      old_offset.Width() / zoom_factor, old_offset.Height() / zoom_factor);
  if (GetLayoutBox()->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
    new_offset.SetWidth(-new_offset.Width());
    adjusted_old_offset.SetWidth(-adjusted_old_offset.Width());
  }

  LayoutSize difference((current_size + new_offset - adjusted_old_offset)
                            .ExpandedTo(MinimumSizeForResizing(zoom_factor)) -
                        current_size);

  bool is_box_sizing_border =
      GetLayoutBox()->StyleRef().BoxSizing() == EBoxSizing::kBorderBox;

  EResize resize = GetLayoutBox()->StyleRef().Resize();
  if (resize != EResize::kVertical && difference.Width()) {
    if (element->IsFormControlElement()) {
      // Make implicit margins from the theme explicit (see
      // <http://bugs.webkit.org/show_bug.cgi?id=9547>).
      element->SetInlineStyleProperty(
          CSSPropertyMarginLeft, GetLayoutBox()->MarginLeft() / zoom_factor,
          CSSPrimitiveValue::UnitType::kPixels);
      element->SetInlineStyleProperty(
          CSSPropertyMarginRight, GetLayoutBox()->MarginRight() / zoom_factor,
          CSSPrimitiveValue::UnitType::kPixels);
    }
    LayoutUnit base_width =
        GetLayoutBox()->Size().Width() -
        (is_box_sizing_border ? LayoutUnit()
                              : GetLayoutBox()->BorderAndPaddingWidth());
    base_width = LayoutUnit(base_width / zoom_factor);
    element->SetInlineStyleProperty(CSSPropertyWidth,
                                    RoundToInt(base_width + difference.Width()),
                                    CSSPrimitiveValue::UnitType::kPixels);
  }

  if (resize != EResize::kHorizontal && difference.Height()) {
    if (element->IsFormControlElement()) {
      // Make implicit margins from the theme explicit (see
      // <http://bugs.webkit.org/show_bug.cgi?id=9547>).
      element->SetInlineStyleProperty(CSSPropertyMarginTop,
                                      GetLayoutBox()->MarginTop() / zoom_factor,
                                      CSSPrimitiveValue::UnitType::kPixels);
      element->SetInlineStyleProperty(
          CSSPropertyMarginBottom, GetLayoutBox()->MarginBottom() / zoom_factor,
          CSSPrimitiveValue::UnitType::kPixels);
    }
    LayoutUnit base_height =
        GetLayoutBox()->Size().Height() -
        (is_box_sizing_border ? LayoutUnit()
                              : GetLayoutBox()->BorderAndPaddingHeight());
    base_height = LayoutUnit(base_height / zoom_factor);
    element->SetInlineStyleProperty(
        CSSPropertyHeight, RoundToInt(base_height + difference.Height()),
        CSSPrimitiveValue::UnitType::kPixels);
  }

  document.UpdateStyleAndLayout();

  // FIXME (Radar 4118564): We should also autoscroll the window as necessary to
  // keep the point under the cursor in view.
}

LayoutRect PaintLayerScrollableArea::ScrollIntoView(
    const LayoutRect& absolute_rect,
    const WebScrollIntoViewParams& params) {
  LayoutRect local_expose_rect =
      AbsoluteToLocal(*GetLayoutBox(), absolute_rect);
  LayoutSize border_origin_to_scroll_origin =
      LayoutSize(-GetLayoutBox()->BorderLeft(), -GetLayoutBox()->BorderTop()) +
      LayoutSize(GetScrollOffset());
  // Represent the rect in the container's scroll-origin coordinate.
  local_expose_rect.Move(border_origin_to_scroll_origin);
  LayoutRect scroll_snapport_rect = VisibleScrollSnapportRect();

  ScrollOffset target_offset = ScrollAlignment::GetScrollOffsetToExpose(
      scroll_snapport_rect, local_expose_rect, params.GetScrollAlignmentX(),
      params.GetScrollAlignmentY(), GetScrollOffset());
  ScrollOffset new_scroll_offset(
      ClampScrollOffset(RoundedIntSize(target_offset)));

  ScrollOffset old_scroll_offset = GetScrollOffset();
  if (params.GetScrollType() == kUserScroll) {
    if (!UserInputScrollable(kHorizontalScrollbar))
      new_scroll_offset.SetWidth(old_scroll_offset.Width());
    if (!UserInputScrollable(kVerticalScrollbar))
      new_scroll_offset.SetHeight(old_scroll_offset.Height());
  }
  if (params.is_for_scroll_sequence) {
    DCHECK(params.GetScrollType() == kProgrammaticScroll ||
           params.GetScrollType() == kUserScroll);
    ScrollBehavior behavior =
        DetermineScrollBehavior(params.GetScrollBehavior(),
                                GetLayoutBox()->StyleRef().GetScrollBehavior());
    GetSmoothScrollSequencer()->QueueAnimation(this, new_scroll_offset,
                                               behavior);
  } else {
    SetScrollOffset(new_scroll_offset, params.GetScrollType(),
                    kScrollBehaviorInstant);
  }

  ScrollOffset scroll_offset_difference = new_scroll_offset - old_scroll_offset;
  // The container hasn't performed the scroll yet if it's for scroll sequence.
  // To calculate the result from the scroll, we move the |local_expose_rect| to
  // the will-be-scrolled location.
  local_expose_rect.Move(-LayoutSize(scroll_offset_difference));

  // Represent the rects in the container's border-box coordinate.
  local_expose_rect.Move(-border_origin_to_scroll_origin);
  scroll_snapport_rect.Move(-border_origin_to_scroll_origin);
  LayoutRect intersect = Intersection(scroll_snapport_rect, local_expose_rect);

  if (intersect.IsEmpty() && !scroll_snapport_rect.IsEmpty() &&
      !local_expose_rect.IsEmpty()) {
    return LocalToAbsolute(*GetLayoutBox(), local_expose_rect);
  }
  intersect = LocalToAbsolute(*GetLayoutBox(), intersect);
  return intersect;
}

void PaintLayerScrollableArea::UpdateScrollableAreaSet() {
  LocalFrame* frame = GetLayoutBox()->GetFrame();
  if (!frame)
    return;

  LocalFrameView* frame_view = frame->View();
  if (!frame_view)
    return;

  bool has_overflow =
      !GetLayoutBox()->Size().IsZero() &&
      ((HasHorizontalOverflow() && GetLayoutBox()->ScrollsOverflowX()) ||
       (HasVerticalOverflow() && GetLayoutBox()->ScrollsOverflowY()));

  bool is_visible_to_hit_test =
      GetLayoutBox()->StyleRef().VisibleToHitTesting();
  bool did_scroll_overflow = scrolls_overflow_;
  if (GetLayoutBox()->IsLayoutView()) {
    ScrollbarMode h_mode;
    ScrollbarMode v_mode;
    ToLayoutView(GetLayoutBox())->CalculateScrollbarModes(h_mode, v_mode);
    if (h_mode == kScrollbarAlwaysOff && v_mode == kScrollbarAlwaysOff)
      has_overflow = false;
  }
  scrolls_overflow_ = has_overflow && is_visible_to_hit_test;
  if (did_scroll_overflow == ScrollsOverflow())
    return;

  if (RuntimeEnabledFeatures::ImplicitRootScrollerEnabled() &&
      scrolls_overflow_) {
    if (GetLayoutBox()->IsLayoutView()) {
      if (Element* owner = GetLayoutBox()->GetDocument().LocalOwner()) {
        owner->GetDocument().GetRootScrollerController().ConsiderForImplicit(
            *owner);
      }
    } else {
      GetLayoutBox()
          ->GetDocument()
          .GetRootScrollerController()
          .ConsiderForImplicit(*GetLayoutBox()->GetNode());
    }
  }

  // The scroll and scroll offset properties depend on |scrollsOverflow| (see:
  // PaintPropertyTreeBuilder::updateScrollAndScrollTranslation).
  GetLayoutBox()->SetNeedsPaintPropertyUpdate();

  if (scrolls_overflow_) {
    DCHECK(CanHaveOverflowScrollbars(*GetLayoutBox()));
    frame_view->AddScrollableArea(this);
  } else {
    frame_view->RemoveScrollableArea(this);
  }

  layer_->DidUpdateScrollsOverflow();
}

void PaintLayerScrollableArea::UpdateCompositingLayersAfterScroll() {
  PaintLayerCompositor* compositor = GetLayoutBox()->View()->Compositor();
  if (!compositor->InCompositingMode())
    return;

  if (UsesCompositedScrolling()) {
    DCHECK(Layer()->HasCompositedLayerMapping());
    ScrollingCoordinator* scrolling_coordinator = GetScrollingCoordinator();
    bool handled_scroll =
        Layer()->IsRootLayer() && scrolling_coordinator &&
        scrolling_coordinator->UpdateCompositedScrollOffset(this);

    if (!handled_scroll) {
      Layer()->GetCompositedLayerMapping()->SetNeedsGraphicsLayerUpdate(
          kGraphicsLayerUpdateSubtree);
      compositor->SetNeedsCompositingUpdate(
          kCompositingUpdateAfterGeometryChange);
    }

    // Sticky constraints and paint property nodes need to be updated
    // to the new sticky locations.
    if (HasStickyDescendants())
      InvalidateAllStickyConstraints();

    // If we have fixed elements and we scroll the root layer we might
    // change compositing since the fixed elements might now overlap a
    // composited layer.
    if (Layer()->IsRootLayer()) {
      LocalFrame* frame = GetLayoutBox()->GetFrame();
      if (frame && frame->View() &&
          frame->View()->HasViewportConstrainedObjects()) {
        Layer()->SetNeedsCompositingInputsUpdate();
      }
    }
  } else {
    Layer()->SetNeedsCompositingInputsUpdate();
  }
}

ScrollingCoordinator* PaintLayerScrollableArea::GetScrollingCoordinator()
    const {
  LocalFrame* frame = GetLayoutBox()->GetFrame();
  if (!frame)
    return nullptr;

  Page* page = frame->GetPage();
  if (!page)
    return nullptr;

  return page->GetScrollingCoordinator();
}

bool PaintLayerScrollableArea::ShouldScrollOnMainThread() const {
  if (HasBeenDisposed())
    return true;
  if (LocalFrame* frame = GetLayoutBox()->GetFrame()) {
    if (frame->View()->GetMainThreadScrollingReasons())
      return true;
  }
  if (HasNonCompositedStickyDescendants())
    return true;
  return ScrollableArea::ShouldScrollOnMainThread();
}

static bool LayerNodeMayNeedCompositedScrolling(const PaintLayer* layer) {
  // Don't force composite scroll for select or text input elements.
  if (Node* node = layer->GetLayoutObject().GetNode()) {
    if (IsHTMLSelectElement(node))
      return false;
    if (TextControlElement* text_control = EnclosingTextControl(node)) {
      if (IsHTMLInputElement(text_control)) {
        return false;
      }
    }
  }
  return true;
}

bool PaintLayerScrollableArea::ComputeNeedsCompositedScrolling(
    const bool layer_has_been_composited,
    const PaintLayer* layer) {
  non_composited_main_thread_scrolling_reasons_ = 0;

  if (CompositingReasonFinder::RequiresCompositingForRootScroller(*layer))
    return true;

  // TODO(crbug.com/839341): Remove ScrollTimeline check once we support
  // main-thread AnimationWorklet and don't need to promote the scroll-source.
  Node* node = layer->GetLayoutObject().GetNode();
  if (!layer->ScrollsOverflow() &&
      !ScrollTimeline::HasActiveScrollTimeline(node)) {
    return false;
  }

  if (layer->Size().IsEmpty())
    return false;

  if (!layer_has_been_composited &&
      !LayerNodeMayNeedCompositedScrolling(layer)) {
    return false;
  }

  bool needs_composited_scrolling = true;

  // TODO(flackr): Allow integer transforms as long as all of the ancestor
  // transforms are also integer.
  bool background_supports_lcd_text =
      GetLayoutBox()->StyleRef().IsStackingContext() &&
      GetLayoutBox()->GetBackgroundPaintLocation(
          &non_composited_main_thread_scrolling_reasons_) &
          kBackgroundPaintInScrollingContents &&
      layer->BackgroundIsKnownToBeOpaqueInRect(
          ToLayoutBox(layer->GetLayoutObject()).PhysicalPaddingBoxRect()) &&
      !layer->CompositesWithTransform() && !layer->CompositesWithOpacity();

  // TODO(crbug.com/839341): Remove ScrollTimeline check once we support
  // main-thread AnimationWorklet and don't need to promote the scroll-source.
  if (!ScrollTimeline::HasActiveScrollTimeline(node) &&
      !layer_has_been_composited &&
      !layer->Compositor()->PreferCompositingToLCDTextEnabled() &&
      !background_supports_lcd_text) {
    if (layer->CompositesWithOpacity()) {
      non_composited_main_thread_scrolling_reasons_ |=
          MainThreadScrollingReason::kHasOpacityAndLCDText;
    }
    if (layer->CompositesWithTransform()) {
      non_composited_main_thread_scrolling_reasons_ |=
          MainThreadScrollingReason::kHasTransformAndLCDText;
    }
    if (!layer->BackgroundIsKnownToBeOpaqueInRect(
            ToLayoutBox(layer->GetLayoutObject()).PhysicalPaddingBoxRect())) {
      non_composited_main_thread_scrolling_reasons_ |=
          MainThreadScrollingReason::kBackgroundNotOpaqueInRectAndLCDText;
    }
    if (!layer->GetLayoutObject().StyleRef().IsStackingContext()) {
      non_composited_main_thread_scrolling_reasons_ |=
          MainThreadScrollingReason::kIsNotStackingContextAndLCDText;
    }

    needs_composited_scrolling = false;
  }

  if (layer->GetLayoutObject().HasClip() ||
      layer->HasDescendantWithClipPath() || !!layer->ClipPathAncestor()) {
    non_composited_main_thread_scrolling_reasons_ |=
        MainThreadScrollingReason::kHasClipRelatedProperty;
    needs_composited_scrolling = false;
  }

  DCHECK(!(non_composited_main_thread_scrolling_reasons_ &
           ~MainThreadScrollingReason::kNonCompositedReasons));
  return needs_composited_scrolling;
}

void PaintLayerScrollableArea::UpdateNeedsCompositedScrolling(
    bool layer_has_been_composited) {
  const bool needs_composited_scrolling =
      ComputeNeedsCompositedScrolling(layer_has_been_composited, Layer());

  if (static_cast<bool>(needs_composited_scrolling_) !=
      needs_composited_scrolling) {
    needs_composited_scrolling_ = needs_composited_scrolling;
  }
}

bool PaintLayerScrollableArea::VisualViewportSuppliesScrollbars() const {
  LocalFrame* frame = GetLayoutBox()->GetFrame();
  if (!frame || !frame->GetSettings())
    return false;

  // On desktop, we always use the layout viewport's scrollbars.
  if (!frame->GetSettings()->GetViewportEnabled())
    return false;

  const TopDocumentRootScrollerController& controller =
      GetLayoutBox()->GetDocument().GetPage()->GlobalRootScrollerController();
  return controller.RootScrollerArea() == this;
}

bool PaintLayerScrollableArea::ScheduleAnimation() {
  if (ChromeClient* client = GetChromeClient()) {
    client->ScheduleAnimation(GetLayoutBox()->GetFrame()->View());
    return true;
  }
  return false;
}

void PaintLayerScrollableArea::ResetRebuildScrollbarLayerFlags() {
  rebuild_horizontal_scrollbar_layer_ = false;
  rebuild_vertical_scrollbar_layer_ = false;
}

CompositorAnimationHost* PaintLayerScrollableArea::GetCompositorAnimationHost()
    const {
  return layer_->GetLayoutObject().GetFrameView()->GetCompositorAnimationHost();
}

CompositorAnimationTimeline*
PaintLayerScrollableArea::GetCompositorAnimationTimeline() const {
  return layer_->GetLayoutObject()
      .GetFrameView()
      ->GetCompositorAnimationTimeline();
}

void PaintLayerScrollableArea::GetTickmarks(Vector<IntRect>& tickmarks) const {
  if (layer_->IsRootLayer())
    tickmarks = ToLayoutView(GetLayoutBox())->GetTickmarks();
}

void PaintLayerScrollableArea::ScrollbarManager::SetHasHorizontalScrollbar(
    bool has_scrollbar) {
  if (has_scrollbar) {
    // This doesn't hit in any tests, but since the equivalent code in
    // setHasVerticalScrollbar does, presumably this code does as well.
    DisableCompositingQueryAsserts disabler;
    if (!h_bar_) {
      h_bar_ = CreateScrollbar(kHorizontalScrollbar);
      h_bar_is_attached_ = 1;
      if (!h_bar_->IsCustomScrollbar())
        ScrollableArea()->DidAddScrollbar(*h_bar_, kHorizontalScrollbar);
    } else {
      h_bar_is_attached_ = 1;
    }
  } else {
    h_bar_is_attached_ = 0;
    if (!DelayScrollOffsetClampScope::ClampingIsDelayed())
      DestroyScrollbar(kHorizontalScrollbar);
  }
}

void PaintLayerScrollableArea::ScrollbarManager::SetHasVerticalScrollbar(
    bool has_scrollbar) {
  if (has_scrollbar) {
    DisableCompositingQueryAsserts disabler;
    if (!v_bar_) {
      v_bar_ = CreateScrollbar(kVerticalScrollbar);
      v_bar_is_attached_ = 1;
      if (!v_bar_->IsCustomScrollbar())
        ScrollableArea()->DidAddScrollbar(*v_bar_, kVerticalScrollbar);
    } else {
      v_bar_is_attached_ = 1;
    }
  } else {
    v_bar_is_attached_ = 0;
    if (!DelayScrollOffsetClampScope::ClampingIsDelayed())
      DestroyScrollbar(kVerticalScrollbar);
  }
}

Scrollbar* PaintLayerScrollableArea::ScrollbarManager::CreateScrollbar(
    ScrollbarOrientation orientation) {
  DCHECK(orientation == kHorizontalScrollbar ? !h_bar_is_attached_
                                             : !v_bar_is_attached_);
  Scrollbar* scrollbar = nullptr;
  const LayoutObject& style_source =
      ScrollbarStyleSource(*ScrollableArea()->GetLayoutBox());
  bool has_custom_scrollbar_style =
      style_source.StyleRef().HasPseudoStyle(kPseudoIdScrollbar);
  if (has_custom_scrollbar_style) {
    DCHECK(style_source.GetNode() && style_source.GetNode()->IsElementNode());
    scrollbar = LayoutScrollbar::CreateCustomScrollbar(
        ScrollableArea(), orientation, ToElement(style_source.GetNode()));
  } else {
    ScrollbarControlSize scrollbar_size = kRegularScrollbar;
    if (style_source.StyleRef().HasAppearance()) {
      scrollbar_size = LayoutTheme::GetTheme().ScrollbarControlSizeForPart(
          style_source.StyleRef().Appearance());
    }
    scrollbar = Scrollbar::Create(ScrollableArea(), orientation, scrollbar_size,
                                  &ScrollableArea()
                                       ->GetLayoutBox()
                                       ->GetFrame()
                                       ->GetPage()
                                       ->GetChromeClient());
  }
  ScrollableArea()->GetLayoutBox()->GetDocument().View()->AddScrollbar(
      scrollbar);
  return scrollbar;
}

void PaintLayerScrollableArea::ScrollbarManager::DestroyScrollbar(
    ScrollbarOrientation orientation) {
  Member<Scrollbar>& scrollbar =
      orientation == kHorizontalScrollbar ? h_bar_ : v_bar_;
  DCHECK(orientation == kHorizontalScrollbar ? !h_bar_is_attached_
                                             : !v_bar_is_attached_);
  if (!scrollbar)
    return;

  ScrollableArea()->SetScrollbarNeedsPaintInvalidation(orientation);
  if (orientation == kHorizontalScrollbar)
    ScrollableArea()->rebuild_horizontal_scrollbar_layer_ = true;
  else
    ScrollableArea()->rebuild_vertical_scrollbar_layer_ = true;

  if (!scrollbar->IsCustomScrollbar())
    ScrollableArea()->WillRemoveScrollbar(*scrollbar, orientation);

  ScrollableArea()->GetLayoutBox()->GetDocument().View()->RemoveScrollbar(
      scrollbar);
  scrollbar->DisconnectFromScrollableArea();
  scrollbar = nullptr;
}

void PaintLayerScrollableArea::ScrollbarManager::DestroyDetachedScrollbars() {
  DCHECK(!h_bar_is_attached_ || h_bar_);
  DCHECK(!v_bar_is_attached_ || v_bar_);
  if (h_bar_ && !h_bar_is_attached_)
    DestroyScrollbar(kHorizontalScrollbar);
  if (v_bar_ && !v_bar_is_attached_)
    DestroyScrollbar(kVerticalScrollbar);
}

void PaintLayerScrollableArea::ScrollbarManager::Dispose() {
  h_bar_is_attached_ = v_bar_is_attached_ = 0;
  DestroyScrollbar(kHorizontalScrollbar);
  DestroyScrollbar(kVerticalScrollbar);
}

void PaintLayerScrollableArea::ScrollbarManager::Trace(
    blink::Visitor* visitor) {
  visitor->Trace(scrollable_area_);
  visitor->Trace(h_bar_);
  visitor->Trace(v_bar_);
}

uint64_t PaintLayerScrollableArea::Id() const {
  return DOMNodeIds::IdForNode(GetLayoutBox()->GetNode());
}

int PaintLayerScrollableArea::PreventRelayoutScope::count_ = 0;
SubtreeLayoutScope*
    PaintLayerScrollableArea::PreventRelayoutScope::layout_scope_ = nullptr;
bool PaintLayerScrollableArea::PreventRelayoutScope::relayout_needed_ = false;

PaintLayerScrollableArea::PreventRelayoutScope::PreventRelayoutScope(
    SubtreeLayoutScope& layout_scope) {
  if (!count_) {
    DCHECK(!layout_scope_);
    DCHECK(NeedsRelayoutList().IsEmpty());
    layout_scope_ = &layout_scope;
  }
  count_++;
}

PaintLayerScrollableArea::PreventRelayoutScope::~PreventRelayoutScope() {
  if (--count_ == 0) {
    if (relayout_needed_) {
      for (auto scrollable_area : NeedsRelayoutList()) {
        DCHECK(scrollable_area->NeedsRelayout());
        LayoutBox* box = scrollable_area->GetLayoutBox();
        layout_scope_->SetNeedsLayout(
            box, LayoutInvalidationReason::kScrollbarChanged);
        if (box->IsLayoutBlock()) {
          bool horizontal_scrollbar_changed =
              scrollable_area->HasHorizontalScrollbar() !=
              scrollable_area->HadHorizontalScrollbarBeforeRelayout();
          bool vertical_scrollbar_changed =
              scrollable_area->HasVerticalScrollbar() !=
              scrollable_area->HadVerticalScrollbarBeforeRelayout();
          if (horizontal_scrollbar_changed || vertical_scrollbar_changed) {
            ToLayoutBlock(box)->ScrollbarsChanged(horizontal_scrollbar_changed,
                                                  vertical_scrollbar_changed);
          }
        }
        scrollable_area->SetNeedsRelayout(false);
      }

      NeedsRelayoutList().clear();
    }
    layout_scope_ = nullptr;
  }
}

void PaintLayerScrollableArea::PreventRelayoutScope::SetBoxNeedsLayout(
    PaintLayerScrollableArea& scrollable_area,
    bool had_horizontal_scrollbar,
    bool had_vertical_scrollbar) {
  DCHECK(count_);
  DCHECK(layout_scope_);
  if (scrollable_area.NeedsRelayout())
    return;
  scrollable_area.SetNeedsRelayout(true);
  scrollable_area.SetHadHorizontalScrollbarBeforeRelayout(
      had_horizontal_scrollbar);
  scrollable_area.SetHadVerticalScrollbarBeforeRelayout(had_vertical_scrollbar);

  relayout_needed_ = true;
  NeedsRelayoutList().push_back(&scrollable_area);
}

void PaintLayerScrollableArea::PreventRelayoutScope::ResetRelayoutNeeded() {
  DCHECK_EQ(count_, 0);
  DCHECK(NeedsRelayoutList().IsEmpty());
  relayout_needed_ = false;
}

HeapVector<Member<PaintLayerScrollableArea>>&
PaintLayerScrollableArea::PreventRelayoutScope::NeedsRelayoutList() {
  DEFINE_STATIC_LOCAL(Persistent<HeapVector<Member<PaintLayerScrollableArea>>>,
                      needs_relayout_list,
                      (new HeapVector<Member<PaintLayerScrollableArea>>));
  return *needs_relayout_list;
}

int PaintLayerScrollableArea::FreezeScrollbarsScope::count_ = 0;

int PaintLayerScrollableArea::DelayScrollOffsetClampScope::count_ = 0;

PaintLayerScrollableArea::DelayScrollOffsetClampScope::
    DelayScrollOffsetClampScope() {
  DCHECK(count_ > 0 || NeedsClampList().IsEmpty());
  count_++;
}

PaintLayerScrollableArea::DelayScrollOffsetClampScope::
    ~DelayScrollOffsetClampScope() {
  if (--count_ == 0)
    DelayScrollOffsetClampScope::ClampScrollableAreas();
}

void PaintLayerScrollableArea::DelayScrollOffsetClampScope::SetNeedsClamp(
    PaintLayerScrollableArea* scrollable_area) {
  if (!scrollable_area->NeedsScrollOffsetClamp()) {
    scrollable_area->SetNeedsScrollOffsetClamp(true);
    NeedsClampList().push_back(scrollable_area);
  }
}

void PaintLayerScrollableArea::DelayScrollOffsetClampScope::
    ClampScrollableAreas() {
  for (auto& scrollable_area : NeedsClampList())
    scrollable_area->ClampScrollOffsetAfterOverflowChange();
  NeedsClampList().clear();
}

HeapVector<Member<PaintLayerScrollableArea>>&
PaintLayerScrollableArea::DelayScrollOffsetClampScope::NeedsClampList() {
  DEFINE_STATIC_LOCAL(Persistent<HeapVector<Member<PaintLayerScrollableArea>>>,
                      needs_clamp_list,
                      (new HeapVector<Member<PaintLayerScrollableArea>>));
  return *needs_clamp_list;
}

ScrollbarTheme& PaintLayerScrollableArea::GetPageScrollbarTheme() const {
  // If PaintLayer is destructed before PaintLayerScrollable area, we can not
  // get the page scrollbar theme setting.
  DCHECK(!HasBeenDisposed());

  Page* page = GetLayoutBox()->GetFrame()->GetPage();
  DCHECK(page);

  return page->GetScrollbarTheme();
}

void PaintLayerScrollableArea::WillRemoveScrollbar(
    Scrollbar& scrollbar,
    ScrollbarOrientation orientation) {
  if (!scrollbar.IsCustomScrollbar() &&
      !(orientation == kHorizontalScrollbar ? LayerForHorizontalScrollbar()
                                            : LayerForVerticalScrollbar())) {
    ObjectPaintInvalidator(*GetLayoutBox())
        .SlowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(
            scrollbar, PaintInvalidationReason::kScrollControl);
  }

  ScrollableArea::WillRemoveScrollbar(scrollbar, orientation);
}

static LayoutRect ScrollControlVisualRect(
    const IntRect& scroll_control_rect,
    const LayoutBox& box,
    const PaintInvalidatorContext& context,
    const LayoutRect& previous_visual_rect) {
  LayoutRect visual_rect(scroll_control_rect);
#if DCHECK_IS_ON()
  FindVisualRectNeedingUpdateScope finder(box, context, previous_visual_rect,
                                          visual_rect);
#endif
  if (!context.NeedsVisualRectUpdate(box))
    return previous_visual_rect;

  // No need to apply any paint offset. Scroll controls paint in a different
  // transform space than their contained box (the scrollbarPaintOffset
  // transform node).
  return visual_rect;
}

// Returns true if the scroll control is invalidated.
static bool InvalidatePaintOfScrollControlIfNeeded(
    const LayoutRect& new_visual_rect,
    const LayoutRect& previous_visual_rect,
    bool needs_paint_invalidation,
    LayoutBox& box,
    const LayoutBoxModelObject& paint_invalidation_container) {
  bool should_invalidate_new_rect = needs_paint_invalidation;
  if (new_visual_rect != previous_visual_rect) {
    should_invalidate_new_rect = true;
  } else if (previous_visual_rect.IsEmpty()) {
    DCHECK(new_visual_rect.IsEmpty());
    // Do not issue an empty invalidation.
    should_invalidate_new_rect = false;
  }

  return should_invalidate_new_rect;
}

static LayoutRect InvalidatePaintOfScrollbarIfNeeded(
    Scrollbar* scrollbar,
    GraphicsLayer* graphics_layer,
    bool& previously_was_overlay,
    const LayoutRect& previous_visual_rect,
    bool needs_paint_invalidation_arg,
    LayoutBox& box,
    const PaintInvalidatorContext& context) {
  bool is_overlay = scrollbar && scrollbar->IsOverlayScrollbar();

  LayoutRect new_visual_rect;
  // Calculate visual rect of the scrollbar, except overlay composited
  // scrollbars because we invalidate the graphics layer only.
  if (scrollbar && !(graphics_layer && is_overlay)) {
    new_visual_rect = ScrollControlVisualRect(scrollbar->FrameRect(), box,
                                              context, previous_visual_rect);
  }

  bool needs_paint_invalidation = needs_paint_invalidation_arg;
  if (needs_paint_invalidation && graphics_layer) {
    // If the scrollbar needs paint invalidation but didn't change location/size
    // or the scrollbar is an overlay scrollbar (visual rect is empty),
    // invalidating the graphics layer is enough (which has been done in
    // ScrollableArea::setScrollbarNeedsPaintInvalidation()).
    // Otherwise invalidatePaintOfScrollControlIfNeeded() below will invalidate
    // the old and new location of the scrollbar on the box's paint invalidation
    // container to ensure newly expanded/shrunk areas of the box to be
    // invalidated.
    needs_paint_invalidation = false;
    DCHECK(!graphics_layer->DrawsContent() ||
           graphics_layer->GetPaintController().GetPaintArtifact().IsEmpty());
  }

  // Invalidate the box's display item client if the box's padding box size is
  // affected by change of the non-overlay scrollbar width. We detect change of
  // visual rect size instead of change of scrollbar width change, which may
  // have some false-positives (e.g. the scrollbar changed length but not width)
  // but won't invalidate more than expected because in the false-positive case
  // the box must have changed size and have been invalidated.
  const LayoutBoxModelObject& paint_invalidation_container =
      *context.paint_invalidation_container;
  LayoutSize new_scrollbar_used_space_in_box;
  if (!is_overlay)
    new_scrollbar_used_space_in_box = new_visual_rect.Size();
  LayoutSize previous_scrollbar_used_space_in_box;
  if (!previously_was_overlay)
    previous_scrollbar_used_space_in_box = previous_visual_rect.Size();

  // The IsEmpty() check avoids invalidaiton in cases when the visual rect
  // changes from (0,0 0x0) to (0,0 0x100).
  if (!(new_scrollbar_used_space_in_box.IsEmpty() &&
        previous_scrollbar_used_space_in_box.IsEmpty()) &&
      new_scrollbar_used_space_in_box != previous_scrollbar_used_space_in_box) {
    context.painting_layer->SetNeedsRepaint();
    ObjectPaintInvalidator(box).InvalidateDisplayItemClient(
        box, PaintInvalidationReason::kGeometry);
  }

  bool invalidated = InvalidatePaintOfScrollControlIfNeeded(
      new_visual_rect, previous_visual_rect, needs_paint_invalidation, box,
      paint_invalidation_container);

  previously_was_overlay = is_overlay;

  if (!invalidated || !scrollbar || graphics_layer)
    return new_visual_rect;

  context.painting_layer->SetNeedsRepaint();
  ObjectPaintInvalidator(box).InvalidateDisplayItemClient(
      *scrollbar, PaintInvalidationReason::kScrollControl);
  if (scrollbar->IsCustomScrollbar()) {
    ToLayoutScrollbar(scrollbar)
        ->InvalidateDisplayItemClientsOfScrollbarParts();
  }

  return new_visual_rect;
}

void PaintLayerScrollableArea::InvalidatePaintOfScrollControlsIfNeeded(
    const PaintInvalidatorContext& context) {
  LayoutBox& box = *GetLayoutBox();
  SetHorizontalScrollbarVisualRect(InvalidatePaintOfScrollbarIfNeeded(
      HorizontalScrollbar(), LayerForHorizontalScrollbar(),
      horizontal_scrollbar_previously_was_overlay_,
      horizontal_scrollbar_visual_rect_,
      HorizontalScrollbarNeedsPaintInvalidation(), box, context));
  SetVerticalScrollbarVisualRect(InvalidatePaintOfScrollbarIfNeeded(
      VerticalScrollbar(), LayerForVerticalScrollbar(),
      vertical_scrollbar_previously_was_overlay_,
      vertical_scrollbar_visual_rect_,
      VerticalScrollbarNeedsPaintInvalidation(), box, context));

  LayoutRect scroll_corner_and_resizer_visual_rect =
      ScrollControlVisualRect(ScrollCornerAndResizerRect(), box, context,
                              scroll_corner_and_resizer_visual_rect_);
  const LayoutBoxModelObject& paint_invalidation_container =
      *context.paint_invalidation_container;
  if (InvalidatePaintOfScrollControlIfNeeded(
          scroll_corner_and_resizer_visual_rect,
          scroll_corner_and_resizer_visual_rect_,
          ScrollCornerNeedsPaintInvalidation(), box,
          paint_invalidation_container)) {
    SetScrollCornerAndResizerVisualRect(scroll_corner_and_resizer_visual_rect);
    if (LayoutScrollbarPart* scroll_corner = ScrollCorner()) {
      ObjectPaintInvalidator(*scroll_corner)
          .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
              PaintInvalidationReason::kScrollControl);
    }
    if (LayoutScrollbarPart* resizer = Resizer()) {
      ObjectPaintInvalidator(*resizer)
          .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
              PaintInvalidationReason::kScrollControl);
    }
  }

  ClearNeedsPaintInvalidationForScrollControls();
}

void PaintLayerScrollableArea::ClearPreviousVisualRects() {
  SetHorizontalScrollbarVisualRect(LayoutRect());
  SetVerticalScrollbarVisualRect(LayoutRect());
  SetScrollCornerAndResizerVisualRect(LayoutRect());
}

void PaintLayerScrollableArea::SetHorizontalScrollbarVisualRect(
    const LayoutRect& rect) {
  horizontal_scrollbar_visual_rect_ = rect;
  if (Scrollbar* scrollbar = HorizontalScrollbar())
    scrollbar->SetVisualRect(rect);
}

void PaintLayerScrollableArea::SetVerticalScrollbarVisualRect(
    const LayoutRect& rect) {
  vertical_scrollbar_visual_rect_ = rect;
  if (Scrollbar* scrollbar = VerticalScrollbar())
    scrollbar->SetVisualRect(rect);
}

void PaintLayerScrollableArea::SetScrollCornerAndResizerVisualRect(
    const LayoutRect& rect) {
  scroll_corner_and_resizer_visual_rect_ = rect;
  if (LayoutScrollbarPart* scroll_corner = ScrollCorner())
    scroll_corner->GetMutableForPainting().FirstFragment().SetVisualRect(rect);
  if (LayoutScrollbarPart* resizer = Resizer())
    resizer->GetMutableForPainting().FirstFragment().SetVisualRect(rect);
}

void PaintLayerScrollableArea::ScrollControlWasSetNeedsPaintInvalidation() {
  GetLayoutBox()->SetShouldCheckForPaintInvalidation();
}

void PaintLayerScrollableArea::DidScrollWithScrollbar(
    ScrollbarPart part,
    ScrollbarOrientation orientation) {
  WebFeature scrollbar_use_uma;
  switch (part) {
    case kBackButtonStartPart:
    case kForwardButtonStartPart:
    case kBackButtonEndPart:
    case kForwardButtonEndPart:
      scrollbar_use_uma =
          (orientation == kVerticalScrollbar
               ? WebFeature::kScrollbarUseVerticalScrollbarButton
               : WebFeature::kScrollbarUseHorizontalScrollbarButton);
      break;
    case kThumbPart:
      scrollbar_use_uma =
          (orientation == kVerticalScrollbar
               ? WebFeature::kScrollbarUseVerticalScrollbarThumb
               : WebFeature::kScrollbarUseHorizontalScrollbarThumb);
      break;
    case kBackTrackPart:
    case kForwardTrackPart:
      scrollbar_use_uma =
          (orientation == kVerticalScrollbar
               ? WebFeature::kScrollbarUseVerticalScrollbarTrack
               : WebFeature::kScrollbarUseHorizontalScrollbarTrack);
      break;
    default:
      return;
  }

  UseCounter::Count(GetLayoutBox()->GetDocument(), scrollbar_use_uma);
}

CompositorElementId PaintLayerScrollableArea::GetCompositorElementId() const {
  return CompositorElementIdFromUniqueObjectId(
      GetLayoutBox()->UniqueId(), CompositorElementIdNamespace::kScroll);
}

LayoutRect
PaintLayerScrollableArea::ScrollingBackgroundDisplayItemClient::VisualRect()
    const {
  const auto* box = scrollable_area_->GetLayoutBox();
  auto overflow_clip_rect = box->OverflowClipRect(LayoutPoint());
  auto scroll_size = scrollable_area_->overflow_rect_.Size();
  // Ensure scrolling contents are at least as large as the scroll clip
  scroll_size = scroll_size.ExpandedTo(overflow_clip_rect.Size());
  LayoutRect result(overflow_clip_rect.Location(), scroll_size);
  result.MoveBy(box->FirstFragment().PaintOffset());
  result = LayoutRect(PixelSnappedIntRect(result));
#if DCHECK_IS_ON()
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    DCHECK_EQ(result,
              scrollable_area_->layer_->GraphicsLayerBacking()->VisualRect());
  }
#endif
  return result;
}

String
PaintLayerScrollableArea::ScrollingBackgroundDisplayItemClient::DebugName()
    const {
  return "Scrolling background of " +
         scrollable_area_->GetLayoutBox()->DebugName();
}

bool PaintLayerScrollableArea::ScrollingBackgroundDisplayItemClient::
    PaintedOutputOfObjectHasNoEffectRegardlessOfSize() const {
  return scrollable_area_->GetLayoutBox()
      ->PaintedOutputOfObjectHasNoEffectRegardlessOfSize();
}

}  // namespace blink
