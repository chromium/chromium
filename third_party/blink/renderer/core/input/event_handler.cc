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

#include "build/build_config.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/text_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_frame_set_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/input/touch_action_util.h"
#include "third_party/blink/renderer/core/input/touch_list.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/drag_state.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_state.h"
#include "third_party/blink/renderer/core/page/touch_adjustment.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/cursor_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/windows_keyboard_codes.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

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
  return target_node->IsShadowRoot() &&
         IsHTMLInputElement(ToShadowRoot(target_node)->host());
}

}  // namespace

using namespace HTMLNames;

// The amount of time to wait for a cursor update on style and layout changes
// Set to 50Hz, no need to be faster than common screen refresh rate
static constexpr TimeDelta kCursorUpdateInterval =
    TimeDelta::FromMilliseconds(20);

static const int kMaximumCursorSize = 128;

// It's pretty unlikely that a scale of less than one would ever be used. But
// all we really need to ensure here is that the scale isn't so small that
// integer overflow can occur when dividing cursor sizes (limited above) by the
// scale.
static const double kMinimumCursorScale = 0.001;

// The minimum amount of time an element stays active after a ShowPress
// This is roughly 9 frames, which should be long enough to be noticeable.
constexpr TimeDelta kMinimumActiveInterval = TimeDelta::FromSecondsD(0.15);

EventHandler::EventHandler(LocalFrame& frame)
    : frame_(frame),
      selection_controller_(SelectionController::Create(frame)),
      hover_timer_(frame.GetTaskRunner(TaskType::kUserInteraction),
                   this,
                   &EventHandler::HoverTimerFired),
      cursor_update_timer_(
          frame.GetTaskRunner(TaskType::kInternalUserInteraction),
          this,
          &EventHandler::CursorUpdateTimerFired),
      event_handler_will_reset_capturing_mouse_events_node_(0),
      should_only_fire_drag_over_event_(false),
      event_handler_registry_(
          frame_->IsLocalRoot()
              ? new EventHandlerRegistry(*frame_)
              : &frame_->LocalFrameRoot().GetEventHandlerRegistry()),
      scroll_manager_(new ScrollManager(frame)),
      mouse_event_manager_(new MouseEventManager(frame, *scroll_manager_)),
      mouse_wheel_event_manager_(new MouseWheelEventManager(frame)),
      keyboard_event_manager_(
          new KeyboardEventManager(frame, *scroll_manager_)),
      pointer_event_manager_(
          new PointerEventManager(frame, *mouse_event_manager_)),
      gesture_manager_(new GestureManager(frame,
                                          *scroll_manager_,
                                          *mouse_event_manager_,
                                          *pointer_event_manager_,
                                          *selection_controller_)),
      active_interval_timer_(frame.GetTaskRunner(TaskType::kUserInteraction),
                             this,
                             &EventHandler::ActiveIntervalTimerFired) {}

void EventHandler::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(selection_controller_);
  visitor->Trace(capturing_mouse_events_node_);
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
  last_mouse_down_user_gesture_token_ = nullptr;
  capturing_mouse_events_node_ = nullptr;
  pointer_event_manager_->Clear();
  scroll_manager_->Clear();
  gesture_manager_->Clear();
  mouse_event_manager_->Clear();
  mouse_wheel_event_manager_->Clear();
  last_show_press_timestamp_.reset();
  last_deferred_tap_element_ = nullptr;
  event_handler_will_reset_capturing_mouse_events_node_ = false;
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
  controller->StartMiddleClickAutoscroll(
      layout_object->GetFrame(),
      frame_->GetPage()->GetVisualViewport().ViewportToRootFrame(
          FloatPoint(mouse_event_manager_->LastKnownMousePosition())),
      mouse_event_manager_->LastKnownMousePositionGlobal());
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
    frame_->GetDocument()->UpdateHoverActiveState(request,
                                                  result.InnerElement());
  }
}

