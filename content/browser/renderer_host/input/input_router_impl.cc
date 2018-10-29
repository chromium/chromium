// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_router_impl.h"

#include <math.h>

#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/input_disposition_handler.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/common/content_constants_internal.h"
#include "content/common/edit_command.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/common/input/web_touch_event_traits.h"
#include "content/common/input_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/input_event_ack_state.h"
#include "ipc/ipc_sender.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using ui::WebInputEventTraits;

namespace {

bool WasHandled(InputEventAckState state) {
  switch (state) {
    case INPUT_EVENT_ACK_STATE_CONSUMED:
    case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS:
    case INPUT_EVENT_ACK_STATE_UNKNOWN:
      return true;
    default:
      return false;
  }
}

ui::WebScopedInputEvent ScaleEvent(const WebInputEvent& event, double scale) {
  std::unique_ptr<blink::WebInputEvent> event_in_viewport =
      ui::ScaleWebInputEvent(event, scale);
  if (event_in_viewport)
    return ui::WebScopedInputEvent(event_in_viewport.release());
  return ui::WebInputEventTraits::Clone(event);
}

}  // namespace

InputRouterImpl::InputRouterImpl(
    InputRouterImplClient* client,
    InputDispositionHandler* disposition_handler,
    FlingControllerSchedulerClient* fling_scheduler_client,
    const Config& config)
    : client_(client),
      disposition_handler_(disposition_handler),
      frame_tree_node_id_(-1),
      active_renderer_fling_count_(0),
      touch_scroll_started_sent_(false),
      wheel_event_queue_(this),
      touch_event_queue_(this, config.touch_config),
      touchpad_pinch_event_queue_(this),
      gesture_event_queue_(this,
                           this,
                           fling_scheduler_client,
                           config.gesture_config),
      device_scale_factor_(1.f),
      host_binding_(this),
      frame_host_binding_(this),
      weak_ptr_factory_(this) {
  weak_this_ = weak_ptr_factory_.GetWeakPtr();

  DCHECK(client);
  DCHECK(disposition_handler);
  DCHECK(fling_scheduler_client);
  UpdateTouchAckTimeoutEnabled();
}

InputRouterImpl::~InputRouterImpl() {}

void InputRouterImpl::SendMouseEvent(
    const MouseEventWithLatencyInfo& mouse_event,
    MouseEventCallback event_result_callback) {
  if ((mouse_event.event.GetType() == WebInputEvent::kMouseDown &&
       gesture_event_queue_.GetTouchpadTapSuppressionController()
           ->ShouldSuppressMouseDown(mouse_event)) ||
      (mouse_event.event.GetType() == WebInputEvent::kMouseUp &&
       gesture_event_queue_.GetTouchpadTapSuppressionController()
           ->ShouldSuppressMouseUp())) {
    std::move(event_result_callback)
        .Run(mouse_event, InputEventAckSource::BROWSER,
             INPUT_EVENT_ACK_STATE_IGNORED);
    return;
  }

  SendMouseEventImmediately(mouse_event, std::move(event_result_callback));
}

void InputRouterImpl::SendWheelEvent(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  wheel_event_queue_.QueueEvent(wheel_event);
}

void InputRouterImpl::SendKeyboardEvent(
    const NativeWebKeyboardEventWithLatencyInfo& key_event,
    KeyboardEventCallback event_result_callback) {
  gesture_event_queue_.StopFling();
  mojom::WidgetInputHandler::DispatchEventCallback callback =
      base::BindOnce(&InputRouterImpl::KeyboardEventHandled, weak_this_,
                     key_event, std::move(event_result_callback));
  FilterAndSendWebInputEvent(key_event.event, key_event.latency,
                             std::move(callback));
}

