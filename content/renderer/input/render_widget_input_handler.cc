// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/render_widget_input_handler.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "cc/trees/swap_promise_monitor.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/common/input/input_event_ack.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/input_event_ack_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/gpu/layer_tree_view.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_widget.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_float_size.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_pointer_event.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/latency/latency_info.h"

#if defined(OS_ANDROID)
#include <android/keycodes.h>
#endif

using blink::WebFloatPoint;
using blink::WebFloatSize;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebInputEventResult;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPointerEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::DidOverscrollParams;

namespace content {

namespace {

int64_t GetEventLatencyMicros(base::TimeTicks event_timestamp,
                              base::TimeTicks now) {
  return (now - event_timestamp).InMicroseconds();
}

void LogInputEventLatencyUma(const WebInputEvent& event, base::TimeTicks now) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Event.AggregatedLatency.Renderer2",
                              GetEventLatencyMicros(event.TimeStamp(), now), 1,
                              10000000, 100);
}

void LogPassiveEventListenersUma(WebInputEventResult result,
                                 WebInputEvent::DispatchType dispatch_type,
                                 base::TimeTicks event_timestamp,
                                 const ui::LatencyInfo& latency_info) {
  // This enum is backing a histogram. Do not remove or reorder members.
  enum ListenerEnum {
    PASSIVE_LISTENER_UMA_ENUM_PASSIVE,
    PASSIVE_LISTENER_UMA_ENUM_UNCANCELABLE,
    PASSIVE_LISTENER_UMA_ENUM_SUPPRESSED,
    PASSIVE_LISTENER_UMA_ENUM_CANCELABLE,
    PASSIVE_LISTENER_UMA_ENUM_CANCELABLE_AND_CANCELED,
    PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING,
    PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_MAIN_THREAD_RESPONSIVENESS_DEPRECATED,
    PASSIVE_LISTENER_UMA_ENUM_COUNT
  };

  ListenerEnum enum_value;
  switch (dispatch_type) {
    case WebInputEvent::kListenersForcedNonBlockingDueToFling:
      enum_value = PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING;
      break;
    case WebInputEvent::kListenersNonBlockingPassive:
      enum_value = PASSIVE_LISTENER_UMA_ENUM_PASSIVE;
      break;
    case WebInputEvent::kEventNonBlocking:
      enum_value = PASSIVE_LISTENER_UMA_ENUM_UNCANCELABLE;
      break;
    case WebInputEvent::kBlocking:
      if (result == WebInputEventResult::kHandledApplication)
        enum_value = PASSIVE_LISTENER_UMA_ENUM_CANCELABLE_AND_CANCELED;
      else if (result == WebInputEventResult::kHandledSuppressed)
        enum_value = PASSIVE_LISTENER_UMA_ENUM_SUPPRESSED;
      else
        enum_value = PASSIVE_LISTENER_UMA_ENUM_CANCELABLE;
      break;
    default:
      NOTREACHED();
      return;
  }

  UMA_HISTOGRAM_ENUMERATION("Event.PassiveListeners", enum_value,
                            PASSIVE_LISTENER_UMA_ENUM_COUNT);

  if (base::TimeTicks::IsHighResolution()) {
    if (enum_value == PASSIVE_LISTENER_UMA_ENUM_CANCELABLE) {
      base::TimeTicks now = base::TimeTicks::Now();
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.PassiveListeners.Latency",
                                  GetEventLatencyMicros(event_timestamp, now),
                                  1, 10000000, 100);
    } else if (enum_value ==
               PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING) {
      base::TimeTicks now = base::TimeTicks::Now();
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.PassiveListeners.ForcedNonBlockingLatencyDueToFling",
          GetEventLatencyMicros(event_timestamp, now), 1, 10000000, 100);
    }
  }
}

