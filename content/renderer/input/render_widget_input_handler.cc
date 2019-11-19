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
#include "cc/paint/element_id.h"
#include "cc/trees/latency_info_swap_promise_monitor.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/common/input/input_event_ack.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/input_event_ack_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/compositor/layer_tree_view.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_widget.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_float_size.h"
#include "third_party/blink/public/platform/web_gesture_device.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_pointer_event.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/events/blink/event_with_callback.h"
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
    blink::WebVector<const WebInputEvent*> coalesced_events,
    blink::WebVector<const WebInputEvent*> predicted_events) {
  blink::WebVector<WebPointerEvent> related_pointer_events;
  for (const WebInputEvent* event : coalesced_events) {
    DCHECK(WebInputEvent::IsTouchEventType(event->GetType()));
    const WebTouchEvent& touch_event =
        static_cast<const WebTouchEvent&>(*event);
    for (unsigned i = 0; i < touch_event.touches_length; ++i) {
      if (touch_event.touches[i].id == pointer_event.id &&
          touch_event.touches[i].state != WebTouchPoint::kStateStationary) {
        related_pointer_events.emplace_back(
            WebPointerEvent(touch_event, touch_event.touches[i]));
      }
    }
  }
  blink::WebVector<WebPointerEvent> predicted_pointer_events;
  for (const WebInputEvent* event : predicted_events) {
    DCHECK(WebInputEvent::IsTouchEventType(event->GetType()));
    const WebTouchEvent& touch_event =
        static_cast<const WebTouchEvent&>(*event);
    for (unsigned i = 0; i < touch_event.touches_length; ++i) {
      if (touch_event.touches[i].id == pointer_event.id &&
          touch_event.touches[i].state != WebTouchPoint::kStateStationary) {
        predicted_pointer_events.emplace_back(
            WebPointerEvent(touch_event, touch_event.touches[i]));
      }
    }
  }

  return blink::WebCoalescedInputEvent(pointer_event, related_pointer_events,
                                       predicted_pointer_events);
}