void InputRouterImpl::SendGestureEvent(
    const GestureEventWithLatencyInfo& original_gesture_event) {
  input_stream_validator_.Validate(original_gesture_event.event,
                                   FlingCancellationIsDeferred());

  GestureEventWithLatencyInfo gesture_event(original_gesture_event);

  if (gesture_event_queue_.FlingControllerFilterEvent(gesture_event)) {
    disposition_handler_->OnGestureEventAck(gesture_event,
                                            InputEventAckSource::BROWSER,
                                            INPUT_EVENT_ACK_STATE_CONSUMED);
    return;
  }

  if (touch_action_filter_.FilterGestureEvent(&gesture_event.event) ==
      FilterGestureEventResult::kFilterGestureEventFiltered) {
    disposition_handler_->OnGestureEventAck(gesture_event,
                                            InputEventAckSource::BROWSER,
                                            INPUT_EVENT_ACK_STATE_CONSUMED);
    return;
  }

  wheel_event_queue_.OnGestureScrollEvent(gesture_event);

  if (gesture_event.event.SourceDevice() ==
      blink::kWebGestureDeviceTouchscreen) {
    if (gesture_event.event.GetType() ==
        blink::WebInputEvent::kGestureScrollBegin) {
      touch_scroll_started_sent_ = false;
    } else if (!touch_scroll_started_sent_ &&
               gesture_event.event.GetType() ==
                   blink::WebInputEvent::kGestureScrollUpdate) {
      // A touch scroll hasn't really started until the first
      // GestureScrollUpdate event.  Eg. if the page consumes all touchmoves
      // then no scrolling really ever occurs (even though we still send
      // GestureScrollBegin).
      touch_scroll_started_sent_ = true;
      touch_event_queue_.PrependTouchScrollNotification();
    }
    touch_event_queue_.OnGestureScrollEvent(gesture_event);
  }

  if (gesture_event.event.IsTouchpadZoomEvent() &&
      gesture_event.event.NeedsWheelEvent()) {
    touchpad_pinch_event_queue_.QueueEvent(gesture_event);
    return;
  }

  if (!gesture_event_queue_.DebounceOrQueueEvent(gesture_event)) {
    disposition_handler_->OnGestureEventAck(gesture_event,
                                            InputEventAckSource::BROWSER,
                                            INPUT_EVENT_ACK_STATE_CONSUMED);
  }
}

void InputRouterImpl::SendTouchEvent(
    const TouchEventWithLatencyInfo& touch_event) {
  TouchEventWithLatencyInfo updated_touch_event = touch_event;
  SetMovementXYForTouchPoints(&updated_touch_event.event);
  input_stream_validator_.Validate(updated_touch_event.event);
  touch_event_queue_.QueueEvent(updated_touch_event);
}

void InputRouterImpl::NotifySiteIsMobileOptimized(bool is_mobile_optimized) {
  touch_event_queue_.SetIsMobileOptimizedSite(is_mobile_optimized);
}

bool InputRouterImpl::HasPendingEvents() const {
  return !touch_event_queue_.Empty() || !gesture_event_queue_.empty() ||
         wheel_event_queue_.has_pending() ||
         touchpad_pinch_event_queue_.has_pending() ||
         active_renderer_fling_count_ > 0;
}

void InputRouterImpl::SetDeviceScaleFactor(float device_scale_factor) {
  device_scale_factor_ = device_scale_factor;
}

void InputRouterImpl::SetFrameTreeNodeId(int frame_tree_node_id) {
  frame_tree_node_id_ = frame_tree_node_id;
}

void InputRouterImpl::SetForceEnableZoom(bool enabled) {
  touch_action_filter_.SetForceEnableZoom(enabled);
}

base::Optional<cc::TouchAction> InputRouterImpl::AllowedTouchAction() {
  return touch_action_filter_.allowed_touch_action();
}

void InputRouterImpl::BindHost(mojom::WidgetInputHandlerHostRequest request,
                               bool frame_handler) {
  if (frame_handler) {
    frame_host_binding_.Close();
    frame_host_binding_.Bind(std::move(request));
  } else {
    host_binding_.Close();
    host_binding_.Bind(std::move(request));
  }
}

void InputRouterImpl::StopFling() {
  gesture_event_queue_.StopFling();
}

bool InputRouterImpl::FlingCancellationIsDeferred() {
  return gesture_event_queue_.FlingCancellationIsDeferred();
}

void InputRouterImpl::CancelTouchTimeout() {
  UpdateTouchAckTimeoutEnabled();
}

void InputRouterImpl::SetWhiteListedTouchAction(cc::TouchAction touch_action,
                                                uint32_t unique_touch_event_id,
                                                InputEventAckState state) {
  touch_action_filter_.OnSetWhiteListedTouchAction(touch_action);
  client_->OnSetWhiteListedTouchAction(touch_action);
}

