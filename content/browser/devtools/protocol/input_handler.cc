// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/input_handler.h"

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/protocol/native_input_event_builder.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/input/synthetic_pointer_action.h"
#include "content/browser/renderer_host/input/touch_emulator.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/range/range.h"

namespace content {
namespace protocol {

namespace {

gfx::PointF CssPixelsToPointF(double x, double y, float page_scale_factor) {
  return gfx::PointF(x * page_scale_factor, y * page_scale_factor);
}

gfx::Vector2dF CssPixelsToVector2dF(double x,
                                    double y,
                                    float page_scale_factor) {
  return gfx::Vector2dF(x * page_scale_factor, y * page_scale_factor);
}

bool StringToGestureSourceType(Maybe<std::string> in,
                               SyntheticGestureParams::GestureSourceType& out) {
  if (!in.isJust()) {
    out = SyntheticGestureParams::GestureSourceType::DEFAULT_INPUT;
    return true;
  }
  if (in.fromJust() == Input::GestureSourceTypeEnum::Default) {
    out = SyntheticGestureParams::GestureSourceType::DEFAULT_INPUT;
    return true;
  }
  if (in.fromJust() == Input::GestureSourceTypeEnum::Touch) {
    out = SyntheticGestureParams::GestureSourceType::TOUCH_INPUT;
    return true;
  }
  if (in.fromJust() == Input::GestureSourceTypeEnum::Mouse) {
    out = SyntheticGestureParams::GestureSourceType::MOUSE_INPUT;
    return true;
  }
  return false;
}

int GetEventModifiers(int modifiers,
                      bool auto_repeat,
                      bool is_keypad,
                      int location,
                      int buttons) {
  int result = blink::WebInputEvent::kFromDebugger;
  if (auto_repeat)
    result |= blink::WebInputEvent::kIsAutoRepeat;
  if (is_keypad)
    result |= blink::WebInputEvent::kIsKeyPad;

  if (modifiers & 1)
    result |= blink::WebInputEvent::kAltKey;
  if (modifiers & 2)
    result |= blink::WebInputEvent::kControlKey;
  if (modifiers & 4)
    result |= blink::WebInputEvent::kMetaKey;
  if (modifiers & 8)
    result |= blink::WebInputEvent::kShiftKey;

  if (location & 1)
    result |= blink::WebInputEvent::kIsLeft;
  if (location & 2)
    result |= blink::WebInputEvent::kIsRight;

  if (buttons & 1)
    result |= blink::WebMouseEvent::kLeftButtonDown;
  if (buttons & 2)
    result |= blink::WebInputEvent::kRightButtonDown;
  if (buttons & 4)
    result |= blink::WebInputEvent::kMiddleButtonDown;
  if (buttons & 8)
    result |= blink::WebInputEvent::kBackButtonDown;
  if (buttons & 16)
    result |= blink::WebInputEvent::kForwardButtonDown;
  return result;
}

base::TimeTicks GetEventTimeTicks(const Maybe<double>& timestamp) {
  // Convert timestamp, in seconds since unix epoch, to an event timestamp
  // which is time ticks since platform start time.
  return timestamp.isJust()
             ? base::TimeDelta::FromSecondsD(timestamp.fromJust()) +
                   base::TimeTicks::UnixEpoch()
             : base::TimeTicks::Now();
}

bool SetKeyboardEventText(blink::WebUChar* to, Maybe<std::string> from) {
  if (!from.isJust())
    return true;

  base::string16 text16 = base::UTF8ToUTF16(from.fromJust());
  if (text16.size() > blink::WebKeyboardEvent::kTextLengthCap)
    return false;

  for (size_t i = 0; i < text16.size(); ++i)
    to[i] = text16[i];
  return true;
}

bool GetMouseEventButton(const std::string& button,
                         blink::WebPointerProperties::Button* event_button,
                         int* event_modifiers) {
  if (button.empty())
    return true;

  if (button == Input::DispatchMouseEvent::ButtonEnum::None) {
    *event_button = blink::WebMouseEvent::Button::kNoButton;
  } else if (button == Input::DispatchMouseEvent::ButtonEnum::Left) {
    *event_button = blink::WebMouseEvent::Button::kLeft;
    *event_modifiers = blink::WebInputEvent::kLeftButtonDown;
  } else if (button == Input::DispatchMouseEvent::ButtonEnum::Middle) {
    *event_button = blink::WebMouseEvent::Button::kMiddle;
    *event_modifiers = blink::WebInputEvent::kMiddleButtonDown;
  } else if (button == Input::DispatchMouseEvent::ButtonEnum::Right) {
    *event_button = blink::WebMouseEvent::Button::kRight;
    *event_modifiers = blink::WebInputEvent::kRightButtonDown;
  } else if (button == Input::DispatchMouseEvent::ButtonEnum::Back) {
    *event_button = blink::WebMouseEvent::Button::kBack;
    *event_modifiers = blink::WebInputEvent::kBackButtonDown;
  } else if (button == Input::DispatchMouseEvent::ButtonEnum::Forward) {
    *event_button = blink::WebMouseEvent::Button::kForward;
    *event_modifiers = blink::WebInputEvent::kForwardButtonDown;
  } else {
    return false;
  }
  return true;
}

blink::WebInputEvent::Type GetMouseEventType(const std::string& type) {
  if (type == Input::DispatchMouseEvent::TypeEnum::MousePressed)
    return blink::WebInputEvent::kMouseDown;
  if (type == Input::DispatchMouseEvent::TypeEnum::MouseReleased)
    return blink::WebInputEvent::kMouseUp;
  if (type == Input::DispatchMouseEvent::TypeEnum::MouseMoved)
    return blink::WebInputEvent::kMouseMove;
  if (type == Input::DispatchMouseEvent::TypeEnum::MouseWheel)
    return blink::WebInputEvent::kMouseWheel;
  return blink::WebInputEvent::kUndefined;
}

blink::WebInputEvent::Type GetTouchEventType(const std::string& type) {
  if (type == Input::DispatchTouchEvent::TypeEnum::TouchStart)
    return blink::WebInputEvent::kTouchStart;
  if (type == Input::DispatchTouchEvent::TypeEnum::TouchEnd)
    return blink::WebInputEvent::kTouchEnd;
  if (type == Input::DispatchTouchEvent::TypeEnum::TouchMove)
    return blink::WebInputEvent::kTouchMove;
  if (type == Input::DispatchTouchEvent::TypeEnum::TouchCancel)
    return blink::WebInputEvent::kTouchCancel;
  return blink::WebInputEvent::kUndefined;
}

blink::WebPointerProperties::PointerType GetPointerType(
    const std::string& type) {
  if (type == Input::DispatchMouseEvent::PointerTypeEnum::Mouse)
    return blink::WebPointerProperties::PointerType::kMouse;
  if (type == Input::DispatchMouseEvent::PointerTypeEnum::Pen)
    return blink::WebPointerProperties::PointerType::kPen;
  return blink::WebPointerProperties::PointerType::kMouse;
}

SyntheticPointerActionParams::PointerActionType GetTouchPointerActionType(
    const std::string& type) {
  if (type == Input::DispatchTouchEvent::TypeEnum::TouchStart)
    return SyntheticPointerActionParams::PointerActionType::PRESS;
  if (type == Input::DispatchTouchEvent::TypeEnum::TouchEnd)
    return SyntheticPointerActionParams::PointerActionType::RELEASE;
  if (type == Input::DispatchTouchEvent::TypeEnum::TouchMove)
    return SyntheticPointerActionParams::PointerActionType::MOVE;
  if (type == Input::DispatchTouchEvent::TypeEnum::TouchCancel)
    return SyntheticPointerActionParams::PointerActionType::CANCEL;
  return SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED;
}

SyntheticPointerActionParams::Button GetPointerActionParamsButton(
    const std::string& button) {
  if (button == Input::DispatchMouseEvent::ButtonEnum::Left)
    return SyntheticPointerActionParams::Button::LEFT;
  if (button == Input::DispatchMouseEvent::ButtonEnum::Middle)
    return SyntheticPointerActionParams::Button::MIDDLE;
  if (button == Input::DispatchMouseEvent::ButtonEnum::Right)
    return SyntheticPointerActionParams::Button::RIGHT;
  if (button == Input::DispatchMouseEvent::ButtonEnum::Back)
    return SyntheticPointerActionParams::Button::BACK;
  if (button == Input::DispatchMouseEvent::ButtonEnum::Forward)
    return SyntheticPointerActionParams::Button::FORWARD;
  return SyntheticPointerActionParams::Button::NO_BUTTON;
}

bool GenerateTouchPoints(
    blink::WebTouchEvent* event,
    blink::WebInputEvent::Type type,
    const base::flat_map<int, blink::WebTouchPoint>& points,
    const blink::WebTouchPoint& changing) {
  event->touches_length = 1;
  event->touches[0] = changing;
  for (auto& it : points) {
    if (it.first == changing.id)
      continue;
    if (event->touches_length == blink::WebTouchEvent::kTouchesLengthCap)
      return false;
    event->touches[event->touches_length] = it.second;
    event->touches[event->touches_length].state =
        blink::WebTouchPoint::kStateStationary;
    event->touches_length++;
  }
  if (type != blink::WebInputEvent::kUndefined) {
    event->touches[0].state = type == blink::WebInputEvent::kTouchCancel
                                  ? blink::WebTouchPoint::kStateCancelled
                                  : blink::WebTouchPoint::kStateReleased;
    event->SetType(type);
  } else if (points.find(changing.id) == points.end()) {
    event->touches[0].state = blink::WebTouchPoint::kStatePressed;
    event->SetType(blink::WebInputEvent::kTouchStart);
  } else {
    event->touches[0].state = blink::WebTouchPoint::kStateMoved;
    event->SetType(blink::WebInputEvent::kTouchMove);
  }
  return true;
}

void SendSynthesizePinchGestureResponse(
    std::unique_ptr<Input::Backend::SynthesizePinchGestureCallback> callback,
    SyntheticGesture::Result result) {
  if (result == SyntheticGesture::Result::GESTURE_FINISHED) {
    callback->sendSuccess();
  } else {
    callback->sendFailure(Response::Error(
        base::StringPrintf("Synthetic pinch failed, result was %d", result)));
  }
}

class TapGestureResponse {
 public:
  TapGestureResponse(
      std::unique_ptr<Input::Backend::SynthesizeTapGestureCallback> callback,
      int count)
      : callback_(std::move(callback)),
        count_(count) {
  }

