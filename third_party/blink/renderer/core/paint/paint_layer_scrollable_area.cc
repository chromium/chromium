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
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/snap_selection_strategy.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"
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
#include "third_party/blink/renderer/core/layout/custom_scrollbar.h"
#include "third_party/blink/renderer/core/layout/layout_custom_scrollbar_part.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_fragment.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

namespace {

// Default value is set to 15 as the default
// minimum size used by firefox is 15x15.
static const int kDefaultMinimumWidthForResizing = 15;
static const int kDefaultMinimumHeightForResizing = 15;

}  // namespace

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
      had_resizer_before_relayout_(false),
      scroll_origin_changed_(false),
      scrollbar_manager_(*this),
      scroll_corner_(nullptr),
      resizer_(nullptr),
      scroll_anchor_(this),
      non_composited_main_thread_scrolling_reasons_(0),
      horizontal_scrollbar_previously_was_overlay_(false),
      vertical_scrollbar_previously_was_overlay_(false),
      scrolling_background_display_item_client_(*this) {
  if (auto* element = DynamicTo<Element>(GetLayoutBox()->GetNode())) {
    // We save and restore only the scrollOffset as the other scroll values are
    // recalculated.
    scroll_offset_ = element->SavedLayerScrollOffset();
    if (!scroll_offset_.IsZero())
      GetScrollAnimator().SetCurrentOffset(scroll_offset_);
    element->SetSavedLayerScrollOffset(ScrollOffset());
  }

  GetLayoutBox()->GetDocument().GetSnapCoordinator().AddSnapContainer(
      *GetLayoutBox());
}

PaintLayerScrollableArea::~PaintLayerScrollableArea() {
  CHECK(HasBeenDisposed());
}

void PaintLayerScrollableArea::DidScroll(const FloatPoint& position) {
  ScrollableArea::DidScroll(position);
  // This should be alive if it receives composited scroll callbacks.
  CHECK(!HasBeenDisposed());
}