void LogAllPassiveEventListenersUma(const WebInputEvent& input_event,
                                    WebInputEventResult result,
                                    const ui::LatencyInfo& latency_info) {
  // TODO(dtapuska): Use the input_event.timeStampSeconds as the start
  // ideally this should be when the event was sent by the compositor to the
  // renderer. https://crbug.com/565348.
  if (input_event.GetType() == WebInputEvent::kTouchStart ||
      input_event.GetType() == WebInputEvent::kTouchMove ||
      input_event.GetType() == WebInputEvent::kTouchEnd) {
    const WebTouchEvent& touch = static_cast<const WebTouchEvent&>(input_event);

    LogPassiveEventListenersUma(result, touch.dispatch_type,
                                input_event.TimeStamp(), latency_info);
  } else if (input_event.GetType() == WebInputEvent::kMouseWheel) {
    LogPassiveEventListenersUma(
        result,
        static_cast<const WebMouseWheelEvent&>(input_event).dispatch_type,
        input_event.TimeStamp(), latency_info);
  }
}

blink::WebCoalescedInputEvent GetCoalescedWebPointerEventForTouch(
    const WebPointerEvent& pointer_event,
    std::vector<const WebInputEvent*> coalesced_events,
    std::vector<const WebInputEvent*> predicted_events) {
  std::vector<WebPointerEvent> related_pointer_events;
  for (const WebInputEvent* event : coalesced_events) {
    DCHECK(WebInputEvent::IsTouchEventType(event->GetType()));
    const WebTouchEvent& touch_event =
        static_cast<const WebTouchEvent&>(*event);
    for (unsigned i = 0; i < touch_event.touches_length; ++i) {
      if (touch_event.touches[i].id == pointer_event.id &&
          touch_event.touches[i].state != WebTouchPoint::kStateStationary) {
        related_pointer_events.push_back(
            WebPointerEvent(touch_event, touch_event.touches[i]));
      }
    }
  }
  std::vector<WebPointerEvent> predicted_pointer_events;
  for (const WebInputEvent* event : predicted_events) {
    DCHECK(WebInputEvent::IsTouchEventType(event->GetType()));
    const WebTouchEvent& touch_event =
        static_cast<const WebTouchEvent&>(*event);
    for (unsigned i = 0; i < touch_event.touches_length; ++i) {
      if (touch_event.touches[i].id == pointer_event.id &&
          touch_event.touches[i].state != WebTouchPoint::kStateStationary) {
        predicted_pointer_events.push_back(
            WebPointerEvent(touch_event, touch_event.touches[i]));
      }
    }
  }

  return blink::WebCoalescedInputEvent(pointer_event, related_pointer_events,
                                       predicted_pointer_events);
}

viz::FrameSinkId GetRemoteFrameSinkId(const blink::WebNode& node) {
  blink::WebFrame* result_frame = blink::WebFrame::FromFrameOwnerElement(node);
  if (result_frame && result_frame->IsWebRemoteFrame()) {
    return RenderFrameProxy::FromWebFrame(result_frame->ToWebRemoteFrame())
        ->frame_sink_id();
  }
  auto* plugin = BrowserPlugin::GetFromNode(node);
  return plugin ? plugin->frame_sink_id() : viz::FrameSinkId();
}

}  // namespace

RenderWidgetInputHandler::RenderWidgetInputHandler(
    RenderWidgetInputHandlerDelegate* delegate,
    RenderWidget* widget)
    : delegate_(delegate),
      widget_(widget),
      handling_input_event_(false),
      handling_event_overscroll_(nullptr),
      handling_event_type_(WebInputEvent::kUndefined),
      suppress_next_char_events_(false) {
  DCHECK(delegate);
  DCHECK(widget);
  delegate->SetInputHandler(this);
}

RenderWidgetInputHandler::~RenderWidgetInputHandler() {}