void InputRouterImpl::DidOverscroll(const ui::DidOverscrollParams& params) {
  // Touchpad and Touchscreen flings are handled on the browser side.
  ui::DidOverscrollParams fling_updated_params = params;
  fling_updated_params.current_fling_velocity =
      gesture_event_queue_.CurrentFlingVelocity();
  client_->DidOverscroll(fling_updated_params);
}

void InputRouterImpl::DidStartScrollingViewport() {
  client_->DidStartScrollingViewport();
}

void InputRouterImpl::ImeCancelComposition() {
  client_->OnImeCancelComposition();
}

void InputRouterImpl::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& bounds) {
  client_->OnImeCompositionRangeChanged(range, bounds);
}

void InputRouterImpl::SetMouseCapture(bool capture) {
  client_->SetMouseCapture(capture);
}

void InputRouterImpl::SetMovementXYForTouchPoints(blink::WebTouchEvent* event) {
  for (size_t i = 0; i < event->touches_length; ++i) {
    blink::WebTouchPoint* touch_point = &event->touches[i];
    if (touch_point->state == blink::WebTouchPoint::kStateMoved) {
      const gfx::Point& last_position = global_touch_position_[touch_point->id];
      touch_point->movement_x =
          touch_point->PositionInScreen().x - last_position.x();
      touch_point->movement_y =
          touch_point->PositionInScreen().y - last_position.y();
      global_touch_position_[touch_point->id].SetPoint(
          touch_point->PositionInScreen().x, touch_point->PositionInScreen().y);
    } else {
      touch_point->movement_x = 0;
      touch_point->movement_y = 0;
      if (touch_point->state == blink::WebTouchPoint::kStateReleased ||
          touch_point->state == blink::WebTouchPoint::kStateCancelled) {
        global_touch_position_.erase(touch_point->id);
      } else if (touch_point->state == blink::WebTouchPoint::kStatePressed) {
        DCHECK(global_touch_position_.find(touch_point->id) ==
               global_touch_position_.end());
        global_touch_position_[touch_point->id] =
            gfx::Point(touch_point->PositionInScreen().x,
                       touch_point->PositionInScreen().y);
      }
    }
  }
}

// Forwards MouseEvent without passing it through
// TouchpadTapSuppressionController.
void InputRouterImpl::SendMouseEventImmediately(
    const MouseEventWithLatencyInfo& mouse_event,
    MouseEventCallback event_result_callback) {
  mojom::WidgetInputHandler::DispatchEventCallback callback =
      base::BindOnce(&InputRouterImpl::MouseEventHandled, weak_this_,
                     mouse_event, std::move(event_result_callback));
  FilterAndSendWebInputEvent(mouse_event.event, mouse_event.latency,
                             std::move(callback));
}

void InputRouterImpl::SendTouchEventImmediately(
    const TouchEventWithLatencyInfo& touch_event) {
  mojom::WidgetInputHandler::DispatchEventCallback callback = base::BindOnce(
      &InputRouterImpl::TouchEventHandled, weak_this_, touch_event);
  FilterAndSendWebInputEvent(touch_event.event, touch_event.latency,
                             std::move(callback));
}

void InputRouterImpl::OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                                      InputEventAckSource ack_source,
                                      InputEventAckState ack_result) {
  if (WebTouchEventTraits::IsTouchSequenceStart(event.event)) {
    touch_action_filter_.AppendToGestureSequenceForDebugging("T");
    touch_action_filter_.AppendToGestureSequenceForDebugging(
        base::NumberToString(ack_result).c_str());
    touch_action_filter_.AppendToGestureSequenceForDebugging(
        base::NumberToString(event.event.unique_touch_event_id).c_str());
    touch_action_filter_.IncreaseActiveTouches();
    // There are some cases the touch action may not have value when receiving
    // the ACK for the touch start, such as input ack state is
    // NO_CONSUMER_EXISTS, or the renderer has swapped out. In these cases, set
    // touch action Auto.
    if (!touch_action_filter_.allowed_touch_action().has_value()) {
      touch_action_filter_.OnSetTouchAction(cc::kTouchActionAuto);
      UpdateTouchAckTimeoutEnabled();
    }
  }
  disposition_handler_->OnTouchEventAck(event, ack_source, ack_result);

  if (WebTouchEventTraits::IsTouchSequenceEnd(event.event)) {
    touch_action_filter_.AppendToGestureSequenceForDebugging("E");
    touch_action_filter_.AppendToGestureSequenceForDebugging(
        base::NumberToString(event.event.unique_touch_event_id).c_str());
    touch_action_filter_.DecreaseActiveTouches();
    touch_action_filter_.ReportAndResetTouchAction();
    UpdateTouchAckTimeoutEnabled();
  }
}

