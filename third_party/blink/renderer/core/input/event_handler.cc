/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2012 Digia Plc. and/or its subsidiary(-ies)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/input/event_handler.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_link_preview_triggerer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/pointer_event_factory.h"
#include "third_party/blink/renderer/core/events/text_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_frame_set_element.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/custom_scrollbar.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_custom_scrollbar_part.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/anchor_element_interaction_tracker.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/drag_state.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/touch_adjustment.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/core/style/cursor_data.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/cursors.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/windows_keyboard_codes.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-blink.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-blink.h"
#include "ui/display/screen_info.h"
#include "ui/display/screen_infos.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

// Refetch the event target node if it is removed or currently is the shadow
// node inside an <input> element.  If a mouse event handler changes the input
// element type to one that has a EmbeddedContentView associated, we'd like to
// EventHandler::handleMousePressEvent to pass the event to the
// EmbeddedContentView and thus the event target node can't still be the shadow
// node.
bool ShouldRefetchEventTarget(const MouseEventWithHitTestResults& mev) {
  Node* target_node = mev.InnerNode();
  if (!target_node || !target_node->parentNode())
    return true;
  if (auto* shadow_root = DynamicTo<ShadowRoot>(target_node))
    return IsA<HTMLInputElement>(shadow_root->host());
  return false;
}

gfx::Point GetMiddleSelectionCaretOfPosition(
    const PositionWithAffinity& position) {
  const LocalCaretRect& local_caret_rect = LocalCaretRectOfPosition(position);
  if (local_caret_rect.IsEmpty())
    return gfx::Point();
  const gfx::Rect rect = AbsoluteCaretBoundsOf(position);
  // In a multiline edit, rect.bottom() would end up on the next line, so
  // take the midpoint in order to use this corner point directly.
  if (local_caret_rect.layout_object->IsHorizontalWritingMode())
    return {rect.x(), (rect.y() + rect.bottom()) / 2};

  // When text is vertical, rect.right() would end up on the next line, so
  // take the midpoint in order to use this corner point directly.
  return {(rect.x() + rect.right()) / 2, rect.y()};
}

bool ContainsEvenAtEdge(const gfx::Rect& rect, const gfx::Point& point) {
  return point.x() >= rect.x() && point.x() <= rect.right() &&
         point.y() >= rect.y() && point.y() <= rect.bottom();
}

gfx::Point DetermineHotSpot(const Image& image,
                            bool hot_spot_specified,
                            const gfx::Point& specified_hot_spot) {
  if (hot_spot_specified) {
    return specified_hot_spot;
  }

  // If hot spot is not specified externally, it can be extracted from some
  // image formats (e.g. .cur).
  gfx::Point intrinsic_hot_spot;
  const bool image_has_intrinsic_hot_spot =
      image.GetHotSpot(intrinsic_hot_spot);
  const gfx::Rect image_rect = image.Rect();
  if (image_has_intrinsic_hot_spot && image_rect.Contains(intrinsic_hot_spot))
    return intrinsic_hot_spot;

  // If neither is provided, use a default value of (0, 0).
  return gfx::Point();
}

// Returns whether the hit element contains a title and isn't a SVGUseElement or
// part of an SVGUseElement.
bool HasTitleAndNotSVGUseElement(const HitTestResult& hovered_node_result) {
  Node* inner_node = hovered_node_result.InnerNode();
  if (!inner_node) {
    return false;
  }
  auto* element = DynamicTo<Element>(inner_node);
  if (!element || element->title().IsNull()) {
    return false;
  }
  ShadowRoot* containing_shadow_root = inner_node->ContainingShadowRoot();
  if (IsA<SVGUseElement>(element) ||
      (containing_shadow_root &&
       IsA<SVGUseElement>(containing_shadow_root->host()))) {
    return false;
  }
  return true;
}

// Get the entire style of scrollbar to get the cursor style of scrollbar
const ComputedStyle* GetComputedStyleFromScrollbar(
    const LayoutObject& layout_object,
    const HitTestResult& result) {
  if (result.IsOverScrollCorner()) {
    PaintLayerScrollableArea* scrollable_area =
        To<LayoutBox>(layout_object).GetScrollableArea();

    // For a frame, hit tests over scroll controls are considered to be over
    // the document element, but the scrollable area belongs to the LayoutView,
    // not the document element's LayoutObject.
    if (layout_object.IsDocumentElement()) {
      scrollable_area = layout_object.View()->GetScrollableArea();
    }

    // TODO(crbug.com/1519197): if the mouse is over a scroll corner, there must
    // be a scrollable area. Investigate where this is coming from.
    if (!scrollable_area) {
      SCOPED_CRASH_KEY_STRING64("cr1519197", "hit-object",
                                layout_object.DebugName().Utf8());
      base::debug::DumpWithoutCrashing();
      return nullptr;
    }

    LayoutCustomScrollbarPart* scroll_corner_layout_object =
        scrollable_area->ScrollCorner();
    if (scroll_corner_layout_object) {
      return scroll_corner_layout_object->Style();
    }
  }

  if (result.GetScrollbar() && result.GetScrollbar()->IsCustomScrollbar()) {
    const auto& custom_scroll_bar = To<CustomScrollbar>(*result.GetScrollbar());

    if (const ComputedStyle* style =
            custom_scroll_bar.GetScrollbarPartStyleForCursor(
                custom_scroll_bar.HoveredPart())) {
      return style;
    }
  }
  return nullptr;
}

}  // namespace

// The amount of time to wait for a cursor update on style and layout changes
// Set to 50Hz, no need to be faster than common screen refresh rate
static constexpr base::TimeDelta kCursorUpdateInterval = base::Milliseconds(20);

// The maximum size a cursor can be without falling back to the default cursor
// when intersecting browser native UI.
// https://developer.mozilla.org/en-US/docs/Web/CSS/cursor#icon_size_limits.
static const int kMaximumCursorSizeWithoutFallback = 32;

// The minimum amount of time an element stays active after a ShowPress
// This is roughly 9 frames, which should be long enough to be noticeable.
constexpr base::TimeDelta kMinimumActiveInterval = base::Seconds(0.15);

EventHandler::EventHandler(LocalFrame& frame)
    : frame_(frame),
      selection_controller_(MakeGarbageCollected<SelectionController>(frame)),
      hover_timer_(frame.GetTaskRunner(TaskType::kUserInteraction),
                   this,
                   &EventHandler::HoverTimerFired),
      cursor_update_timer_(
          frame.GetTaskRunner(TaskType::kInternalUserInteraction),
          this,
          &EventHandler::CursorUpdateTimerFired),
      should_only_fire_drag_over_event_(false),
      event_handler_registry_(
          frame_->IsLocalRoot()
              ? MakeGarbageCollected<EventHandlerRegistry>(*frame_)
              : &frame_->LocalFrameRoot().GetEventHandlerRegistry()),
      scroll_manager_(MakeGarbageCollected<ScrollManager>(frame)),
      mouse_event_manager_(
          MakeGarbageCollected<MouseEventManager>(frame, *scroll_manager_)),
      mouse_wheel_event_manager_(
          MakeGarbageCollected<MouseWheelEventManager>(frame,
                                                       *scroll_manager_)),
      keyboard_event_manager_(
          MakeGarbageCollected<KeyboardEventManager>(frame, *scroll_manager_)),
      pointer_event_manager_(
          MakeGarbageCollected<PointerEventManager>(frame,
                                                    *mouse_event_manager_)),
      gesture_manager_(
          MakeGarbageCollected<GestureManager>(frame,
                                               *scroll_manager_,
                                               *mouse_event_manager_,
                                               *pointer_event_manager_,
                                               *selection_controller_)),
      active_interval_timer_(frame.GetTaskRunner(TaskType::kUserInteraction),
                             this,
                             &EventHandler::ActiveIntervalTimerFired) {}

void EventHandler::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(selection_controller_);
  visitor->Trace(hover_timer_);
  visitor->Trace(cursor_update_timer_);
  visitor->Trace(capturing_mouse_events_element_);
  visitor->Trace(capturing_subframe_element_);
  visitor->Trace(last_mouse_move_event_subframe_);
  visitor->Trace(last_scrollbar_under_mouse_);
  visitor->Trace(drag_target_);
  visitor->Trace(frame_set_being_resized_);
  visitor->Trace(event_handler_registry_);
  visitor->Trace(scroll_manager_);
  visitor->Trace(mouse_event_manager_);
  visitor->Trace(mouse_wheel_event_manager_);
  visitor->Trace(keyboard_event_manager_);
  visitor->Trace(pointer_event_manager_);
  visitor->Trace(gesture_manager_);
  visitor->Trace(active_interval_timer_);
  visitor->Trace(last_deferred_tap_element_);
}

void EventHandler::Clear() {
  hover_timer_.Stop();
  cursor_update_timer_.Stop();
  active_interval_timer_.Stop();
  last_mouse_move_event_subframe_ = nullptr;
  last_scrollbar_under_mouse_ = nullptr;
  frame_set_being_resized_ = nullptr;
  drag_target_ = nullptr;
  should_only_fire_drag_over_event_ = false;
  capturing_mouse_events_element_ = nullptr;
  capturing_subframe_element_ = nullptr;
  pointer_event_manager_->Clear();
  scroll_manager_->Clear();
  gesture_manager_->Clear();
  mouse_event_manager_->Clear();
  mouse_wheel_event_manager_->Clear();
  last_show_press_timestamp_.reset();
  last_deferred_tap_element_ = nullptr;
  should_use_touch_event_adjusted_point_ = false;
  touch_adjustment_result_.unique_event_id = 0;
}

void EventHandler::UpdateSelectionForMouseDrag() {
  mouse_event_manager_->UpdateSelectionForMouseDrag();
}

void EventHandler::StartMiddleClickAutoscroll(LayoutObject* layout_object) {
  DCHECK(RuntimeEnabledFeatures::MiddleClickAutoscrollEnabled());
  if (!layout_object->IsBox())
    return;
  AutoscrollController* controller = scroll_manager_->GetAutoscrollController();
  if (!controller)
    return;

  LayoutBox* scrollable = LayoutBox::FindAutoscrollable(
      layout_object, /*is_middle_click_autoscroll*/ true);

  controller->StartMiddleClickAutoscroll(
      layout_object->GetFrame(), scrollable,
      LastKnownMousePositionInRootFrame(),
      mouse_event_manager_->LastKnownMouseScreenPosition());
  mouse_event_manager_->InvalidateClick();
}

void EventHandler::PerformHitTest(const HitTestLocation& location,
                                  HitTestResult& result,
                                  bool no_lifecycle_update) const {
  // LayoutView::hitTest causes a layout, and we don't want to hit that until
  // the first layout because until then, there is nothing shown on the screen -
  // the user can't have intentionally clicked on something belonging to this
  // page.  Furthermore, mousemove events before the first layout should not
  // lead to a premature layout() happening, which could show a flash of white.
  // See also the similar code in Document::performMouseEventHitTest.
  // The check to LifecycleUpdatesActive() prevents hit testing to frames
  // that have already had layout but are throttled to prevent painting
  // because the current Document isn't ready to render yet. In that case
  // the lifecycle update prompted by HitTest() would fail.
  if (!frame_->ContentLayoutObject() || !frame_->View() ||
      !frame_->View()->DidFirstLayout() ||
      !frame_->View()->LifecycleUpdatesActive())
    return;

  if (no_lifecycle_update) {
    frame_->ContentLayoutObject()->HitTestNoLifecycleUpdate(location, result);
  } else {
    frame_->ContentLayoutObject()->HitTest(location, result);
  }
  const HitTestRequest& request = result.GetHitTestRequest();
  if (!request.ReadOnly()) {
    frame_->GetDocument()->UpdateHoverActiveState(
        request.Active(), !request.Move(), result.InnerElement());
  }
}

HitTestResult EventHandler::HitTestResultAtLocation(
    const HitTestLocation& location,
    HitTestRequest::HitTestRequestType hit_type,
    const LayoutObject* stop_node,
    bool no_lifecycle_update,
    std::optional<HitTestRequest::HitNodeCb> hit_node_cb) {
  TRACE_EVENT0("blink", "EventHandler::HitTestResultAtLocation");

  // We always send HitTestResultAtLocation to the main frame if we have one,
  // otherwise we might hit areas that are obscured by higher frames.
  if (frame_->GetPage()) {
    LocalFrame& main_frame = frame_->LocalFrameRoot();
    if (frame_ != &main_frame) {
      LocalFrameView* frame_view = frame_->View();
      LocalFrameView* main_view = main_frame.View();
      if (frame_view && main_view) {
        HitTestLocation adjusted_location;
        if (location.IsRectBasedTest()) {
          DCHECK(location.IsRectilinear());
          if (hit_type & HitTestRequest::kHitTestVisualOverflow) {
            // Apply ancestor transforms to location rect
            PhysicalRect local_rect = location.BoundingBox();
            PhysicalRect main_frame_rect =
                frame_view->GetLayoutView()->LocalToAncestorRect(
                    local_rect, main_view->GetLayoutView(),
                    kTraverseDocumentBoundaries);
            adjusted_location = HitTestLocation(main_frame_rect);
          } else {
            // Don't apply ancestor transforms to bounding box
            PhysicalOffset main_content_point = main_view->ConvertFromRootFrame(
                frame_view->ConvertToRootFrame(location.BoundingBox().offset));
            adjusted_location = HitTestLocation(
                PhysicalRect(main_content_point, location.BoundingBox().size));
          }
        } else {
          adjusted_location = HitTestLocation(main_view->ConvertFromRootFrame(
              frame_view->ConvertToRootFrame(location.Point())));
        }
        return main_frame.GetEventHandler().HitTestResultAtLocation(
            adjusted_location, hit_type, stop_node, no_lifecycle_update);
      }
    }
  }
  // HitTestResultAtLocation is specifically used to hitTest into all frames,
  // thus it always allows child frame content.
  HitTestRequest request(hit_type | HitTestRequest::kAllowChildFrameContent,
                         stop_node, std::move(hit_node_cb));
  HitTestResult result(request, location);
  PerformHitTest(location, result, no_lifecycle_update);
  return result;
}