viz::FrameSinkId RenderWidgetInputHandler::GetFrameSinkIdAtPoint(
    const gfx::PointF& point,
    gfx::PointF* local_point) {
  gfx::PointF point_in_pixel(point);
  if (widget_->compositor_deps()->IsUseZoomForDSFEnabled()) {
    point_in_pixel = gfx::ConvertPointToPixel(
        widget_->GetOriginalScreenInfo().device_scale_factor, point_in_pixel);
  }
  blink::WebHitTestResult result = widget_->GetWebWidget()->HitTestResultAt(
      blink::WebPoint(ToRoundedPoint(point_in_pixel)));

  blink::WebNode result_node = result.GetNode();
  *local_point = gfx::PointF(point);

  // TODO(crbug.com/797828): When the node is null the caller may
  // need to do extra checks. Like maybe update the layout and then
  // call the hit-testing API. Either way it might be better to have
  // a DCHECK for the node rather than a null check here.
  if (result_node.IsNull()) {
    return viz::FrameSinkId(RenderThread::Get()->GetClientId(),
                            widget_->routing_id());
  }

  viz::FrameSinkId frame_sink_id = GetRemoteFrameSinkId(result_node);
  if (frame_sink_id.is_valid()) {
    *local_point = gfx::PointF(result.LocalPointWithoutContentBoxOffset());
    if (widget_->compositor_deps()->IsUseZoomForDSFEnabled()) {
      *local_point = gfx::ConvertPointToDIP(
          widget_->GetOriginalScreenInfo().device_scale_factor, *local_point);
    }
    return frame_sink_id;
  }

  // Return the FrameSinkId for the current widget if the point did not hit
  // test to a remote frame, or the remote frame doesn't have a valid
  // FrameSinkId yet.
  return viz::FrameSinkId(RenderThread::Get()->GetClientId(),
                          widget_->routing_id());
}

WebInputEventResult RenderWidgetInputHandler::HandleTouchEvent(
    const blink::WebCoalescedInputEvent& coalesced_event) {
  const WebInputEvent& input_event = coalesced_event.Event();

  if (input_event.GetType() == WebInputEvent::kTouchScrollStarted) {
    WebPointerEvent pointer_event =
        WebPointerEvent::CreatePointerCausesUaActionEvent(
            blink::WebPointerProperties::PointerType::kUnknown,
            input_event.TimeStamp());
    return widget_->GetWebWidget()->HandleInputEvent(
        blink::WebCoalescedInputEvent(pointer_event));
  }

  const WebTouchEvent touch_event =
      static_cast<const WebTouchEvent&>(input_event);
  for (unsigned i = 0; i < touch_event.touches_length; ++i) {
    const WebTouchPoint& touch_point = touch_event.touches[i];
    if (touch_point.state != blink::WebTouchPoint::kStateStationary) {
      const WebPointerEvent& pointer_event =
          WebPointerEvent(touch_event, touch_point);
      const blink::WebCoalescedInputEvent& coalesced_pointer_event =
          GetCoalescedWebPointerEventForTouch(
              pointer_event, coalesced_event.GetCoalescedEventsPointers(),
              coalesced_event.GetPredictedEventsPointers());
      widget_->GetWebWidget()->HandleInputEvent(coalesced_pointer_event);
    }
  }
  return widget_->GetWebWidget()->DispatchBufferedTouchEvents();
}