void PaintLayerScrollableArea::DisposeImpl() {
  rare_data_.reset();

  GetLayoutBox()->GetDocument().GetSnapCoordinator().RemoveSnapContainer(
      *GetLayoutBox());

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
    // FIXME: Make setSavedLayerScrollOffset take DoubleSize. crbug.com/414283.
    if (auto* element = DynamicTo<Element>(GetLayoutBox()->GetNode()))
      element->SetSavedLayerScrollOffset(scroll_offset_);
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

  RunScrollCompleteCallbacks();

  layer_ = nullptr;
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

cc::Layer* PaintLayerScrollableArea::LayerForScrolling() const {
  if (auto* graphics_layer = GraphicsLayerForScrolling())
    return graphics_layer->CcLayer();
  return nullptr;
}

cc::Layer* PaintLayerScrollableArea::LayerForHorizontalScrollbar() const {
  if (auto* graphics_layer = GraphicsLayerForHorizontalScrollbar())
    return graphics_layer->ContentsLayer();
  return nullptr;
}

cc::Layer* PaintLayerScrollableArea::LayerForVerticalScrollbar() const {
  if (auto* graphics_layer = GraphicsLayerForVerticalScrollbar())
    return graphics_layer->ContentsLayer();
  return nullptr;
}

cc::Layer* PaintLayerScrollableArea::LayerForScrollCorner() const {
  if (auto* graphics_layer = GraphicsLayerForScrollCorner())
    return graphics_layer->CcLayer();
  return nullptr;
}

GraphicsLayer* PaintLayerScrollableArea::GraphicsLayerForScrolling() const {
  return Layer()->HasCompositedLayerMapping()
             ? Layer()->GetCompositedLayerMapping()->ScrollingContentsLayer()
             : nullptr;
}

GraphicsLayer* PaintLayerScrollableArea::GraphicsLayerForHorizontalScrollbar()
    const {
  // See crbug.com/343132.
  DisableCompositingQueryAsserts disabler;

  return Layer()->HasCompositedLayerMapping()
             ? Layer()
                   ->GetCompositedLayerMapping()
                   ->LayerForHorizontalScrollbar()
             : nullptr;
}

GraphicsLayer* PaintLayerScrollableArea::GraphicsLayerForVerticalScrollbar()
    const {
  // See crbug.com/343132.
  DisableCompositingQueryAsserts disabler;

  return Layer()->HasCompositedLayerMapping()
             ? Layer()->GetCompositedLayerMapping()->LayerForVerticalScrollbar()
             : nullptr;
}

GraphicsLayer* PaintLayerScrollableArea::GraphicsLayerForScrollCorner() const {
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
    horizontal_thickness =
        GetLayoutBox()
            ->GetDocument()
            .GetPage()
            ->GetChromeClient()
            .WindowToViewportScalar(
                GetLayoutBox()->GetFrame(),
                GetPageScrollbarTheme().ScrollbarThickness());
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
  bool has_resizer = GetLayoutBox()->StyleRef().HasResize();
  if ((has_horizontal_bar && has_vertical_bar) ||
      (has_resizer && (has_horizontal_bar || has_vertical_bar))) {
    return CornerRect(GetLayoutBox()->PixelSnappedBorderBoxRect(
        Layer()->SubpixelAccumulation()));
  }
  return IntRect();
}

void PaintLayerScrollableArea::SetScrollbarNeedsPaintInvalidation(
    ScrollbarOrientation orientation) {
  if (auto* graphics_layer = orientation == kHorizontalScrollbar
                                 ? GraphicsLayerForHorizontalScrollbar()
                                 : GraphicsLayerForVerticalScrollbar()) {
    graphics_layer->SetNeedsDisplay();
    graphics_layer->SetContentsNeedsDisplay();
  }
  ScrollableArea::SetScrollbarNeedsPaintInvalidation(orientation);
}

void PaintLayerScrollableArea::SetScrollCornerNeedsPaintInvalidation() {
  if (GraphicsLayer* graphics_layer = GraphicsLayerForScrollCorner()) {
    graphics_layer->SetNeedsDisplay();
    return;
  }
  ScrollableArea::SetScrollCornerNeedsPaintInvalidation();
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

  TRACE_EVENT2("blink", "PaintLayerScrollableArea::UpdateScrollOffset", "x",
               new_offset.Width(), "y", new_offset.Height());
  TRACE_EVENT_INSTANT1("blink", "Type", TRACE_EVENT_SCOPE_THREAD, "type",
                       scroll_type);

  scroll_offset_ = new_offset;

  LocalFrame* frame = GetLayoutBox()->GetFrame();
  DCHECK(frame);

  LocalFrameView* frame_view = GetLayoutBox()->GetFrameView();
  bool is_root_layer = Layer()->IsRootLayer();

  TRACE_EVENT1("devtools.timeline", "ScrollLayer", "data",
               inspector_scroll_layer_event::Data(GetLayoutBox()));

  // FIXME(420741): Resolve circular dependency between scroll offset and
  // compositing state, and remove this disabler.
  DisableCompositingQueryAsserts disabler;

  // Update the positions of our child layers (if needed as only fixed layers
  // should be impacted by a scroll).
  if (!frame_view->IsInPerformLayout()) {
    if (!Layer()->IsRootLayer()) {
      Layer()->SetNeedsCompositingInputsUpdate();
      Layer()->ClearClipRects();
    }

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

  InvalidatePaintForScrollOffsetChange();

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

  if (FragmentAnchor* anchor = frame_view->GetFragmentAnchor())
    anchor->DidScroll(scroll_type);

  if (IsExplicitScrollType(scroll_type)) {
    if (scroll_type != kCompositorScroll)
      ShowOverlayScrollbars();
    GetScrollAnchor()->Clear();
  }
  if (ContentCaptureManager* manager =
          frame_view->GetFrame().LocalFrameRoot().GetContentCaptureManager()) {
    manager->OnScrollPositionChanged();
  }
  if (AXObjectCache* cache =
          GetLayoutBox()->GetDocument().ExistingAXObjectCache())
    cache->HandleScrollPositionChanged(GetLayoutBox());
}

void PaintLayerScrollableArea::InvalidatePaintForScrollOffsetChange() {
  InvalidatePaintForStickyDescendants();

  auto* box = GetLayoutBox();
  auto* frame_view = box->GetFrameView();
  frame_view->InvalidateBackgroundAttachmentFixedDescendantsOnScroll(*box);

  if (box->IsLayoutView() && frame_view->HasViewportConstrainedObjects() &&
      !frame_view->InvalidateViewportConstrainedObjects()) {
    box->SetShouldDoFullPaintInvalidation();
    box->SetSubtreeShouldCheckForPaintInvalidation();
  }

  // TODO(chrishtr): remove this slow path once crbug.com/906885 is fixed.
  // See also https://bugs.chromium.org/p/chromium/issues/detail?id=903287#c10.
  if (Layer()->EnclosingPaginationLayer())
    box->SetSubtreeShouldCheckForPaintInvalidation();

  // If not composited, background always paints into the main graphics layer.
  bool background_paint_in_graphics_layer = true;
  bool background_paint_in_scrolling_contents = false;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
      UsesCompositedScrolling()) {
    auto background_paint_location = box->GetBackgroundPaintLocation();
    background_paint_in_graphics_layer =
        background_paint_location & kBackgroundPaintInGraphicsLayer;
    background_paint_in_scrolling_contents =
        background_paint_location & kBackgroundPaintInScrollingContents;
  }

  // Both local attachment background painted in graphics layer and normal
  // attachment background painted in scrolling contents require paint
  // invalidation. Fixed attachment background has been dealt with in
  // frame_view->InvalidateBackgroundAttachmentFixedDescendantsOnScroll().
  auto background_layers = box->StyleRef().BackgroundLayers();
  if ((background_layers.AnyLayerHasLocalAttachmentImage() &&
       background_paint_in_graphics_layer) ||
      (background_layers.AnyLayerHasDefaultAttachmentImage() &&
       background_paint_in_scrolling_contents))
    box->SetBackgroundNeedsFullPaintInvalidation();

  // If any scrolling content might have been clipped by a cull rect, then
  // that cull rect could be affected by scroll offset. For composited
  // scrollers, this will be taken care of by the interest rect computation
  // in CompositedLayerMapping.
  // TODO(wangxianzhu): replace this shortcut with interest rects.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
      !UsesCompositedScrolling())
    Layer()->SetNeedsRepaint();
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
  if (!GetLayoutBox() || !GetLayoutBox()->HasOverflowClip())
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

PhysicalRect PaintLayerScrollableArea::LayoutContentRect(
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

  PhysicalSize size(
      layer_size.Width() - border_width - vertical_scrollbar_width,
      layer_size.Height() - border_height - horizontal_scrollbar_height);
  size.ClampNegativeToZero();
  return PhysicalRect(PhysicalOffset::FromFloatPointRound(ScrollPosition()),
                      size);
}

IntRect PaintLayerScrollableArea::VisibleContentRect(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  PhysicalRect layout_content_rect(LayoutContentRect(scrollbar_inclusion));
  // TODO(szager): It's not clear that Floor() is the right thing to do here;
  // what is the correct behavior for fractional scroll offsets?
  return IntRect(FlooredIntPoint(layout_content_rect.offset),
                 PixelSnappedIntSize(layout_content_rect.size.ToLayoutSize(),
                                     GetLayoutBox()->Location()));
}

PhysicalRect PaintLayerScrollableArea::VisibleScrollSnapportRect(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  const ComputedStyle* style = GetLayoutBox()->Style();
  PhysicalRect layout_content_rect(LayoutContentRect(scrollbar_inclusion));
  layout_content_rect.Move(PhysicalOffset(-ScrollOrigin()));
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
  PhysicalOffset offset(
      GetLayoutBox()->ClientLeft() + GetLayoutBox()->Location().X(),
      GetLayoutBox()->ClientTop() + GetLayoutBox()->Location().Y());
  // TODO(crbug.com/962299): The pixel snapping is incorrect in some cases.
  return PixelSnappedContentsSize(offset);
}

IntSize PaintLayerScrollableArea::PixelSnappedContentsSize(
    const PhysicalOffset& paint_offset) const {
  return PixelSnappedIntRect(PhysicalRect(paint_offset, overflow_rect_.size))
      .Size();
}

void PaintLayerScrollableArea::ContentsResized() {
  ScrollableArea::ContentsResized();
  // Need to update the bounds of the scroll property.
  GetLayoutBox()->SetNeedsPaintPropertyUpdate();
  Layer()->SetNeedsCompositingInputsUpdate();
}

IntPoint PaintLayerScrollableArea::LastKnownMousePosition() const {
  return GetLayoutBox()->GetFrame()
             ? FlooredIntPoint(GetLayoutBox()
                                   ->GetFrame()
                                   ->GetEventHandler()
                                   .LastKnownMousePositionInRootFrame())
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
    return mode == ScrollbarMode::kAuto || mode == ScrollbarMode::kAlwaysOn;
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
  return layer_ ? layer_->GetLayoutBox() : nullptr;
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
  if (overflow_rect_.IsEmpty())
    return;
  PhysicalRect scrollable_overflow = overflow_rect_;
  scrollable_overflow.Move(-PhysicalOffset(GetLayoutBox()->BorderLeft(),
                                           GetLayoutBox()->BorderTop()));
  IntPoint new_origin(FlooredIntPoint(-scrollable_overflow.offset) +
                      GetLayoutBox()->OriginAdjustmentForScrollbars());
  if (new_origin != scroll_origin_)
    scroll_origin_changed_ = true;
  scroll_origin_ = new_origin;
}

void PaintLayerScrollableArea::UpdateScrollDimensions() {
  PhysicalRect new_overflow_rect = GetLayoutBox()->PhysicalLayoutOverflowRect();

  // The layout viewport can be larger than the document's layout overflow when
  // top controls are hidden.  Expand the overflow here to ensure that our
  // contents size >= visible size.
  new_overflow_rect.Unite(PhysicalRect(
      new_overflow_rect.offset, LayoutContentRect(kExcludeScrollbars).size));

  if (overflow_rect_.size != new_overflow_rect.size)
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

  bool has_resizer = GetLayoutBox()->CanResize();
  bool resizer_will_change = had_resizer_before_relayout_ != has_resizer;
  had_resizer_before_relayout_ = has_resizer;

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
      if (IsManagedByLayoutNG(*GetLayoutBox())) {
        // If the box is managed by LayoutNG, don't go here. We don't want to
        // re-enter the NG layout algorithm for this box from here. Just update
        // the rectangles, in case scrollbars were added or removed. LayoutNG
        // has its own scrollbar change detection mechanism.
        UpdateScrollDimensions();
      } else {
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
              GetLayoutBox(), layout_invalidation_reason::kScrollbarChanged);
          if (auto* block = DynamicTo<LayoutBlock>(GetLayoutBox())) {
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
  } else if (!HasScrollbar() && resizer_will_change) {
    Layer()->DirtyStackingContextZOrderLists();
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
  if (auto* element = DynamicTo<Element>(GetLayoutBox()->GetNode()))
    element->SetNeedsCompositingUpdate();
  GetLayoutBox()->SetNeedsPaintPropertyUpdate();

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

  // Recalculate the snap container data since the scrolling behaviour for this
  // layout box changed (i.e. it either became the layout viewport or it
  // is no longer the layout viewport).
  GetLayoutBox()->GetDocument().GetSnapCoordinator().UpdateSnapContainerData(
      *GetLayoutBox());
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

WebColorScheme PaintLayerScrollableArea::UsedColorScheme() const {
  return GetLayoutBox()->StyleRef().UsedColorScheme();
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
  if (!overflow_rect_.size.IsZero()) {
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

  UpdateResizerStyle(old_style);

  // Avoid some unnecessary computation if there were and will be no scrollbars.
  if (!HasScrollbar() && !needs_horizontal_scrollbar &&
      !needs_vertical_scrollbar)
    return;

  bool horizontal_scrollbar_changed =
      SetHasHorizontalScrollbar(needs_horizontal_scrollbar);
  bool vertical_scrollbar_changed =
      SetHasVerticalScrollbar(needs_vertical_scrollbar);

  auto* layout_block = DynamicTo<LayoutBlock>(GetLayoutBox());
  if (layout_block &&
      (horizontal_scrollbar_changed || vertical_scrollbar_changed)) {
    layout_block->ScrollbarsChanged(
        horizontal_scrollbar_changed, vertical_scrollbar_changed,
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
}

void PaintLayerScrollableArea::UpdateAfterOverflowRecalc() {
  UpdateScrollDimensions();
  UpdateScrollbarProportions();
  UpdateScrollbarEnabledState();

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
        layout_invalidation_reason::kUnknown);
  }

  ClampScrollOffsetAfterOverflowChange();
  UpdateScrollableAreaSet();
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
        body->GetLayoutObject()->StyleRef().HasPseudoElementStyle(
            kPseudoIdScrollbar))
      return *body->GetLayoutObject();

    // If the <body> didn't have a custom style, then the root element might.
    Element* doc_element = doc.documentElement();
    if (doc_element && doc_element->GetLayoutObject() &&
        doc_element->GetLayoutObject()->StyleRef().HasPseudoElementStyle(
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
      style_source.StyleRef().HasPseudoElementStyle(kPseudoIdScrollbar);
  if (has_custom_scrollbar_style) {
    return CustomScrollbar::HypotheticalScrollbarThickness(
        orientation, *GetLayoutBox(), style_source);
  }

  ScrollbarControlSize scrollbar_size = kRegularScrollbar;
  if (style_source.StyleRef().HasEffectiveAppearance()) {
    scrollbar_size = LayoutTheme::GetTheme().ScrollbarControlSizeForPart(
        style_source.StyleRef().EffectiveAppearance());
  }
  ScrollbarTheme& theme = GetPageScrollbarTheme();
  if (theme.UsesOverlayScrollbars())
    return 0;
  int thickness = theme.ScrollbarThickness(scrollbar_size);
  return GetLayoutBox()
      ->GetDocument()
      .GetPage()
      ->GetChromeClient()
      .WindowToViewportScalar(GetLayoutBox()->GetFrame(), thickness);
}

bool PaintLayerScrollableArea::NeedsScrollbarReconstruction() const {
  if (!HasScrollbar())
    return false;

  const LayoutObject& style_source = ScrollbarStyleSource(*GetLayoutBox());
  bool needs_custom =
      style_source.IsBox() &&
      style_source.StyleRef().HasPseudoElementStyle(kPseudoIdScrollbar);

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
      if (To<CustomScrollbar>(scrollbar)->StyleSource()->GetLayoutObject() !=
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
    if (h_mode == ScrollbarMode::kAlwaysOn)
      needs_horizontal_scrollbar = true;
    else if (h_mode == ScrollbarMode::kAlwaysOff)
      needs_horizontal_scrollbar = false;
    if (v_mode == ScrollbarMode::kAlwaysOn)
      needs_vertical_scrollbar = true;
    else if (v_mode == ScrollbarMode::kAlwaysOff)
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
    if (h_mode != ScrollbarMode::kAuto || v_mode != ScrollbarMode::kAuto)
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

    PhysicalSize client_size_with_scrollbars =
        LayoutContentRect(kIncludeScrollbars).size;
    if (ScrollWidth() <= client_size_with_scrollbars.width &&
        ScrollHeight() <= client_size_with_scrollbars.height) {
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

  if (GetLayoutBox()->GetDocument().IsVerticalScrollEnforced()) {
    // When the policy is enforced the contents of document cannot be scrolled.
    // This would make rendering a scrollbar look strange
    // (https://crbug.com/898151).
    return false;
  }

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

const cc::SnapContainerData* PaintLayerScrollableArea::GetSnapContainerData()
    const {
  return RareData() && RareData()->snap_container_data_
             ? &RareData()->snap_container_data_.value()
             : nullptr;
}

void PaintLayerScrollableArea::SetSnapContainerData(
    base::Optional<cc::SnapContainerData> data) {
  EnsureRareData().snap_container_data_ = data;
}

base::Optional<FloatPoint>
PaintLayerScrollableArea::GetSnapPositionAndSetTarget(
    const cc::SnapSelectionStrategy& strategy) {
  if (!RareData() || !RareData()->snap_container_data_)
    return base::nullopt;

  cc::SnapContainerData& data = RareData()->snap_container_data_.value();
  if (!data.size())
    return base::nullopt;

  cc::TargetSnapAreaElementIds snap_targets;
  gfx::ScrollOffset snap_position;
  base::Optional<FloatPoint> snap_point;
  if (data.FindSnapPosition(strategy, &snap_position, &snap_targets))
    snap_point = FloatPoint(snap_position.x(), snap_position.y());
  if (data.SetTargetSnapAreaElementIds(snap_targets))
    GetLayoutBox()->SetNeedsPaintPropertyUpdate();

  return snap_point;
}

bool PaintLayerScrollableArea::HasOverflowControls() const {
  // We do not need to check for ScrollCorner because it only exists iff there
  // are scrollbars, see: |ScrollCornerRect| and |UpdateScrollCornerStyle|.
  DCHECK(!ScrollCorner() || HasScrollbar());
  return HasScrollbar() || GetLayoutBox()->CanResize();
}

bool PaintLayerScrollableArea::HasOverlayOverflowControls() const {
  return HasOverlayScrollbars() ||
         (!HasScrollbar() && GetLayoutBox()->CanResize());
}

bool PaintLayerScrollableArea::HasNonOverlayOverflowControls() const {
  return HasScrollbar() && !HasOverlayScrollbars();
}

void PaintLayerScrollableArea::PositionOverflowControls() {
  if (!HasOverflowControls())
    return;

  const IntRect border_box =
      GetLayoutBox()->PixelSnappedBorderBoxRect(layer_->SubpixelAccumulation());

  if (Scrollbar* vertical_scrollbar = VerticalScrollbar())
    vertical_scrollbar->SetFrameRect(RectForVerticalScrollbar(border_box));

  if (Scrollbar* horizontal_scrollbar = HorizontalScrollbar())
    horizontal_scrollbar->SetFrameRect(RectForHorizontalScrollbar(border_box));

  if (scroll_corner_)
    scroll_corner_->SetFrameRect(LayoutRect(ScrollCornerRect()));

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
  if (!HasNonOverlayOverflowControls()) {
    if (scroll_corner_) {
      scroll_corner_->Destroy();
      scroll_corner_ = nullptr;
    }
    return;
  }
  const LayoutObject& style_source = ScrollbarStyleSource(*GetLayoutBox());
  scoped_refptr<ComputedStyle> corner =
      GetLayoutBox()->HasOverflowClip()
          ? style_source.GetUncachedPseudoElementStyle(
                PseudoElementStyleRequest(kPseudoIdScrollbarCorner),
                style_source.Style())
          : scoped_refptr<ComputedStyle>(nullptr);
  if (corner) {
    if (!scroll_corner_) {
      scroll_corner_ = LayoutCustomScrollbarPart::CreateAnonymous(
          &GetLayoutBox()->GetDocument(), this);
    }
    scroll_corner_->SetStyle(std::move(corner));
  } else if (scroll_corner_) {
    scroll_corner_->Destroy();
    scroll_corner_ = nullptr;
  }
}

bool PaintLayerScrollableArea::HitTestOverflowControls(
    HitTestResult& result,
    const IntPoint& local_point) {
  if (!HasOverflowControls())
    return false;

  IntRect resize_control_rect;
  if (GetLayoutBox()->StyleRef().HasResize()) {
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
  if (!GetLayoutBox()->StyleRef().HasResize())
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

  IntPoint local_point = RoundedIntPoint(
      GetLayoutBox()->AbsoluteToLocalPoint(PhysicalOffset(absolute_point)));
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

void PaintLayerScrollableArea::UpdateResizerStyle(
    const ComputedStyle* old_style) {
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() && old_style &&
      old_style->UnresolvedResize() !=
          GetLayoutBox()->StyleRef().UnresolvedResize()) {
    // Invalidate the composited scroll corner layer on resize style change.
    if (auto* graphics_layer = GraphicsLayerForScrollCorner())
      graphics_layer->SetNeedsDisplay();
  }

  if (!resizer_ && !GetLayoutBox()->CanResize())
    return;

  const LayoutObject& style_source = ScrollbarStyleSource(*GetLayoutBox());
  scoped_refptr<ComputedStyle> resizer =
      GetLayoutBox()->HasOverflowClip()
          ? style_source.GetUncachedPseudoElementStyle(
                PseudoElementStyleRequest(kPseudoIdResizer),
                style_source.Style())
          : scoped_refptr<ComputedStyle>(nullptr);
  if (resizer) {
    if (!resizer_) {
      resizer_ = LayoutCustomScrollbarPart::CreateAnonymous(
          &GetLayoutBox()->GetDocument(), this);
    }
    resizer_->SetStyle(std::move(resizer));
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
  IntPoint local_point = RoundedIntPoint(
      GetLayoutBox()->AbsoluteToLocalPoint(PhysicalOffset(absolute_point)));
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
  auto* element = To<Element>(GetLayoutBox()->GetNode());

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

  EResize resize = GetLayoutBox()->StyleRef().Resize(
      GetLayoutBox()->ContainingBlock()->StyleRef());
  if (resize != EResize::kVertical && difference.Width()) {
    if (element->IsFormControlElement()) {
      // Make implicit margins from the theme explicit (see
      // <http://bugs.webkit.org/show_bug.cgi?id=9547>).
      element->SetInlineStyleProperty(
          CSSPropertyID::kMarginLeft,
          GetLayoutBox()->MarginLeft() / zoom_factor,
          CSSPrimitiveValue::UnitType::kPixels);
      element->SetInlineStyleProperty(
          CSSPropertyID::kMarginRight,
          GetLayoutBox()->MarginRight() / zoom_factor,
          CSSPrimitiveValue::UnitType::kPixels);
    }
    LayoutUnit base_width =
        GetLayoutBox()->Size().Width() -
        (is_box_sizing_border ? LayoutUnit()
                              : GetLayoutBox()->BorderAndPaddingWidth());
    base_width = LayoutUnit(base_width / zoom_factor);
    element->SetInlineStyleProperty(CSSPropertyID::kWidth,
                                    RoundToInt(base_width + difference.Width()),
                                    CSSPrimitiveValue::UnitType::kPixels);
  }

  if (resize != EResize::kHorizontal && difference.Height()) {
    if (element->IsFormControlElement()) {
      // Make implicit margins from the theme explicit (see
      // <http://bugs.webkit.org/show_bug.cgi?id=9547>).
      element->SetInlineStyleProperty(CSSPropertyID::kMarginTop,
                                      GetLayoutBox()->MarginTop() / zoom_factor,
                                      CSSPrimitiveValue::UnitType::kPixels);
      element->SetInlineStyleProperty(
          CSSPropertyID::kMarginBottom,
          GetLayoutBox()->MarginBottom() / zoom_factor,
          CSSPrimitiveValue::UnitType::kPixels);
    }
    LayoutUnit base_height =
        GetLayoutBox()->Size().Height() -
        (is_box_sizing_border ? LayoutUnit()
                              : GetLayoutBox()->BorderAndPaddingHeight());
    base_height = LayoutUnit(base_height / zoom_factor);
    element->SetInlineStyleProperty(
        CSSPropertyID::kHeight, RoundToInt(base_height + difference.Height()),
        CSSPrimitiveValue::UnitType::kPixels);
  }

  document.UpdateStyleAndLayout();

  // FIXME (Radar 4118564): We should also autoscroll the window as necessary to
  // keep the point under the cursor in view.
}

PhysicalRect PaintLayerScrollableArea::ScrollIntoView(
    const PhysicalRect& absolute_rect,
    const WebScrollIntoViewParams& params) {
  PhysicalRect local_expose_rect =
      GetLayoutBox()->AbsoluteToLocalRect(absolute_rect);
  PhysicalOffset border_origin_to_scroll_origin(-GetLayoutBox()->BorderLeft(),
                                                -GetLayoutBox()->BorderTop());
  // There might be scroll bar between border_origin and scroll_origin.
  IntSize scroll_bar_adjustment =
      GetLayoutBox()->OriginAdjustmentForScrollbars();
  border_origin_to_scroll_origin.left -= scroll_bar_adjustment.Width();
  border_origin_to_scroll_origin.top -= scroll_bar_adjustment.Height();
  border_origin_to_scroll_origin +=
      PhysicalOffset::FromFloatSizeFloor(GetScrollOffset());
  // Represent the rect in the container's scroll-origin coordinate.
  local_expose_rect.Move(border_origin_to_scroll_origin);
  PhysicalRect scroll_snapport_rect = VisibleScrollSnapportRect();

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

  FloatPoint end_point = ScrollOffsetToPosition(new_scroll_offset);
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndPosition(
          gfx::ScrollOffset(end_point), true, true);
  end_point = GetSnapPositionAndSetTarget(*strategy).value_or(end_point);
  new_scroll_offset = ScrollPositionToOffset(end_point);

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
  local_expose_rect.Move(
      -PhysicalOffset::FromFloatSizeRound(scroll_offset_difference));

  // Represent the rects in the container's border-box coordinate.
  local_expose_rect.Move(-border_origin_to_scroll_origin);
  scroll_snapport_rect.Move(-border_origin_to_scroll_origin);
  PhysicalRect intersect =
      Intersection(scroll_snapport_rect, local_expose_rect);

  if (intersect.IsEmpty() && !scroll_snapport_rect.IsEmpty() &&
      !local_expose_rect.IsEmpty()) {
    return GetLayoutBox()->LocalToAbsoluteRect(local_expose_rect);
  }
  intersect = GetLayoutBox()->LocalToAbsoluteRect(intersect);
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
    if (h_mode == ScrollbarMode::kAlwaysOff &&
        v_mode == ScrollbarMode::kAlwaysOff)
      has_overflow = false;
  }
  scrolls_overflow_ = has_overflow && is_visible_to_hit_test;
  if (did_scroll_overflow == ScrollsOverflow())
    return;

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // Change of scrolls_overflow may affect whether we create ScrollTranslation
    // which is referenced from ScrollDisplayItem. Invalidate scrollbars (but
    // not their parts) to repaint the display item.
    if (auto* scrollbar = HorizontalScrollbar())
      scrollbar->SetNeedsPaintInvalidation(kNoPart);
    if (auto* scrollbar = VerticalScrollbar())
      scrollbar->SetNeedsPaintInvalidation(kNoPart);
  }

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

  // Scroll hit test display items depend on whether the box scrolls overflow.
  // The scroll hit test display items paint in the background phase
  // (see: BoxPainter::PaintBoxDecorationBackground).
  GetLayoutBox()->SetBackgroundNeedsFullPaintInvalidation();

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
        scrolling_coordinator &&
        scrolling_coordinator->UpdateCompositedScrollOffset(this);

    if (!handled_scroll) {
      compositor->SetNeedsCompositingUpdate(
          kCompositingUpdateAfterGeometryChange);
    }

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

  // TODO(crbug.com/985127, crbug.com/1015833): We should just use the main
  // thread scrolling reasons on the scroll node which should have all required
  // reasons. If it was not, we would have inconsistent results here and
  // ScrollNode::GetMainThreadScrollingReasons().
  if (LocalFrame* frame = GetLayoutBox()->GetFrame()) {
    if (frame->View()->GetMainThreadScrollingReasons())
      return true;
  }
  if (HasNonCompositedStickyDescendants())
    return true;

  if (GraphicsLayer* layer = GraphicsLayerForScrolling()) {
    // Property tree state is not available until the PrePaint lifecycle stage.
    DCHECK_GE(GetDocument()->Lifecycle().GetState(),
              DocumentLifecycle::kPrePaintClean);
    const auto* scroll = layer->GetPropertyTreeState()
                             .Transform()
                             .NearestScrollTranslationNode()
                             .ScrollNode();
    DCHECK(scroll);
    return scroll->GetMainThreadScrollingReasons() != 0;
  }
  return true;
}

static bool LayerNodeMayNeedCompositedScrolling(const PaintLayer* layer) {
  // Don't force composite scroll for select or text input elements.
  if (Node* node = layer->GetLayoutObject().GetNode()) {
    if (IsA<HTMLSelectElement>(node))
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
    bool force_prefer_compositing_to_lcd_text) {
  DCHECK_EQ(RuntimeEnabledFeatures::CompositeAfterPaintEnabled()
                ? DocumentLifecycle::kInPrePaint
                : DocumentLifecycle::kInCompositingUpdate,
            GetDocument()->Lifecycle().GetState());

  non_composited_main_thread_scrolling_reasons_ = 0;

  if (CompositingReasonFinder::RequiresCompositingForRootScroller(*layer_))
    return true;

  if (!layer_->ScrollsOverflow())
    return false;

  if (layer_->Size().IsEmpty())
    return false;

  const auto* box = GetLayoutBox();

  // Although trivial 3D transforms are not always a direct compositing reason
  // (see CompositingReasonFinder::RequiresCompositingFor3DTransform), we treat
  // them as one for composited scrolling. This is because of the amount of
  // content that depends on this optimization, and because of the long-term
  // desire to use composited scrolling whenever possible.
  if (box->HasTransformRelatedProperty() &&
      box->StyleRef().Has3DTransformOperation()) {
    return true;
  }

  if (!force_prefer_compositing_to_lcd_text &&
      !LayerNodeMayNeedCompositedScrolling(layer_)) {
    return false;
  }

  bool needs_composited_scrolling = true;

  // TODO(flackr): Allow integer transforms as long as all of the ancestor
  // transforms are also integer.
  bool background_supports_lcd_text =
      box->StyleRef().IsStackingContext() &&
      (box->GetBackgroundPaintLocation(
           &non_composited_main_thread_scrolling_reasons_) &
       kBackgroundPaintInScrollingContents) &&
      layer_->BackgroundIsKnownToBeOpaqueInRect(box->PhysicalPaddingBoxRect(),
                                                true) &&
      !layer_->CompositesWithTransform() && !layer_->CompositesWithOpacity();

  if (!force_prefer_compositing_to_lcd_text &&
      !layer_->Compositor()->PreferCompositingToLCDTextEnabled() &&
      !background_supports_lcd_text) {
    if (layer_->CompositesWithOpacity()) {
      non_composited_main_thread_scrolling_reasons_ |=
          cc::MainThreadScrollingReason::kHasOpacityAndLCDText;
    }
    if (layer_->CompositesWithTransform()) {
      non_composited_main_thread_scrolling_reasons_ |=
          cc::MainThreadScrollingReason::kHasTransformAndLCDText;
    }
    if (!layer_->BackgroundIsKnownToBeOpaqueInRect(
            box->PhysicalPaddingBoxRect(), true)) {
      non_composited_main_thread_scrolling_reasons_ |=
          cc::MainThreadScrollingReason::kBackgroundNotOpaqueInRectAndLCDText;
    }
    if (!box->StyleRef().IsStackingContext()) {
      non_composited_main_thread_scrolling_reasons_ |=
          cc::MainThreadScrollingReason::kIsNotStackingContextAndLCDText;
    }

    needs_composited_scrolling = false;
  }

  if (box->HasClip() || layer_->HasDescendantWithClipPath() ||
      !!layer_->ClipPathAncestor()) {
    non_composited_main_thread_scrolling_reasons_ |=
        cc::MainThreadScrollingReason::kHasClipRelatedProperty;
    needs_composited_scrolling = false;
  }

  DCHECK(!(non_composited_main_thread_scrolling_reasons_ &
           ~cc::MainThreadScrollingReason::kNonCompositedReasons));
  return needs_composited_scrolling;
}

void PaintLayerScrollableArea::UpdateNeedsCompositedScrolling(
    bool force_prefer_compositing_to_lcd_text) {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  needs_composited_scrolling_ =
      ComputeNeedsCompositedScrolling(force_prefer_compositing_to_lcd_text);
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
  if (ChromeClient* client =
          GetLayoutBox()->GetFrameView()->GetChromeClient()) {
    client->ScheduleAnimation(GetLayoutBox()->GetFrameView());
    return true;
  }
  return false;
}

void PaintLayerScrollableArea::ResetRebuildScrollbarLayerFlags() {
  rebuild_horizontal_scrollbar_layer_ = false;
  rebuild_vertical_scrollbar_layer_ = false;
}

cc::AnimationHost* PaintLayerScrollableArea::GetCompositorAnimationHost()
    const {
  return layer_->GetLayoutObject().GetFrameView()->GetCompositorAnimationHost();
}

CompositorAnimationTimeline*
PaintLayerScrollableArea::GetCompositorAnimationTimeline() const {
  return layer_->GetLayoutObject()
      .GetFrameView()
      ->GetCompositorAnimationTimeline();
}

bool PaintLayerScrollableArea::HasTickmarks() const {
  return layer_->IsRootLayer() && ToLayoutView(GetLayoutBox())->HasTickmarks();
}

Vector<IntRect> PaintLayerScrollableArea::GetTickmarks() const {
  if (layer_->IsRootLayer())
    return ToLayoutView(GetLayoutBox())->GetTickmarks();
  return Vector<IntRect>();
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
      style_source.StyleRef().HasPseudoElementStyle(kPseudoIdScrollbar);
  if (has_custom_scrollbar_style) {
    DCHECK(style_source.GetNode() && style_source.GetNode()->IsElementNode());
    scrollbar = CustomScrollbar::CreateCustomScrollbar(
        ScrollableArea(), orientation, To<Element>(style_source.GetNode()));
  } else {
    ScrollbarControlSize scrollbar_size = kRegularScrollbar;
    if (style_source.StyleRef().HasEffectiveAppearance()) {
      scrollbar_size = LayoutTheme::GetTheme().ScrollbarControlSizeForPart(
          style_source.StyleRef().EffectiveAppearance());
    }
    Element* style_source_element = nullptr;
    if (RuntimeEnabledFeatures::FormControlsRefreshEnabled()) {
      style_source_element = DynamicTo<Element>(style_source.GetNode());
    }
    scrollbar = MakeGarbageCollected<Scrollbar>(
        ScrollableArea(), orientation, scrollbar_size, style_source_element,
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
            box, layout_invalidation_reason::kScrollbarChanged);
        if (auto* layout_block = DynamicTo<LayoutBlock>(box)) {
          bool horizontal_scrollbar_changed =
              scrollable_area->HasHorizontalScrollbar() !=
              scrollable_area->HadHorizontalScrollbarBeforeRelayout();
          bool vertical_scrollbar_changed =
              scrollable_area->HasVerticalScrollbar() !=
              scrollable_area->HadVerticalScrollbarBeforeRelayout();
          if (horizontal_scrollbar_changed || vertical_scrollbar_changed) {
            layout_block->ScrollbarsChanged(horizontal_scrollbar_changed,
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
  DEFINE_STATIC_LOCAL(
      Persistent<HeapVector<Member<PaintLayerScrollableArea>>>,
      needs_relayout_list,
      (MakeGarbageCollected<HeapVector<Member<PaintLayerScrollableArea>>>()));
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
  DEFINE_STATIC_LOCAL(
      Persistent<HeapVector<Member<PaintLayerScrollableArea>>>,
      needs_clamp_list,
      (MakeGarbageCollected<HeapVector<Member<PaintLayerScrollableArea>>>()));
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

void PaintLayerScrollableArea::DidAddScrollbar(
    Scrollbar& scrollbar,
    ScrollbarOrientation orientation) {
  // Z-order of reparented scrollbar is updated along with the z-order lists.
  if (scrollbar.IsOverlayScrollbar())
    layer_->DirtyStackingContextZOrderLists();

  ScrollableArea::DidAddScrollbar(scrollbar, orientation);
}

void PaintLayerScrollableArea::WillRemoveScrollbar(
    Scrollbar& scrollbar,
    ScrollbarOrientation orientation) {
  if (layer_->NeedsReorderOverlayOverflowControls()) {
    // Z-order of reparented scrollbar is updated along with the z-order lists.
    DCHECK(scrollbar.IsOverlayScrollbar());
    layer_->DirtyStackingContextZOrderLists();
  }

  if (!scrollbar.IsCustomScrollbar() &&
      !(orientation == kHorizontalScrollbar
            ? GraphicsLayerForHorizontalScrollbar()
            : GraphicsLayerForVerticalScrollbar())) {
    ObjectPaintInvalidator(*GetLayoutBox())
        .SlowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(
            scrollbar, PaintInvalidationReason::kScrollControl);
  }

  ScrollableArea::WillRemoveScrollbar(scrollbar, orientation);
}

// Returns true if the scroll control is invalidated.
static bool ScrollControlNeedsPaintInvalidation(
    const IntRect& new_visual_rect,
    const IntRect& previous_visual_rect,
    bool needs_paint_invalidation) {
  if (new_visual_rect != previous_visual_rect)
    return true;
  if (previous_visual_rect.IsEmpty()) {
    DCHECK(new_visual_rect.IsEmpty());
    // Do not issue an empty invalidation.
    return false;
  }

  return needs_paint_invalidation;
}

static IntRect InvalidatePaintOfScrollbarIfNeeded(
    Scrollbar* scrollbar,
    GraphicsLayer* graphics_layer,
    bool& previously_was_overlay,
    const IntRect& previous_visual_rect,
    bool needs_paint_invalidation,
    LayoutBox& box,
    bool& box_geometry_has_been_invalidated,
    const PaintInvalidatorContext& context) {
  bool is_overlay = scrollbar && scrollbar->IsOverlayScrollbar();

  IntRect new_visual_rect;
  // Calculate visual rect of the scrollbar, except overlay composited
  // scrollbars because we invalidate the graphics layer only.
  // TODO(wangxianzhu): Investigate how non-empty visual rect of composited
  // scrollbar affects GPU texture memory (http://crbug.com/1011996).
  if (scrollbar && !(graphics_layer && is_overlay))
    new_visual_rect = scrollbar->FrameRect();

  if (needs_paint_invalidation && graphics_layer) {
    // If the scrollbar needs paint invalidation but didn't change location/size
    // or the scrollbar is an overlay scrollbar (visual rect is empty),
    // invalidating the graphics layer is enough (which has been done in
    // ScrollableArea::setScrollbarNeedsPaintInvalidation()).
    needs_paint_invalidation = false;
    DCHECK(!graphics_layer->PaintsContentOrHitTest() ||
           graphics_layer->GetPaintController().GetPaintArtifact().IsEmpty());
  }

  // Invalidate the box's display item client if the box's padding box size is
  // affected by change of the non-overlay scrollbar width. We detect change of
  // visual rect size instead of change of scrollbar width, which may have some
  // false-positives (e.g. the scrollbar changed length but not width) but won't
  // invalidate more than expected because in the false-positive case the box
  // must have changed size and have been invalidated.
  IntSize new_scrollbar_used_space_in_box;
  if (!is_overlay)
    new_scrollbar_used_space_in_box = new_visual_rect.Size();
  IntSize previous_scrollbar_used_space_in_box;
  if (!previously_was_overlay)
    previous_scrollbar_used_space_in_box = previous_visual_rect.Size();

  // The IsEmpty() check avoids invalidaiton in cases when the visual rect
  // changes from (0,0 0x0) to (0,0 0x100).
  if (!box_geometry_has_been_invalidated &&
      !(new_scrollbar_used_space_in_box.IsEmpty() &&
        previous_scrollbar_used_space_in_box.IsEmpty()) &&
      new_scrollbar_used_space_in_box != previous_scrollbar_used_space_in_box) {
    context.painting_layer->SetNeedsRepaint();
    ObjectPaintInvalidator(box).InvalidateDisplayItemClient(
        box, PaintInvalidationReason::kGeometry);
    box_geometry_has_been_invalidated = true;
  }

  previously_was_overlay = is_overlay;

  if (!scrollbar || graphics_layer ||
      !ScrollControlNeedsPaintInvalidation(
          new_visual_rect, previous_visual_rect, needs_paint_invalidation))
    return new_visual_rect;

  context.painting_layer->SetNeedsRepaint();
  ObjectPaintInvalidator(box).InvalidateDisplayItemClient(
      *scrollbar, PaintInvalidationReason::kScrollControl);
  if (scrollbar->IsCustomScrollbar()) {
    To<CustomScrollbar>(scrollbar)
        ->InvalidateDisplayItemClientsOfScrollbarParts();
  }

  return new_visual_rect;
}

void PaintLayerScrollableArea::InvalidatePaintOfScrollControlsIfNeeded(
    const PaintInvalidatorContext& context) {
  LayoutBox& box = *GetLayoutBox();
  bool box_geometry_has_been_invalidated = false;
  SetHorizontalScrollbarVisualRect(InvalidatePaintOfScrollbarIfNeeded(
      HorizontalScrollbar(), GraphicsLayerForHorizontalScrollbar(),
      horizontal_scrollbar_previously_was_overlay_,
      horizontal_scrollbar_visual_rect_,
      HorizontalScrollbarNeedsPaintInvalidation(), box,
      box_geometry_has_been_invalidated, context));
  SetVerticalScrollbarVisualRect(InvalidatePaintOfScrollbarIfNeeded(
      VerticalScrollbar(), GraphicsLayerForVerticalScrollbar(),
      vertical_scrollbar_previously_was_overlay_,
      vertical_scrollbar_visual_rect_,
      VerticalScrollbarNeedsPaintInvalidation(), box,
      box_geometry_has_been_invalidated, context));

  IntRect scroll_corner_and_resizer_visual_rect = ScrollCornerAndResizerRect();
  if (ScrollControlNeedsPaintInvalidation(
          scroll_corner_and_resizer_visual_rect,
          scroll_corner_and_resizer_visual_rect_,
          ScrollCornerNeedsPaintInvalidation())) {
    SetScrollCornerAndResizerVisualRect(scroll_corner_and_resizer_visual_rect);
    if (LayoutCustomScrollbarPart* scroll_corner = ScrollCorner()) {
      ObjectPaintInvalidator(*scroll_corner)
          .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
              PaintInvalidationReason::kScrollControl);
    }
    if (LayoutCustomScrollbarPart* resizer = Resizer()) {
      ObjectPaintInvalidator(*resizer)
          .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
              PaintInvalidationReason::kScrollControl);
    } else if (!box_geometry_has_been_invalidated && box.CanResize() &&
               !HasLayerForScrollCorner()) {
      context.painting_layer->SetNeedsRepaint();
      ObjectPaintInvalidator(box).InvalidateDisplayItemClient(
          box, PaintInvalidationReason::kGeometry);
    }
  }

  ClearNeedsPaintInvalidationForScrollControls();
}

void PaintLayerScrollableArea::ClearPreviousVisualRects() {
  SetHorizontalScrollbarVisualRect(IntRect());
  SetVerticalScrollbarVisualRect(IntRect());
  SetScrollCornerAndResizerVisualRect(IntRect());
}

void PaintLayerScrollableArea::SetHorizontalScrollbarVisualRect(
    const IntRect& rect) {
  horizontal_scrollbar_visual_rect_ = rect;
  if (Scrollbar* scrollbar = HorizontalScrollbar())
    scrollbar->SetVisualRect(rect);
}

void PaintLayerScrollableArea::SetVerticalScrollbarVisualRect(
    const IntRect& rect) {
  vertical_scrollbar_visual_rect_ = rect;
  if (Scrollbar* scrollbar = VerticalScrollbar())
    scrollbar->SetVisualRect(rect);
}

void PaintLayerScrollableArea::SetScrollCornerAndResizerVisualRect(
    const IntRect& rect) {
  scroll_corner_and_resizer_visual_rect_ = rect;
  if (LayoutCustomScrollbarPart* scroll_corner = ScrollCorner())
    scroll_corner->GetMutableForPainting().FirstFragment().SetVisualRect(rect);
  if (LayoutCustomScrollbarPart* resizer = Resizer())
    resizer->GetMutableForPainting().FirstFragment().SetVisualRect(rect);
}

void PaintLayerScrollableArea::ScrollControlWasSetNeedsPaintInvalidation() {
  GetLayoutBox()->SetShouldCheckForPaintInvalidation();
}

void PaintLayerScrollableArea::DidScrollWithScrollbar(
    ScrollbarPart part,
    ScrollbarOrientation orientation,
    WebInputEvent::Type type) {
  WebFeature scrollbar_use_uma;
  switch (part) {
    case kBackButtonEndPart:
    case kForwardButtonStartPart:
      UseCounter::Count(
          GetLayoutBox()->GetDocument(),
          WebFeature::kScrollbarUseScrollbarButtonReversedDirection);
      U_FALLTHROUGH;
    case kBackButtonStartPart:
    case kForwardButtonEndPart:
      scrollbar_use_uma =
          (orientation == kVerticalScrollbar
               ? WebFeature::kScrollbarUseVerticalScrollbarButton
               : WebFeature::kScrollbarUseHorizontalScrollbarButton);
      break;
    case kThumbPart:
      if (orientation == kVerticalScrollbar) {
        scrollbar_use_uma =
            (WebInputEvent::IsMouseEventType(type)
                 ? WebFeature::kVerticalScrollbarThumbScrollingWithMouse
                 : WebFeature::kVerticalScrollbarThumbScrollingWithTouch);
      } else {
        scrollbar_use_uma =
            (WebInputEvent::IsMouseEventType(type)
                 ? WebFeature::kHorizontalScrollbarThumbScrollingWithMouse
                 : WebFeature::kHorizontalScrollbarThumbScrollingWithTouch);
      }
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

  Document& document = GetLayoutBox()->GetDocument();

  UseCounter::Count(document, scrollbar_use_uma);
}

CompositorElementId PaintLayerScrollableArea::GetCompositorElementId() const {
  return CompositorElementIdFromUniqueObjectId(
      GetLayoutBox()->UniqueId(), CompositorElementIdNamespace::kScroll);
}

IntRect
PaintLayerScrollableArea::ScrollingBackgroundDisplayItemClient::VisualRect()
    const {
  const auto* box = scrollable_area_->GetLayoutBox();
  const auto& paint_offset = box->FirstFragment().PaintOffset();
  auto overflow_clip_rect =
      PixelSnappedIntRect(box->OverflowClipRect(paint_offset));
  auto scroll_size = scrollable_area_->PixelSnappedContentsSize(paint_offset);
  // Ensure scrolling contents are at least as large as the scroll clip
  scroll_size = scroll_size.ExpandedTo(overflow_clip_rect.Size());
  IntRect result(overflow_clip_rect.Location(), scroll_size);
#if DCHECK_IS_ON()
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
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

DOMNodeId
PaintLayerScrollableArea::ScrollingBackgroundDisplayItemClient::OwnerNodeId()
    const {
  return static_cast<const DisplayItemClient*>(scrollable_area_->GetLayoutBox())
      ->OwnerNodeId();
}

bool PaintLayerScrollableArea::ScrollingBackgroundDisplayItemClient::
    PaintedOutputOfObjectHasNoEffectRegardlessOfSize() const {
  return scrollable_area_->GetLayoutBox()
      ->PaintedOutputOfObjectHasNoEffectRegardlessOfSize();
}

}  // namespace blink