void EventHandler::StopAutoscroll() {
  scroll_manager_->StopMiddleClickAutoscroll();
  scroll_manager_->StopAutoscroll();
}

// TODO(bokan): This should be merged with logicalScroll assuming
// defaultSpaceEventHandler's chaining scroll can be done crossing frames.
bool EventHandler::BubblingScroll(mojom::blink::ScrollDirection direction,
                                  ui::ScrollGranularity granularity,
                                  Node* starting_node) {
  return scroll_manager_->BubblingScroll(
      direction, granularity, starting_node,
      mouse_event_manager_->MousePressNode());
}

gfx::PointF EventHandler::LastKnownMousePositionInRootFrame() const {
  return frame_->GetPage()->GetVisualViewport().ViewportToRootFrame(
      mouse_event_manager_->LastKnownMousePositionInViewport());
}

gfx::PointF EventHandler::LastKnownMouseScreenPosition() const {
  return mouse_event_manager_->LastKnownMouseScreenPosition();
}

gfx::Point EventHandler::DragDataTransferLocationForTesting() {
  if (mouse_event_manager_->GetDragState().drag_data_transfer_)
    return mouse_event_manager_->GetDragState()
        .drag_data_transfer_->DragLocation();

  return gfx::Point();
}

static bool IsSubmitImage(const Node* node) {
  auto* html_input_element = DynamicTo<HTMLInputElement>(node);
  return html_input_element &&
         html_input_element->FormControlType() == FormControlType::kInputImage;
}

bool EventHandler::UsesHandCursor(const Node* node) {
  if (!node)
    return false;
  return ((node->IsLink() || IsSubmitImage(node)) && !IsEditable(*node));
}

void EventHandler::CursorUpdateTimerFired(TimerBase*) {
  DCHECK(frame_);
  DCHECK(frame_->GetDocument());

  UpdateCursor();
}

void EventHandler::UpdateCursor() {
  TRACE_EVENT0("input", "EventHandler::updateCursor");

  // We must do a cross-frame hit test because the frame that triggered the
  // cursor update could be occluded by a different frame.
  DCHECK_EQ(frame_, &frame_->LocalFrameRoot());

  LocalFrameView* view = frame_->View();
  if (!view || !view->ShouldSetCursor())
    return;

  auto* layout_view = view->GetLayoutView();
  if (!layout_view)
    return;

  frame_->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kInput);

  HitTestRequest request(HitTestRequest::kReadOnly |
                         HitTestRequest::kAllowChildFrameContent);
  HitTestLocation location(view->ViewportToFrame(
      mouse_event_manager_->LastKnownMousePositionInViewport()));
  HitTestResult result(request, location);
  layout_view->HitTest(location, result);

  if (LocalFrame* frame = result.InnerNodeFrame()) {
    std::optional<ui::Cursor> optional_cursor =
        frame->GetEventHandler().SelectCursor(location, result);
    if (optional_cursor.has_value()) {
      view->SetCursor(optional_cursor.value());
    }
  }
}

bool EventHandler::ShouldShowResizeForNode(const LayoutObject& layout_object,
                                           const HitTestLocation& location) {
  const PaintLayer* layer = layout_object.EnclosingLayer();
  const PaintLayerScrollableArea* scrollable_area = layer->GetScrollableArea();
  return scrollable_area &&
         scrollable_area->IsAbsolutePointInResizeControl(
             ToRoundedPoint(location.Point()), kResizerForPointer);
}

bool EventHandler::IsSelectingLink(const HitTestResult& result) {
  // If a drag may be starting or we're capturing mouse events for a particular
  // node, don't treat this as a selection.
  // TODO(editing-dev): The use of UpdateStyleAndLayout needs to be audited. See
  // http://crbug.com/590369 for more details.
  frame_->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kSelection);

  const bool mouse_selection =
      !capturing_mouse_events_element_ &&
      mouse_event_manager_->MousePressed() &&
      GetSelectionController().MouseDownMayStartSelect() &&
      !mouse_event_manager_->MouseDownMayStartDrag() &&
      !frame_->Selection().ComputeVisibleSelectionInDOMTree().IsNone();
  return mouse_selection && result.IsOverLink();
}

bool EventHandler::ShouldShowIBeamForNode(const Node* node,
                                          const HitTestResult& result) {
  if (!node)
    return false;

  if (node->IsTextNode() && (node->CanStartSelection() || result.IsOverLink()))
    return true;

  return IsEditable(*node);
}

std::optional<ui::Cursor> EventHandler::SelectCursor(
    const HitTestLocation& location,
    const HitTestResult& result) {
  if (scroll_manager_->InResizeMode())
    return std::nullopt;

  Page* page = frame_->GetPage();
  if (!page)
    return std::nullopt;
  if (scroll_manager_->MiddleClickAutoscrollInProgress())
    return std::nullopt;

  if (result.GetScrollbar() && !result.GetScrollbar()->IsCustomScrollbar()) {
    return PointerCursor();
  }

  Node* node = result.InnerPossiblyPseudoNode();
  if (!node || !node->GetLayoutObject()) {
    return SelectAutoCursor(result, node, IBeamCursor());
  }

  const LayoutObject& layout_object = *node->GetLayoutObject();
  if (ShouldShowResizeForNode(layout_object, location)) {
    const LayoutBox* box = layout_object.EnclosingLayer()->GetLayoutBox();
    EResize resize = box->StyleRef().UsedResize();
    switch (resize) {
      case EResize::kVertical:
        return NorthSouthResizeCursor();
      case EResize::kHorizontal:
        return EastWestResizeCursor();
      case EResize::kBoth:
        if (box->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
          return SouthWestResizeCursor();
        } else {
          return SouthEastResizeCursor();
        }
      default:
        return PointerCursor();
    }
  }

  {
    ui::Cursor override_cursor;
    switch (layout_object.GetCursor(result.LocalPoint(), override_cursor)) {
      case kSetCursorBasedOnStyle:
        break;
      case kSetCursor:
        return override_cursor;
      case kDoNotSetCursor:
        return std::nullopt;
    }
  }

  const ComputedStyle* scrollbar_style =
      GetComputedStyleFromScrollbar(layout_object, result);

  const ComputedStyle& style =
      scrollbar_style ? *scrollbar_style : layout_object.StyleRef();

  if (const CursorList* cursors = style.Cursors()) {
    for (const auto& cursor : *cursors) {
      const StyleImage* style_image = cursor.GetImage();
      if (!style_image || !style_image->CanRender()) {
        continue;
      }
      // The 'cursor' property only allow url() and image-set(). Either of
      // those will return false from their CanRender() implementation if they
      // don't have an ImageResourceContent (and the former should always have
      // one).
      CHECK(style_image->CachedImage());

      // Compute the concrete object size in DIP based on the
      // default cursor size obtained from the OS.
      gfx::SizeF size =
          style_image->ImageSize(1,
                                 gfx::SizeF(page->GetChromeClient()
                                                .GetScreenInfos(*frame_)
                                                .system_cursor_size),
                                 kRespectImageOrientation);

      float scale = style_image->ImageScaleFactor();
      Image* image = style_image->CachedImage()->GetImage();
      if (image->IsSVGImage()) {
        // `StyleImage::ImageSize` does not take `StyleImage::ImageScaleFactor`
        // into account when computing the size for SVG images.
        size.Scale(1 / scale);
      }

      if (size.IsEmpty() ||
          !ui::Cursor::AreDimensionsValidForWeb(
              gfx::ToCeiledSize(gfx::ScaleSize(size, scale)), scale)) {
        continue;
      }

      const float device_scale_factor =
          page->GetChromeClient().GetScreenInfo(*frame_).device_scale_factor;

      // If the image is an SVG, then adjust the scale to reflect the device
      // scale factor so that the SVG can be rasterized in the native
      // resolution and scaled down to the correct size for the cursor.
      scoped_refptr<Image> svg_image_holder;
      if (auto* svg_image = DynamicTo<SVGImage>(image)) {
        scale *= device_scale_factor;
        // Re-scale back from DIP to device pixels.
        size.Scale(scale);

        // TODO(fs): Should pass proper URL. Use StyleImage::GetImage.
        svg_image_holder = SVGImageForContainer::Create(
            *svg_image, size, device_scale_factor, nullptr,
            frame_->GetDocument()
                ->GetStyleEngine()
                .ResolveColorSchemeForEmbedding(&style));
        image = svg_image_holder.get();
      }

      // Convert from DIP to physical pixels.
      gfx::Point hot_spot = gfx::ScaleToRoundedPoint(cursor.HotSpot(), scale);

      const bool hot_spot_specified = cursor.HotSpotSpecified();
      ui::Cursor custom_cursor = ui::Cursor::NewCustom(
          image->AsSkBitmapForCurrentFrame(kRespectImageOrientation),
          DetermineHotSpot(*image, hot_spot_specified, hot_spot), scale);

      // For large cursors below the max size, limit their ability to cover UI
      // elements by removing them when they are not fully contained by the
      // visual viewport. Careful, we need to make sure to translate coordinate
      // spaces if we are in an OOPIF.
      //
      // TODO(csharrison): Consider sending a fallback cursor in the IPC to the
      // browser process so we can do that calculation there instead, this would
      // ensure even a compromised renderer could not obscure browser UI with a
      // large cursor. Also, consider augmenting the intervention to drop the
      // cursor for iframes if the cursor image obscures content in the parent
      // frame.
      gfx::SizeF custom_bitmap_size(custom_cursor.custom_bitmap().width(),
                                    custom_cursor.custom_bitmap().height());
      custom_bitmap_size.Scale(1.f / custom_cursor.image_scale_factor());
      if (custom_bitmap_size.width() > kMaximumCursorSizeWithoutFallback ||
          custom_bitmap_size.height() > kMaximumCursorSizeWithoutFallback) {
        PhysicalOffset ancestor_location =
            frame_->ContentLayoutObject()->LocalToAncestorPoint(
                location.Point(),
                nullptr,  // no ancestor maps all the way up the hierarchy
                kTraverseDocumentBoundaries | kApplyRemoteMainFrameTransform);

        // Check the cursor rect with device and accessibility scaling applied.
        const float scale_factor =
            cursor_accessibility_scale_factor_ *
            (image->IsSVGImage() ? 1.f : device_scale_factor);
        gfx::SizeF scaled_size(custom_bitmap_size);
        scaled_size.Scale(scale_factor);
        gfx::PointF scaled_hot_spot(custom_cursor.custom_hotspot());
        scaled_hot_spot.Scale(scale_factor /
                              custom_cursor.image_scale_factor());
        PhysicalRect cursor_rect(
            ancestor_location -
                PhysicalOffset::FromPointFFloor(scaled_hot_spot),
            PhysicalSize::FromSizeFFloor(scaled_size));

        PhysicalRect frame_rect(page->GetVisualViewport().VisibleContentRect());
        frame_->ContentLayoutObject()->MapToVisualRectInAncestorSpace(
            nullptr, frame_rect);

        if (!frame_rect.Contains(cursor_rect)) {
          continue;
        }
      }

      return custom_cursor;
    }
  }

  const ui::Cursor& i_beam =
      style.IsHorizontalWritingMode() ? IBeamCursor() : VerticalTextCursor();

  switch (style.Cursor()) {
    case ECursor::kAuto:
      return SelectAutoCursor(result, node, i_beam);
    case ECursor::kCrosshair:
      return CrossCursor();
    case ECursor::kPointer:
      return IsSelectingLink(result) ? i_beam : HandCursor();
    case ECursor::kMove:
      return MoveCursor();
    case ECursor::kAllScroll:
      return MoveCursor();
    case ECursor::kEResize:
      return EastResizeCursor();
    case ECursor::kWResize:
      return WestResizeCursor();
    case ECursor::kNResize:
      return NorthResizeCursor();
    case ECursor::kSResize:
      return SouthResizeCursor();
    case ECursor::kNeResize:
      return NorthEastResizeCursor();
    case ECursor::kSwResize:
      return SouthWestResizeCursor();
    case ECursor::kNwResize:
      return NorthWestResizeCursor();
    case ECursor::kSeResize:
      return SouthEastResizeCursor();
    case ECursor::kNsResize:
      return NorthSouthResizeCursor();
    case ECursor::kEwResize:
      return EastWestResizeCursor();
    case ECursor::kNeswResize:
      return NorthEastSouthWestResizeCursor();
    case ECursor::kNwseResize:
      return NorthWestSouthEastResizeCursor();
    case ECursor::kColResize:
      return ColumnResizeCursor();
    case ECursor::kRowResize:
      return RowResizeCursor();
    case ECursor::kText:
      return i_beam;
    case ECursor::kWait:
      return WaitCursor();
    case ECursor::kHelp:
      return HelpCursor();
    case ECursor::kVerticalText:
      return VerticalTextCursor();
    case ECursor::kCell:
      return CellCursor();
    case ECursor::kContextMenu:
      return ContextMenuCursor();
    case ECursor::kProgress:
      return ProgressCursor();
    case ECursor::kNoDrop:
      return NoDropCursor();
    case ECursor::kAlias:
      return AliasCursor();
    case ECursor::kCopy:
      return CopyCursor();
    case ECursor::kNone:
      return NoneCursor();
    case ECursor::kNotAllowed:
      return NotAllowedCursor();
    case ECursor::kDefault:
      return PointerCursor();
    case ECursor::kZoomIn:
      return ZoomInCursor();
    case ECursor::kZoomOut:
      return ZoomOutCursor();
    case ECursor::kGrab:
      return GrabCursor();
    case ECursor::kGrabbing:
      return GrabbingCursor();
  }
  return PointerCursor();
}