void RenderWidgetInputHandler::HandleInputEvent(
    const blink::WebCoalescedInputEvent& coalesced_event,
    const ui::LatencyInfo& latency_info,
    HandledEventCallback callback) {
  const WebInputEvent& input_event = coalesced_event.Event();
  base::AutoReset<bool> handling_input_event_resetter(&handling_input_event_,
                                                      true);
  base::AutoReset<WebInputEvent::Type> handling_event_type_resetter(
      &handling_event_type_, input_event.GetType());

  // Calls into |didOverscroll()| while handling this event will populate
  // |event_overscroll|, which in turn will be bundled with the event ack.
  std::unique_ptr<DidOverscrollParams> event_overscroll;
  base::AutoReset<std::unique_ptr<DidOverscrollParams>*>
      handling_event_overscroll_resetter(&handling_event_overscroll_,
                                         &event_overscroll);

  // Calls into |ProcessTouchAction()| while handling this event will
  // populate |handling_touch_action_|, which in turn will be bundled with
  // the event ack.
  base::AutoReset<base::Optional<cc::TouchAction>>
      handling_touch_action_resetter(&handling_touch_action_, base::nullopt);

#if defined(OS_ANDROID)
  ImeEventGuard guard(widget_);
#endif

  base::TimeTicks start_time;
  if (base::TimeTicks::IsHighResolution())
    start_time = base::TimeTicks::Now();

  TRACE_EVENT1("renderer,benchmark,rail",
               "RenderWidgetInputHandler::OnHandleInputEvent", "event",
               WebInputEvent::GetName(input_event.GetType()));
  TRACE_EVENT_WITH_FLOW1("input,benchmark", "LatencyInfo.Flow",
                         TRACE_ID_DONT_MANGLE(latency_info.trace_id()),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "HandleInputEventMain");

  // If we don't have a high res timer, these metrics won't be accurate enough
  // to be worth collecting. Note that this does introduce some sampling bias.
  if (!start_time.is_null())
    LogInputEventLatencyUma(input_event, start_time);

  std::unique_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor;
  ui::LatencyInfo swap_latency_info(latency_info);

  swap_latency_info.AddLatencyNumber(
      ui::LatencyComponentType::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT);
  if (widget_->layer_tree_view()) {
    latency_info_swap_promise_monitor =
        widget_->layer_tree_view()->CreateLatencyInfoSwapPromiseMonitor(
            &swap_latency_info);
  }

  bool prevent_default = false;
  if (WebInputEvent::IsMouseEventType(input_event.GetType())) {
    const WebMouseEvent& mouse_event =
        static_cast<const WebMouseEvent&>(input_event);
    TRACE_EVENT2("renderer", "HandleMouseMove", "x",
                 mouse_event.PositionInWidget().x, "y",
                 mouse_event.PositionInWidget().y);
    prevent_default = delegate_->WillHandleMouseEvent(mouse_event);
  }

  if (WebInputEvent::IsKeyboardEventType(input_event.GetType())) {
#if defined(OS_ANDROID)
    // The DPAD_CENTER key on Android has a dual semantic: (1) in the general
    // case it should behave like a select key (i.e. causing a click if a button
    // is focused). However, if a text field is focused (2), its intended
    // behavior is to just show the IME and don't propagate the key.
    // A typical use case is a web form: the DPAD_CENTER should bring up the IME
    // when clicked on an input text field and cause the form submit if clicked
    // when the submit button is focused, but not vice-versa.
    // The UI layer takes care of translating DPAD_CENTER into a RETURN key,
    // but at this point we have to swallow the event for the scenario (2).
    const WebKeyboardEvent& key_event =
        static_cast<const WebKeyboardEvent&>(input_event);
    if (key_event.native_key_code == AKEYCODE_DPAD_CENTER &&
        widget_->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE) {
      // Show the keyboard on keyup (not keydown) to match the behavior of
      // Android's TextView.
      if (key_event.GetType() == WebInputEvent::kKeyUp)
        widget_->ShowVirtualKeyboardOnElementFocus();
      // Prevent default for both keydown and keyup (letting the keydown go
      // through to the web app would cause compatibility problems since
      // DPAD_CENTER is also used as a "confirm" button).
      prevent_default = true;
    }
#endif
  }

  if (WebInputEvent::IsGestureEventType(input_event.GetType())) {
    const WebGestureEvent& gesture_event =
        static_cast<const WebGestureEvent&>(input_event);
    prevent_default =
        prevent_default || delegate_->WillHandleGestureEvent(gesture_event);
  }

  WebInputEventResult processed = prevent_default
                                      ? WebInputEventResult::kHandledSuppressed
                                      : WebInputEventResult::kNotHandled;
  if (input_event.GetType() != WebInputEvent::kChar ||
      !suppress_next_char_events_) {
    suppress_next_char_events_ = false;
    if (processed == WebInputEventResult::kNotHandled &&
        widget_->GetWebWidget()) {
      if (!widget_->GetWebWidget()->IsPepperWidget() &&
          WebInputEvent::IsTouchEventType(input_event.GetType()))
        processed = HandleTouchEvent(coalesced_event);
      else
        processed = widget_->GetWebWidget()->HandleInputEvent(coalesced_event);
    }
  }

  LogAllPassiveEventListenersUma(input_event, processed, latency_info);

  // If this RawKeyDown event corresponds to a browser keyboard shortcut and
  // it's not processed by webkit, then we need to suppress the upcoming Char
  // events.
  bool is_keyboard_shortcut =
      input_event.GetType() == WebInputEvent::kRawKeyDown &&
      static_cast<const WebKeyboardEvent&>(input_event).is_browser_shortcut;
  if (processed == WebInputEventResult::kNotHandled && is_keyboard_shortcut)
    suppress_next_char_events_ = true;

  InputEventAckState ack_result = processed == WebInputEventResult::kNotHandled
                                      ? INPUT_EVENT_ACK_STATE_NOT_CONSUMED
                                      : INPUT_EVENT_ACK_STATE_CONSUMED;

  // Send gesture scroll events and their dispositions to the compositor thread,
  // so that they can be used to produce the elastic overscroll effect on Mac.
  if (input_event.GetType() == WebInputEvent::kGestureScrollBegin ||
      input_event.GetType() == WebInputEvent::kGestureScrollEnd ||
      input_event.GetType() == WebInputEvent::kGestureScrollUpdate) {
    const WebGestureEvent& gesture_event =
        static_cast<const WebGestureEvent&>(input_event);
    if (gesture_event.SourceDevice() == blink::kWebGestureDeviceTouchpad) {
      gfx::Vector2dF latest_overscroll_delta =
          event_overscroll ? event_overscroll->latest_overscroll_delta
                           : gfx::Vector2dF();
      cc::OverscrollBehavior overscroll_behavior =
          event_overscroll ? event_overscroll->overscroll_behavior
                           : cc::OverscrollBehavior();
      delegate_->ObserveGestureEventAndResult(
          gesture_event, latest_overscroll_delta, overscroll_behavior,
          processed != WebInputEventResult::kNotHandled);
    }
  }

  if (callback) {
    std::move(callback).Run(ack_result, swap_latency_info,
                            std::move(event_overscroll),
                            handling_touch_action_);
  } else {
    DCHECK(!event_overscroll) << "Unexpected overscroll for un-acked event";
  }

  // Show the virtual keyboard if enabled and a user gesture triggers a focus
  // change.
  if (processed != WebInputEventResult::kNotHandled &&
      (input_event.GetType() == WebInputEvent::kTouchEnd ||
       input_event.GetType() == WebInputEvent::kMouseUp)) {
    delegate_->ShowVirtualKeyboard();
  }

  if (!prevent_default &&
      WebInputEvent::IsKeyboardEventType(input_event.GetType()))
    delegate_->OnDidHandleKeyEvent();

// TODO(rouslan): Fix ChromeOS and Windows 8 behavior of autofill popup with
// virtual keyboard.
#if !defined(OS_ANDROID)
  // Virtual keyboard is not supported, so react to focus change immediately.
  if (processed != WebInputEventResult::kNotHandled &&
      (input_event.GetType() == WebInputEvent::kTouchEnd ||
       input_event.GetType() == WebInputEvent::kMouseDown)) {
    delegate_->FocusChangeComplete();
  }
#endif
}