void InputRouterImpl::OnFilteringTouchEvent(const WebTouchEvent& touch_event) {
  // The event stream given to the renderer is not guaranteed to be
  // valid based on the current TouchEventStreamValidator rules. This event will
  // never be given to the renderer, but in order to ensure that the event
  // stream |output_stream_validator_| sees is valid, we give events which are
  // filtered out to the validator. crbug.com/589111 proposes adding an
  // additional validator for the events which are actually sent to the
  // renderer.
  output_stream_validator_.Validate(touch_event);
}

void InputRouterImpl::SendGestureEventImmediately(
    const GestureEventWithLatencyInfo& gesture_event) {
  mojom::WidgetInputHandler::DispatchEventCallback callback = base::BindOnce(
      &InputRouterImpl::GestureEventHandled, weak_this_, gesture_event);
  FilterAndSendWebInputEvent(gesture_event.event, gesture_event.latency,
                             std::move(callback));
}

void InputRouterImpl::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckSource ack_source,
    InputEventAckState ack_result) {
  touch_event_queue_.OnGestureEventAck(event, ack_result);
  disposition_handler_->OnGestureEventAck(event, ack_source, ack_result);
}

void InputRouterImpl::SendGeneratedWheelEvent(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  client_->ForwardWheelEventWithLatencyInfo(wheel_event.event,
                                            wheel_event.latency);
}

void InputRouterImpl::SendGeneratedGestureScrollEvents(
    const GestureEventWithLatencyInfo& gesture_event) {
  client_->ForwardGestureEventWithLatencyInfo(gesture_event.event,
                                              gesture_event.latency);
}

void InputRouterImpl::SendMouseWheelEventImmediately(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  mojom::WidgetInputHandler::DispatchEventCallback callback = base::BindOnce(
      &InputRouterImpl::MouseWheelEventHandled, weak_this_, wheel_event);
  FilterAndSendWebInputEvent(wheel_event.event, wheel_event.latency,
                             std::move(callback));
}

void InputRouterImpl::OnMouseWheelEventAck(
    const MouseWheelEventWithLatencyInfo& event,
    InputEventAckSource ack_source,
    InputEventAckState ack_result) {
  disposition_handler_->OnWheelEventAck(event, ack_source, ack_result);
}

void InputRouterImpl::ForwardGestureEventWithLatencyInfo(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo& latency_info) {
  client_->ForwardGestureEventWithLatencyInfo(event, latency_info);
}

void InputRouterImpl::SendMouseWheelEventForPinchImmediately(
    const MouseWheelEventWithLatencyInfo& event) {
  SendMouseWheelEventImmediately(event);
}

void InputRouterImpl::OnGestureEventForPinchAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckSource ack_source,
    InputEventAckState ack_result) {
  OnGestureEventAck(event, ack_source, ack_result);
}

bool InputRouterImpl::IsWheelScrollInProgress() {
  return client_->IsWheelScrollInProgress();
}