viz::FrameSinkId GetRemoteFrameSinkId(const blink::WebHitTestResult& result) {
  const blink::WebNode& node = result.GetNode();
  DCHECK(!node.IsNull());
  blink::WebFrame* result_frame = blink::WebFrame::FromFrameOwnerElement(node);
  if (result_frame && result_frame->IsWebRemoteFrame()) {
    blink::WebRemoteFrame* remote_frame = result_frame->ToWebRemoteFrame();
    if (remote_frame->IsIgnoredForHitTest())
      return viz::FrameSinkId();

    if (!result.ContentBoxContainsPoint())
      return viz::FrameSinkId();

    return RenderFrameProxy::FromWebFrame(remote_frame)->frame_sink_id();
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
      handling_injected_scroll_params_(nullptr),
      handling_event_type_(WebInputEvent::kUndefined),
      suppress_next_char_events_(false),
      last_injected_gesture_was_begin_(false) {
  DCHECK(delegate);
  DCHECK(widget);
  delegate->SetInputHandler(this);
}

RenderWidgetInputHandler::~RenderWidgetInputHandler() {}

viz::FrameSinkId RenderWidgetInputHandler::GetFrameSinkIdAtPoint(
    const gfx::PointF& point,
    gfx::PointF* local_point) {
  // This method must only be called on a local root, which is guaranteed to
  // have a WebWidget.
  // TODO(https://crbug.com/995981): Eventually we should be able to remove this
  // DCHECK, since RenderWidget's lifetime [and thus this instance's] will be
  // synchronized with the WebWidget.
  DCHECK(widget_->GetWebWidget());
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
    return widget_->GetFrameSinkId();
  }

  viz::FrameSinkId frame_sink_id = GetRemoteFrameSinkId(result);
  if (frame_sink_id.is_valid()) {
    *local_point = gfx::PointF(result.LocalPointWithoutContentBoxOffset());
    if (widget_->compositor_deps()->IsUseZoomForDSFEnabled()) {
      *local_point = gfx::ConvertPointToDIP(
          widget_->GetOriginalScreenInfo().device_scale_factor, *local_point);
    }
    return frame_sink_id;
  }

  // Return the FrameSinkId for the current widget if the point did not hit
  // test to a remote frame, or the point is outside of the remote frame's
  // content box, or the remote frame doesn't have a valid FrameSinkId yet.
  return widget_->GetFrameSinkId();
}

WebInputEventResult RenderWidgetInputHandler::HandleTouchEvent(
    const blink::WebCoalescedInputEvent& coalesced_event) {
  // This method must only be called on non-undead RenderWidget, which is
  // guaranteed to have a WebWidget.
  // TODO(https://crbug.com/995981): Eventually we should be able to remote this
  // DCHECK, since RenderWidget's lifetime [and thus this instance's] will be
  // synchronized with the WebWidget.
  DCHECK(widget_->GetWebWidget());

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
  // This method must only be called on non-undead RenderWidget, which is
  // guaranteed to have a WebWidget.
  // TODO(https://crbug.com/995981): Eventually we should be able to remote this
  // DCHECK, since RenderWidget's lifetime [and thus this instance's] will be
  // synchronized with the WebWidget.
  DCHECK(widget_->GetWebWidget());

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

  // Calls into |InjectGestureScrollEvent()| while handling this event
  // will populate injected_scroll_params.
  std::unique_ptr<std::vector<InjectScrollGestureParams>>
      injected_scroll_params;
  base::AutoReset<std::unique_ptr<std::vector<InjectScrollGestureParams>>*>
      injected_scroll_resetter(&handling_injected_scroll_params_,
                               &injected_scroll_params);

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

  ui::LatencyInfo swap_latency_info(latency_info);
  swap_latency_info.AddLatencyNumber(
      ui::LatencyComponentType::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT);
  cc::LatencyInfoSwapPromiseMonitor swap_promise_monitor(
      &swap_latency_info, widget_->layer_tree_host()->GetSwapPromiseManager(),
      nullptr);

  bool prevent_default = false;
  bool show_virtual_keyboard_for_mouse = false;
  if (WebInputEvent::IsMouseEventType(input_event.GetType())) {
    const WebMouseEvent& mouse_event =
        static_cast<const WebMouseEvent&>(input_event);
    TRACE_EVENT2("renderer", "HandleMouseMove", "x",
                 mouse_event.PositionInWidget().x, "y",
                 mouse_event.PositionInWidget().y);

    prevent_default = delegate_->WillHandleMouseEvent(mouse_event);

    // Reset the last known cursor if mouse has left this widget. So next
    // time that the mouse enters we always set the cursor accordingly.
    if (mouse_event.GetType() == WebInputEvent::kMouseLeave)
      current_cursor_.reset();

    if (mouse_event.button == WebPointerProperties::Button::kLeft &&
        mouse_event.GetType() == WebInputEvent::kMouseUp) {
      show_virtual_keyboard_for_mouse = true;
    }
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

  // The handling of some input events on the main thread may require injecting
  // scroll gestures back into blink, e.g., a mousedown on a scrollbar. We
  // do this here so that we can attribute lateny information from the mouse as
  // a scroll interaction, instead of just classifying as mouse input.
  if (injected_scroll_params && injected_scroll_params->size()) {
    HandleInjectedScrollGestures(std::move(*injected_scroll_params),
                                 input_event, latency_info);
  }

  // Send gesture scroll events and their dispositions to the compositor thread,
  // so that they can be used to produce the elastic overscroll effect on Mac.
  if (input_event.GetType() == WebInputEvent::kGestureScrollBegin ||
      input_event.GetType() == WebInputEvent::kGestureScrollEnd ||
      input_event.GetType() == WebInputEvent::kGestureScrollUpdate) {
    const WebGestureEvent& gesture_event =
        static_cast<const WebGestureEvent&>(input_event);
    if (gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchpad) {
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
  if ((processed != WebInputEventResult::kNotHandled &&
       input_event.GetType() == WebInputEvent::kTouchEnd) ||
      show_virtual_keyboard_for_mouse) {
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

  // Ensure all injected scrolls were handled or queue up - any remaining
  // injected scrolls at this point would not be processed.
  DCHECK(!handling_injected_scroll_params_ ||
         !*handling_injected_scroll_params_ ||
         (*handling_injected_scroll_params_)->empty());
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

void RenderWidgetInputHandler::InjectGestureScrollEvent(
    blink::WebGestureDevice device,
    const blink::WebFloatSize& delta,
    ui::input_types::ScrollGranularity granularity,
    cc::ElementId scrollable_area_element_id,
    WebInputEvent::Type injected_type) {
  DCHECK(ui::IsGestureScroll(injected_type));
  // If we're currently handling an input event, cache the appropriate
  // parameters so we can dispatch the events directly once blink finishes
  // handling the event.
  // Otherwise, queue the event on the main thread event queue.
  // The latter may occur when scrollbar scrolls are injected due to
  // autoscroll timer - i.e. not within the handling of a mouse event.
  // We don't always just enqueue events, since events queued to the
  // MainThreadEventQueue in the middle of dispatch (which we are) won't
  // be dispatched until the next time the queue gets to run. The side effect
  // of that would be an extra frame of latency if we're injecting a scroll
  // during the handling of a rAF aligned input event, such as mouse move.
  if (handling_injected_scroll_params_) {
    // Multiple gestures may be injected during the dispatch of a single
    // input event (e.g. Begin/Update). Create a vector and append to the
    // end of it - the gestures will subsequently be injected in order.
    if (!*handling_injected_scroll_params_) {
      *handling_injected_scroll_params_ =
          std::make_unique<std::vector<InjectScrollGestureParams>>();
    }

    InjectScrollGestureParams params{device, delta, granularity,
                                     scrollable_area_element_id, injected_type};
    (*handling_injected_scroll_params_)->push_back(params);
  } else {
    base::TimeTicks now = base::TimeTicks::Now();
    std::unique_ptr<WebGestureEvent> gesture_event =
        ui::GenerateInjectedScrollGesture(injected_type, now, device,
                                          WebFloatPoint(0, 0), delta,
                                          granularity);
    if (injected_type == WebInputEvent::Type::kGestureScrollBegin) {
      gesture_event->data.scroll_begin.scrollable_area_element_id =
          scrollable_area_element_id.GetStableId();
    }

    ui::LatencyInfo latency_info;
    ui::WebScopedInputEvent web_scoped_gesture_event(gesture_event.release());

    widget_->GetInputEventQueue()->HandleEvent(
        std::move(web_scoped_gesture_event), latency_info,
        DISPATCH_TYPE_NON_BLOCKING, INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
        HandledEventCallback());
  }
}

void RenderWidgetInputHandler::HandleInjectedScrollGestures(
    std::vector<InjectScrollGestureParams> injected_scroll_params,
    const WebInputEvent& input_event,
    const ui::LatencyInfo& original_latency_info) {
  // This method must only be called on non-undead RenderWidget, which is
  // guaranteed to have a WebWidget.
  // TODO(https://crbug.com/995981): Eventually we should be able to remote this
  // DCHECK, since RenderWidget's lifetime [and thus this instance's] will be
  // synchronized with the WebWidget.
  DCHECK(widget_->GetWebWidget());

  DCHECK(injected_scroll_params.size());

  base::TimeTicks original_timestamp;
  bool found_original_component = original_latency_info.FindLatency(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, &original_timestamp);
  DCHECK(found_original_component);

  WebFloatPoint position = ui::PositionInWidgetFromInputEvent(input_event);
  for (const InjectScrollGestureParams& params : injected_scroll_params) {
    // Set up a new LatencyInfo for the injected scroll - this is the original
    // LatencyInfo for the input event that was being handled when the scroll
    // was injected. This new LatencyInfo will have a modified type, and an
    // additional scroll update component. Also set up a SwapPromiseMonitor that
    // will cause the LatencyInfo to be sent up with the compositor frame, if
    // the GSU causes a commit. This allows end to end latency to be logged for
    // the injected scroll, annotated with the correct type.
    ui::LatencyInfo scrollbar_latency_info(original_latency_info);

    // Currently only scrollbar is supported - if this DCHECK hits due to a
    // new type being injected, please modify the type passed to
    // |set_source_event_type()|.
    DCHECK(params.device == blink::WebGestureDevice::kScrollbar);
    scrollbar_latency_info.set_source_event_type(
        ui::SourceEventType::SCROLLBAR);
    scrollbar_latency_info.AddLatencyNumber(
        ui::LatencyComponentType::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT);

    if (params.type == WebInputEvent::Type::kGestureScrollUpdate) {
      if (input_event.GetType() != WebInputEvent::Type::kGestureScrollUpdate) {
        scrollbar_latency_info.AddLatencyNumberWithTimestamp(
            last_injected_gesture_was_begin_
                ? ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT
                : ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
            original_timestamp);
      } else {
        // If we're injecting a GSU in response to a GSU (touch drags of the
        // scrollbar thumb in Blink handles GSUs, and reverses them with
        // injected GSUs), the LatencyInfo will already have the appropriate
        // SCROLL_UPDATE component set.
        DCHECK(
            scrollbar_latency_info.FindLatency(
                ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
                nullptr) ||
            scrollbar_latency_info.FindLatency(
                ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
                nullptr));
      }
    }

    std::unique_ptr<WebGestureEvent> gesture_event =
        ui::GenerateInjectedScrollGesture(
            params.type, input_event.TimeStamp(), params.device, position,
            params.scroll_delta, params.granularity);
    if (params.type == WebInputEvent::Type::kGestureScrollBegin) {
      gesture_event->data.scroll_begin.scrollable_area_element_id =
          params.scrollable_area_element_id.GetStableId();
      last_injected_gesture_was_begin_ = true;
    } else {
      last_injected_gesture_was_begin_ = false;
    }

    {
      cc::LatencyInfoSwapPromiseMonitor swap_promise_monitor(
          &scrollbar_latency_info,
          widget_->layer_tree_host()->GetSwapPromiseManager(), nullptr);
      widget_->GetWebWidget()->HandleInputEvent(
          blink::WebCoalescedInputEvent(*gesture_event.get()));
    }
  }
}

bool RenderWidgetInputHandler::DidChangeCursor(const WebCursor& cursor) {
  if (current_cursor_.has_value() && current_cursor_.value() == cursor)
    return false;
  current_cursor_ = cursor;
  return true;
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