  void OnGestureResult(SyntheticGesture::Result result) {
    --count_;
    // Still waiting for more taps to finish.
    if (result == SyntheticGesture::Result::GESTURE_FINISHED && count_)
      return;
    if (callback_) {
      if (result == SyntheticGesture::Result::GESTURE_FINISHED) {
        callback_->sendSuccess();
      } else {
        callback_->sendFailure(Response::Error(
            base::StringPrintf("Synthetic tap failed, result was %d", result)));
      }
      callback_.reset();
    }
    if (!count_)
      delete this;
  }

 private:
  std::unique_ptr<Input::Backend::SynthesizeTapGestureCallback> callback_;
  int count_;
};

void SendSynthesizeScrollGestureResponse(
    std::unique_ptr<Input::Backend::SynthesizeScrollGestureCallback> callback,
    SyntheticGesture::Result result) {
  if (result == SyntheticGesture::Result::GESTURE_FINISHED) {
    callback->sendSuccess();
  } else {
    callback->sendFailure(Response::Error(
        base::StringPrintf("Synthetic scroll failed, result was %d", result)));
  }
}

void DispatchPointerActionsResponse(
    std::unique_ptr<Input::Backend::DispatchTouchEventCallback> callback,
    SyntheticGesture::Result result) {
  if (result == SyntheticGesture::Result::GESTURE_FINISHED) {
    callback->sendSuccess();
  } else {
    callback->sendFailure(Response::Error(
        base::StringPrintf("Action sequence failed, result was %d", result)));
  }
}

}  // namespace

class InputHandler::InputInjector
    : public RenderWidgetHost::InputEventObserver {
 public:
  InputInjector(InputHandler* owner, RenderWidgetHostImpl* widget_host)
      : owner_(owner), widget_host_(widget_host->GetWeakPtr()) {
    widget_host->AddInputEventObserver(this);
  }

  void Cleanup() {
    for (auto& callback : pending_key_callbacks_)
      callback->sendSuccess();
    pending_key_callbacks_.clear();
    for (auto& callback : pending_mouse_callbacks_)
      callback->sendSuccess();
    pending_mouse_callbacks_.clear();
    MaybeSelfDestruct();
  }

  bool HasWidgetHost(RenderWidgetHostImpl* widget_host) {
    return widget_host == widget_host_.get();
  }

  void InjectWheelEvent(blink::WebMouseWheelEvent* wheel_event,
                        std::unique_ptr<DispatchMouseEventCallback> callback) {
    if (!widget_host_) {
      callback->sendFailure(Response::InternalError());
      return;
    }

    widget_host_->Focus();
    input_queued_ = false;
    pending_mouse_callbacks_.push_back(std::move(callback));
    widget_host_->ForwardWheelEvent(*wheel_event);
    if (!input_queued_) {
      pending_mouse_callbacks_.back()->sendSuccess();
      pending_mouse_callbacks_.pop_back();
      MaybeSelfDestruct();
      return;
    }

    // Send a synthetic wheel event with phaseEnded to finish scrolling.
    wheel_event->delta_x = 0;
    wheel_event->delta_y = 0;
    wheel_event->phase = blink::WebMouseWheelEvent::kPhaseEnded;
    wheel_event->dispatch_type = blink::WebInputEvent::kEventNonBlocking;
    widget_host_->ForwardWheelEvent(*wheel_event);
  }

  void InjectMouseEvent(const blink::WebMouseEvent& mouse_event,
                        std::unique_ptr<DispatchMouseEventCallback> callback) {
    if (!widget_host_) {
      callback->sendFailure(Response::InternalError());
      return;
    }

    widget_host_->Focus();
    input_queued_ = false;
    pending_mouse_callbacks_.push_back(std::move(callback));
    widget_host_->ForwardMouseEvent(mouse_event);
    if (!input_queued_) {
      pending_mouse_callbacks_.back()->sendSuccess();
      pending_mouse_callbacks_.pop_back();
      MaybeSelfDestruct();
    }
  }

  void InjectKeyboardEvent(const NativeWebKeyboardEvent& keyboard_event,
                           std::unique_ptr<DispatchKeyEventCallback> callback) {
    if (!widget_host_) {
      callback->sendFailure(Response::InternalError());
      return;
    }

    widget_host_->Focus();
    input_queued_ = false;
    pending_key_callbacks_.push_back(std::move(callback));
    widget_host_->ForwardKeyboardEvent(keyboard_event);
    if (!input_queued_) {
      pending_key_callbacks_.back()->sendSuccess();
      pending_key_callbacks_.pop_back();
      MaybeSelfDestruct();
    }
  }

  void InjectTouchEvents(const std::vector<blink::WebTouchEvent>& events,
                         std::unique_ptr<DispatchTouchEventCallback> callback) {
    if (!widget_host_) {
      callback->sendFailure(Response::InternalError());
      return;
    }

    widget_host_->Focus();
    widget_host_->GetTouchEmulator()->Enable(
        TouchEmulator::Mode::kInjectingTouchEvents,
        ui::GestureProviderConfigType::CURRENT_PLATFORM);
    base::OnceClosure closure = base::BindOnce(
        &DispatchTouchEventCallback::sendSuccess, std::move(callback));
    for (size_t i = 0; i < events.size(); i++) {
      widget_host_->GetTouchEmulator()->InjectTouchEvent(
          events[i], widget_host_->GetView(),
          i == events.size() - 1 ? std::move(closure) : base::OnceClosure());
    }
    MaybeSelfDestruct();
  }

 private:
  void OnInputEvent(const blink::WebInputEvent& event) override {
    input_queued_ = true;
  }

  void OnInputEventAck(InputEventAckSource source,
                       InputEventAckState state,
                       const blink::WebInputEvent& event) override {
    if ((event.GetModifiers() & blink::WebInputEvent::kFromDebugger) == 0)
      return;

    if (blink::WebInputEvent::IsKeyboardEventType(event.GetType()) &&
        !pending_key_callbacks_.empty()) {
      pending_key_callbacks_.front()->sendSuccess();
      pending_key_callbacks_.pop_front();
      MaybeSelfDestruct();
      return;
    }

    if ((blink::WebInputEvent::IsMouseEventType(event.GetType()) ||
         event.GetType() == blink::WebInputEvent::kMouseWheel) &&
        !pending_mouse_callbacks_.empty()) {
      pending_mouse_callbacks_.front()->sendSuccess();
      pending_mouse_callbacks_.pop_front();
      MaybeSelfDestruct();
      return;
    }
  }

  void MaybeSelfDestruct() {
    if (!pending_key_callbacks_.empty() || !pending_mouse_callbacks_.empty())
      return;
    if (widget_host_)
      widget_host_->RemoveInputEventObserver(this);
    owner_->injectors_.erase(this);
  }

  InputHandler* const owner_;
  base::WeakPtr<RenderWidgetHostImpl> widget_host_;
  // Callbacks for calls to Input.dispatchKey/MouseEvent that have been sent to
  // the renderer, but that we haven't yet received an ack for.
  bool input_queued_ = false;
  base::circular_deque<std::unique_ptr<DispatchKeyEventCallback>>
      pending_key_callbacks_;
  base::circular_deque<std::unique_ptr<DispatchMouseEventCallback>>
      pending_mouse_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(InputInjector);
};

InputHandler::InputHandler()
    : DevToolsDomainHandler(Input::Metainfo::domainName),
      host_(nullptr),
      page_scale_factor_(1.0),
      last_id_(0) {}

InputHandler::~InputHandler() {
}

// static
std::vector<InputHandler*> InputHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<InputHandler>(Input::Metainfo::domainName);
}

void InputHandler::SetRenderer(int process_host_id,
                               RenderFrameHostImpl* frame_host) {
  if (frame_host == host_)
    return;
  ClearInputState();

  WebContents* old_web_contents = WebContents::FromRenderFrameHost(host_);
  WebContents* new_web_contents = WebContents::FromRenderFrameHost(frame_host);

  // When navigating, the new renderer might have a different page scale.
  // It emits a changed event iff the new page scale is not 1
  // (see crbug.com/929806)
  // If attaching to a new host, we've got OnPageScaleFactorChanged(),
  // so don't override it.
  if (host_)
    page_scale_factor_ = 1.0;

  host_ = frame_host;

  if (ignore_input_events_ && old_web_contents != new_web_contents) {
    if (old_web_contents)
      old_web_contents->SetIgnoreInputEvents(false);
    if (new_web_contents)
      new_web_contents->SetIgnoreInputEvents(true);
  }
}

void InputHandler::Wire(UberDispatcher* dispatcher) {
  Input::Dispatcher::wire(dispatcher, this);
}

void InputHandler::OnPageScaleFactorChanged(float page_scale_factor) {
  page_scale_factor_ = page_scale_factor;
}

Response InputHandler::Disable() {
  ClearInputState();
  WebContents* web_contents = WebContents::FromRenderFrameHost(host_);
  if (web_contents && ignore_input_events_)
    web_contents->SetIgnoreInputEvents(false);
  ignore_input_events_ = false;
  pointer_ids_.clear();
  touch_points_.clear();
  return Response::OK();
}

void InputHandler::DispatchKeyEvent(
    const std::string& type,
    Maybe<int> modifiers,
    Maybe<double> timestamp,
    Maybe<std::string> text,
    Maybe<std::string> unmodified_text,
    Maybe<std::string> key_identifier,
    Maybe<std::string> code,
    Maybe<std::string> key,
    Maybe<int> windows_virtual_key_code,
    Maybe<int> native_virtual_key_code,
    Maybe<bool> auto_repeat,
    Maybe<bool> is_keypad,
    Maybe<bool> is_system_key,
    Maybe<int> location,
    std::unique_ptr<DispatchKeyEventCallback> callback) {
  blink::WebInputEvent::Type web_event_type;

  if (type == Input::DispatchKeyEvent::TypeEnum::KeyDown) {
    web_event_type = blink::WebInputEvent::kKeyDown;
  } else if (type == Input::DispatchKeyEvent::TypeEnum::KeyUp) {
    web_event_type = blink::WebInputEvent::kKeyUp;
  } else if (type == Input::DispatchKeyEvent::TypeEnum::Char) {
    web_event_type = blink::WebInputEvent::kChar;
  } else if (type == Input::DispatchKeyEvent::TypeEnum::RawKeyDown) {
    web_event_type = blink::WebInputEvent::kRawKeyDown;
  } else {
    callback->sendFailure(Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", type.c_str())));
    return;
  }

  NativeWebKeyboardEvent event(
      web_event_type,
      GetEventModifiers(modifiers.fromMaybe(blink::WebInputEvent::kNoModifiers),
                        auto_repeat.fromMaybe(false),
                        is_keypad.fromMaybe(false), location.fromMaybe(0), 0),
      GetEventTimeTicks(timestamp));

  if (!SetKeyboardEventText(event.text, std::move(text))) {
    callback->sendFailure(Response::InvalidParams("Invalid 'text' parameter"));
    return;
  }
  if (!SetKeyboardEventText(event.unmodified_text,
                            std::move(unmodified_text))) {
    callback->sendFailure(
        Response::InvalidParams("Invalid 'unmodifiedText' parameter"));
    return;
  }

  if (windows_virtual_key_code.isJust())
    event.windows_key_code = windows_virtual_key_code.fromJust();
  if (native_virtual_key_code.isJust())
    event.native_key_code = native_virtual_key_code.fromJust();
  if (is_system_key.isJust())
    event.is_system_key = is_system_key.fromJust();

  if (code.isJust()) {
    event.dom_code = static_cast<int>(
        ui::KeycodeConverter::CodeStringToDomCode(code.fromJust()));
  }

  if (key.isJust()) {
    event.dom_key = static_cast<int>(
        ui::KeycodeConverter::KeyStringToDomKey(key.fromJust()));
  }

  if (!host_ || !host_->GetRenderWidgetHost()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  RenderWidgetHostImpl* widget_host = host_->GetRenderWidgetHost();
  if (!host_->GetParent() && widget_host->delegate()) {
    RenderWidgetHostImpl* target_host =
        widget_host->delegate()->GetFocusedRenderWidgetHost(widget_host);
    if (target_host)
      widget_host = target_host;
  }

  // We do not pass events to browser if there is no native key event
  // due to Mac needing the actual os_event.
  if (event.native_key_code)
    event.os_event = NativeInputEventBuilder::CreateEvent(event);
  else
    event.skip_in_browser = true;

  EnsureInjector(widget_host)->InjectKeyboardEvent(event, std::move(callback));
}

void InputHandler::InsertText(const std::string& text,
                              std::unique_ptr<InsertTextCallback> callback) {
  base::string16 text16 = base::UTF8ToUTF16(text);
  base::OnceClosure closure =
      base::BindOnce(&InsertTextCallback::sendSuccess, std::move(callback));

  if (!host_ || !host_->GetRenderWidgetHost()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  RenderWidgetHostImpl* widget_host = host_->GetRenderWidgetHost();
  if (!host_->GetParent() && widget_host->delegate()) {
    RenderWidgetHostImpl* target_host =
        widget_host->delegate()->GetFocusedRenderWidgetHost(widget_host);
    if (target_host)
      widget_host = target_host;
  }

  widget_host->Focus();
  widget_host->GetWidgetInputHandler()->ImeCommitText(
      text16, std::vector<ui::ImeTextSpan>(), gfx::Range::InvalidRange(), 0,
      std::move(closure));
}

void InputHandler::DispatchMouseEvent(
    const std::string& event_type,
    double x,
    double y,
    Maybe<int> maybe_modifiers,
    Maybe<double> maybe_timestamp,
    Maybe<std::string> maybe_button,
    Maybe<int> buttons,
    Maybe<int> click_count,
    Maybe<double> delta_x,
    Maybe<double> delta_y,
    Maybe<std::string> pointer_type,
    std::unique_ptr<DispatchMouseEventCallback> callback) {
  blink::WebInputEvent::Type type = GetMouseEventType(event_type);
  if (type == blink::WebInputEvent::kUndefined) {
    callback->sendFailure(Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", event_type.c_str())));
    return;
  }

  blink::WebPointerProperties::Button button =
      blink::WebPointerProperties::Button::kNoButton;
  int button_modifiers = 0;
  if (!GetMouseEventButton(maybe_button.fromMaybe(""), &button,
                           &button_modifiers)) {
    callback->sendFailure(Response::InvalidParams("Invalid mouse button"));
    return;
  }

  int modifiers = GetEventModifiers(
      maybe_modifiers.fromMaybe(blink::WebInputEvent::kNoModifiers), false,
      false, 0, buttons.fromMaybe(0));
  modifiers |= button_modifiers;
  base::TimeTicks timestamp = GetEventTimeTicks(maybe_timestamp);

  std::unique_ptr<blink::WebMouseEvent, ui::WebInputEventDeleter> mouse_event;
  blink::WebMouseWheelEvent* wheel_event = nullptr;

  if (type == blink::WebInputEvent::kMouseWheel) {
    wheel_event = new blink::WebMouseWheelEvent(type, modifiers, timestamp);
    mouse_event.reset(wheel_event);
    if (!delta_x.isJust() || !delta_y.isJust()) {
      callback->sendFailure(Response::InvalidParams(
          "'deltaX' and 'deltaY' are expected for mouseWheel event"));
      return;
    }
    wheel_event->delta_x = static_cast<float>(-delta_x.fromJust());
    wheel_event->delta_y = static_cast<float>(-delta_y.fromJust());
    wheel_event->phase = blink::WebMouseWheelEvent::kPhaseBegan;
    wheel_event->dispatch_type = blink::WebInputEvent::kBlocking;
  } else {
    mouse_event.reset(new blink::WebMouseEvent(type, modifiers, timestamp));
  }

  mouse_event->button = button;
  mouse_event->click_count = click_count.fromMaybe(0);
  mouse_event->pointer_type = GetPointerType(pointer_type.fromMaybe(""));

  gfx::PointF point;
  RenderWidgetHostImpl* widget_host =
      FindTargetWidgetHost(CssPixelsToPointF(x, y, page_scale_factor_), &point);
  if (!widget_host) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  mouse_event->SetPositionInWidget(point.x(), point.y());
  mouse_event->SetPositionInScreen(point.x(), point.y());
  if (wheel_event) {
    EnsureInjector(widget_host)
        ->InjectWheelEvent(wheel_event, std::move(callback));
  } else {
    EnsureInjector(widget_host)
        ->InjectMouseEvent(*mouse_event, std::move(callback));
  }
}

void InputHandler::DispatchTouchEvent(
    const std::string& event_type,
    std::unique_ptr<Array<Input::TouchPoint>> touch_points,
    protocol::Maybe<int> maybe_modifiers,
    protocol::Maybe<double> maybe_timestamp,
    std::unique_ptr<DispatchTouchEventCallback> callback) {
  if (base::FeatureList::IsEnabled(features::kSyntheticPointerActions)) {
    DispatchSyntheticPointerActionTouch(
        event_type, std::move(touch_points), std::move(maybe_modifiers),
        std::move(maybe_timestamp), std::move(callback));
    return;
  }

  DispatchWebTouchEvent(event_type, std::move(touch_points),
                        std::move(maybe_modifiers), std::move(maybe_timestamp),
                        std::move(callback));
}

void InputHandler::DispatchWebTouchEvent(
    const std::string& event_type,
    std::unique_ptr<Array<Input::TouchPoint>> touch_points,
    protocol::Maybe<int> maybe_modifiers,
    protocol::Maybe<double> maybe_timestamp,
    std::unique_ptr<DispatchTouchEventCallback> callback) {
  blink::WebInputEvent::Type type = GetTouchEventType(event_type);
  if (type == blink::WebInputEvent::kUndefined) {
    callback->sendFailure(Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", event_type.c_str())));
    return;
  }

  int modifiers = GetEventModifiers(
      maybe_modifiers.fromMaybe(blink::WebInputEvent::kNoModifiers), false,
      false, 0, 0);
  base::TimeTicks timestamp = GetEventTimeTicks(maybe_timestamp);

  if ((type == blink::WebInputEvent::kTouchStart ||
       type == blink::WebInputEvent::kTouchMove) &&
      touch_points->empty()) {
    callback->sendFailure(Response::InvalidParams(
        "TouchStart and TouchMove must have at least one touch point."));
    return;
  }
  if ((type == blink::WebInputEvent::kTouchEnd ||
       type == blink::WebInputEvent::kTouchCancel) &&
      !touch_points->empty()) {
    callback->sendFailure(Response::InvalidParams(
        "TouchEnd and TouchCancel must not have any touch points."));
    return;
  }
  if (type == blink::WebInputEvent::kTouchStart && !touch_points_.empty()) {
    callback->sendFailure(Response::InvalidParams(
        "Must have no prior active touch points to start a new touch."));
    return;
  }
  if (type != blink::WebInputEvent::kTouchStart && touch_points_.empty()) {
    callback->sendFailure(Response::InvalidParams(
        "Must send a TouchStart first to start a new touch."));
    return;
  }

  base::flat_map<int, blink::WebTouchPoint> points;
  size_t with_id = 0;
  for (size_t i = 0; i < touch_points->size(); ++i) {
    Input::TouchPoint* point = (*touch_points)[i].get();
    int id = point->GetId(i);  // index |i| is default for the id.
    if (point->HasId())
      with_id++;
    points[id].id = id;
    points[id].radius_x = point->GetRadiusX(1.0);
    points[id].radius_y = point->GetRadiusY(1.0);
    points[id].rotation_angle = point->GetRotationAngle(0.0);
    points[id].force = point->GetForce(1.0);
    points[id].pointer_type = blink::WebPointerProperties::PointerType::kTouch;
    points[id].SetPositionInWidget(point->GetX() * page_scale_factor_,
                                   point->GetY() * page_scale_factor_);
    points[id].SetPositionInScreen(point->GetX() * page_scale_factor_,
                                   point->GetY() * page_scale_factor_);
  }
  if (with_id > 0 && with_id < touch_points->size()) {
    callback->sendFailure(Response::InvalidParams(
        "All or none of the provided TouchPoints must supply ids."));
    return;
  }

  std::vector<blink::WebTouchEvent> events;
  bool ok = true;
  for (auto& id_point : points) {
    if (touch_points_.find(id_point.first) != touch_points_.end())
      continue;
    events.emplace_back(type, modifiers, timestamp);
    ok &= GenerateTouchPoints(&events.back(), blink::WebInputEvent::kUndefined,
                              touch_points_, id_point.second);
    touch_points_.insert(id_point);
  }
  for (auto& id_point : points) {
    DCHECK(touch_points_.find(id_point.first) != touch_points_.end());
    if (touch_points_[id_point.first].PositionInWidget() ==
        id_point.second.PositionInWidget()) {
      continue;
    }
    events.emplace_back(type, modifiers, timestamp);
    ok &= GenerateTouchPoints(&events.back(), blink::WebInputEvent::kUndefined,
                              touch_points_, id_point.second);
    touch_points_[id_point.first] = id_point.second;
  }
  if (type != blink::WebInputEvent::kTouchCancel)
    type = blink::WebInputEvent::kTouchEnd;
  for (auto it = touch_points_.begin(); it != touch_points_.end();) {
    if (points.find(it->first) != points.end()) {
      it++;
      continue;
    }
    events.emplace_back(type, modifiers, timestamp);
    ok &= GenerateTouchPoints(&events.back(), type, touch_points_, it->second);
    it = touch_points_.erase(it);
  }
  if (!ok) {
    callback->sendFailure(Response::Error(
        base::StringPrintf("Exceeded maximum touch points limit of %d",
                           blink::WebTouchEvent::kTouchesLengthCap)));
    return;
  }

  if (events.empty()) {
    callback->sendSuccess();
    return;
  }

  gfx::PointF original(events[0].touches[0].PositionInWidget().x,
                       events[0].touches[0].PositionInWidget().y);
  gfx::PointF transformed;
  RenderWidgetHostImpl* widget_host =
      FindTargetWidgetHost(original, &transformed);
  if (!widget_host) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  gfx::Vector2dF delta = transformed - original;
  for (size_t i = 0; i < events.size(); i++) {
    events[i].dispatch_type =
        events[i].GetType() == blink::WebInputEvent::kTouchCancel
            ? blink::WebInputEvent::kEventNonBlocking
            : blink::WebInputEvent::kBlocking;
    events[i].moved_beyond_slop_region = true;
    events[i].unique_touch_event_id = ui::GetNextTouchEventId();
    for (unsigned j = 0; j < events[i].touches_length; j++) {
      blink::WebFloatPoint point = events[i].touches[j].PositionInWidget();
      events[i].touches[j].SetPositionInWidget(point.x + delta.x(),
                                               point.y + delta.y());
      point = events[i].touches[j].PositionInScreen();
      events[i].touches[j].SetPositionInScreen(point.x + delta.x(),
                                               point.y + delta.y());
    }
  }
  EnsureInjector(widget_host)->InjectTouchEvents(events, std::move(callback));
}

void InputHandler::DispatchSyntheticPointerActionTouch(
    const std::string& event_type,
    std::unique_ptr<Array<Input::TouchPoint>> touch_points,
    protocol::Maybe<int> maybe_modifiers,
    protocol::Maybe<double> maybe_timestamp,
    std::unique_ptr<DispatchTouchEventCallback> callback) {
  if (!host_ || !host_->GetRenderWidgetHost()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  SyntheticPointerActionParams::PointerActionType pointer_action_type =
      GetTouchPointerActionType(event_type);
  if (pointer_action_type ==
      SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED) {
    callback->sendFailure(Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", event_type.c_str())));
    return;
  }

  int modifiers = GetEventModifiers(
      maybe_modifiers.fromMaybe(blink::WebInputEvent::kNoModifiers), false,
      false, 0, 0);

  if ((pointer_action_type ==
           SyntheticPointerActionParams::PointerActionType::PRESS ||
       pointer_action_type ==
           SyntheticPointerActionParams::PointerActionType::MOVE) &&
      touch_points->empty()) {
    callback->sendFailure(Response::InvalidParams(
        "TouchStart and TouchMove must have at least one touch point."));
    return;
  }
  if ((pointer_action_type ==
           SyntheticPointerActionParams::PointerActionType::RELEASE ||
       pointer_action_type ==
           SyntheticPointerActionParams::PointerActionType::CANCEL) &&
      !touch_points->empty()) {
    callback->sendFailure(Response::InvalidParams(
        "TouchEnd and TouchCancel must not have any touch points."));
    return;
  }
  if (pointer_action_type ==
          SyntheticPointerActionParams::PointerActionType::PRESS &&
      !pointer_ids_.empty()) {
    callback->sendFailure(Response::InvalidParams(
        "Must have no prior active touch points to start a new touch."));
    return;
  }
  if (pointer_action_type !=
          SyntheticPointerActionParams::PointerActionType::PRESS &&
      pointer_ids_.empty()) {
    callback->sendFailure(Response::InvalidParams(
        "Must send a TouchStart first to start a new touch."));
    return;
  }

  SyntheticGestureParams::GestureSourceType gesture_source_type =
      SyntheticGestureParams::GestureSourceType::TOUCH_INPUT;
  SyntheticPointerActionListParams action_list_params;
  SyntheticPointerActionListParams::ParamList param_list;
  action_list_params.gesture_source_type = gesture_source_type;
  if (pointer_action_type ==
          SyntheticPointerActionParams::PointerActionType::RELEASE ||
      pointer_action_type ==
          SyntheticPointerActionParams::PointerActionType::CANCEL) {
    for (auto it = pointer_ids_.begin(); it != pointer_ids_.end();) {
      SyntheticPointerActionParams action_params =
          PrepareSyntheticPointerActionParams(pointer_action_type, *it, "", 0,
                                              0, modifiers);
      param_list.push_back(action_params);
      it = pointer_ids_.erase(it);
    }
  }

  size_t with_id = 0;
  gfx::PointF original;
  std::set<int> current_pointer_ids;
  for (size_t i = 0; i < touch_points->size(); ++i) {
    Input::TouchPoint* point = (*touch_points)[i].get();
    int id = point->GetId(i);  // index |i| is default for the id.
    if (point->HasId())
      with_id++;

    SyntheticPointerActionParams::PointerActionType action_type =
        SyntheticPointerActionParams::PointerActionType::MOVE;
    if (pointer_ids_.find(id) == pointer_ids_.end()) {
      pointer_ids_.insert(id);
      action_type = SyntheticPointerActionParams::PointerActionType::PRESS;
    }
    SyntheticPointerActionParams action_params =
        PrepareSyntheticPointerActionParams(
            action_type, id, "", point->GetX(), point->GetY(), modifiers,
            point->GetRadiusX(1.0), point->GetRadiusY(1.0),
            point->GetRotationAngle(0.0), point->GetForce(1.0));
    param_list.push_back(action_params);
    original = gfx::PointF(point->GetX(), point->GetY());
    current_pointer_ids.insert(id);
  }
  if (with_id > 0 && with_id < touch_points->size()) {
    callback->sendFailure(Response::InvalidParams(
        "All or none of the provided TouchPoints must supply ids."));
    return;
  }

  if (pointer_action_type ==
          SyntheticPointerActionParams::PointerActionType::MOVE &&
      current_pointer_ids.size() < pointer_ids_.size()) {
    for (auto it = pointer_ids_.begin(); it != pointer_ids_.end();) {
      if (current_pointer_ids.find(*it) != current_pointer_ids.end()) {
        it++;
        continue;
      }
      SyntheticPointerActionParams action_params =
          PrepareSyntheticPointerActionParams(
              SyntheticPointerActionParams::PointerActionType::RELEASE, *it, "",
              0, 0, modifiers);
      param_list.push_back(action_params);
      it = pointer_ids_.erase(it);
    }
  }
  action_list_params.PushPointerActionParamsList(param_list);

  if (!synthetic_pointer_driver_) {
    synthetic_pointer_driver_ =
        SyntheticPointerDriver::Create(gesture_source_type);
  }
  std::unique_ptr<SyntheticPointerAction> synthetic_gesture =
      std::make_unique<SyntheticPointerAction>(action_list_params);
  synthetic_gesture->SetSyntheticPointerDriver(synthetic_pointer_driver_.get());

  RenderWidgetHostViewBase* root_view = GetRootView();
  if (!root_view) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  root_view->host()->QueueSyntheticGesture(
      std::move(synthetic_gesture),
      base::BindOnce(&DispatchPointerActionsResponse, std::move(callback)));
}

SyntheticPointerActionParams InputHandler::PrepareSyntheticPointerActionParams(
    SyntheticPointerActionParams::PointerActionType pointer_action_type,
    int id,
    const std::string& button_name,
    double x,
    double y,
    int key_modifiers,
    float radius_x,
    float radius_y,
    float rotation_angle,
    float force) {
  SyntheticPointerActionParams action_params(pointer_action_type);
  action_params.set_pointer_id(id);
  SyntheticPointerActionParams::Button button =
      GetPointerActionParamsButton(button_name);
  switch (pointer_action_type) {
    case SyntheticPointerActionParams::PointerActionType::PRESS:
      action_params.set_position(
          gfx::PointF(x * page_scale_factor_, y * page_scale_factor_));
      action_params.set_button(button);
      action_params.set_key_modifiers(key_modifiers);
      action_params.set_width(radius_x * 2.f);
      action_params.set_height(radius_y * 2.f);
      action_params.set_rotation_angle(rotation_angle);
      action_params.set_force(force);
      break;
    case SyntheticPointerActionParams::PointerActionType::MOVE:
      action_params.set_position(
          gfx::PointF(x * page_scale_factor_, y * page_scale_factor_));
      action_params.set_key_modifiers(key_modifiers);
      action_params.set_width(radius_x * 2.f);
      action_params.set_height(radius_y * 2.f);
      action_params.set_rotation_angle(rotation_angle);
      action_params.set_force(force);
      break;
    case SyntheticPointerActionParams::PointerActionType::RELEASE:
    case SyntheticPointerActionParams::PointerActionType::CANCEL:
      action_params.set_button(button);
      action_params.set_key_modifiers(key_modifiers);
      break;
    case SyntheticPointerActionParams::PointerActionType::LEAVE:
    case SyntheticPointerActionParams::PointerActionType::IDLE:
    case SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED:
      NOTREACHED();
      break;
  }
  return action_params;
}

Response InputHandler::EmulateTouchFromMouseEvent(const std::string& type,
                                                  int x,
                                                  int y,
                                                  const std::string& button,
                                                  Maybe<double> maybe_timestamp,
                                                  Maybe<double> delta_x,
                                                  Maybe<double> delta_y,
                                                  Maybe<int> modifiers,
                                                  Maybe<int> click_count) {
  blink::WebInputEvent::Type event_type;
  if (type == Input::EmulateTouchFromMouseEvent::TypeEnum::MouseWheel) {
    event_type = blink::WebInputEvent::kMouseWheel;
    if (!delta_x.isJust() || !delta_y.isJust()) {
      return Response::InvalidParams(
          "'deltaX' and 'deltaY' are expected for mouseWheel event");
    }
  } else {
    event_type = GetMouseEventType(type);
    if (event_type == blink::WebInputEvent::kUndefined) {
      return Response::InvalidParams(
          base::StringPrintf("Unexpected event type '%s'", type.c_str()));
    }
  }

  blink::WebPointerProperties::Button event_button =
      blink::WebPointerProperties::Button::kNoButton;
  int button_modifiers = 0;
  if (!GetMouseEventButton(button, &event_button, &button_modifiers))
    return Response::InvalidParams("Invalid mouse button");

  ui::WebScopedInputEvent event;
  blink::WebMouseWheelEvent* wheel_event = nullptr;
  blink::WebMouseEvent* mouse_event = nullptr;
  if (type == Input::EmulateTouchFromMouseEvent::TypeEnum::MouseWheel) {
    wheel_event = new blink::WebMouseWheelEvent(
        event_type,
        GetEventModifiers(
            modifiers.fromMaybe(blink::WebInputEvent::kNoModifiers), false,
            false, 0, 0) |
            button_modifiers,
        GetEventTimeTicks(maybe_timestamp));
    mouse_event = wheel_event;
    event.reset(wheel_event);
    wheel_event->delta_x = static_cast<float>(delta_x.fromJust());
    wheel_event->delta_y = static_cast<float>(delta_y.fromJust());
    wheel_event->phase = blink::WebMouseWheelEvent::kPhaseBegan;
  } else {
    mouse_event = new blink::WebMouseEvent(
        event_type,
        GetEventModifiers(
            modifiers.fromMaybe(blink::WebInputEvent::kNoModifiers), false,
            false, 0, 0) |
            button_modifiers,
        GetEventTimeTicks(maybe_timestamp));
    event.reset(mouse_event);
  }

  mouse_event->SetPositionInWidget(x, y);
  mouse_event->button = event_button;
  mouse_event->SetPositionInScreen(x, y);
  mouse_event->click_count = click_count.fromMaybe(0);
  mouse_event->pointer_type = blink::WebPointerProperties::PointerType::kTouch;

  if (!host_ || !host_->GetRenderWidgetHost())
    return Response::InternalError();

  if (wheel_event) {
    host_->GetRenderWidgetHost()->ForwardWheelEvent(*wheel_event);
    // Send a synthetic wheel event with phaseEnded to finish scrolling.
    wheel_event->delta_x = 0;
    wheel_event->delta_y = 0;
    wheel_event->phase = blink::WebMouseWheelEvent::kPhaseEnded;
    wheel_event->dispatch_type = blink::WebInputEvent::kEventNonBlocking;
    host_->GetRenderWidgetHost()->ForwardWheelEvent(*wheel_event);
  } else {
    host_->GetRenderWidgetHost()->ForwardMouseEvent(*mouse_event);
  }
  return Response::OK();
}

Response InputHandler::SetIgnoreInputEvents(bool ignore) {
  ignore_input_events_ = ignore;
  WebContents* web_contents = WebContents::FromRenderFrameHost(host_);
  if (web_contents)
    web_contents->SetIgnoreInputEvents(ignore);
  return Response::OK();
}

void InputHandler::SynthesizePinchGesture(
    double x,
    double y,
    double scale_factor,
    Maybe<int> relative_speed,
    Maybe<std::string> gesture_source_type,
    std::unique_ptr<SynthesizePinchGestureCallback> callback) {
  if (!host_ || !host_->GetRenderWidgetHost()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  SyntheticPinchGestureParams gesture_params;
  const int kDefaultRelativeSpeed = 800;

  gesture_params.scale_factor = scale_factor;
  gesture_params.anchor = CssPixelsToPointF(x, y, page_scale_factor_);
  if (!PointIsWithinContents(gesture_params.anchor)) {
    callback->sendFailure(Response::InvalidParams("Position out of bounds"));
    return;
  }

  gesture_params.relative_pointer_speed_in_pixels_s =
      relative_speed.fromMaybe(kDefaultRelativeSpeed);

  if (!StringToGestureSourceType(
      std::move(gesture_source_type),
      gesture_params.gesture_source_type)) {
    callback->sendFailure(
        Response::InvalidParams("Unknown gestureSourceType"));
    return;
  }

  RenderWidgetHostViewBase* root_view = GetRootView();
  if (!root_view) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  root_view->host()->QueueSyntheticGesture(
      SyntheticGesture::Create(gesture_params),
      base::BindOnce(&SendSynthesizePinchGestureResponse, std::move(callback)));
}

void InputHandler::SynthesizeScrollGesture(
    double x,
    double y,
    Maybe<double> x_distance,
    Maybe<double> y_distance,
    Maybe<double> x_overscroll,
    Maybe<double> y_overscroll,
    Maybe<bool> prevent_fling,
    Maybe<int> speed,
    Maybe<std::string> gesture_source_type,
    Maybe<int> repeat_count,
    Maybe<int> repeat_delay_ms,
    Maybe<std::string> interaction_marker_name,
    std::unique_ptr<SynthesizeScrollGestureCallback> callback) {
  if (!host_ || !host_->GetRenderWidgetHost()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  SyntheticSmoothScrollGestureParams gesture_params;
  const bool kDefaultPreventFling = true;
  const int kDefaultSpeed = 800;

  gesture_params.anchor = CssPixelsToPointF(x, y, page_scale_factor_);
  if (!PointIsWithinContents(gesture_params.anchor)) {
    callback->sendFailure(Response::InvalidParams("Position out of bounds"));
    return;
  }

  gesture_params.prevent_fling =
      prevent_fling.fromMaybe(kDefaultPreventFling);
  gesture_params.speed_in_pixels_s = speed.fromMaybe(kDefaultSpeed);

  if (x_distance.isJust() || y_distance.isJust()) {
    gesture_params.distances.push_back(
        CssPixelsToVector2dF(x_distance.fromMaybe(0),
                             y_distance.fromMaybe(0), page_scale_factor_));
  }

  if (x_overscroll.isJust() || y_overscroll.isJust()) {
    gesture_params.distances.push_back(CssPixelsToVector2dF(
        -x_overscroll.fromMaybe(0), -y_overscroll.fromMaybe(0),
        page_scale_factor_));
  }

  if (!StringToGestureSourceType(
      std::move(gesture_source_type),
      gesture_params.gesture_source_type)) {
    callback->sendFailure(
        Response::InvalidParams("Unknown gestureSourceType"));
    return;
  }

  SynthesizeRepeatingScroll(
      gesture_params, repeat_count.fromMaybe(0),
      base::TimeDelta::FromMilliseconds(repeat_delay_ms.fromMaybe(250)),
      interaction_marker_name.fromMaybe(""), ++last_id_, std::move(callback));
}

void InputHandler::SynthesizeRepeatingScroll(
    SyntheticSmoothScrollGestureParams gesture_params,
    int repeat_count,
    base::TimeDelta repeat_delay,
    std::string interaction_marker_name,
    int id,
    std::unique_ptr<SynthesizeScrollGestureCallback> callback) {
  RenderWidgetHostViewBase* root_view = GetRootView();
  if (!root_view) {
    callback->sendFailure(Response::Error("Frame was detached"));
    return;
  }

  if (!interaction_marker_name.empty()) {
    // TODO(alexclarke): Can we move this elsewhere? It doesn't really fit here.
    TRACE_EVENT_COPY_ASYNC_BEGIN0("benchmark", interaction_marker_name.c_str(),
                                  id);
  }

  root_view->host()->QueueSyntheticGesture(
      SyntheticGesture::Create(gesture_params),
      base::BindOnce(&InputHandler::OnScrollFinished,
                     weak_factory_.GetWeakPtr(), gesture_params, repeat_count,
                     repeat_delay, interaction_marker_name, id,
                     std::move(callback)));
}

void InputHandler::OnScrollFinished(
    SyntheticSmoothScrollGestureParams gesture_params,
    int repeat_count,
    base::TimeDelta repeat_delay,
    std::string interaction_marker_name,
    int id,
    std::unique_ptr<SynthesizeScrollGestureCallback> callback,
    SyntheticGesture::Result result) {
  if (!interaction_marker_name.empty()) {
    TRACE_EVENT_COPY_ASYNC_END0("benchmark", interaction_marker_name.c_str(),
                                id);
  }

  if (repeat_count > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&InputHandler::SynthesizeRepeatingScroll,
                       weak_factory_.GetWeakPtr(), gesture_params,
                       repeat_count - 1, repeat_delay, interaction_marker_name,
                       id, std::move(callback)),
        repeat_delay);
  } else {
    SendSynthesizeScrollGestureResponse(std::move(callback), result);
  }
}

void InputHandler::SynthesizeTapGesture(
    double x,
    double y,
    Maybe<int> duration,
    Maybe<int> tap_count,
    Maybe<std::string> gesture_source_type,
    std::unique_ptr<SynthesizeTapGestureCallback> callback) {
  if (!host_ || !host_->GetRenderWidgetHost()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  SyntheticTapGestureParams gesture_params;
  const int kDefaultDuration = 50;
  const int kDefaultTapCount = 1;

  gesture_params.position = CssPixelsToPointF(x, y, page_scale_factor_);
  if (!PointIsWithinContents(gesture_params.position)) {
    callback->sendFailure(Response::InvalidParams("Position out of bounds"));
    return;
  }

  gesture_params.duration_ms = duration.fromMaybe(kDefaultDuration);

  if (!StringToGestureSourceType(
      std::move(gesture_source_type),
      gesture_params.gesture_source_type)) {
    callback->sendFailure(
        Response::InvalidParams("Unknown gestureSourceType"));
    return;
  }

  int count = tap_count.fromMaybe(kDefaultTapCount);
  if (!count) {
    callback->sendSuccess();
    return;
  }

  RenderWidgetHostViewBase* root_view = GetRootView();
  if (!root_view) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  TapGestureResponse* response =
      new TapGestureResponse(std::move(callback), count);
  for (int i = 0; i < count; i++) {
    root_view->host()->QueueSyntheticGesture(
        SyntheticGesture::Create(gesture_params),
        base::BindOnce(&TapGestureResponse::OnGestureResult,
                       base::Unretained(response)));
  }
}

void InputHandler::ClearInputState() {
  while (!injectors_.empty())
    (*injectors_.begin())->Cleanup();
  // TODO(dgozman): cleanup touch callbacks as well?
  pointer_ids_.clear();
}

bool InputHandler::PointIsWithinContents(gfx::PointF point) const {
  gfx::Rect bounds = host_->GetView()->GetViewBounds();
  bounds -= bounds.OffsetFromOrigin();  // Translate the bounds to (0,0).
  return bounds.Contains(point.x(), point.y());
}

InputHandler::InputInjector* InputHandler::EnsureInjector(
    RenderWidgetHostImpl* widget_host) {
  for (auto& it : injectors_) {
    if (it->HasWidgetHost(widget_host))
      return it.get();
  }
  InputInjector* injector = new InputInjector(this, widget_host);
  injectors_.emplace(injector);
  return injector;
}

RenderWidgetHostImpl* InputHandler::FindTargetWidgetHost(
    const gfx::PointF& point,
    gfx::PointF* transformed) {
  *transformed = point;

  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (!widget_host)
    return nullptr;

  if (!host_->GetParent() && widget_host->delegate() &&
      widget_host->delegate()->GetInputEventRouter() &&
      widget_host->GetView()) {
    widget_host = widget_host->delegate()
                      ->GetInputEventRouter()
                      ->GetRenderWidgetHostAtPoint(widget_host->GetView(),
                                                   point, transformed);
  }

  return widget_host;
}

RenderWidgetHostViewBase* InputHandler::GetRootView() {
  if (!host_)
    return nullptr;

  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(host_->GetView());
  if (!view)
    return nullptr;

  return view->GetRootView();
}

}  // namespace protocol
}  // namespace content