void InputRouterImpl::FilterAndSendWebInputEvent(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info,
    mojom::WidgetInputHandler::DispatchEventCallback callback) {
  TRACE_EVENT1("input", "InputRouterImpl::FilterAndSendWebInputEvent", "type",
               WebInputEvent::GetName(input_event.GetType()));
  TRACE_EVENT_WITH_FLOW2(
      "input,benchmark,devtools.timeline", "LatencyInfo.Flow",
      TRACE_ID_DONT_MANGLE(latency_info.trace_id()),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "SendInputEventUI", "frameTreeNodeId", frame_tree_node_id_);

  output_stream_validator_.Validate(input_event);
  InputEventAckState filtered_state =
      client_->FilterInputEvent(input_event, latency_info);
  if (WasHandled(filtered_state)) {
    TRACE_EVENT_INSTANT0("input", "InputEventFiltered",
                         TRACE_EVENT_SCOPE_THREAD);
    if (filtered_state != INPUT_EVENT_ACK_STATE_UNKNOWN) {
      std::move(callback).Run(InputEventAckSource::BROWSER, latency_info,
                              filtered_state, base::nullopt, base::nullopt);
    }
    return;
  }

  std::unique_ptr<InputEvent> event = std::make_unique<InputEvent>(
      ScaleEvent(input_event, device_scale_factor_), latency_info);
  if (WebInputEventTraits::ShouldBlockEventStream(input_event)) {
    TRACE_EVENT_INSTANT0("input", "InputEventSentBlocking",
                         TRACE_EVENT_SCOPE_THREAD);
    client_->IncrementInFlightEventCount();
    client_->GetWidgetInputHandler()->DispatchEvent(std::move(event),
                                                    std::move(callback));
  } else {
    TRACE_EVENT_INSTANT0("input", "InputEventSentNonBlocking",
                         TRACE_EVENT_SCOPE_THREAD);
    client_->GetWidgetInputHandler()->DispatchNonBlockingEvent(
        std::move(event));
    std::move(callback).Run(InputEventAckSource::BROWSER, latency_info,
                            INPUT_EVENT_ACK_STATE_IGNORED, base::nullopt,
                            base::nullopt);
  }
}

void InputRouterImpl::KeyboardEventHandled(
    const NativeWebKeyboardEventWithLatencyInfo& event,
    KeyboardEventCallback event_result_callback,
    InputEventAckSource source,
    const ui::LatencyInfo& latency,
    InputEventAckState state,
    const base::Optional<ui::DidOverscrollParams>& overscroll,
    const base::Optional<cc::TouchAction>& touch_action) {
  TRACE_EVENT2("input", "InputRouterImpl::KeboardEventHandled", "type",
               WebInputEvent::GetName(event.event.GetType()), "ack",
               InputEventAckStateToString(state));

  if (source != InputEventAckSource::BROWSER)
    client_->DecrementInFlightEventCount(source);
  event.latency.AddNewLatencyFrom(latency);
  std::move(event_result_callback).Run(event, source, state);

  // WARNING: This InputRouterImpl can be deallocated at this point
  // (i.e.  in the case of Ctrl+W, where the call to
  // HandleKeyboardEvent destroys this InputRouterImpl).
  // TODO(jdduke): crbug.com/274029 - Make ack-triggered shutdown async.
}

void InputRouterImpl::MouseEventHandled(
    const MouseEventWithLatencyInfo& event,
    MouseEventCallback event_result_callback,
    InputEventAckSource source,
    const ui::LatencyInfo& latency,
    InputEventAckState state,
    const base::Optional<ui::DidOverscrollParams>& overscroll,
    const base::Optional<cc::TouchAction>& touch_action) {
  TRACE_EVENT2("input", "InputRouterImpl::MouseEventHandled", "type",
               WebInputEvent::GetName(event.event.GetType()), "ack",
               InputEventAckStateToString(state));

  if (source != InputEventAckSource::BROWSER)
    client_->DecrementInFlightEventCount(source);
  event.latency.AddNewLatencyFrom(latency);
  std::move(event_result_callback).Run(event, source, state);
}

void InputRouterImpl::TouchEventHandled(
    const TouchEventWithLatencyInfo& touch_event,
    InputEventAckSource source,
    const ui::LatencyInfo& latency,
    InputEventAckState state,
    const base::Optional<ui::DidOverscrollParams>& overscroll,
    const base::Optional<cc::TouchAction>& touch_action) {
  TRACE_EVENT2("input", "InputRouterImpl::TouchEventHandled", "type",
               WebInputEvent::GetName(touch_event.event.GetType()), "ack",
               InputEventAckStateToString(state));
  if (source != InputEventAckSource::BROWSER)
    client_->DecrementInFlightEventCount(source);
  touch_event.latency.AddNewLatencyFrom(latency);

  // The SetTouchAction IPC occurs on a different channel so always
  // send it in the input event ack to ensure it is available at the
  // time the ACK is handled.
  if (touch_action.has_value())
    OnSetTouchAction(touch_action.value());

  // |touch_event_queue_| will forward to OnTouchEventAck when appropriate.
  touch_event_queue_.ProcessTouchAck(source, state, latency,
                                     touch_event.event.unique_touch_event_id);
}