HitTestResult EventHandler::HitTestResultAtLocation(
    const HitTestLocation& location,
    HitTestRequest::HitTestRequestType hit_type,
    const LayoutObject* stop_node,
    bool no_lifecycle_update) {
  TRACE_EVENT0("blink", "EventHandler::HitTestResultAtLocation");

  // We always send HitTestResultAtLocation to the main frame if we have one,
  // otherwise we might hit areas that are obscured by higher frames.
  if (frame_->GetPage()) {
    LocalFrame& main_frame = frame_->LocalFrameRoot();
    if (frame_ != &main_frame) {
      LocalFrameView* frame_view = frame_->View();
      LocalFrameView* main_view = main_frame.View();
      if (frame_view && main_view) {
        if (location.IsRectBasedTest()) {
          DCHECK(location.IsRectilinear());
          LayoutPoint main_content_point =
              main_view->ConvertFromRootFrame(frame_view->ConvertToRootFrame(
                  location.BoundingBox().Location()));
          HitTestLocation adjusted_location(
              (LayoutRect(main_content_point, location.BoundingBox().Size())));
          return main_frame.GetEventHandler().HitTestResultAtLocation(
              adjusted_location, hit_type, stop_node, no_lifecycle_update);
        } else {
          HitTestLocation adjusted_location(main_view->ConvertFromRootFrame(
              frame_view->ConvertToRootFrame(location.Point())));
          return main_frame.GetEventHandler().HitTestResultAtLocation(
              adjusted_location, hit_type, stop_node, no_lifecycle_update);
        }
      }
    }
  }
  // HitTestResultAtLocation is specifically used to hitTest into all frames,
  // thus it always allows child frame content.
  HitTestRequest request(hit_type | HitTestRequest::kAllowChildFrameContent,
                         stop_node);
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
bool EventHandler::BubblingScroll(ScrollDirection direction,
                                  ScrollGranularity granularity,
                                  Node* starting_node) {
  return scroll_manager_->BubblingScroll(
      direction, granularity, starting_node,
      mouse_event_manager_->MousePressNode());
}

IntPoint EventHandler::LastKnownMousePositionInRootFrame() const {
  return frame_->GetPage()->GetVisualViewport().ViewportToRootFrame(
      mouse_event_manager_->LastKnownMousePosition());
}

IntPoint EventHandler::DragDataTransferLocationForTesting() {
  if (mouse_event_manager_->GetDragState().drag_data_transfer_)
    return mouse_event_manager_->GetDragState()
        .drag_data_transfer_->DragLocation();

  return IntPoint();
}

static bool IsSubmitImage(Node* node) {
  return IsHTMLInputElement(node) &&
         ToHTMLInputElement(node)->type() == InputTypeNames::image;
}

bool EventHandler::UseHandCursor(Node* node, bool is_over_link) {
  if (!node)
    return false;

  return ((is_over_link || IsSubmitImage(node)) && !HasEditableStyle(*node));
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

  frame_->GetDocument()->UpdateStyleAndLayout();

  HitTestRequest request(HitTestRequest::kReadOnly |
                         HitTestRequest::kAllowChildFrameContent);
  HitTestLocation location(
      view->ViewportToFrame(mouse_event_manager_->LastKnownMousePosition()));
  HitTestResult result(request, location);
  layout_view->HitTest(location, result);

  if (LocalFrame* frame = result.InnerNodeFrame()) {
    EventHandler::OptionalCursor optional_cursor =
        frame->GetEventHandler().SelectCursor(location, result);
    if (optional_cursor.IsCursorChange()) {
      view->SetCursor(optional_cursor.GetCursor());
    }
  }
}

bool EventHandler::ShouldShowResizeForNode(const Node* node,
                                           const HitTestLocation& location) {
  if (LayoutObject* layout_object = node->GetLayoutObject()) {
    PaintLayer* layer = layout_object->EnclosingLayer();
    if (layer->GetScrollableArea() &&
        layer->GetScrollableArea()->IsPointInResizeControl(
            RoundedIntPoint(location.Point()), kResizerForPointer)) {
      return true;
    }
  }
  return false;
}

bool EventHandler::IsSelectingLink(const HitTestResult& result) {
  // If a drag may be starting or we're capturing mouse events for a particular
  // node, don't treat this as a selection. Note calling
  // ComputeVisibleSelectionInDOMTreeDeprecated may update layout.
  const bool mouse_selection =
      !capturing_mouse_events_node_ &&
      mouse_event_manager_->MousePressed() &&
      GetSelectionController().MouseDownMayStartSelect() &&
      !mouse_event_manager_->MouseDownMayStartDrag() &&
      !frame_->Selection()
           .ComputeVisibleSelectionInDOMTreeDeprecated()
           .IsNone();
  return mouse_selection && result.IsOverLink();
}

bool EventHandler::ShouldShowIBeamForNode(const Node* node,
                                          const HitTestResult& result) {
  if (!node)
    return false;

  if (node->IsTextNode() && (node->CanStartSelection() || result.IsOverLink()))
    return true;

  return HasEditableStyle(*node);
}

EventHandler::OptionalCursor EventHandler::SelectCursor(
    const HitTestLocation& location,
    const HitTestResult& result) {
  if (scroll_manager_->InResizeMode())
    return kNoCursorChange;

  Page* page = frame_->GetPage();
  if (!page)
    return kNoCursorChange;
  if (scroll_manager_->MiddleClickAutoscrollInProgress())
    return kNoCursorChange;

  if (result.GetScrollbar() && !result.GetScrollbar()->IsCustomScrollbar())
    return PointerCursor();

  Node* node = result.InnerPossiblyPseudoNode();
  if (!node)
    return SelectAutoCursor(result, node, IBeamCursor());

  if (ShouldShowResizeForNode(node, location))
    return PointerCursor();

  LayoutObject* layout_object = node->GetLayoutObject();
  const ComputedStyle* style = layout_object ? layout_object->Style() : nullptr;

  if (layout_object) {
    Cursor override_cursor;
    switch (layout_object->GetCursor(RoundedIntPoint(result.LocalPoint()),
                                     override_cursor)) {
      case kSetCursorBasedOnStyle:
        break;
      case kSetCursor:
        return override_cursor;
      case kDoNotSetCursor:
        return kNoCursorChange;
    }
  }

  if (style && style->Cursors()) {
    const CursorList* cursors = style->Cursors();
    for (unsigned i = 0; i < cursors->size(); ++i) {
      StyleImage* style_image = (*cursors)[i].GetImage();
      if (!style_image)
        continue;
      ImageResourceContent* cached_image = style_image->CachedImage();
      if (!cached_image)
        continue;
      float scale = style_image->ImageScaleFactor();
      bool hot_spot_specified = (*cursors)[i].HotSpotSpecified();
      // Get hotspot and convert from logical pixels to physical pixels.
      IntPoint hot_spot = (*cursors)[i].HotSpot();
      hot_spot.Scale(scale, scale);
      IntSize size = cached_image->GetImage()->Size();
      if (cached_image->ErrorOccurred())
        continue;
      // Limit the size of cursors (in UI pixels) so that they cannot be
      // used to cover UI elements in chrome.
      size.Scale(1 / scale);
      if (size.Width() > kMaximumCursorSize ||
          size.Height() > kMaximumCursorSize)
        continue;

      Image* image = cached_image->GetImage();
      // Ensure no overflow possible in calculations above.
      if (scale < kMinimumCursorScale)
        continue;
      return Cursor(image, hot_spot_specified, hot_spot, scale);
    }
  }

  bool horizontal_text = !style || style->IsHorizontalWritingMode();
  const Cursor& i_beam = horizontal_text ? IBeamCursor() : VerticalTextCursor();

  switch (style ? style->Cursor() : ECursor::kAuto) {
    case ECursor::kAuto: {
      return SelectAutoCursor(result, node, i_beam);
    }
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

EventHandler::OptionalCursor EventHandler::SelectAutoCursor(
    const HitTestResult& result,
    Node* node,
    const Cursor& i_beam) {
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

  if (event_handler_will_reset_capturing_mouse_events_node_)
    capturing_mouse_events_node_ = nullptr;
  mouse_event_manager_->HandleMousePressEventUpdateStates(mouse_event);
  if (!frame_->View())
    return WebInputEventResult::kNotHandled;

  HitTestRequest request(HitTestRequest::kActive);
  // Save the document point we generate in case the window coordinate is
  // invalidated by what happens when we dispatch the event.
  LayoutPoint document_point = frame_->View()->ConvertFromRootFrame(
      FlooredIntPoint(mouse_event.PositionInRootFrame()));
  MouseEventWithHitTestResults mev =
      frame_->GetDocument()->PerformMouseEventHitTest(request, document_point,
                                                      mouse_event);
  if (!mev.InnerNode()) {
    mouse_event_manager_->InvalidateClick();
    return WebInputEventResult::kNotHandled;
  }

  mouse_event_manager_->SetMousePressNode(mev.InnerNode());
  frame_->GetDocument()->SetSequentialFocusNavigationStartingPoint(
      mev.InnerNode());

  LocalFrame* subframe = event_handling_util::SubframeForHitTestResult(mev);
  if (subframe) {
    WebInputEventResult result = PassMousePressEventToSubframe(mev, subframe);
    // Start capturing future events for this frame.  We only do this if we
    // didn't clear the m_mousePressed flag, which may happen if an AppKit
    // EmbeddedContentView entered a modal event loop.  The capturing should be
    // done only when the result indicates it has been handled. See
    // crbug.com/269917
    mouse_event_manager_->SetCapturesDragging(
        subframe->GetEventHandler().mouse_event_manager_->CapturesDragging());
    if (mouse_event_manager_->MousePressed() &&
        mouse_event_manager_->CapturesDragging()) {
      capturing_mouse_events_node_ = mev.InnerNode();
      event_handler_will_reset_capturing_mouse_events_node_ = true;
    }
    mouse_event_manager_->InvalidateClick();
    return result;
  }

  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::NotifyUserActivation(frame_);
  frame_->LocalFrameRoot()
      .GetEventHandler()
      .last_mouse_down_user_gesture_token_ =
      UserGestureIndicator::CurrentToken();
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
  mouse_event_manager_->SetClickElement(mev.InnerElement());

  if (!mouse_event.FromTouch())
    frame_->Selection().SetCaretBlinkingSuspended(true);

  WebInputEventResult event_result = DispatchMousePointerEvent(
      WebInputEvent::kPointerDown, mev.InnerNode(), mev.CanvasRegionId(),
      mev.Event(), Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  // Disabled form controls still need to resize the scrollable area.
  if ((event_result == WebInputEventResult::kNotHandled ||
       event_result == WebInputEventResult::kHandledSuppressed) &&
      frame_->View()) {
    LocalFrameView* view = frame_->View();
    PaintLayer* layer =
        mev.InnerNode()->GetLayoutObject()
            ? mev.InnerNode()->GetLayoutObject()->EnclosingLayer()
            : nullptr;
    IntPoint p = view->ConvertFromRootFrame(
        FlooredIntPoint(mouse_event.PositionInRootFrame()));
    if (layer && layer->GetScrollableArea() &&
        layer->GetScrollableArea()->IsPointInResizeControl(
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
      frame_->GetDocument()
          ->domWindow()
          ->GetInputDeviceCapabilities()
          ->FiresTouchEvents(mouse_event.FromTouch());

  if (event_result == WebInputEventResult::kNotHandled) {
    event_result = mouse_event_manager_->HandleMouseFocus(hit_test_result,
                                                          source_capabilities);
  }

  if (event_result == WebInputEventResult::kNotHandled || mev.GetScrollbar()) {
    mouse_event_manager_->SetCapturesDragging(true);
    // Main frames don't implicitly capture mouse input on MouseDown, just
    // subframes do (regardless of whether local or remote).
    if (!frame_->IsMainFrame())
      CaptureMouseEventsToWidget(true);
  } else {
    mouse_event_manager_->SetCapturesDragging(false);
  }

  // If the hit testing originally determined the event was in a scrollbar,
  // refetch the MouseEventWithHitTestResults in case the scrollbar
  // EmbeddedContentView was destroyed when the mouse event was handled.
  if (mev.GetScrollbar()) {
    const bool was_last_scroll_bar =
        mev.GetScrollbar() == last_scrollbar_under_mouse_.Get();
    HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
    mev = frame_->GetDocument()->PerformMouseEventHitTest(
        request, document_point, mouse_event);
    if (was_last_scroll_bar &&
        mev.GetScrollbar() != last_scrollbar_under_mouse_.Get())
      last_scrollbar_under_mouse_ = nullptr;
  }

  if (event_result != WebInputEventResult::kNotHandled) {
    // Scrollbars should get events anyway, even disabled controls might be
    // scrollable.
    PassMousePressEventToScrollbar(mev);
  } else {
    if (ShouldRefetchEventTarget(mev)) {
      HitTestRequest request(HitTestRequest::kReadOnly |
                             HitTestRequest::kActive);
      mev = frame_->GetDocument()->PerformMouseEventHitTest(
          request, document_point, mouse_event);
    }

    if (PassMousePressEventToScrollbar(mev))
      event_result = WebInputEventResult::kHandledSystem;
    else
      event_result = mouse_event_manager_->HandleMousePressEvent(mev);
  }

  if (mev.GetHitTestResult().InnerNode() &&
      mouse_event.button == WebPointerProperties::Button::kLeft) {
    DCHECK_EQ(WebInputEvent::kMouseDown, mouse_event.GetType());
    HitTestResult result = mev.GetHitTestResult();
    result.SetToShadowHostIfInRestrictedShadowRoot();
    frame_->GetChromeClient().OnMouseDown(*result.InnerNode());
  }
  return event_result;
}

WebInputEventResult EventHandler::HandleMouseMoveEvent(
    const WebMouseEvent& event,
    const Vector<WebMouseEvent>& coalesced_events,
    const Vector<WebMouseEvent>& predicted_events) {
  TRACE_EVENT0("blink", "EventHandler::handleMouseMoveEvent");
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

  hovered_node_result.SetToShadowHostIfInRestrictedShadowRoot();
  page->GetChromeClient().MouseDidMoveOverElement(*frame_, location,
                                                  hovered_node_result);

  return result;
}

void EventHandler::HandleMouseLeaveEvent(const WebMouseEvent& event) {
  TRACE_EVENT0("blink", "EventHandler::handleMouseLeaveEvent");

  Page* page = frame_->GetPage();
  if (page)
    page->GetChromeClient().ClearToolTip(*frame_);
  HandleMouseMoveOrLeaveEvent(event, Vector<WebMouseEvent>(),
                              Vector<WebMouseEvent>(), nullptr, nullptr, false,
                              true);
}

WebInputEventResult EventHandler::HandleMouseMoveOrLeaveEvent(
    const WebMouseEvent& mouse_event,
    const Vector<WebMouseEvent>& coalesced_events,
    const Vector<WebMouseEvent>& predicted_events,
    HitTestResult* hovered_node_result,
    HitTestLocation* hit_test_location,
    bool only_update_scrollbars,
    bool force_leave) {
  DCHECK(frame_);
  DCHECK(frame_->View());

  mouse_event_manager_->SetLastKnownMousePosition(mouse_event);

  hover_timer_.Stop();
  cursor_update_timer_.Stop();

  mouse_event_manager_->CancelFakeMouseMoveEvent();
  mouse_event_manager_->HandleSvgPanIfNeeded(false);

  // Mouse states need to be reset when mouse move with no button down.
  // This is for popup/context_menu opened at mouse_down event and
  // mouse_release is not handled in page.
  // crbug.com/527582
  if (mouse_event.button == WebPointerProperties::Button::kNoButton &&
      !(mouse_event.GetModifiers() &
        WebInputEvent::Modifiers::kRelativeMotionEvent)) {
    mouse_event_manager_->ClearDragHeuristicState();
    if (event_handler_will_reset_capturing_mouse_events_node_)
      capturing_mouse_events_node_ = nullptr;
    CaptureMouseEventsToWidget(false);
  }

  if (RuntimeEnabledFeatures::MiddleClickAutoscrollEnabled()) {
    if (Page* page = frame_->GetPage()) {
      if (mouse_event.GetType() == WebInputEvent::kMouseLeave &&
          mouse_event.button != WebPointerProperties::Button::kMiddle) {
        page->GetAutoscrollController().StopMiddleClickAutoscroll(frame_);
      } else {
        page->GetAutoscrollController().HandleMouseMoveForMiddleClickAutoscroll(
            frame_, mouse_event_manager_->LastKnownMousePositionGlobal(),
            mouse_event.button == WebPointerProperties::Button::kMiddle);
      }
    }
  }

  if (frame_set_being_resized_) {
    return DispatchMousePointerEvent(
        WebInputEvent::kPointerMove, frame_set_being_resized_.Get(), String(),
        mouse_event, coalesced_events, predicted_events);
  }

  // Send events right to a scrollbar if the mouse is pressed.
  if (last_scrollbar_under_mouse_ && mouse_event_manager_->MousePressed()) {
    last_scrollbar_under_mouse_->MouseMoved(mouse_event);
    return WebInputEventResult::kHandledSystem;
  }

  HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kMove;
  if (mouse_event_manager_->MousePressed()) {
    hit_type |= HitTestRequest::kActive;
  } else if (only_update_scrollbars) {
    // Mouse events should be treated as "read-only" if we're updating only
    // scrollbars. This means that :hover and :active freeze in the state they
    // were in, rather than updating for nodes the mouse moves while the
    // window is not key (which will be the case if onlyUpdateScrollbars is
    // true).
    hit_type |= HitTestRequest::kReadOnly;
  }

  // Treat any mouse move events as readonly if the user is currently touching
  // the screen.
  if (pointer_event_manager_->IsAnyTouchActive() && !force_leave)
    hit_type |= HitTestRequest::kActive | HitTestRequest::kReadOnly;
  HitTestRequest request(hit_type);
  HitTestLocation out_location((LayoutPoint()));
  MouseEventWithHitTestResults mev = MouseEventWithHitTestResults(
      mouse_event, out_location, HitTestResult(request, out_location));

  // We don't want to do a hit-test in forceLeave scenarios because there
  // might actually be some other frame above this one at the specified
  // co-ordinate. So we must force the hit-test to fail, while still clearing
  // hover/active state.
  if (force_leave) {
    frame_->GetDocument()->UpdateHoverActiveState(request, nullptr);
  } else {
    mev = event_handling_util::PerformMouseEventHitTest(frame_, request,
                                                        mouse_event);
  }

  if (hovered_node_result)
    *hovered_node_result = mev.GetHitTestResult();

  if (hit_test_location)
    *hit_test_location = mev.GetHitTestLocation();

  Scrollbar* scrollbar = nullptr;

  if (scroll_manager_->InResizeMode()) {
    scroll_manager_->Resize(mev.Event());
  } else {
    if (!scrollbar)
      scrollbar = mev.GetScrollbar();

    UpdateLastScrollbarUnderMouse(scrollbar,
                                  !mouse_event_manager_->MousePressed());
    if (only_update_scrollbars)
      return WebInputEventResult::kHandledSuppressed;
  }

  WebInputEventResult event_result = WebInputEventResult::kNotHandled;
  LocalFrame* new_subframe =
      capturing_mouse_events_node_.Get()
          ? event_handling_util::SubframeForTargetNode(
                capturing_mouse_events_node_.Get())
          : event_handling_util::SubframeForHitTestResult(mev);

  // We want mouseouts to happen first, from the inside out.  First send a
  // move event to the last subframe so that it will fire mouseouts.
  if (last_mouse_move_event_subframe_ &&
      last_mouse_move_event_subframe_->Tree().IsDescendantOf(frame_) &&
      last_mouse_move_event_subframe_ != new_subframe) {
    last_mouse_move_event_subframe_->GetEventHandler().HandleMouseLeaveEvent(
        mev.Event());
    last_mouse_move_event_subframe_->GetEventHandler()
        .mouse_event_manager_->SetLastMousePositionAsUnknown();
  }

  if (new_subframe) {
    // Update over/out state before passing the event to the subframe.
    pointer_event_manager_->SendMouseAndPointerBoundaryEvents(
        EffectiveMouseEventTargetNode(mev.InnerNode()), mev.CanvasRegionId(),
        mev.Event());

    // Event dispatch in sendMouseAndPointerBoundaryEvents may have caused the
    // subframe of the target node to be detached from its LocalFrameView, in
    // which case the event should not be passed.
    if (new_subframe->View()) {
      event_result =
          PassMouseMoveEventToSubframe(mev, coalesced_events, predicted_events,
                                       new_subframe, hovered_node_result);
    }
  } else {
    if (scrollbar && !mouse_event_manager_->MousePressed()) {
      // Handle hover effects on platforms that support visual feedback on
      // scrollbar hovering.
      scrollbar->MouseMoved(mev.Event());
    }
    if (LocalFrameView* view = frame_->View()) {
      EventHandler::OptionalCursor optional_cursor =
          SelectCursor(mev.GetHitTestLocation(), mev.GetHitTestResult());
      if (optional_cursor.IsCursorChange()) {
        view->SetCursor(optional_cursor.GetCursor());
      }
    }
  }

  last_mouse_move_event_subframe_ = new_subframe;

  if (event_result != WebInputEventResult::kNotHandled)
    return event_result;

  event_result = DispatchMousePointerEvent(
      WebInputEvent::kPointerMove, mev.InnerNode(), mev.CanvasRegionId(),
      mev.Event(), coalesced_events, predicted_events);
  // TODO(crbug.com/346473): Since there is no default action for the mousemove
  // event we should consider doing drag&drop even when js cancels the
  // mouse move event.
  // https://w3c.github.io/uievents/#event-type-mousemove
  if (event_result != WebInputEventResult::kNotHandled)
    return event_result;

  return mouse_event_manager_->HandleMouseDraggedEvent(mev);
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
    CaptureMouseEventsToWidget(false);
    return mouse_event_manager_->SetMousePositionAndDispatchMouseEvent(
        EffectiveMouseEventTargetNode(frame_set_being_resized_.Get()), String(),
        EventTypeNames::mouseup, mouse_event);
  }

  if (last_scrollbar_under_mouse_) {
    mouse_event_manager_->InvalidateClick();
    last_scrollbar_under_mouse_->MouseUp(mouse_event);
    CaptureMouseEventsToWidget(false);
    return DispatchMousePointerEvent(
        WebInputEvent::kPointerUp, mouse_event_manager_->GetNodeUnderMouse(),
        String(), mouse_event, Vector<WebMouseEvent>(),
        Vector<WebMouseEvent>());
  }

  // Mouse events simulated from touch should not hit-test again.
  DCHECK(!mouse_event.FromTouch());

  HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kRelease;
  HitTestRequest request(hit_type);
  MouseEventWithHitTestResults mev =
      event_handling_util::PerformMouseEventHitTest(frame_, request,
                                                    mouse_event);
  Element* mouse_release_target = mev.InnerElement();
  LocalFrame* subframe =
      capturing_mouse_events_node_.Get()
          ? event_handling_util::SubframeForTargetNode(
                capturing_mouse_events_node_.Get())
          : event_handling_util::SubframeForHitTestResult(mev);
  if (event_handler_will_reset_capturing_mouse_events_node_)
    capturing_mouse_events_node_ = nullptr;
  if (subframe)
    return PassMouseReleaseEventToSubframe(mev, subframe);

  // Mouse events will be associated with the Document where mousedown
  // occurred. If, e.g., there is a mousedown, then a drag to a different
  // Document and mouseup there, the mouseup's gesture will be associated with
  // the mousedown's Document. It's not absolutely certain that this is the
  // correct behavior.
  std::unique_ptr<UserGestureIndicator> gesture_indicator;
  if (frame_->LocalFrameRoot()
          .GetEventHandler()
          .last_mouse_down_user_gesture_token_) {
    gesture_indicator = std::make_unique<UserGestureIndicator>(
        std::move(frame_->LocalFrameRoot()
                      .GetEventHandler()
                      .last_mouse_down_user_gesture_token_));
  } else {
    gesture_indicator = LocalFrame::NotifyUserActivation(frame_);
  }

  WebInputEventResult event_result = DispatchMousePointerEvent(
      WebInputEvent::kPointerUp, mev.InnerNode(), mev.CanvasRegionId(),
      mev.Event(), Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  WebInputEventResult click_event_result =
      mouse_release_target ? mouse_event_manager_->DispatchMouseClickIfNeeded(
                                 mev, *mouse_release_target)
                           : WebInputEventResult::kNotHandled;

  scroll_manager_->ClearResizeScrollableArea(false);

  if (event_result == WebInputEventResult::kNotHandled)
    event_result = mouse_event_manager_->HandleMouseReleaseEvent(mev);

  mouse_event_manager_->HandleMouseReleaseEventUpdateStates();
  CaptureMouseEventsToWidget(false);

  return event_handling_util::MergeEventResult(click_event_result,
                                               event_result);
}

static bool TargetIsFrame(Node* target, LocalFrame*& frame) {
  if (!IsHTMLFrameElementBase(target))
    return false;

  // Cross-process drag and drop is not yet supported.
  if (ToHTMLFrameElementBase(target)->ContentFrame() &&
      !ToHTMLFrameElementBase(target)->ContentFrame()->IsLocalFrame())
    return false;

  frame = ToLocalFrame(ToHTMLFrameElementBase(target)->ContentFrame());
  return true;
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
    controller->UpdateDragAndDrop(new_target,
                                  FlooredIntPoint(event.PositionInRootFrame()),
                                  event.TimeStamp());
  }

  if (drag_target_ != new_target) {
    // FIXME: this ordering was explicitly chosen to match WinIE. However,
    // it is sometimes incorrect when dragging within subframes, as seen with
    // LayoutTests/fast/events/drag-in-frames.html.
    //
    // Moreover, this ordering conforms to section 7.9.4 of the HTML 5 spec.
    // <http://dev.w3.org/html5/spec/Overview.html#drag-and-drop-processing-model>.
    LocalFrame* target_frame;
    if (TargetIsFrame(new_target, target_frame)) {
      if (target_frame)
        event_result = target_frame->GetEventHandler().UpdateDragAndDrop(
            event, data_transfer);
    } else if (new_target) {
      // As per section 7.9.4 of the HTML 5 spec., we must always fire a drag
      // event before firing a dragenter, dragleave, or dragover event.
      if (mouse_event_manager_->GetDragState().drag_src_) {
        // For now we don't care if event handler cancels default behavior,
        // since there is none.
        mouse_event_manager_->DispatchDragSrcEvent(EventTypeNames::drag, event);
      }
      event_result = mouse_event_manager_->DispatchDragEvent(
          EventTypeNames::dragenter, new_target, drag_target_, event,
          data_transfer);
    }

    if (TargetIsFrame(drag_target_.Get(), target_frame)) {
      if (target_frame)
        event_result = target_frame->GetEventHandler().UpdateDragAndDrop(
            event, data_transfer);
    } else if (drag_target_) {
      mouse_event_manager_->DispatchDragEvent(EventTypeNames::dragleave,
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
    LocalFrame* target_frame;
    if (TargetIsFrame(new_target, target_frame)) {
      if (target_frame)
        event_result = target_frame->GetEventHandler().UpdateDragAndDrop(
            event, data_transfer);
    } else if (new_target) {
      // Note, when dealing with sub-frames, we may need to fire only a dragover
      // event as a drag event may have been fired earlier.
      if (!should_only_fire_drag_over_event_ &&
          mouse_event_manager_->GetDragState().drag_src_) {
        // For now we don't care if event handler cancels default behavior,
        // since there is none.
        mouse_event_manager_->DispatchDragSrcEvent(EventTypeNames::drag, event);
      }
      event_result = mouse_event_manager_->DispatchDragEvent(
          EventTypeNames::dragover, new_target, nullptr, event, data_transfer);
      should_only_fire_drag_over_event_ = false;
    }
  }
  drag_target_ = new_target;

  return event_result;
}

void EventHandler::CancelDragAndDrop(const WebMouseEvent& event,
                                     DataTransfer* data_transfer) {
  LocalFrame* target_frame;
  if (TargetIsFrame(drag_target_.Get(), target_frame)) {
    if (target_frame)
      target_frame->GetEventHandler().CancelDragAndDrop(event, data_transfer);
  } else if (drag_target_.Get()) {
    if (mouse_event_manager_->GetDragState().drag_src_)
      mouse_event_manager_->DispatchDragSrcEvent(EventTypeNames::drag, event);
    mouse_event_manager_->DispatchDragEvent(EventTypeNames::dragleave,
                                            drag_target_.Get(), nullptr, event,
                                            data_transfer);
  }
  ClearDragState();
}

WebInputEventResult EventHandler::PerformDragAndDrop(
    const WebMouseEvent& event,
    DataTransfer* data_transfer) {
  LocalFrame* target_frame;
  WebInputEventResult result = WebInputEventResult::kNotHandled;
  if (TargetIsFrame(drag_target_.Get(), target_frame)) {
    if (target_frame)
      result = target_frame->GetEventHandler().PerformDragAndDrop(
          event, data_transfer);
  } else if (drag_target_.Get()) {
    result = mouse_event_manager_->DispatchDragEvent(
        EventTypeNames::drop, drag_target_.Get(), nullptr, event,
        data_transfer);
  }
  ClearDragState();
  return result;
}

void EventHandler::ClearDragState() {
  scroll_manager_->StopAutoscroll();
  drag_target_ = nullptr;
  capturing_mouse_events_node_ = nullptr;
  should_only_fire_drag_over_event_ = false;
}

void EventHandler::AnimateSnapFling(base::TimeTicks monotonic_time) {
  scroll_manager_->AnimateSnapFling(monotonic_time);
}

void EventHandler::SetCapturingMouseEventsNode(Node* n) {
  CaptureMouseEventsToWidget(n);
  capturing_mouse_events_node_ = n;
}

Node* EventHandler::EffectiveMouseEventTargetNode(Node* target_node) {
  Node* new_node_under_mouse = target_node;

  if (capturing_mouse_events_node_) {
    new_node_under_mouse = capturing_mouse_events_node_.Get();
  } else {
    // If the target node is a text node, dispatch on the parent node -
    // rdar://4196646
    if (new_node_under_mouse && new_node_under_mouse->IsTextNode())
      new_node_under_mouse = FlatTreeTraversal::Parent(*new_node_under_mouse);
  }
  return new_node_under_mouse;
}

bool EventHandler::IsTouchPointerIdActiveOnFrame(int pointer_id,
                                                 LocalFrame* frame) const {
  DCHECK_EQ(frame_, &frame_->LocalFrameRoot());
  return pointer_event_manager_->IsTouchPointerIdActiveOnFrame(pointer_id,
                                                               frame);
}

bool EventHandler::RootFrameTouchPointerActiveInCurrentFrame(
    int pointer_id) const {
  return frame_ != &frame_->LocalFrameRoot() &&
         frame_->LocalFrameRoot()
             .GetEventHandler()
             .IsTouchPointerIdActiveOnFrame(pointer_id, frame_);
}

bool EventHandler::IsPointerEventActive(int pointer_id) {
  return pointer_event_manager_->IsActive(pointer_id) ||
         RootFrameTouchPointerActiveInCurrentFrame(pointer_id);
}

void EventHandler::SetPointerCapture(int pointer_id, EventTarget* target) {
  // TODO(crbug.com/591387): This functionality should be per page not per
  // frame.
  if (RootFrameTouchPointerActiveInCurrentFrame(pointer_id)) {
    frame_->LocalFrameRoot().GetEventHandler().SetPointerCapture(pointer_id,
                                                                 target);
  } else {
    pointer_event_manager_->SetPointerCapture(pointer_id, target);
  }
}

void EventHandler::ReleasePointerCapture(int pointer_id, EventTarget* target) {
  if (RootFrameTouchPointerActiveInCurrentFrame(pointer_id)) {
    frame_->LocalFrameRoot().GetEventHandler().ReleasePointerCapture(pointer_id,
                                                                     target);
  } else {
    pointer_event_manager_->ReleasePointerCapture(pointer_id, target);
  }
}

void EventHandler::ReleaseMousePointerCapture() {
  pointer_event_manager_->ReleaseMousePointerCapture();
}

bool EventHandler::HasPointerCapture(int pointer_id,
                                     const EventTarget* target) const {
  if (RootFrameTouchPointerActiveInCurrentFrame(pointer_id)) {
    return frame_->LocalFrameRoot().GetEventHandler().HasPointerCapture(
        pointer_id, target);
  } else {
    return pointer_event_manager_->HasPointerCapture(pointer_id, target);
  }
}

bool EventHandler::HasProcessedPointerCapture(int pointer_id,
                                              const EventTarget* target) const {
  return pointer_event_manager_->HasProcessedPointerCapture(pointer_id, target);
}

void EventHandler::ProcessPendingPointerCaptureForPointerLock(
    const WebMouseEvent& mouse_event) {
  pointer_event_manager_->ProcessPendingPointerCaptureForPointerLock(
      mouse_event);
}

void EventHandler::ElementRemoved(EventTarget* target) {
  pointer_event_manager_->ElementRemoved(target);
  if (target)
    mouse_wheel_event_manager_->ElementRemoved(target->ToNode());
}

WebInputEventResult EventHandler::DispatchMousePointerEvent(
    const WebInputEvent::Type event_type,
    Node* target_node,
    const String& canvas_region_id,
    const WebMouseEvent& mouse_event,
    const Vector<WebMouseEvent>& coalesced_events,
    const Vector<WebMouseEvent>& predicted_events) {
  const auto& event_result = pointer_event_manager_->SendMousePointerEvent(
      EffectiveMouseEventTargetNode(target_node), canvas_region_id, event_type,
      mouse_event, coalesced_events, predicted_events);
  return event_result;
}

WebInputEventResult EventHandler::HandleWheelEvent(
    const WebMouseWheelEvent& event) {
  return mouse_wheel_event_manager_->HandleWheelEvent(event);
}

// TODO(crbug.com/665924): This function bypasses all Handle*Event path.
// It should be using that flow instead of creating/sending events directly.
WebInputEventResult EventHandler::HandleTargetedMouseEvent(
    Node* target,
    const WebMouseEvent& event,
    const AtomicString& mouse_event_type,
    const Vector<WebMouseEvent>& coalesced_events,
    const Vector<WebMouseEvent>& predicted_events,
    const String& canvas_region_id) {
  mouse_event_manager_->SetClickCount(event.click_count);
  return pointer_event_manager_->DirectDispatchMousePointerEvent(
      target, event, mouse_event_type, coalesced_events, predicted_events,
      canvas_region_id);
}

WebInputEventResult EventHandler::HandleGestureEvent(
    const WebGestureEvent& gesture_event) {
  // Propagation to inner frames is handled below this function.
  DCHECK_EQ(frame_, &frame_->LocalFrameRoot());
  DCHECK_NE(0, gesture_event.FrameScale());

  // Scrolling-related gesture events invoke EventHandler recursively for each
  // frame down the chain, doing a single-frame hit-test per frame. This matches
  // handleWheelEvent.
  // FIXME: Add a test that traverses this path, e.g. for devtools overlay.
  if (gesture_event.IsScrollEvent())
    return HandleGestureScrollEvent(gesture_event);

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

  if (targeted_event.Event().GetType() == WebInputEvent::kGestureShowPress)
    last_show_press_timestamp_ = CurrentTimeTicks();

  // Update mouseout/leave/over/enter events before jumping directly to the
  // inner most frame.
  if (targeted_event.Event().GetType() == WebInputEvent::kGestureTap)
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
  return gesture_manager_->HandleGestureEventInFrame(targeted_event);
}

WebInputEventResult EventHandler::HandleGestureScrollEvent(
    const WebGestureEvent& gesture_event) {
  TRACE_EVENT0("input", "EventHandler::handleGestureScrollEvent");
  if (!frame_->GetPage())
    return WebInputEventResult::kNotHandled;

  return scroll_manager_->HandleGestureScrollEvent(gesture_event);
}

WebInputEventResult EventHandler::HandleGestureScrollEnd(
    const WebGestureEvent& gesture_event) {
  if (!frame_->GetPage())
    return WebInputEventResult::kNotHandled;
  return scroll_manager_->HandleGestureScrollEnd(gesture_event);
}

void EventHandler::SetMouseDownMayStartAutoscroll() {
  mouse_event_manager_->SetMouseDownMayStartAutoscroll();
}

bool EventHandler::IsScrollbarHandlingGestures() const {
  return scroll_manager_->IsScrollbarHandlingGestures();
}

bool EventHandler::ShouldApplyTouchAdjustment(
    const WebGestureEvent& event) const {
  if (frame_->GetSettings() &&
      !frame_->GetSettings()->GetTouchAdjustmentEnabled())
    return false;

  if (event.primary_pointer_type == WebPointerProperties::PointerType::kPen)
    return false;

  return !event.TapAreaInRootFrame().IsEmpty();
}

void EventHandler::CacheTouchAdjustmentResult(uint32_t id,
                                              FloatPoint adjusted_point) {
  touch_adjustment_result_.unique_event_id = id;
  touch_adjustment_result_.adjusted_point = adjusted_point;
}

bool EventHandler::GestureCorrespondsToAdjustedTouch(
    const WebGestureEvent& event) {
  if (!RuntimeEnabledFeatures::UnifiedTouchAdjustmentEnabled())
    return false;
  // Gesture events start with a GestureTapDown. If GestureTapDown's unique id
  // matches stored adjusted touchstart event id, then we can use the stored
  // result for following gesture event.
  if (event.GetType() == WebInputEvent::kGestureTapDown) {
    should_use_touch_event_adjusted_point_ =
        (event.unique_touch_event_id != 0 &&
         event.unique_touch_event_id ==
             touch_adjustment_result_.unique_event_id);
  }

  // Check if the adjusted point is in the gesture event tap rect.
  // If not, should not use this touch point in following events.
  if (should_use_touch_event_adjusted_point_) {
    FloatRect tap_rect(FloatPoint(event.PositionInRootFrame()) -
                           FloatSize(event.TapAreaInRootFrame()) * 0.5,
                       FloatSize(event.TapAreaInRootFrame()));
    should_use_touch_event_adjusted_point_ =
        tap_rect.Contains(touch_adjustment_result_.adjusted_point);
  }

  return should_use_touch_event_adjusted_point_;
}

bool EventHandler::BestClickableNodeForHitTestResult(
    const HitTestLocation& location,
    const HitTestResult& result,
    IntPoint& target_point,
    Node*& target_node) {
  // FIXME: Unify this with the other best* functions which are very similar.

  TRACE_EVENT0("input", "EventHandler::bestClickableNodeForHitTestResult");
  DCHECK(location.IsRectBasedTest());

  // If the touch is over a scrollbar, don't adjust the touch point since touch
  // adjustment only takes into account DOM nodes so a touch over a scrollbar
  // will be adjusted towards nearby nodes. This leads to things like textarea
  // scrollbars being untouchable.
  if (result.GetScrollbar()) {
    target_node = nullptr;
    return false;
  }

  IntPoint touch_center =
      frame_->View()->ConvertToRootFrame(RoundedIntPoint(location.Point()));
  IntRect touch_rect =
      frame_->View()->ConvertToRootFrame(location.EnclosingIntRect());

  HeapVector<Member<Node>, 11> nodes;
  CopyToVector(result.ListBasedTestResult(), nodes);

  // FIXME: the explicit Vector conversion copies into a temporary and is
  // wasteful.
  return FindBestClickableCandidate(target_node, target_point, touch_center,
                                    touch_rect,
                                    HeapVector<Member<Node>>(nodes));
}

bool EventHandler::BestContextMenuNodeForHitTestResult(
    const HitTestLocation& location,
    const HitTestResult& result,
    IntPoint& target_point,
    Node*& target_node) {
  DCHECK(location.IsRectBasedTest());
  IntPoint touch_center =
      frame_->View()->ConvertToRootFrame(RoundedIntPoint(location.Point()));
  IntRect touch_rect =
      frame_->View()->ConvertToRootFrame(location.EnclosingIntRect());
  HeapVector<Member<Node>, 11> nodes;
  CopyToVector(result.ListBasedTestResult(), nodes);

  // FIXME: the explicit Vector conversion copies into a temporary and is
  // wasteful.
  return FindBestContextMenuCandidate(target_node, target_point, touch_center,
                                      touch_rect,
                                      HeapVector<Member<Node>>(nodes));
}

// Update the hover and active state across all frames for this gesture.
// This logic is different than the mouse case because mice send MouseLeave
// events to frames as they're exited.  With gestures, a single event
// conceptually both 'leaves' whatever frame currently had hover and enters a
// new frame
void EventHandler::UpdateGestureHoverActiveState(const HitTestRequest& request,
                                                 Element* inner_element) {
  DCHECK_EQ(frame_, &frame_->LocalFrameRoot());

  HeapVector<Member<LocalFrame>> new_hover_frame_chain;
  LocalFrame* new_hover_frame_in_document =
      inner_element ? inner_element->GetDocument().GetFrame() : nullptr;
  // Insert the ancestors of the frame having the new hovered element to the
  // frame chain.  The frame chain doesn't include the main frame to avoid the
  // redundant work that cleans the hover state because the hover state for the
  // main frame is updated by calling Document::updateHoverActiveState.
  while (new_hover_frame_in_document && new_hover_frame_in_document != frame_) {
    new_hover_frame_chain.push_back(new_hover_frame_in_document);
    Frame* parent_frame = new_hover_frame_in_document->Tree().Parent();
    new_hover_frame_in_document = parent_frame && parent_frame->IsLocalFrame()
                                      ? ToLocalFrame(parent_frame)
                                      : nullptr;
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

      HTMLFrameOwnerElement* owner =
          ToHTMLFrameOwnerElement(old_hover_element_in_cur_doc);
      if (!owner->ContentFrame() || !owner->ContentFrame()->IsLocalFrame())
        break;

      LocalFrame* old_hover_frame = ToLocalFrame(owner->ContentFrame());
      Document* doc = old_hover_frame->GetDocument();
      if (!doc)
        break;

      old_hover_element_in_cur_doc = doc->HoverElement();
      // If the old hovered frame is different from the new hovered frame.
      // we should clear the old hovered element from the old hovered frame.
      if (new_hover_frame != old_hover_frame)
        doc->UpdateHoverActiveState(request, nullptr);
    }
  }

  // Recursively set the new active/hover states on every frame in the chain of
  // innerElement.
  frame_->GetDocument()->UpdateHoverActiveState(request, inner_element);
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
    entered_frame_in_document = parent_frame && parent_frame->IsLocalFrame()
                                    ? ToLocalFrame(parent_frame)
                                    : nullptr;
  }

  wtf_size_t index_entered_frame_chain = entered_frame_chain.size();
  LocalFrame* exited_frame_in_document = frame_;
  HeapVector<Member<LocalFrame>, 2> exited_frame_chain;
  // Insert the frame from the disagreement between last frames and entered
  // frames.
  while (exited_frame_in_document) {
    Node* last_node_under_tap = exited_frame_in_document->GetEventHandler()
                                    .mouse_event_manager_->GetNodeUnderMouse();
    if (!last_node_under_tap)
      break;

    LocalFrame* next_exited_frame_in_document = nullptr;
    if (last_node_under_tap->IsFrameOwnerElement()) {
      HTMLFrameOwnerElement* owner =
          ToHTMLFrameOwnerElement(last_node_under_tap);
      if (owner->ContentFrame() && owner->ContentFrame()->IsLocalFrame())
        next_exited_frame_in_document = ToLocalFrame(owner->ContentFrame());
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
      WebInputEvent::kMouseMove, gesture_event,
      WebPointerProperties::Button::kNoButton,
      /* clickCount */ 0,
      modifiers | WebInputEvent::Modifiers::kIsCompatibilityEventForTouch,
      gesture_event.TimeStamp());

  // Update the mouseout/mouseleave event
  wtf_size_t index_exited_frame_chain = exited_frame_chain.size();
  while (index_exited_frame_chain) {
    LocalFrame* leave_frame = exited_frame_chain[--index_exited_frame_chain];
    leave_frame->GetEventHandler().mouse_event_manager_->SetNodeUnderMouse(
        EffectiveMouseEventTargetNode(nullptr), String(), fake_mouse_move);
  }

  // update the mouseover/mouseenter event
  while (index_entered_frame_chain) {
    Frame* parent_frame =
        entered_frame_chain[--index_entered_frame_chain]->Tree().Parent();
    if (parent_frame && parent_frame->IsLocalFrame()) {
      ToLocalFrame(parent_frame)
          ->GetEventHandler()
          .mouse_event_manager_->SetNodeUnderMouse(
              EffectiveMouseEventTargetNode(ToHTMLFrameOwnerElement(
                  entered_frame_chain[index_entered_frame_chain]->Owner())),
              String(), fake_mouse_move);
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
  TimeDelta active_interval;
  bool should_keep_active_for_min_interval = false;
  if (read_only) {
    hit_type |= HitTestRequest::kReadOnly;
  } else if (gesture_event.GetType() == WebInputEvent::kGestureTap &&
             last_show_press_timestamp_) {
    // If the Tap is received very shortly after ShowPress, we want to
    // delay clearing of the active state so that it's visible to the user
    // for at least a couple of frames.
    active_interval = CurrentTimeTicks() - last_show_press_timestamp_.value();
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
    UpdateGestureHoverActiveState(
        request, event_with_hit_test_results.GetHitTestResult().InnerElement());
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
  LayoutSize hit_rect_size;
  if (ShouldApplyTouchAdjustment(gesture_event)) {
    // If gesture_event unique id matches the stored touch event result, do
    // point-base hit test. Otherwise add padding and do rect-based hit test.
    if (GestureCorrespondsToAdjustedTouch(gesture_event)) {
      adjusted_event.ApplyTouchAdjustment(
          touch_adjustment_result_.adjusted_point);
    } else {
      hit_rect_size = GetHitTestRectForAdjustment(
          *frame_, LayoutSize(adjusted_event.TapAreaInRootFrame()));
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
    LayoutPoint top_left(adjusted_event.PositionInRootFrame());
    top_left.Move(-hit_rect_size * 0.5f);
    location = HitTestLocation(LayoutRect(top_left, hit_rect_size));
    hit_test_result = root_frame.GetEventHandler().HitTestResultAtLocation(
        location, hit_type);
  }

  if (location.IsRectBasedTest()) {
    // Adjust the location of the gesture to the most likely nearby node, as
    // appropriate for the type of event.
    ApplyTouchAdjustment(&adjusted_event, location, &hit_test_result);
    // Do a new hit-test at the (adjusted) gesture co-ordinates. This is
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

  if (ShouldApplyTouchAdjustment(gesture_event) &&
      (gesture_event.GetType() == WebInputEvent::kGestureTap ||
       gesture_event.GetType() == WebInputEvent::kGestureLongPress)) {
    float adjusted_distance = FloatSize(adjusted_event.PositionInRootFrame() -
                                        gesture_event.PositionInRootFrame())
                                  .DiagonalLength();
    UMA_HISTOGRAM_COUNTS_100("Event.Touch.TouchAdjustment.AdjustDistance",
                             static_cast<int>(adjusted_distance));
  }
  return GestureEventWithHitTestResults(adjusted_event, location,
                                        hit_test_result);
}

void EventHandler::ApplyTouchAdjustment(WebGestureEvent* gesture_event,
                                        HitTestLocation& location,
                                        HitTestResult* hit_test_result) {
  Node* adjusted_node = nullptr;
  IntPoint adjusted_point =
      FlooredIntPoint(gesture_event->PositionInRootFrame());
  bool adjusted = false;
  switch (gesture_event->GetType()) {
    case WebInputEvent::kGestureTap:
    case WebInputEvent::kGestureTapUnconfirmed:
    case WebInputEvent::kGestureTapDown:
    case WebInputEvent::kGestureShowPress:
      adjusted = BestClickableNodeForHitTestResult(
          location, *hit_test_result, adjusted_point, adjusted_node);
      break;
    case WebInputEvent::kGestureLongPress:
    case WebInputEvent::kGestureLongTap:
    case WebInputEvent::kGestureTwoFingerTap:
      adjusted = BestContextMenuNodeForHitTestResult(
          location, *hit_test_result, adjusted_point, adjusted_node);
      break;
    default:
      NOTREACHED();
  }

  // Update the hit-test result to be a point-based result instead of a
  // rect-based result.
  // FIXME: We should do this even when no candidate matches the node filter.
  // crbug.com/398914
  if (adjusted) {
    LayoutPoint point = frame_->View()->ConvertFromRootFrame(adjusted_point);
    DCHECK(location.ContainsPoint(FloatPoint(point)));
    DCHECK(location.IsRectBasedTest());
    location = hit_test_result->ResolveRectBasedTest(adjusted_node, point);
    gesture_event->ApplyTouchAdjustment(
        WebFloatPoint(adjusted_point.X(), adjusted_point.Y()));
  }
}

WebInputEventResult EventHandler::SendContextMenuEvent(
    const WebMouseEvent& event,
    Node* override_target_node) {
  LocalFrameView* v = frame_->View();
  if (!v)
    return WebInputEventResult::kNotHandled;

  // Clear mouse press state to avoid initiating a drag while context menu is
  // up.
  mouse_event_manager_->ReleaseMousePress();
  if (last_scrollbar_under_mouse_)
    last_scrollbar_under_mouse_->MouseUp(event);

  LayoutPoint position_in_contents =
      v->ConvertFromRootFrame(FlooredIntPoint(event.PositionInRootFrame()));
  HitTestRequest request(HitTestRequest::kActive);
  MouseEventWithHitTestResults mev =
      frame_->GetDocument()->PerformMouseEventHitTest(
          request, position_in_contents, event);
  // Since |Document::performMouseEventHitTest()| modifies layout tree for
  // setting hover element, we need to update layout tree for requirement of
  // |SelectionController::sendContextMenuEvent()|.
  frame_->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  GetSelectionController().SendContextMenuEvent(mev, position_in_contents);

  Node* target_node =
      override_target_node ? override_target_node : mev.InnerNode();
  return mouse_event_manager_->DispatchMouseEvent(
      EffectiveMouseEventTargetNode(target_node), EventTypeNames::contextmenu,
      event, mev.GetHitTestResult().CanvasRegionId(), nullptr);
}

static bool ShouldShowContextMenuAtSelection(const FrameSelection& selection) {
  // TODO(editing-dev): The use of UpdateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  selection.GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

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

#if defined(OS_WIN)
  int right_aligned = ::GetSystemMetrics(SM_MENUDROPALIGNMENT);
#else
  int right_aligned = 0;
#endif
  IntPoint location_in_root_frame;

  Element* focused_element =
      override_target_element ? override_target_element : doc->FocusedElement();
  FrameSelection& selection = frame_->Selection();
  VisualViewport& visual_viewport = frame_->GetPage()->GetVisualViewport();

  if (!override_target_element && ShouldShowContextMenuAtSelection(selection)) {
    DCHECK(!doc->NeedsLayoutTreeUpdate());

    IntRect first_rect =
        FirstRectForRange(selection.ComputeVisibleSelectionInDOMTree()
                              .ToNormalizedEphemeralRange());

    int x = right_aligned ? first_rect.MaxX() : first_rect.X();
    // In a multiline edit, firstRect.maxY() would end up on the next line, so
    // take the midpoint.
    int y = (first_rect.MaxY() + first_rect.Y()) / 2;
    location_in_root_frame = view->ConvertToRootFrame(IntPoint(x, y));
  } else if (focused_element) {
    IntRect clipped_rect = focused_element->BoundsInViewport();
    location_in_root_frame =
        visual_viewport.ViewportToRootFrame(clipped_rect.Center());
  } else {
    location_in_root_frame = IntPoint(
        right_aligned
            ? visual_viewport.VisibleRect(kIncludeScrollbars).MaxX() -
                  kContextMenuMargin
            : visual_viewport.GetScrollOffset().Width() + kContextMenuMargin,
        visual_viewport.GetScrollOffset().Height() + kContextMenuMargin);
  }

  frame_->View()->SetCursor(PointerCursor());
  IntPoint location_in_viewport =
      visual_viewport.RootFrameToViewport(location_in_root_frame);
  IntPoint global_position =
      view->GetChromeClient()->ViewportToScreen(
          IntRect(location_in_viewport, IntSize()), frame_->View())
          .Location();

  Node* target_node =
      override_target_element ? override_target_element : doc->FocusedElement();
  if (!target_node)
    target_node = doc;

  // Use the focused node as the target for hover and active.
  HitTestRequest request(HitTestRequest::kActive);
  HitTestLocation location(location_in_root_frame);
  HitTestResult result(request, location);
  result.SetInnerNode(target_node);
  doc->UpdateHoverActiveState(request, result.InnerElement());

  // The contextmenu event is a mouse event even when invoked using the
  // keyboard.  This is required for web compatibility.
  WebInputEvent::Type event_type = WebInputEvent::kMouseDown;
  if (frame_->GetSettings() &&
      frame_->GetSettings()->GetShowContextMenuOnMouseUp())
    event_type = WebInputEvent::kMouseUp;

  WebMouseEvent mouse_event(
      event_type,
      WebFloatPoint(location_in_root_frame.X(), location_in_root_frame.Y()),
      WebFloatPoint(global_position.X(), global_position.Y()),
      WebPointerProperties::Button::kNoButton, /* clickCount */ 0,
      WebInputEvent::kNoModifiers, CurrentTimeTicks(), source_type);

  // TODO(dtapuska): Transition the mouseEvent to be created really in viewport
  // coordinates instead of root frame coordinates.
  mouse_event.SetFrameScale(1);

  return SendContextMenuEvent(mouse_event, override_target_element);
}

void EventHandler::ScheduleHoverStateUpdate() {
  // TODO(https://crbug.com/668758): Use a normal BeginFrame update for this.
  if (!hover_timer_.IsActive() &&
      !mouse_event_manager_->IsMousePositionUnknown())
    hover_timer_.StartOneShot(TimeDelta(), FROM_HERE);
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

bool EventHandler::FakeMouseMovePending() const {
  return mouse_event_manager_->FakeMouseMovePending();
}

void EventHandler::MayUpdateHoverWhenContentUnderMouseChanged(
    MouseEventManager::UpdateHoverReason update_hover_reason) {
  mouse_event_manager_->MayUpdateHoverWhenContentUnderMouseChanged(
      update_hover_reason);
}

void EventHandler::MayUpdateHoverAfterScroll(
    const FloatQuad& scroller_rect_in_frame) {
  mouse_event_manager_->MayUpdateHoverAfterScroll(scroller_rect_in_frame);
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
          mouse_event_manager_->LastKnownMousePosition()));
      HitTestResult result(request, location);
      layout_object->HitTest(location, result);
      frame_->GetDocument()->UpdateHoverActiveState(request,
                                                    result.InnerElement());
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
        request, last_deferred_tap_element_.Get());
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

void EventHandler::DragSourceEndedAt(const WebMouseEvent& event,
                                     DragOperation operation) {
  // Asides from routing the event to the correct frame, the hit test is also an
  // opportunity for Layer to update the :hover and :active pseudoclasses.
  HitTestRequest request(HitTestRequest::kRelease);
  MouseEventWithHitTestResults mev =
      event_handling_util::PerformMouseEventHitTest(frame_, request, event);

  LocalFrame* target_frame;
  if (TargetIsFrame(mev.InnerNode(), target_frame)) {
    if (target_frame) {
      target_frame->GetEventHandler().DragSourceEndedAt(event, operation);
      return;
    }
  }

  mouse_event_manager_->DragSourceEndedAt(event, operation);
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
  DCHECK(!underlying_event || !underlying_event->IsKeyboardEvent() ||
         ToKeyboardEvent(underlying_event)->type() == EventTypeNames::keypress);

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
  WebInputEventResult result =
      subframe->GetEventHandler().HandleMouseReleaseEvent(mev.Event());
  if (result != WebInputEventResult::kNotHandled)
    return result;
  return WebInputEventResult::kHandledSystem;
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

}  // namespace blink