std::optional<ui::Cursor> EventHandler::SelectAutoCursor(
    const HitTestResult& result,
    Node* node,
    const ui::Cursor& i_beam) {
  if (ShouldShowIBeamForNode(node, result))
    return i_beam;

  return PointerCursor();
}

WebInputEventResult EventHandler::DispatchBufferedTouchEvents() {
  return pointer_event_manager_->FlushEvents();
}

WebInputEventResult EventHandler::HandlePointerEvent(
    const WebPointerEvent& web_pointer_event,
    const Vector<WebPointerEvent>& coalesced_events,
    const Vector<WebPointerEvent>& predicted_events) {
  return pointer_event_manager_->HandlePointerEvent(
      web_pointer_event, coalesced_events, predicted_events);
}

WebInputEventResult EventHandler::HandleMousePressEvent(
    const WebMouseEvent& mouse_event) {
  TRACE_EVENT0("blink", "EventHandler::handleMousePressEvent");

  // For 4th/5th button in the mouse since Chrome does not yet send
  // button value to Blink but in some cases it does send the event.
  // This check is needed to suppress such an event (crbug.com/574959)
  if (mouse_event.button == WebPointerProperties::Button::kNoButton)
    return WebInputEventResult::kHandledSuppressed;

  capturing_mouse_events_element_ = nullptr;
  mouse_event_manager_->HandleMousePressEventUpdateStates(mouse_event);
  if (!frame_->View())
    return WebInputEventResult::kNotHandled;

  HitTestRequest request(HitTestRequest::kActive);
  // Save the document point we generate in case the window coordinate is
  // invalidated by what happens when we dispatch the event.
  PhysicalOffset document_point = frame_->View()->ConvertFromRootFrame(
      PhysicalOffset(gfx::ToFlooredPoint(mouse_event.PositionInRootFrame())));
  MouseEventWithHitTestResults mev = GetMouseEventTarget(request, mouse_event);
  if (!mev.InnerNode()) {
    // An anonymous box can be scrollable.
    if (PassMousePressEventToScrollbar(mev))
      return WebInputEventResult::kHandledSystem;

    mouse_event_manager_->InvalidateClick();
    return WebInputEventResult::kNotHandled;
  }

  mouse_event_manager_->SetMousePressNode(mev.InnerNode());
  frame_->GetDocument()->SetSequentialFocusNavigationStartingPoint(
      mev.InnerNode());

  LocalFrame* subframe = event_handling_util::GetTargetSubframe(mev);
  if (subframe) {
    WebInputEventResult result = PassMousePressEventToSubframe(mev, subframe);
    if (mouse_event_manager_->MousePressed()) {
      capturing_mouse_events_element_ = mev.InnerElement();
      capturing_subframe_element_ = mev.InnerElement();
    }

    mouse_event_manager_->InvalidateClick();
    return result;
  }

  if (discarded_events_.mouse_down_target != kInvalidDOMNodeId &&
      discarded_events_.mouse_down_target == mev.InnerNode()->GetDomNodeId() &&
      mouse_event.TimeStamp() - discarded_events_.mouse_down_time <
          event_handling_util::kDiscardedEventMistakeInterval) {
    mev.InnerNode()->GetDocument().CountUse(
        WebFeature::kInputEventToRecentlyMovedIframeMistakenlyDiscarded);
  }
  if (event_handling_util::ShouldDiscardEventTargetingFrame(mev.Event(),
                                                            *frame_)) {
    discarded_events_.mouse_down_target = mev.InnerNode()->GetDomNodeId();
    discarded_events_.mouse_down_time = mouse_event.TimeStamp();
    return WebInputEventResult::kHandledSuppressed;
  } else {
    discarded_events_.mouse_down_target = kInvalidDOMNodeId;
    discarded_events_.mouse_down_time = base::TimeTicks();
  }

  LocalFrame::NotifyUserActivation(
      frame_, mojom::blink::UserActivationNotificationType::kInteraction,
      RuntimeEnabledFeatures::BrowserVerifiedUserActivationMouseEnabled());

  if (RuntimeEnabledFeatures::MiddleClickAutoscrollEnabled()) {
    // We store whether middle click autoscroll is in progress before calling
    // stopAutoscroll() because it will set m_autoscrollType to NoAutoscroll on
    // return.
    bool is_middle_click_autoscroll_in_progress =
        scroll_manager_->MiddleClickAutoscrollInProgress();
    scroll_manager_->StopMiddleClickAutoscroll();
    if (is_middle_click_autoscroll_in_progress) {
      // We invalidate the click when exiting middle click auto scroll so that
      // we don't inadvertently navigate away from the current page (e.g. the
      // click was on a hyperlink). See <rdar://problem/6095023>.
      mouse_event_manager_->InvalidateClick();
      return WebInputEventResult::kHandledSuppressed;
    }
  }

  mouse_event_manager_->SetClickCount(mouse_event.click_count);
  mouse_event_manager_->SetMouseDownElement(mev.InnerElement());

  if (!mouse_event.FromTouch())
    frame_->Selection().SetCaretBlinkingSuspended(true);

  WebInputEventResult event_result = DispatchMousePointerEvent(
      WebInputEvent::Type::kPointerDown, mev.InnerElement(), mev.Event(),
      Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  // Disabled form controls still need to resize the scrollable area.
  if ((event_result == WebInputEventResult::kNotHandled ||
       event_result == WebInputEventResult::kHandledSuppressed) &&
      frame_->View()) {
    LocalFrameView* view = frame_->View();
    PaintLayer* layer =
        mev.InnerNode()->GetLayoutObject()
            ? mev.InnerNode()->GetLayoutObject()->EnclosingLayer()
            : nullptr;
    gfx::Point p = view->ConvertFromRootFrame(
        gfx::ToFlooredPoint(mouse_event.PositionInRootFrame()));
    if (layer && layer->GetScrollableArea() &&
        layer->GetScrollableArea()->IsAbsolutePointInResizeControl(
            p, kResizerForPointer)) {
      scroll_manager_->SetResizeScrollableArea(layer, p);
      return WebInputEventResult::kHandledSystem;
    }
  }

  // m_selectionInitiationState is initialized after dispatching mousedown
  // event in order not to keep the selection by DOM APIs because we can't
  // give the user the chance to handle the selection by user action like
  // dragging if we keep the selection in case of mousedown. FireFox also has
  // the same behavior and it's more compatible with other browsers.
  GetSelectionController().InitializeSelectionState();

  HitTestResult hit_test_result = event_handling_util::HitTestResultInFrame(
      frame_, HitTestLocation(document_point), HitTestRequest::kReadOnly);
  InputDeviceCapabilities* source_capabilities =
      frame_->DomWindow()->GetInputDeviceCapabilities()->FiresTouchEvents(
          mouse_event.FromTouch());

  if (event_result == WebInputEventResult::kNotHandled) {
    event_result = mouse_event_manager_->HandleMouseFocus(hit_test_result,
                                                          source_capabilities);
  }

  if (event_result == WebInputEventResult::kNotHandled || mev.GetScrollbar()) {
    // Outermost main frames don't implicitly capture mouse input on MouseDown,
    // all subframes do (regardless of whether local or remote or fenced).
    if (frame_->IsAttached() && !frame_->IsOutermostMainFrame())
      CaptureMouseEventsToWidget(true);
  }

  if (PassMousePressEventToScrollbar(mev))
    event_result = WebInputEventResult::kHandledSystem;

  if (event_result == WebInputEventResult::kNotHandled) {
    if (ShouldRefetchEventTarget(mev)) {
      HitTestRequest read_only_request(HitTestRequest::kReadOnly |
                                       HitTestRequest::kActive);
      mev = frame_->GetDocument()->PerformMouseEventHitTest(
          read_only_request, document_point, mouse_event);
    }
    event_result = mouse_event_manager_->HandleMousePressEvent(mev);
  }

  if (mev.GetHitTestResult().InnerNode() &&
      mouse_event.button == WebPointerProperties::Button::kLeft) {
    DCHECK_EQ(WebInputEvent::Type::kMouseDown, mouse_event.GetType());
    HitTestResult result = mev.GetHitTestResult();
    result.SetToShadowHostIfInUAShadowRoot();
    frame_->GetChromeClient().OnMouseDown(*result.InnerNode());
  }

  return event_result;
}

WebInputEventResult EventHandler::HandleMouseMoveEvent(
    const WebMouseEvent& event,
    const Vector<WebMouseEvent>& coalesced_events,
    const Vector<WebMouseEvent>& predicted_events) {
  TRACE_EVENT0("blink", "EventHandler::handleMouseMoveEvent");
  DCHECK(event.GetType() == WebInputEvent::Type::kMouseMove);
  HitTestResult hovered_node_result;
  HitTestLocation location;
  WebInputEventResult result =
      HandleMouseMoveOrLeaveEvent(event, coalesced_events, predicted_events,
                                  &hovered_node_result, &location);

  Page* page = frame_->GetPage();
  if (!page)
    return result;

  if (PaintLayer* layer =
          event_handling_util::LayerForNode(hovered_node_result.InnerNode())) {
    if (ScrollableArea* layer_scrollable_area =
            event_handling_util::AssociatedScrollableArea(layer))
      layer_scrollable_area->MouseMovedInContentArea();
  }

  // Should not convert the hit shadow element to its shadow host, so that
  // tooltips in the shadow tree appear correctly.
  if (!HasTitleAndNotSVGUseElement(hovered_node_result)) {
    hovered_node_result.SetToShadowHostIfInUAShadowRoot();
  }
  page->GetChromeClient().MouseDidMoveOverElement(*frame_, location,
                                                  hovered_node_result);

  return result;
}

void EventHandler::HandleMouseLeaveEvent(const WebMouseEvent& event) {
  TRACE_EVENT0("blink", "EventHandler::handleMouseLeaveEvent");
  DCHECK(event.GetType() == WebInputEvent::Type::kMouseLeave);

  Page* page = frame_->GetPage();
  if (page)
    page->GetChromeClient().ClearToolTip(*frame_);

  WebLinkPreviewTriggerer* triggerer =
      frame_->GetOrCreateLinkPreviewTriggerer();
  if (triggerer) {
    triggerer->MaybeChangedKeyEventModifier(WebInputEvent::kNoModifiers);
  }

  HandleMouseMoveOrLeaveEvent(event, Vector<WebMouseEvent>(),
                              Vector<WebMouseEvent>());
  pointer_event_manager_->RemoveLastMousePosition();
}

WebInputEventResult EventHandler::HandleMouseMoveOrLeaveEvent(
    const WebMouseEvent& mouse_event,
    const Vector<WebMouseEvent>& coalesced_events,
    const Vector<WebMouseEvent>& predicted_events,
    HitTestResult* hovered_node_result,
    HitTestLocation* hit_test_location) {
  DCHECK(frame_);
  DCHECK(frame_->View());
  DCHECK(mouse_event.GetType() == WebInputEvent::Type::kMouseMove ||
         mouse_event.GetType() == WebInputEvent::Type::kMouseLeave);
  mouse_event_manager_->SetLastKnownMousePosition(mouse_event);

  hover_timer_.Stop();
  cursor_update_timer_.Stop();

  mouse_event_manager_->HandleSvgPanIfNeeded(false);

  if (mouse_event.GetType() == WebInputEvent::Type::kMouseMove) {
    AnchorElementInteractionTracker* tracker =
        frame_->GetDocument()->GetAnchorElementInteractionTracker();
    if (tracker) {
      tracker->OnMouseMoveEvent(mouse_event);
    }
  }

  // Mouse states need to be reset when mouse move with no button down.
  // This is for popup/context_menu opened at mouse_down event and
  // mouse_release is not handled in page.
  // crbug.com/527582
  if (mouse_event.button == WebPointerProperties::Button::kNoButton &&
      !(mouse_event.GetModifiers() &
        WebInputEvent::Modifiers::kRelativeMotionEvent)) {
    mouse_event_manager_->ClearDragHeuristicState();
    capturing_mouse_events_element_ = nullptr;
    ReleaseMouseCaptureFromLocalRoot();

    // If the scrollbar still thinks it's being dragged, tell it to stop.
    // Can happen on Win if we lose focus (e.g. from Alt-Tab) mid-drag.
    if (last_scrollbar_under_mouse_ &&
        last_scrollbar_under_mouse_->PressedPart() != ScrollbarPart::kNoPart)
      last_scrollbar_under_mouse_->MouseUp(mouse_event);
  }

  if (RuntimeEnabledFeatures::MiddleClickAutoscrollEnabled()) {
    if (Page* page = frame_->GetPage()) {
      page->GetAutoscrollController().HandleMouseMoveForMiddleClickAutoscroll(
          frame_, mouse_event_manager_->LastKnownMouseScreenPosition(),
          mouse_event.button == WebPointerProperties::Button::kMiddle);
    }
  }

  if (frame_set_being_resized_) {
    return DispatchMousePointerEvent(
        WebInputEvent::Type::kPointerMove, frame_set_being_resized_.Get(),
        mouse_event, coalesced_events, predicted_events);
  }

  // Send events right to a scrollbar if the mouse is pressed.
  if (last_scrollbar_under_mouse_ && mouse_event_manager_->MousePressed()) {
    last_scrollbar_under_mouse_->MouseMoved(mouse_event);
    return WebInputEventResult::kHandledSystem;
  }

  // TODO(crbug.com/1519197): This crash key is set during the hit test if a
  // scroll corner is hit. It will be reported in the DumpWithoutCrashing that
  // occurs from GetComputedStyleFromScrollbar via the SelectCursor call below.
  // Clear it here to ensure we're using the value from this hit test if we do
  // end up calling DumpWithoutCrashing.
  base::debug::ClearCrashKeyString(CrashKeyForBug1519197());
  HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kMove;
  if (mouse_event_manager_->MousePressed()) {
    hit_type |= HitTestRequest::kActive;
  }

  // Treat any mouse move events as readonly if the user is currently touching
  // the screen.
  if (pointer_event_manager_->IsAnyTouchActive() &&
      mouse_event.GetType() == WebInputEvent::Type::kMouseMove) {
    hit_type |= HitTestRequest::kActive | HitTestRequest::kReadOnly;
  }
  HitTestRequest request(hit_type);
  HitTestLocation out_location((PhysicalOffset()));
  MouseEventWithHitTestResults mev = MouseEventWithHitTestResults(
      mouse_event, out_location, HitTestResult(request, out_location));

  // We don't want to do a hit-test in MouseLeave scenarios because there
  // might actually be some other frame above this one at the specified
  // coordinate. So we avoid the hit test but still clear the hover/active
  // state.
  if (mouse_event.GetType() == WebInputEvent::Type::kMouseLeave) {
    frame_->GetDocument()->UpdateHoverActiveState(request.Active(),
                                                  /*update_active_chain=*/false,
                                                  nullptr);
  } else {
    mev = GetMouseEventTarget(request, mouse_event);
  }

  if (hovered_node_result)
    *hovered_node_result = mev.GetHitTestResult();

  if (hit_test_location)
    *hit_test_location = mev.GetHitTestLocation();

  Scrollbar* scrollbar = nullptr;

  if (scroll_manager_->InResizeMode()) {
    scroll_manager_->Resize(mev.Event());
  } else {
    scrollbar = mev.GetScrollbar();

    UpdateLastScrollbarUnderMouse(scrollbar,
                                  !mouse_event_manager_->MousePressed());
  }

  WebInputEventResult event_result = WebInputEventResult::kNotHandled;
  bool is_remote_frame = false;
  LocalFrame* current_subframe =
      event_handling_util::GetTargetSubframe(mev, &is_remote_frame);

  // We want mouseouts to happen first, from the inside out.  First send a
  // move event to the last subframe so that it will fire mouseouts.
  // TODO(lanwei): figure out here if we should call HandleMouseLeaveEvent on a
  // mouse move event.
  if (last_mouse_move_event_subframe_ &&
      last_mouse_move_event_subframe_->Tree().IsDescendantOf(frame_) &&
      last_mouse_move_event_subframe_ != current_subframe) {
    WebMouseEvent event = mev.Event();
    event.SetType(WebInputEvent::Type::kMouseLeave);
    last_mouse_move_event_subframe_->GetEventHandler().HandleMouseLeaveEvent(
        event);
    last_mouse_move_event_subframe_->GetEventHandler()
        .mouse_event_manager_->SetLastMousePositionAsUnknown();
  }

  if (current_subframe) {
    // Update over/out state before passing the event to the subframe.
    pointer_event_manager_->SendMouseAndPointerBoundaryEvents(
        EffectiveMouseEventTargetElement(mev.InnerElement()), mev.Event());

    // Event dispatch in sendMouseAndPointerBoundaryEvents may have caused the
    // subframe of the target node to be detached from its LocalFrameView, in
    // which case the event should not be passed.
    if (current_subframe->View()) {
      event_result =
          PassMouseMoveEventToSubframe(mev, coalesced_events, predicted_events,
                                       current_subframe, hovered_node_result);
    }
  } else {
    if (scrollbar && !mouse_event_manager_->MousePressed()) {
      // Handle hover effects on platforms that support visual feedback on
      // scrollbar hovering.
      scrollbar->MouseMoved(mev.Event());
    }

    // Set Effective pan action before Pointer cursor is updated.
    const WebPointerEvent web_pointer_event(WebInputEvent::Type::kPointerMove,
                                            mev.Event().FlattenTransform());
    pointer_event_manager_->SendEffectivePanActionAtPointer(web_pointer_event,
                                                            mev.InnerNode());

    LocalFrameView* view = frame_->View();
    if (!is_remote_frame && view) {
      std::optional<ui::Cursor> optional_cursor =
          SelectCursor(mev.GetHitTestLocation(), mev.GetHitTestResult());
      if (optional_cursor.has_value()) {
        view->SetCursor(optional_cursor.value());
      }
    }
  }

  base::debug::ClearCrashKeyString(CrashKeyForBug1519197());
  last_mouse_move_event_subframe_ = current_subframe;

  if (event_result != WebInputEventResult::kNotHandled) {
    return event_result;
  }

  event_result = DispatchMousePointerEvent(WebInputEvent::Type::kPointerMove,
                                           mev.InnerElement(), mev.Event(),
                                           coalesced_events, predicted_events);
  // Since there is no default action for the mousemove event, MouseEventManager
  // handles drag for text selection even when js cancels the mouse move event.
  // https://w3c.github.io/uievents/#event-type-mousemove
  if (event_result == WebInputEventResult::kNotHandled ||
      event_result == WebInputEventResult::kHandledApplication) {
    event_result = mouse_event_manager_->HandleMouseDraggedEvent(mev);
  }

  return event_result;
}

WebInputEventResult EventHandler::HandleMouseReleaseEvent(
    const WebMouseEvent& mouse_event) {
  TRACE_EVENT0("blink", "EventHandler::handleMouseReleaseEvent");

  // For 4th/5th button in the mouse since Chrome does not yet send
  // button value to Blink but in some cases it does send the event.
  // This check is needed to suppress such an event (crbug.com/574959)
  if (mouse_event.button == WebPointerProperties::Button::kNoButton)
    return WebInputEventResult::kHandledSuppressed;

  if (!mouse_event.FromTouch())
    frame_->Selection().SetCaretBlinkingSuspended(false);

  if (RuntimeEnabledFeatures::MiddleClickAutoscrollEnabled()) {
    if (Page* page = frame_->GetPage()) {
      page->GetAutoscrollController()
          .HandleMouseReleaseForMiddleClickAutoscroll(
              frame_,
              mouse_event.button == WebPointerProperties::Button::kMiddle);
    }
  }

  mouse_event_manager_->ReleaseMousePress();
  mouse_event_manager_->SetLastKnownMousePosition(mouse_event);
  mouse_event_manager_->HandleSvgPanIfNeeded(true);

  if (frame_set_being_resized_) {
    WebInputEventResult result =
        mouse_event_manager_->SetElementUnderMouseAndDispatchMouseEvent(
            EffectiveMouseEventTargetElement(frame_set_being_resized_.Get()),
            event_type_names::kMouseup, mouse_event);
    // crbug.com/1053385 release mouse capture only if there are no more mouse
    // buttons depressed
    if (MouseEvent::WebInputEventModifiersToButtons(
            mouse_event.GetModifiers()) == 0)
      ReleaseMouseCaptureFromLocalRoot();
    return result;
  }

  if (last_scrollbar_under_mouse_) {
    mouse_event_manager_->InvalidateClick();
    last_scrollbar_under_mouse_->MouseUp(mouse_event);
    // crbug.com/1053385 release mouse capture only if there are no more mouse
    // buttons depressed
    if (MouseEvent::WebInputEventModifiersToButtons(
            mouse_event.GetModifiers()) == 0) {
      ReleaseMouseCaptureFromLocalRoot();
    }
    return DispatchMousePointerEvent(
        WebInputEvent::Type::kPointerUp,
        mouse_event_manager_->GetElementUnderMouse(), mouse_event,
        Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  }

  // Mouse events simulated from touch should not hit-test again.
  DCHECK(!mouse_event.FromTouch());
  HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kRelease;
  HitTestRequest request(hit_type);
  MouseEventWithHitTestResults mev = GetMouseEventTarget(request, mouse_event);
  LocalFrame* subframe = event_handling_util::GetTargetSubframe(mev);
  capturing_mouse_events_element_ = nullptr;
  if (subframe)
    return PassMouseReleaseEventToSubframe(mev, subframe);

  WebInputEventResult event_result = WebInputEventResult::kNotHandled;

  if (event_handling_util::ShouldDiscardEventTargetingFrame(mev.Event(),
                                                            *frame_)) {
    event_result = WebInputEventResult::kHandledSuppressed;
  } else {
    event_result = DispatchMousePointerEvent(
        WebInputEvent::Type::kPointerUp, mev.InnerElement(), mev.Event(),
        Vector<WebMouseEvent>(), Vector<WebMouseEvent>(),
        (GetSelectionController().HasExtendedSelection() &&
         IsSelectionOverLink(mev)));
  }
  scroll_manager_->ClearResizeScrollableArea(false);

  if (event_result == WebInputEventResult::kNotHandled)
    event_result = mouse_event_manager_->HandleMouseReleaseEvent(mev);

  mouse_event_manager_->HandleMouseReleaseEventUpdateStates();

  // crbug.com/1053385 release mouse capture only if there are no more mouse
  // buttons depressed
  if (MouseEvent::WebInputEventModifiersToButtons(mouse_event.GetModifiers()) ==
      0)
    ReleaseMouseCaptureFromLocalRoot();

  return event_result;
}

static LocalFrame* LocalFrameFromTargetNode(Node* target) {
  auto* html_frame_base_element = DynamicTo<HTMLFrameElementBase>(target);
  if (!html_frame_base_element)
    return nullptr;

  // Cross-process drag and drop is not yet supported.
  return DynamicTo<LocalFrame>(html_frame_base_element->ContentFrame());
}

WebInputEventResult EventHandler::UpdateDragAndDrop(
    const WebMouseEvent& event,
    DataTransfer* data_transfer) {
  WebInputEventResult event_result = WebInputEventResult::kNotHandled;

  if (!frame_->View())
    return event_result;

  HitTestRequest request(HitTestRequest::kReadOnly);
  MouseEventWithHitTestResults mev =
      event_handling_util::PerformMouseEventHitTest(frame_, request, event);

  // Drag events should never go to text nodes (following IE, and proper
  // mouseover/out dispatch)
  Node* new_target = mev.InnerNode();
  if (new_target && new_target->IsTextNode())
    new_target = FlatTreeTraversal::Parent(*new_target);

  if (AutoscrollController* controller =
          scroll_manager_->GetAutoscrollController()) {
    controller->UpdateDragAndDrop(new_target, event.PositionInRootFrame(),
                                  event.TimeStamp());
  }

  if (drag_target_ != new_target) {
    // FIXME: this ordering was explicitly chosen to match WinIE. However,
    // it is sometimes incorrect when dragging within subframes, as seen with
    // web_tests/fast/events/drag-in-frames.html.
    //
    // Moreover, this ordering conforms to section 7.9.4 of the HTML 5 spec.
    // <http://dev.w3.org/html5/spec/Overview.html#drag-and-drop-processing-model>.
    if (auto* target_frame = LocalFrameFromTargetNode(new_target)) {
      event_result = target_frame->GetEventHandler().UpdateDragAndDrop(
          event, data_transfer);
    } else if (new_target) {
      // As per section 7.9.4 of the HTML 5 spec., we must always fire a drag
      // event before firing a dragenter, dragleave, or dragover event.
      if (mouse_event_manager_->GetDragState().drag_src_) {
        // For now we don't care if event handler cancels default behavior,
        // since there is none.
        mouse_event_manager_->DispatchDragSrcEvent(event_type_names::kDrag,
                                                   event);
      }
      event_result = mouse_event_manager_->DispatchDragEvent(
          event_type_names::kDragenter, new_target, drag_target_, event,
          data_transfer);
    }

    if (auto* target_frame = LocalFrameFromTargetNode(drag_target_.Get())) {
      event_result = target_frame->GetEventHandler().UpdateDragAndDrop(
          event, data_transfer);
    } else if (drag_target_) {
      mouse_event_manager_->DispatchDragEvent(event_type_names::kDragleave,
                                              drag_target_.Get(), new_target,
                                              event, data_transfer);
    }

    if (new_target) {
      // We do not explicitly call m_mouseEventManager->dispatchDragEvent here
      // because it could ultimately result in the appearance that two dragover
      // events fired. So, we mark that we should only fire a dragover event on
      // the next call to this function.
      should_only_fire_drag_over_event_ = true;
    }
  } else {
    if (auto* target_frame = LocalFrameFromTargetNode(new_target)) {
      event_result = target_frame->GetEventHandler().UpdateDragAndDrop(
          event, data_transfer);
    } else if (new_target) {
      // Note, when dealing with sub-frames, we may need to fire only a dragover
      // event as a drag event may have been fired earlier.
      if (!should_only_fire_drag_over_event_ &&
          mouse_event_manager_->GetDragState().drag_src_) {
        // For now we don't care if event handler cancels default behavior,
        // since there is none.
        mouse_event_manager_->DispatchDragSrcEvent(event_type_names::kDrag,
                                                   event);
      }
      event_result = mouse_event_manager_->DispatchDragEvent(
          event_type_names::kDragover, new_target, nullptr, event,
          data_transfer);
      should_only_fire_drag_over_event_ = false;
    }
  }
  drag_target_ = new_target;

  return event_result;
}

void EventHandler::CancelDragAndDrop(const WebMouseEvent& event,
                                     DataTransfer* data_transfer) {
  if (auto* target_frame = LocalFrameFromTargetNode(drag_target_.Get())) {
    target_frame->GetEventHandler().CancelDragAndDrop(event, data_transfer);
  } else if (drag_target_.Get()) {
    if (mouse_event_manager_->GetDragState().drag_src_) {
      mouse_event_manager_->DispatchDragSrcEvent(event_type_names::kDrag,
                                                 event);
    }
    mouse_event_manager_->DispatchDragEvent(event_type_names::kDragleave,
                                            drag_target_.Get(), nullptr, event,
                                            data_transfer);
  }
  ClearDragState();
}

WebInputEventResult EventHandler::PerformDragAndDrop(
    const WebMouseEvent& event,
    DataTransfer* data_transfer) {
  WebInputEventResult result = WebInputEventResult::kNotHandled;
  if (auto* target_frame = LocalFrameFromTargetNode(drag_target_.Get())) {
    result = target_frame->GetEventHandler().PerformDragAndDrop(event,
                                                                data_transfer);
  } else if (drag_target_.Get()) {
    result = mouse_event_manager_->DispatchDragEvent(
        event_type_names::kDrop, drag_target_.Get(), nullptr, event,
        data_transfer);
  }
  ClearDragState();
  return result;
}

void EventHandler::ClearDragState() {
  scroll_manager_->StopAutoscroll();
  drag_target_ = nullptr;
  capturing_mouse_events_element_ = nullptr;
  ReleaseMouseCaptureFromLocalRoot();
  should_only_fire_drag_over_event_ = false;
}

void EventHandler::RecomputeMouseHoverStateIfNeeded() {
  mouse_event_manager_->RecomputeMouseHoverStateIfNeeded();
}

void EventHandler::MarkHoverStateDirty() {
  mouse_event_manager_->MarkHoverStateDirty();
}

Element* EventHandler::EffectiveMouseEventTargetElement(
    Element* target_element) {
  Element* new_element_under_mouse = target_element;
  if (pointer_event_manager_->GetMouseCaptureTarget())
    new_element_under_mouse = pointer_event_manager_->GetMouseCaptureTarget();
  return new_element_under_mouse;
}

void EventHandler::OnScrollbarDestroyed(const Scrollbar& scrollbar) {
  if (last_scrollbar_under_mouse_ == &scrollbar) {
    last_scrollbar_under_mouse_ = nullptr;
  }
}

Element* EventHandler::GetElementUnderMouse() {
  return mouse_event_manager_->GetElementUnderMouse();
}

Element* EventHandler::CurrentTouchDownElement() {
  return pointer_event_manager_->CurrentTouchDownElement();
}

void EventHandler::SetDelayedNavigationTaskHandle(TaskHandle task_handle) {
  delayed_navigation_task_handle_ = std::move(task_handle);
}

TaskHandle& EventHandler::GetDelayedNavigationTaskHandle() {
  return delayed_navigation_task_handle_;
}

bool EventHandler::IsPointerIdActiveOnFrame(PointerId pointer_id,
                                            LocalFrame* frame) const {
  DCHECK(frame_ == &frame_->LocalFrameRoot() || frame_ == frame);
  return pointer_event_manager_->IsPointerIdActiveOnFrame(pointer_id, frame);
}

bool EventHandler::RootFrameTrackedActivePointerInCurrentFrame(
    PointerId pointer_id) const {
  return frame_ != &frame_->LocalFrameRoot() &&
         frame_->LocalFrameRoot().GetEventHandler().IsPointerIdActiveOnFrame(
             pointer_id, frame_);
}

bool EventHandler::IsPointerEventActive(PointerId pointer_id) {
  return pointer_event_manager_->IsActive(pointer_id) ||
         RootFrameTrackedActivePointerInCurrentFrame(pointer_id);
}

LocalFrame* EventHandler::DetermineActivePointerTrackerFrame(
    PointerId pointer_id) const {
  // If pointer_id is active on current |frame_|, pointer states are in
  // current frame's PEM; otherwise, check if it's a touch-like pointer that
  // have its active states in the local frame root's PEM.
  if (IsPointerIdActiveOnFrame(pointer_id, frame_))
    return frame_.Get();
  if (RootFrameTrackedActivePointerInCurrentFrame(pointer_id))
    return &frame_->LocalFrameRoot();
  return nullptr;
}

void EventHandler::SetPointerCapture(PointerId pointer_id,
                                     Element* target,
                                     bool explicit_capture) {
  // TODO(crbug.com/591387): This functionality should be per page not per
  // frame.
  LocalFrame* tracking_frame = DetermineActivePointerTrackerFrame(pointer_id);

  bool captured =
      tracking_frame && tracking_frame->GetEventHandler()
                            .pointer_event_manager_->SetPointerCapture(
                                pointer_id, target, explicit_capture);

  if (captured && pointer_id == PointerEventFactory::kMouseId) {
    CaptureMouseEventsToWidget(true);
  }
}

void EventHandler::ReleasePointerCapture(PointerId pointer_id,
                                         Element* target) {
  LocalFrame* tracking_frame = DetermineActivePointerTrackerFrame(pointer_id);

  bool released =
      tracking_frame &&
      tracking_frame->GetEventHandler()
          .pointer_event_manager_->ReleasePointerCapture(pointer_id, target);

  if (released && pointer_id == PointerEventFactory::kMouseId) {
    CaptureMouseEventsToWidget(false);
  }
}

void EventHandler::ReleaseMousePointerCapture() {
  ReleaseMouseCaptureFromLocalRoot();
}

bool EventHandler::HasPointerCapture(PointerId pointer_id,
                                     const Element* target) const {
  if (LocalFrame* tracking_frame =
          DetermineActivePointerTrackerFrame(pointer_id)) {
    return tracking_frame->GetEventHandler()
        .pointer_event_manager_->HasPointerCapture(pointer_id, target);
  }
  return false;
}

void EventHandler::ElementRemoved(Element* target) {
  if (!target->GetDocument().StatePreservingAtomicMoveInProgress()) {
    pointer_event_manager_->ElementRemoved(target);
  }
  if (target)
    mouse_wheel_event_manager_->ElementRemoved(target);
}

void EventHandler::ResetMousePositionForPointerUnlock() {
  pointer_event_manager_->RemoveLastMousePosition();
}

bool EventHandler::LongTapShouldInvokeContextMenu() {
  return gesture_manager_->GestureContextMenuDeferred();
}

WebInputEventResult EventHandler::DispatchMousePointerEvent(
    const WebInputEvent::Type event_type,
    Element* target_element,
    const WebMouseEvent& mouse_event,
    const Vector<WebMouseEvent>& coalesced_events,
    const Vector<WebMouseEvent>& predicted_events,
    bool skip_click_dispatch) {
  const auto& event_result = pointer_event_manager_->SendMousePointerEvent(
      EffectiveMouseEventTargetElement(target_element), event_type, mouse_event,
      coalesced_events, predicted_events, skip_click_dispatch);
  return event_result;
}

WebInputEventResult EventHandler::HandleWheelEvent(
    const WebMouseWheelEvent& event) {
  return mouse_wheel_event_manager_->HandleWheelEvent(event);
}

// TODO(crbug.com/665924): This function bypasses all Handle*Event path.
// It should be using that flow instead of creating/sending events directly.
WebInputEventResult EventHandler::HandleTargetedMouseEvent(
    Element* target,
    const WebMouseEvent& event,
    const AtomicString& mouse_event_type,
    const Vector<WebMouseEvent>& coalesced_events,
    const Vector<WebMouseEvent>& predicted_events) {
  mouse_event_manager_->SetClickCount(event.click_count);
  return pointer_event_manager_->DirectDispatchMousePointerEvent(
      target, event, mouse_event_type, coalesced_events, predicted_events);
}

WebInputEventResult EventHandler::HandleGestureEvent(
    const WebGestureEvent& gesture_event) {
  // Propagation to inner frames is handled below this function.
  DCHECK_EQ(frame_, &frame_->LocalFrameRoot());
  DCHECK_NE(0, gesture_event.FrameScale());

  // Gesture scroll events are handled on the compositor thread.
  DCHECK(!gesture_event.IsScrollEvent());

  // Hit test across all frames and do touch adjustment as necessary for the
  // event type.
  GestureEventWithHitTestResults targeted_event =
      TargetGestureEvent(gesture_event);

  return HandleGestureEvent(targeted_event);
}

WebInputEventResult EventHandler::HandleGestureEvent(
    const GestureEventWithHitTestResults& targeted_event) {
  TRACE_EVENT0("input", "EventHandler::handleGestureEvent");
  if (!frame_->GetPage())
    return WebInputEventResult::kNotHandled;

  // Propagation to inner frames is handled below this function.
  DCHECK_EQ(frame_, &frame_->LocalFrameRoot());

  // Non-scrolling related gesture events do a single cross-frame hit-test and
  // jump directly to the inner most frame. This matches handleMousePressEvent
  // etc.
  DCHECK(!targeted_event.Event().IsScrollEvent());

  if (targeted_event.Event().GetType() ==
      WebInputEvent::Type::kGestureShowPress)
    last_show_press_timestamp_ = base::TimeTicks::Now();

  // Update mouseout/leave/over/enter events before jumping directly to the
  // inner most frame.
  if (targeted_event.Event().GetType() == WebInputEvent::Type::kGestureTap)
    UpdateGestureTargetNodeForMouseEvent(targeted_event);

  // Route to the correct frame.
  if (LocalFrame* inner_frame =
          targeted_event.GetHitTestResult().InnerNodeFrame())
    return inner_frame->GetEventHandler().HandleGestureEventInFrame(
        targeted_event);

  // No hit test result, handle in root instance. Perhaps we should just return
  // false instead?
  return gesture_manager_->HandleGestureEventInFrame(targeted_event);
}

WebInputEventResult EventHandler::HandleGestureEventInFrame(
    const GestureEventWithHitTestResults& targeted_event) {
  bool is_tap =
      targeted_event.Event().GetType() == WebInputEvent::Type::kGestureTap;
  if (is_tap && discarded_events_.tap_target != kInvalidDOMNodeId &&
      discarded_events_.tap_target ==
          targeted_event.InnerNode()->GetDomNodeId() &&
      targeted_event.Event().TimeStamp() - discarded_events_.tap_time <
          event_handling_util::kDiscardedEventMistakeInterval) {
    targeted_event.InnerNode()->GetDocument().CountUse(
        WebFeature::kInputEventToRecentlyMovedIframeMistakenlyDiscarded);
  }
  if (event_handling_util::ShouldDiscardEventTargetingFrame(
          targeted_event.Event(), *frame_)) {
    if (is_tap) {
      discarded_events_.tap_target = targeted_event.InnerNode()->GetDomNodeId();
      discarded_events_.tap_time = targeted_event.Event().TimeStamp();
    }
    return WebInputEventResult::kHandledSuppressed;
  }
  if (is_tap) {
    discarded_events_.tap_target = kInvalidDOMNodeId;
    discarded_events_.tap_time = base::TimeTicks();
  }
  return gesture_manager_->HandleGestureEventInFrame(targeted_event);
}

void EventHandler::SetMouseDownMayStartAutoscroll() {
  mouse_event_manager_->SetMouseDownMayStartAutoscroll();
}

bool EventHandler::ShouldApplyTouchAdjustment(
    const WebGestureEvent& event) const {
  if (event.primary_pointer_type == WebPointerProperties::PointerType::kPen)
    return false;

  return !event.TapAreaInRootFrame().IsEmpty();
}

void EventHandler::CacheTouchAdjustmentResult(uint32_t id,
                                              gfx::PointF adjusted_point) {
  touch_adjustment_result_.unique_event_id = id;
  touch_adjustment_result_.adjusted_point = adjusted_point;
}

bool EventHandler::GestureCorrespondsToAdjustedTouch(
    const WebGestureEvent& event) {
  // Gesture events start with a GestureTapDown. If GestureTapDown's unique id
  // matches stored adjusted touchstart event id, then we can use the stored
  // result for following gesture event.
  if (event.GetType() == WebInputEvent::Type::kGestureTapDown) {
    should_use_touch_event_adjusted_point_ =
        (event.unique_touch_event_id != 0 &&
         event.unique_touch_event_id ==
             touch_adjustment_result_.unique_event_id);
  }

  // Check if the adjusted point is in the gesture event tap rect.
  // If not, should not use this touch point in following events.
  if (should_use_touch_event_adjusted_point_) {
    gfx::SizeF size = event.TapAreaInRootFrame();
    gfx::RectF tap_rect(
        event.PositionInRootFrame() -
            gfx::Vector2dF(size.width() * 0.5, size.height() * 0.5),
        size);
    should_use_touch_event_adjusted_point_ =
        tap_rect.InclusiveContains(touch_adjustment_result_.adjusted_point);
  }

  return should_use_touch_event_adjusted_point_;
}

bool EventHandler::BestNodeForHitTestResult(
    TouchAdjustmentCandidateType candidate_type,
    const HitTestLocation& location,
    const HitTestResult& result,
    gfx::Point& adjusted_point,
    Node*& adjusted_node) {
  TRACE_EVENT0("input", "EventHandler::BestNodeForHitTestResult");
  CHECK(location.IsRectBasedTest());

  // If the touch is over a scrollbar or a resizer, we don't adjust the touch
  // point.  This is because touch adjustment only takes into account DOM nodes
  // so a touch over a scrollbar or a resizer would be adjusted towards a nearby
  // DOM node, making the scrollbar/resizer unusable.
  //
  // Context-menu hittests are excluded from this consideration because a
  // right-click/long-press doesn't drag the scrollbar therefore prefers DOM
  // nodes with relevant contextmenu properties.
  if (candidate_type != TouchAdjustmentCandidateType::kContextMenu &&
      (result.GetScrollbar() || result.IsOverResizer())) {
    return false;
  }

  gfx::Point touch_hotspot =
      frame_->View()->ConvertToRootFrame(location.RoundedPoint());
  gfx::Rect touch_rect =
      frame_->View()->ConvertToRootFrame(location.ToEnclosingRect());

  if (touch_rect.IsEmpty()) {
    return false;
  }

  CHECK(location.BoundingBox().Contains(location.Point()) ||
        (location.BoundingBox().Right() == LayoutUnit::Max() &&
         location.Point().left == LayoutUnit::Max()) ||
        (location.BoundingBox().Bottom() == LayoutUnit::Max() &&
         location.Point().top == LayoutUnit::Max()));

  HeapVector<Member<Node>, 11> nodes(result.ListBasedTestResult());

  return FindBestTouchAdjustmentCandidate(candidate_type, adjusted_node,
                                          adjusted_point, touch_hotspot,
                                          touch_rect, nodes);
}

// Update the hover and active state across all frames.  This logic is
// different than the mouse case because mice send MouseLeave events to frames
// as they're exited.  With gestures or manual applications, a single event
// conceptually both 'leaves' whatever frame currently had hover and enters a
// new frame so we need to update state in the old frame chain as well.
void EventHandler::UpdateCrossFrameHoverActiveState(bool is_active,
                                                    Element* inner_element) {
  DCHECK_EQ(frame_, &frame_->LocalFrameRoot());

  HeapVector<Member<LocalFrame>> new_hover_frame_chain;
  LocalFrame* new_hover_frame_in_document =
      inner_element ? inner_element->GetDocument().GetFrame() : nullptr;
  // Insert the ancestors of the frame having the new hovered element to the
  // frame chain.  The frame chain doesn't include the main frame to avoid the
  // redundant work that cleans the hover state because the hover state for the
  // main frame is updated by calling Document::UpdateHoverActiveState.
  while (new_hover_frame_in_document && new_hover_frame_in_document != frame_) {
    new_hover_frame_chain.push_back(new_hover_frame_in_document);
    Frame* parent_frame = new_hover_frame_in_document->Tree().Parent();
    new_hover_frame_in_document = DynamicTo<LocalFrame>(parent_frame);
  }

  Element* old_hover_element_in_cur_doc = frame_->GetDocument()->HoverElement();
  Element* new_innermost_hover_element = inner_element;

  if (new_innermost_hover_element != old_hover_element_in_cur_doc) {
    wtf_size_t index_frame_chain = new_hover_frame_chain.size();

    // Clear the hover state on any frames which are no longer in the frame
    // chain of the hovered element.
    while (old_hover_element_in_cur_doc &&
           old_hover_element_in_cur_doc->IsFrameOwnerElement()) {
      LocalFrame* new_hover_frame = nullptr;
      // If we can't get the frame from the new hover frame chain,
      // the newHoverFrame will be null and the old hover state will be cleared.
      if (index_frame_chain > 0)
        new_hover_frame = new_hover_frame_chain[--index_frame_chain];

      auto* owner = To<HTMLFrameOwnerElement>(old_hover_element_in_cur_doc);
      LocalFrame* old_hover_frame =
          DynamicTo<LocalFrame>(owner->ContentFrame());
      if (!old_hover_frame)
        break;

      Document* doc = old_hover_frame->GetDocument();
      if (!doc)
        break;

      old_hover_element_in_cur_doc = doc->HoverElement();
      // If the old hovered frame is different from the new hovered frame.
      // we should clear the old hovered element from the old hovered frame.
      if (new_hover_frame != old_hover_frame) {
        doc->UpdateHoverActiveState(is_active,
                                    /*update_active_chain=*/true, nullptr);
      }
    }
  }

  // Recursively set the new active/hover states on every frame in the chain of
  // innerElement.
  frame_->GetDocument()->UpdateHoverActiveState(is_active,
                                                /*update_active_chain=*/true,
                                                inner_element);
}

// Update the mouseover/mouseenter/mouseout/mouseleave events across all frames
// for this gesture, before passing the targeted gesture event directly to a hit
// frame.
void EventHandler::UpdateGestureTargetNodeForMouseEvent(
    const GestureEventWithHitTestResults& targeted_event) {
  DCHECK_EQ(frame_, &frame_->LocalFrameRoot());

  // Behaviour of this function is as follows:
  // - Create the chain of all entered frames.
  // - Compare the last frame chain under the gesture to newly entered frame
  //   chain from the main frame one by one.
  // - If the last frame doesn't match with the entered frame, then create the
  //   chain of exited frames from the last frame chain.
  // - Dispatch mouseout/mouseleave events of the exited frames from the inside
  //   out.
  // - Dispatch mouseover/mouseenter events of the entered frames into the
  //   inside.

  // Insert the ancestors of the frame having the new target node to the entered
  // frame chain.
  HeapVector<Member<LocalFrame>, 2> entered_frame_chain;
  LocalFrame* entered_frame_in_document =
      targeted_event.GetHitTestResult().InnerNodeFrame();
  while (entered_frame_in_document) {
    entered_frame_chain.push_back(entered_frame_in_document);
    Frame* parent_frame = entered_frame_in_document->Tree().Parent();
    entered_frame_in_document = DynamicTo<LocalFrame>(parent_frame);
  }

  wtf_size_t index_entered_frame_chain = entered_frame_chain.size();
  LocalFrame* exited_frame_in_document = frame_;
  HeapVector<Member<LocalFrame>, 2> exited_frame_chain;
  // Insert the frame from the disagreement between last frames and entered
  // frames.
  while (exited_frame_in_document) {
    Node* last_node_under_tap =
        exited_frame_in_document->GetEventHandler()
            .mouse_event_manager_->GetElementUnderMouse();
    if (!last_node_under_tap)
      break;

    LocalFrame* next_exited_frame_in_document = nullptr;
    if (auto* owner = DynamicTo<HTMLFrameOwnerElement>(last_node_under_tap)) {
      next_exited_frame_in_document =
          DynamicTo<LocalFrame>(owner->ContentFrame());
    }

    if (exited_frame_chain.size() > 0) {
      exited_frame_chain.push_back(exited_frame_in_document);
    } else {
      LocalFrame* last_entered_frame_in_document =
          index_entered_frame_chain
              ? entered_frame_chain[index_entered_frame_chain - 1]
              : nullptr;
      if (exited_frame_in_document != last_entered_frame_in_document)
        exited_frame_chain.push_back(exited_frame_in_document);
      else if (next_exited_frame_in_document && index_entered_frame_chain)
        --index_entered_frame_chain;
    }
    exited_frame_in_document = next_exited_frame_in_document;
  }

  const WebGestureEvent& gesture_event = targeted_event.Event();
  unsigned modifiers = gesture_event.GetModifiers();
  WebMouseEvent fake_mouse_move(
      WebInputEvent::Type::kMouseMove, gesture_event,
      WebPointerProperties::Button::kNoButton,
      /* clickCount */ 0,
      modifiers | WebInputEvent::Modifiers::kIsCompatibilityEventForTouch,
      gesture_event.TimeStamp());

  // Update the mouseout/mouseleave event
  wtf_size_t index_exited_frame_chain = exited_frame_chain.size();
  while (index_exited_frame_chain) {
    LocalFrame* leave_frame = exited_frame_chain[--index_exited_frame_chain];
    leave_frame->GetEventHandler().mouse_event_manager_->SetElementUnderMouse(
        EffectiveMouseEventTargetElement(nullptr), fake_mouse_move);
  }

  // update the mouseover/mouseenter event
  while (index_entered_frame_chain) {
    Frame* parent_frame =
        entered_frame_chain[--index_entered_frame_chain]->Tree().Parent();
    if (auto* parent_local_frame = DynamicTo<LocalFrame>(parent_frame)) {
      parent_local_frame->GetEventHandler()
          .mouse_event_manager_->SetElementUnderMouse(
              EffectiveMouseEventTargetElement(To<HTMLFrameOwnerElement>(
                  entered_frame_chain[index_entered_frame_chain]->Owner())),
              fake_mouse_move);
    }
  }
}

GestureEventWithHitTestResults EventHandler::TargetGestureEvent(
    const WebGestureEvent& gesture_event,
    bool read_only) {
  TRACE_EVENT0("input", "EventHandler::targetGestureEvent");

  DCHECK_EQ(frame_, &frame_->LocalFrameRoot());
  // Scrolling events get hit tested per frame (like wheel events do).
  DCHECK(!gesture_event.IsScrollEvent());

  HitTestRequest::HitTestRequestType hit_type =
      gesture_manager_->GetHitTypeForGestureType(gesture_event.GetType());
  base::TimeDelta active_interval;
  bool should_keep_active_for_min_interval = false;
  if (read_only) {
    hit_type |= HitTestRequest::kReadOnly;
  } else if (gesture_event.GetType() == WebInputEvent::Type::kGestureTap &&
             last_show_press_timestamp_) {
    // If the Tap is received very shortly after ShowPress, we want to
    // delay clearing of the active state so that it's visible to the user
    // for at least a couple of frames.
    active_interval =
        base::TimeTicks::Now() - last_show_press_timestamp_.value();
    should_keep_active_for_min_interval =
        active_interval < kMinimumActiveInterval;
    if (should_keep_active_for_min_interval)
      hit_type |= HitTestRequest::kReadOnly;
  }

  GestureEventWithHitTestResults event_with_hit_test_results =
      HitTestResultForGestureEvent(gesture_event, hit_type);
  // Now apply hover/active state to the final target.
  HitTestRequest request(hit_type | HitTestRequest::kAllowChildFrameContent);
  if (!request.ReadOnly()) {
    UpdateCrossFrameHoverActiveState(
        request.Active(),
        event_with_hit_test_results.GetHitTestResult().InnerElement());
  }

  if (should_keep_active_for_min_interval) {
    last_deferred_tap_element_ =
        event_with_hit_test_results.GetHitTestResult().InnerElement();
    // TODO(https://crbug.com/668758): Use a normal BeginFrame update for this.
    active_interval_timer_.StartOneShot(
        kMinimumActiveInterval - active_interval, FROM_HERE);
  }

  return event_with_hit_test_results;
}

GestureEventWithHitTestResults EventHandler::HitTestResultForGestureEvent(
    const WebGestureEvent& gesture_event,
    HitTestRequest::HitTestRequestType hit_type) {
  // Perform the rect-based hit-test (or point-based if adjustment is
  // disabled). Note that we don't yet apply hover/active state here because
  // we need to resolve touch adjustment first so that we apply hover/active
  // it to the final adjusted node.
  hit_type |= HitTestRequest::kReadOnly;
  WebGestureEvent adjusted_event = gesture_event;
  PhysicalSize hit_rect_size;
  if (ShouldApplyTouchAdjustment(gesture_event)) {
    // If gesture_event unique id matches the stored touch event result, do
    // point-base hit test. Otherwise add padding and do rect-based hit test.
    if (GestureCorrespondsToAdjustedTouch(gesture_event)) {
      adjusted_event.ApplyTouchAdjustment(
          touch_adjustment_result_.adjusted_point);
    } else {
      gfx::SizeF tap_area = adjusted_event.TapAreaInRootFrame();
      hit_rect_size = GetHitTestRectForAdjustment(
          *frame_, PhysicalSize(LayoutUnit(tap_area.width()),
                                LayoutUnit(tap_area.height())));
      if (!hit_rect_size.IsEmpty())
        hit_type |= HitTestRequest::kListBased;
    }
  }

  HitTestLocation location;
  LocalFrame& root_frame = frame_->LocalFrameRoot();
  HitTestResult hit_test_result;
  if (hit_rect_size.IsEmpty()) {
    location = HitTestLocation(adjusted_event.PositionInRootFrame());
    hit_test_result = root_frame.GetEventHandler().HitTestResultAtLocation(
        location, hit_type);
  } else {
    PhysicalOffset top_left =
        PhysicalOffset::FromPointFRound(adjusted_event.PositionInRootFrame());
    top_left -= PhysicalOffset(LayoutUnit(hit_rect_size.width * 0.5f),
                               LayoutUnit(hit_rect_size.height * 0.5f));
    location = HitTestLocation(PhysicalRect(top_left, hit_rect_size));
    hit_test_result = root_frame.GetEventHandler().HitTestResultAtLocation(
        location, hit_type);

    // Adjust the location of the gesture to the most likely nearby node, as
    // appropriate for the type of event.
    ApplyTouchAdjustment(&adjusted_event, location, hit_test_result);
    // Do a new hit-test at the (adjusted) gesture coordinates. This is
    // necessary because rect-based hit testing and touch adjustment sometimes
    // return a different node than what a point-based hit test would return for
    // the same point.
    // FIXME: Fix touch adjustment to avoid the need for a redundant hit test.
    // http://crbug.com/398914
    LocalFrame* hit_frame = hit_test_result.InnerNodeFrame();
    if (!hit_frame)
      hit_frame = frame_;
    location = HitTestLocation(adjusted_event.PositionInRootFrame());
    hit_test_result = root_frame.GetEventHandler().HitTestResultAtLocation(
        location,
        (hit_type | HitTestRequest::kReadOnly) & ~HitTestRequest::kListBased);
  }

  // If we did a rect-based hit test it must be resolved to the best single node
  // by now to ensure consumers don't accidentally use one of the other
  // candidates.
  DCHECK(!location.IsRectBasedTest());

  return GestureEventWithHitTestResults(adjusted_event, location,
                                        hit_test_result);
}

void EventHandler::ApplyTouchAdjustment(WebGestureEvent* gesture_event,
                                        HitTestLocation& location,
                                        HitTestResult& hit_test_result) {
  TouchAdjustmentCandidateType touch_adjustment_candiate_type =
      TouchAdjustmentCandidateType::kClickable;
  switch (gesture_event->GetType()) {
    case WebInputEvent::Type::kGestureTap:
    case WebInputEvent::Type::kGestureTapUnconfirmed:
    case WebInputEvent::Type::kGestureTapDown:
    case WebInputEvent::Type::kGestureShowPress:
      break;
    case WebInputEvent::Type::kGestureShortPress:
    case WebInputEvent::Type::kGestureLongPress:
    case WebInputEvent::Type::kGestureLongTap:
    case WebInputEvent::Type::kGestureTwoFingerTap:
      touch_adjustment_candiate_type =
          TouchAdjustmentCandidateType::kContextMenu;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  Node* adjusted_node = nullptr;
  gfx::Point adjusted_point;
  if (BestNodeForHitTestResult(touch_adjustment_candiate_type, location,
                               hit_test_result, adjusted_point,
                               adjusted_node)) {
    // Update the hit-test result to be a point-based result instead of a
    // rect-based result.
    PhysicalOffset point(frame_->View()->ConvertFromRootFrame(adjusted_point));
    DCHECK(location.ContainsPoint(gfx::PointF(point)));
    DCHECK(location.IsRectBasedTest());
    location = hit_test_result.ResolveRectBasedTest(adjusted_node, point);
    gesture_event->ApplyTouchAdjustment(
        gfx::PointF(adjusted_point.x(), adjusted_point.y()));
  }
}

WebInputEventResult EventHandler::SendContextMenuEvent(
    const WebMouseEvent& event,
    Element* override_target_element) {
  LocalFrameView* v = frame_->View();
  if (!v)
    return WebInputEventResult::kNotHandled;

  // Clear mouse press state to avoid initiating a drag while context menu is
  // up.
  mouse_event_manager_->ReleaseMousePress();
  if (last_scrollbar_under_mouse_)
    last_scrollbar_under_mouse_->MouseUp(event);

  PhysicalOffset position_in_contents(v->ConvertFromRootFrame(
      gfx::ToFlooredPoint(event.PositionInRootFrame())));
  HitTestRequest request(HitTestRequest::kActive);
  MouseEventWithHitTestResults mev =
      frame_->GetDocument()->PerformMouseEventHitTest(
          request, position_in_contents, event);
  // Since |Document::performMouseEventHitTest()| modifies layout tree for
  // setting hover element, we need to update layout tree for requirement of
  // |SelectionController::sendContextMenuEvent()|.
  frame_->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kContextMenu);

  Element* target_element =
      override_target_element ? override_target_element : mev.InnerElement();
  return mouse_event_manager_->DispatchMouseEvent(
      EffectiveMouseEventTargetElement(target_element),
      event_type_names::kContextmenu, event, nullptr, nullptr, false, event.id,
      PointerEventFactory::PointerTypeNameForWebPointPointerType(
          event.pointer_type));
}

static bool ShouldShowContextMenuAtSelection(const FrameSelection& selection) {
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  selection.GetDocument().UpdateStyleAndLayout(
      DocumentUpdateReason::kContextMenu);

  const VisibleSelection& visible_selection =
      selection.ComputeVisibleSelectionInDOMTree();
  if (!visible_selection.IsRange() && !visible_selection.RootEditableElement())
    return false;
  return selection.SelectionHasFocus();
}

WebInputEventResult EventHandler::ShowNonLocatedContextMenu(
    Element* override_target_element,
    WebMenuSourceType source_type) {
  LocalFrameView* view = frame_->View();
  if (!view)
    return WebInputEventResult::kNotHandled;

  Document* doc = frame_->GetDocument();
  if (!doc)
    return WebInputEventResult::kNotHandled;

  static const int kContextMenuMargin = 1;

  gfx::Point location_in_root_frame;

  Element* focused_element =
      override_target_element ? override_target_element : doc->FocusedElement();
  FrameSelection& selection = frame_->Selection();
  VisualViewport& visual_viewport = frame_->GetPage()->GetVisualViewport();

  if (!override_target_element && ShouldShowContextMenuAtSelection(selection)) {
    DCHECK(!doc->NeedsLayoutTreeUpdate());

    // Enclose the selection rect fully between the handles. If the handles are
    // on the same line, the selection rect is empty.
    const SelectionInDOMTree& visible_selection =
        selection.ComputeVisibleSelectionInDOMTree().AsSelection();
    const PositionWithAffinity start_position(
        visible_selection.ComputeStartPosition(), visible_selection.Affinity());
    const gfx::Point start_point =
        GetMiddleSelectionCaretOfPosition(start_position);
    const PositionWithAffinity end_position(
        visible_selection.ComputeEndPosition(), visible_selection.Affinity());
    const gfx::Point end_point =
        GetMiddleSelectionCaretOfPosition(end_position);

    int left = std::min(start_point.x(), end_point.x());
    int top = std::min(start_point.y(), end_point.y());
    int right = std::max(start_point.x(), end_point.x());
    int bottom = std::max(start_point.y(), end_point.y());

    // If selection is a caret and is inside an anchor element, then set that
    // as the "focused" element so we can show "open link" option in context
    // menu.
    if (visible_selection.IsCaret()) {
      Element* anchor_element =
          EnclosingAnchorElement(visible_selection.ComputeStartPosition());
      if (anchor_element)
        focused_element = anchor_element;
    }
    // Intersect the selection rect and the visible bounds of focused_element.
    if (focused_element) {
      gfx::Rect clipped_rect = view->ConvertFromRootFrame(
          GetFocusedElementRectForNonLocatedContextMenu(focused_element));
      left = std::max(clipped_rect.x(), left);
      top = std::max(clipped_rect.y(), top);
      right = std::min(clipped_rect.right(), right);
      bottom = std::min(clipped_rect.bottom(), bottom);
    }
    gfx::Rect selection_rect = gfx::Rect(left, top, right - left, bottom - top);

    if (ContainsEvenAtEdge(selection_rect, start_point)) {
      location_in_root_frame = view->ConvertToRootFrame(start_point);
    } else if (ContainsEvenAtEdge(selection_rect, end_point)) {
      location_in_root_frame = view->ConvertToRootFrame(end_point);
    } else {
      location_in_root_frame =
          view->ConvertToRootFrame(selection_rect.CenterPoint());
    }
  } else if (focused_element) {
    gfx::Rect clipped_rect =
        GetFocusedElementRectForNonLocatedContextMenu(focused_element);
    location_in_root_frame = clipped_rect.CenterPoint();
  } else {
    // TODO(crbug.com/1274078): Should this use ScrollPosition()?
    location_in_root_frame =
        gfx::Point(visual_viewport.GetScrollOffset().x() + kContextMenuMargin,
                   visual_viewport.GetScrollOffset().y() + kContextMenuMargin);
  }

  frame_->View()->SetCursor(PointerCursor());
  gfx::Point global_position =
      view->GetChromeClient()
          ->LocalRootToScreenDIPs(
              gfx::Rect(location_in_root_frame, gfx::Size()), frame_->View())
          .origin();

  // Use the focused node as the target for hover and active.
  HitTestRequest request(HitTestRequest::kActive);
  HitTestLocation location(location_in_root_frame);
  HitTestResult result(request, location);
  result.SetInnerNode(focused_element ? static_cast<Node*>(focused_element)
                                      : doc);
  doc->UpdateHoverActiveState(request.Active(), !request.Move(),
                              result.InnerElement());

  // The contextmenu event is a mouse event even when invoked using the
  // keyboard.  This is required for web compatibility.
  WebInputEvent::Type event_type = WebInputEvent::Type::kMouseDown;
  if (frame_->GetSettings() &&
      frame_->GetSettings()->GetShowContextMenuOnMouseUp())
    event_type = WebInputEvent::Type::kMouseUp;

  WebMouseEvent mouse_event(
      event_type,
      gfx::PointF(location_in_root_frame.x(), location_in_root_frame.y()),
      gfx::PointF(global_position.x(), global_position.y()),
      WebPointerProperties::Button::kNoButton, /* clickCount */ 0,
      WebInputEvent::kNoModifiers, base::TimeTicks::Now(), source_type);
  mouse_event.id = PointerEventFactory::kMouseId;

  // TODO(dtapuska): Transition the mouseEvent to be created really in viewport
  // coordinates instead of root frame coordinates.
  mouse_event.SetFrameScale(1);

  return SendContextMenuEvent(mouse_event, focused_element);
}

gfx::Rect EventHandler::GetFocusedElementRectForNonLocatedContextMenu(
    Element* focused_element) {
  gfx::Rect visible_rect = focused_element->VisibleBoundsInLocalRoot();

  VisualViewport& visual_viewport = frame_->GetPage()->GetVisualViewport();

  // TODO(bokan): This method may not work as expected when the local root
  // isn't the main frame since the result won't be transformed and clipped by
  // the visual viewport (which is accessible only from the outermost main
  // frame).
  if (frame_->LocalFrameRoot().IsOutermostMainFrame()) {
    visible_rect = visual_viewport.RootFrameToViewport(visible_rect);
    visible_rect.Intersect(gfx::Rect(visual_viewport.Size()));
  }

  gfx::Rect clipped_rect = visible_rect;
  // The bounding rect of multiline elements may include points that are
  // not within the element. Intersect the clipped rect with the first
  // outline rect to ensure that the selection rect only includes visible
  // points within the focused element.
  Vector<gfx::Rect> outline_rects = focused_element->OutlineRectsInWidget();
  if (outline_rects.size() > 1)
    clipped_rect.Intersect(outline_rects[0]);

  return visual_viewport.ViewportToRootFrame(clipped_rect);
}

void EventHandler::ScheduleHoverStateUpdate() {
  // TODO(https://crbug.com/668758): Use a normal BeginFrame update for this.
  if (!hover_timer_.IsActive() &&
      !mouse_event_manager_->IsMousePositionUnknown())
    hover_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void EventHandler::ScheduleCursorUpdate() {
  // We only want one timer for the page, rather than each frame having it's own
  // timer competing which eachother (since there's only one mouse cursor).
  DCHECK_EQ(frame_, &frame_->LocalFrameRoot());

  // TODO(https://crbug.com/668758): Use a normal BeginFrame update for this.
  if (!cursor_update_timer_.IsActive())
    cursor_update_timer_.StartOneShot(kCursorUpdateInterval, FROM_HERE);
}

bool EventHandler::CursorUpdatePending() {
  return cursor_update_timer_.IsActive();
}

bool EventHandler::IsHandlingKeyEvent() const {
  return keyboard_event_manager_->is_handling_key_event();
}

void EventHandler::SetResizingFrameSet(HTMLFrameSetElement* frame_set) {
  CaptureMouseEventsToWidget(true);
  frame_set_being_resized_ = frame_set;
}

void EventHandler::ResizeScrollableAreaDestroyed() {
  scroll_manager_->ClearResizeScrollableArea(true);
}

void EventHandler::HoverTimerFired(TimerBase*) {
  TRACE_EVENT0("input", "EventHandler::hoverTimerFired");

  DCHECK(frame_);
  DCHECK(frame_->GetDocument());

  if (auto* layout_object = frame_->ContentLayoutObject()) {
    if (LocalFrameView* view = frame_->View()) {
      HitTestRequest request(HitTestRequest::kMove);
      HitTestLocation location(view->ViewportToFrame(
          mouse_event_manager_->LastKnownMousePositionInViewport()));
      HitTestResult result(request, location);
      layout_object->HitTest(location, result);
      frame_->GetDocument()->UpdateHoverActiveState(
          request.Active(), !request.Move(), result.InnerElement());
    }
  }
}

void EventHandler::ActiveIntervalTimerFired(TimerBase*) {
  TRACE_EVENT0("input", "EventHandler::activeIntervalTimerFired");

  if (frame_ && frame_->GetDocument() && last_deferred_tap_element_) {
    // FIXME: Enable condition when http://crbug.com/226842 lands
    // m_lastDeferredTapElement.get() == m_frame->document()->activeElement()
    HitTestRequest request(HitTestRequest::kTouchEvent |
                           HitTestRequest::kRelease);
    frame_->GetDocument()->UpdateHoverActiveState(
        request.Active(), !request.Move(), last_deferred_tap_element_.Get());
  }
  last_deferred_tap_element_ = nullptr;
}

void EventHandler::NotifyElementActivated() {
  // Since another element has been set to active, stop current timer and clear
  // reference.
  active_interval_timer_.Stop();
  last_deferred_tap_element_ = nullptr;
}

bool EventHandler::HandleAccessKey(const WebKeyboardEvent& evt) {
  return keyboard_event_manager_->HandleAccessKey(evt);
}

WebInputEventResult EventHandler::KeyEvent(
    const WebKeyboardEvent& initial_key_event) {
  return keyboard_event_manager_->KeyEvent(initial_key_event);
}

void EventHandler::DefaultKeyboardEventHandler(KeyboardEvent* event) {
  keyboard_event_manager_->DefaultKeyboardEventHandler(
      event, mouse_event_manager_->MousePressNode());
}

void EventHandler::DragSourceEndedAt(
    const WebMouseEvent& event,
    ui::mojom::blink::DragOperation operation) {
  // Asides from routing the event to the correct frame, the hit test is also an
  // opportunity for Layer to update the :hover and :active pseudoclasses.
  HitTestRequest request(HitTestRequest::kRelease);
  MouseEventWithHitTestResults mev =
      event_handling_util::PerformMouseEventHitTest(frame_, request, event);

  if (auto* target_frame = LocalFrameFromTargetNode(mev.InnerNode())) {
    target_frame->GetEventHandler().DragSourceEndedAt(event, operation);
    return;
  }

  mouse_event_manager_->DragSourceEndedAt(event, operation);

  if (frame_->GetSettings() &&
      frame_->GetSettings()->GetTouchDragDropEnabled() &&
      frame_->GetSettings()->GetTouchDragEndContextMenu()) {
    gesture_manager_->SendContextMenuEventTouchDragEnd(event);
  }
}

void EventHandler::UpdateDragStateAfterEditDragIfNeeded(
    Element* root_editable_element) {
  // If inserting the dragged contents removed the drag source, we still want to
  // fire dragend at the root editble element.
  if (mouse_event_manager_->GetDragState().drag_src_ &&
      !mouse_event_manager_->GetDragState().drag_src_->isConnected())
    mouse_event_manager_->GetDragState().drag_src_ = root_editable_element;
}

bool EventHandler::HandleTextInputEvent(const String& text,
                                        Event* underlying_event,
                                        TextEventInputType input_type) {
  // Platforms should differentiate real commands like selectAll from text input
  // in disguise (like insertNewline), and avoid dispatching text input events
  // from keydown default handlers.
  auto* keyboard_event = DynamicTo<KeyboardEvent>(underlying_event);
  DCHECK(!keyboard_event ||
         keyboard_event->type() == event_type_names::kKeypress);

  if (!frame_)
    return false;

  EventTarget* target;
  if (underlying_event)
    target = underlying_event->target();
  else
    target = EventTargetNodeForDocument(frame_->GetDocument());
  if (!target)
    return false;

  TextEvent* event = TextEvent::Create(frame_->DomWindow(), text, input_type);
  event->SetUnderlyingEvent(underlying_event);

  target->DispatchEvent(*event);
  return event->DefaultHandled() || event->defaultPrevented();
}

void EventHandler::DefaultTextInputEventHandler(TextEvent* event) {
  if (frame_->GetEditor().HandleTextEvent(event))
    event->SetDefaultHandled();
}

void EventHandler::CapsLockStateMayHaveChanged() {
  keyboard_event_manager_->CapsLockStateMayHaveChanged();
}

bool EventHandler::PassMousePressEventToScrollbar(
    MouseEventWithHitTestResults& mev) {
  // Do not pass the mouse press to scrollbar if scrollbar pressed. If the
  // user's left button is down, then the cursor moves outside the scrollbar
  // and presses the middle button , we should not clear
  // last_scrollbar_under_mouse_.
  if (last_scrollbar_under_mouse_ &&
      last_scrollbar_under_mouse_->PressedPart() != ScrollbarPart::kNoPart) {
    return false;
  }

  Scrollbar* scrollbar = mev.GetScrollbar();
  UpdateLastScrollbarUnderMouse(scrollbar, true);

  if (!scrollbar || !scrollbar->Enabled())
    return false;
  scrollbar->MouseDown(mev.Event());
  if (scrollbar->PressedPart() == ScrollbarPart::kThumbPart)
    CaptureMouseEventsToWidget(true);
  return true;
}

// If scrollbar (under mouse) is different from last, send a mouse exited. Set
// last to scrollbar if setLast is true; else set last to 0.
void EventHandler::UpdateLastScrollbarUnderMouse(Scrollbar* scrollbar,
                                                 bool set_last) {
  if (last_scrollbar_under_mouse_ != scrollbar) {
    // Send mouse exited to the old scrollbar.
    if (last_scrollbar_under_mouse_)
      last_scrollbar_under_mouse_->MouseExited();

    // Send mouse entered if we're setting a new scrollbar.
    if (scrollbar && set_last)
      scrollbar->MouseEntered();

    last_scrollbar_under_mouse_ = set_last ? scrollbar : nullptr;
  }
}

WebInputEventResult EventHandler::PassMousePressEventToSubframe(
    MouseEventWithHitTestResults& mev,
    LocalFrame* subframe) {
  GetSelectionController().PassMousePressEventToSubframe(mev);
  WebInputEventResult result =
      subframe->GetEventHandler().HandleMousePressEvent(mev.Event());
  if (result != WebInputEventResult::kNotHandled)
    return result;
  return WebInputEventResult::kHandledSystem;
}

WebInputEventResult EventHandler::PassMouseMoveEventToSubframe(
    MouseEventWithHitTestResults& mev,
    const Vector<WebMouseEvent>& coalesced_events,
    const Vector<WebMouseEvent>& predicted_events,
    LocalFrame* subframe,
    HitTestResult* hovered_node,
    HitTestLocation* hit_test_location) {
  if (mouse_event_manager_->MouseDownMayStartDrag())
    return WebInputEventResult::kNotHandled;
  WebInputEventResult result =
      subframe->GetEventHandler().HandleMouseMoveOrLeaveEvent(
          mev.Event(), coalesced_events, predicted_events, hovered_node,
          hit_test_location);
  if (result != WebInputEventResult::kNotHandled)
    return result;
  return WebInputEventResult::kHandledSystem;
}

WebInputEventResult EventHandler::PassMouseReleaseEventToSubframe(
    MouseEventWithHitTestResults& mev,
    LocalFrame* subframe) {
  return subframe->GetEventHandler().HandleMouseReleaseEvent(mev.Event());
}

void EventHandler::CaptureMouseEventsToWidget(bool capture) {
  if (!frame_->IsLocalRoot()) {
    frame_->LocalFrameRoot().GetEventHandler().CaptureMouseEventsToWidget(
        capture);
    return;
  }

  if (capture == is_widget_capturing_mouse_events_)
    return;

  frame_->LocalFrameRoot().Client()->SetMouseCapture(capture);
  is_widget_capturing_mouse_events_ = capture;
}

MouseEventWithHitTestResults EventHandler::GetMouseEventTarget(
    const HitTestRequest& request,
    const WebMouseEvent& event) {
  PhysicalOffset document_point =
      event_handling_util::ContentPointFromRootFrame(
          frame_, event.PositionInRootFrame());

  // TODO(eirage): This does not handle chorded buttons yet.
  if (event.GetType() != WebInputEvent::Type::kMouseDown) {
    HitTestResult result(request, HitTestLocation(document_point));

    Element* capture_target;
    if (event_handling_util::SubframeForTargetNode(
            capturing_subframe_element_)) {
      capture_target = capturing_subframe_element_;
      result.SetIsOverEmbeddedContentView(true);
    } else {
      capture_target = pointer_event_manager_->GetMouseCaptureTarget();
    }

    if (capture_target) {
      LayoutObject* layout_object = capture_target->GetLayoutObject();
      PhysicalOffset local_point =
          layout_object ? layout_object->AbsoluteToLocalPoint(document_point)
                        : document_point;
      result.SetNodeAndPosition(capture_target, local_point);

      result.SetScrollbar(last_scrollbar_under_mouse_);
      result.SetURLElement(capture_target->EnclosingLinkEventParentOrSelf());

      if (!request.ReadOnly()) {
        frame_->GetDocument()->UpdateHoverActiveState(
            request.Active(), !request.Move(), result.InnerElement());
      }

      return MouseEventWithHitTestResults(
          event, HitTestLocation(document_point), result);
    }
  }
  return frame_->GetDocument()->PerformMouseEventHitTest(request,
                                                         document_point, event);
}

void EventHandler::ReleaseMouseCaptureFromLocalRoot() {
  CaptureMouseEventsToWidget(false);

  frame_->LocalFrameRoot()
      .GetEventHandler()
      .ReleaseMouseCaptureFromCurrentFrame();
}

void EventHandler::ReleaseMouseCaptureFromCurrentFrame() {
  if (LocalFrame* subframe = event_handling_util::SubframeForTargetNode(
          capturing_subframe_element_))
    subframe->GetEventHandler().ReleaseMouseCaptureFromCurrentFrame();
  pointer_event_manager_->ReleaseMousePointerCapture();
  capturing_subframe_element_ = nullptr;
}

base::debug::CrashKeyString* EventHandler::CrashKeyForBug1519197() const {
  static auto* const scroll_corner_crash_key =
      base::debug::AllocateCrashKeyString("cr1519197-area-object",
                                          base::debug::CrashKeySize::Size64);
  return scroll_corner_crash_key;
}

void EventHandler::ResetLastMousePositionForWebTest() {
  // When starting a new web test, forget the mouse position, which may have
  // been affected by the previous test.
  // TODO(crbug.com/40946696): This code is temporary and can be removed once
  // we replace the RenderFrameHost; see TODO in WebFrameTestProxy::Reset.
  mouse_event_manager_->SetLastMousePositionAsUnknown();
}

}  // namespace blink