void InputRouterImpl::GestureEventHandled(
    const GestureEventWithLatencyInfo& gesture_event,
    InputEventAckSource source,
    const ui::LatencyInfo& latency,
    InputEventAckState state,
    const base::Optional<ui::DidOverscrollParams>& overscroll,
    const base::Optional<cc::TouchAction>& touch_action) {
  TRACE_EVENT2("input", "InputRouterImpl::GestureEventHandled", "type",
               WebInputEvent::GetName(gesture_event.event.GetType()), "ack",
               InputEventAckStateToString(state));
  if (source != InputEventAckSource::BROWSER)
    client_->DecrementInFlightEventCount(source);

  if (overscroll) {
    DCHECK_EQ(WebInputEvent::kGestureScrollUpdate,
              gesture_event.event.GetType());
    DidOverscroll(overscroll.value());
  }

  // |gesture_event_queue_| will forward to OnGestureEventAck when appropriate.
  gesture_event_queue_.ProcessGestureAck(
      source, state, gesture_event.event.GetType(), latency);
}

void InputRouterImpl::MouseWheelEventHandled(
    const MouseWheelEventWithLatencyInfo& event,
    InputEventAckSource source,
    const ui::LatencyInfo& latency,
    InputEventAckState state,
    const base::Optional<ui::DidOverscrollParams>& overscroll,
    const base::Optional<cc::TouchAction>& touch_action) {
  TRACE_EVENT2("input", "InputRouterImpl::MouseWheelEventHandled", "type",
               WebInputEvent::GetName(event.event.GetType()), "ack",
               InputEventAckStateToString(state));
  if (source != InputEventAckSource::BROWSER)
    client_->DecrementInFlightEventCount(source);
  event.latency.AddNewLatencyFrom(latency);

  if (overscroll)
    DidOverscroll(overscroll.value());

  wheel_event_queue_.ProcessMouseWheelAck(source, state, event.latency);
  touchpad_pinch_event_queue_.ProcessMouseWheelAck(source, state,
                                                   event.latency);
}

void InputRouterImpl::OnHasTouchEventHandlers(bool has_handlers) {
  TRACE_EVENT1("input", "InputRouterImpl::OnHasTouchEventHandlers",
               "has_handlers", has_handlers);

  touch_action_filter_.OnHasTouchEventHandlers(has_handlers);
  touch_event_queue_.OnHasTouchEventHandlers(has_handlers);
}

void InputRouterImpl::ForceSetTouchActionAuto() {
  touch_action_filter_.AppendToGestureSequenceForDebugging("F");
  touch_action_filter_.OnSetTouchAction(cc::kTouchActionAuto);
}

void InputRouterImpl::ForceResetTouchActionForTest() {
  touch_action_filter_.ForceResetTouchActionForTest();
}

void InputRouterImpl::OnSetTouchAction(cc::TouchAction touch_action) {
  TRACE_EVENT1("input", "InputRouterImpl::OnSetTouchAction", "action",
               touch_action);

  // It is possible we get a touch action for a touch start that is no longer
  // in the queue. eg. Events that have fired the Touch ACK timeout.
  if (!touch_event_queue_.IsPendingAckTouchStart())
    return;

  touch_action_filter_.AppendToGestureSequenceForDebugging("S");
  touch_action_filter_.AppendToGestureSequenceForDebugging(
      base::NumberToString(touch_action).c_str());
  touch_action_filter_.OnSetTouchAction(touch_action);

  // kTouchActionNone should disable the touch ack timeout.
  UpdateTouchAckTimeoutEnabled();
}

void InputRouterImpl::UpdateTouchAckTimeoutEnabled() {
  // kTouchActionNone will prevent scrolling, in which case the timeout serves
  // little purpose. It's also a strong signal that touch handling is critical
  // to page functionality, so the timeout could do more harm than good.
  const bool touch_ack_timeout_enabled =
      touch_action_filter_.allowed_touch_action() != cc::kTouchActionNone;
  touch_event_queue_.SetAckTimeoutEnabled(touch_ack_timeout_enabled);
}

}  // namespace content