void RenderWidgetInputHandler::DidOverscrollFromBlink(
    const WebFloatSize& overscrollDelta,
    const WebFloatSize& accumulatedOverscroll,
    const WebFloatPoint& position,
    const WebFloatSize& velocity,
    const cc::OverscrollBehavior& behavior) {
  std::unique_ptr<DidOverscrollParams> params(new DidOverscrollParams());
  params->accumulated_overscroll = gfx::Vector2dF(
      accumulatedOverscroll.width, accumulatedOverscroll.height);
  params->latest_overscroll_delta =
      gfx::Vector2dF(overscrollDelta.width, overscrollDelta.height);
  params->current_fling_velocity =
      gfx::Vector2dF(velocity.width, velocity.height);
  params->causal_event_viewport_point = gfx::PointF(position.x, position.y);
  params->overscroll_behavior = behavior;

  // If we're currently handling an event, stash the overscroll data such that
  // it can be bundled in the event ack.
  if (handling_event_overscroll_) {
    *handling_event_overscroll_ = std::move(params);
    return;
  }

  delegate_->OnDidOverscroll(*params);
}

bool RenderWidgetInputHandler::ProcessTouchAction(
    cc::TouchAction touch_action) {
  // Ignore setTouchAction calls that result from synthetic touch events (eg.
  // when blink is emulating touch with mouse).
  if (handling_event_type_ != WebInputEvent::kTouchStart)
    return false;

  handling_touch_action_ = touch_action;
  return true;
}

}  // namespace content
