// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/input_handler.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/protocol/native_input_event_builder.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/renderer_host/data_transfer_util.h"
#include "content/browser/renderer_host/input/synthetic_pointer_action.h"
#include "content/browser/renderer_host/input/touch_emulator.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/range/range.h"

namespace content::protocol {

namespace {

gfx::PointF CssPixelsToPointF(double x, double y, float scale_factor) {
  return gfx::PointF(x * scale_factor, y * scale_factor);
}

gfx::Vector2dF CssPixelsToVector2dF(double x, double y, float scale_factor) {
  return gfx::Vector2dF(x * scale_factor, y * scale_factor);
}

bool StringToGestureSourceType(Maybe<std::string> in,
                               content::mojom::GestureSourceType& out) {
  if (!in.isJust()) {
    out = content::mojom::GestureSourceType::kDefaultInput;
    return true;
  }
  if (in.fromJust() == Input::GestureSourceTypeEnum::Default) {
    out = content::mojom::GestureSourceType::kDefaultInput;
    return true;
  }
  if (in.fromJust() == Input::GestureSourceTypeEnum::Touch) {
    out = content::mojom::GestureSourceType::kTouchInput;
    return true;
  }
  if (in.fromJust() == Input::GestureSourceTypeEnum::Mouse) {
    out = content::mojom::GestureSourceType::kMouseInput;
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
  return timestamp.isJust() ? base::Seconds(timestamp.fromJust()) +
                                  base::TimeTicks::UnixEpoch()
                            : base::TimeTicks::Now();
}

bool SetKeyboardEventText(
    char16_t (&to)[blink::WebKeyboardEvent::kTextLengthCap],
    Maybe<std::string> from) {
  if (!from.isJust())
    return true;

  std::u16string text16 = base::UTF8ToUTF16(from.fromJust());
  if (text16.size() >= blink::WebKeyboardEvent::kTextLengthCap)
    return false;

  for (size_t i = 0; i < text16.size(); ++i)
    to[i] = text16[i];
  to[text16.size()] = 0;
  return true;
}

bool GetMouseEventButton(const std::string& button,
                         blink::WebPointerProperties::Button* event_button,
                         int* event_modifiers) {
  *event_modifiers = blink::WebInputEvent::kFromDebugger;
  if (button.empty())
    return true;

  if (button == Input::MouseButtonEnum::None) {
    *event_button = blink::WebMouseEvent::Button::kNoButton;
  } else if (button == Input::MouseButtonEnum::Left) {
    *event_button = blink::WebMouseEvent::Button::kLeft;
    *event_modifiers |= blink::WebInputEvent::kLeftButtonDown;
  } else if (button == Input::MouseButtonEnum::Middle) {
    *event_button = blink::WebMouseEvent::Button::kMiddle;
    *event_modifiers |= blink::WebInputEvent::kMiddleButtonDown;
  } else if (button == Input::MouseButtonEnum::Right) {
    *event_button = blink::WebMouseEvent::Button::kRight;
    *event_modifiers |= blink::WebInputEvent::kRightButtonDown;
  } else if (button == Input::MouseButtonEnum::Back) {
    *event_button = blink::WebMouseEvent::Button::kBack;
    *event_modifiers |= blink::WebInputEvent::kBackButtonDown;
  } else if (button == Input::MouseButtonEnum::Forward) {
    *event_button = blink::WebMouseEvent::Button::kForward;
    *event_modifiers |= blink::WebInputEvent::kForwardButtonDown;
  } else {
    return false;
  }
  return true;
}

blink::WebInputEvent::Type GetMouseEventType(const std::string& type) {
  if (type == Input::DispatchMouseEvent::TypeEnum::MousePressed)
    return blink::WebInputEvent::Type::kMouseDown;
  if (type == Input::DispatchMouseEvent::TypeEnum::MouseReleased)
    return blink::WebInputEvent::Type::kMouseUp;
  if (type == Input::DispatchMouseEvent::TypeEnum::MouseMoved)
    return blink::WebInputEvent::Type::kMouseMove;
  if (type == Input::DispatchMouseEvent::TypeEnum::MouseWheel)
    return blink::WebInputEvent::Type::kMouseWheel;
  return blink::WebInputEvent::Type::kUndefined;
}

blink::WebInputEvent::Type GetTouchEventType(const std::string& type) {
  if (type == Input::DispatchTouchEvent::TypeEnum::TouchStart)
    return blink::WebInputEvent::Type::kTouchStart;
  if (type == Input::DispatchTouchEvent::TypeEnum::TouchEnd)
    return blink::WebInputEvent::Type::kTouchEnd;
  if (type == Input::DispatchTouchEvent::TypeEnum::TouchMove)
    return blink::WebInputEvent::Type::kTouchMove;
  if (type == Input::DispatchTouchEvent::TypeEnum::TouchCancel)
    return blink::WebInputEvent::Type::kTouchCancel;
  return blink::WebInputEvent::Type::kUndefined;
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
        type == blink::WebInputEvent::Type::kTouchCancel
            ? blink::WebTouchPoint::State::kStateCancelled
            : blink::WebTouchPoint::State::kStateStationary;
    event->touches_length++;
  }
  if (type == blink::WebInputEvent::Type::kTouchCancel ||
      type == blink::WebInputEvent::Type::kTouchEnd) {
    event->touches[0].state = type == blink::WebInputEvent::Type::kTouchCancel
                                  ? blink::WebTouchPoint::State::kStateCancelled
                                  : blink::WebTouchPoint::State::kStateReleased;
    event->SetType(type);
  } else if (points.find(changing.id) == points.end()) {
    event->touches[0].state = blink::WebTouchPoint::State::kStatePressed;
    event->SetType(blink::WebInputEvent::Type::kTouchStart);
  } else {
    event->touches[0].state = blink::WebTouchPoint::State::kStateMoved;
    event->SetType(blink::WebInputEvent::Type::kTouchMove);
  }
  return true;
}

std::string ValidatePointerEventProperties(double force,
                                           double tangential_pressure,
                                           int tilt_x,
                                           int tilt_y,
                                           int twist) {
  if (force < 0 || force > 1)
    return "'force' should be in the range of  [0,1]";
  if (tangential_pressure < -1 || tangential_pressure > 1)
    return "'tangential_pressure' should be in the range of  [-1,1]";
  if (tilt_x < -90 || tilt_x > 90)
    return "'tilt_x' should be in the range of  [-90,90]";
  if (tilt_y < -90 || tilt_y > 90)
    return "'tilt_y' should be in the range of  [-90,90]";
  if (twist < 0 || twist > 359)
    return "'twist' should be in the range of  [0,359]";
  return "";
}

void SendSynthesizePinchGestureResponse(
    std::unique_ptr<Input::Backend::SynthesizePinchGestureCallback> callback,
    SyntheticGesture::Result result) {
  if (result == SyntheticGesture::Result::GESTURE_FINISHED) {
    callback->sendSuccess();
  } else {
    callback->sendFailure(Response::ServerError(
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
        callback_->sendFailure(Response::ServerError(
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
    callback->sendFailure(Response::ServerError(
        base::StringPrintf("Synthetic scroll failed, result was %d", result)));
  }
}

void DispatchPointerActionsResponse(
    std::unique_ptr<Input::Backend::DispatchTouchEventCallback> callback,
    SyntheticGesture::Result result) {
  if (result == SyntheticGesture::Result::GESTURE_FINISHED) {
    callback->sendSuccess();
  } else {
    callback->sendFailure(Response::ServerError(
        base::StringPrintf("Action sequence failed, result was %d", result)));
  }
}

DropData ProtocolDragDataToDropData(std::unique_ptr<Input::DragData> data) {
  std::vector<blink::mojom::DragItemPtr> items;

  for (const auto& item : *data->GetItems()) {
    blink::mojom::DragItemStringPtr mojo_item =
        blink::mojom::DragItemString::New();
    mojo_item->string_type = item->GetMimeType();
    mojo_item->string_data = base::UTF8ToUTF16(item->GetData());
    if (item->HasBaseURL())
      mojo_item->base_url = GURL(item->GetBaseURL(""));
    if (item->HasTitle())
      mojo_item->title = base::UTF8ToUTF16(item->GetTitle(""));
    items.push_back(blink::mojom::DragItem::NewString(std::move(mojo_item)));
  }

  blink::mojom::DragDataPtr mojo_data =
      blink::mojom::DragData::New(std::move(items), absl::nullopt,
                                  network::mojom::ReferrerPolicy::kDefault);
  DropData drop_data = DragDataToDropData(*mojo_data);

  protocol::Array<protocol::String> default_value;
  for (const auto& file : *data->GetFiles(&default_value)) {
    drop_data.filenames.emplace_back(base::FilePath::FromUTF8Unsafe(file),
                                     base::FilePath());
  }

  return drop_data;
}

base::expected<std::unique_ptr<blink::WebMouseEvent>, protocol::Response>
CreateWebMouseEvent(const std::string& event_type,
                    Maybe<int> modifiers,
                    Maybe<double> timestamp,
                    Maybe<std::string> button,
                    Maybe<int> buttons,
                    Maybe<int> click_count,
                    Maybe<double> force,
                    Maybe<double> tangential_pressure,
                    Maybe<int> tilt_x,
                    Maybe<int> tilt_y,
                    Maybe<int> twist,
                    Maybe<double> delta_x,
                    Maybe<double> delta_y,
                    Maybe<std::string> pointer_type) {
  blink::WebInputEvent::Type type = GetMouseEventType(event_type);
  if (type == blink::WebInputEvent::Type::kUndefined) {
    return base::unexpected(Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", event_type.c_str())));
  }

  blink::WebPointerProperties::Button event_button =
      blink::WebPointerProperties::Button::kNoButton;
  int button_modifiers = 0;
  if (!GetMouseEventButton(button.fromMaybe(""), &event_button,
                           &button_modifiers)) {
    return base::unexpected(Response::InvalidParams("Invalid mouse button"));
  }

  int event_modifiers =
      GetEventModifiers(modifiers.fromMaybe(blink::WebInputEvent::kNoModifiers),
                        false, false, 0, buttons.fromMaybe(0));
  event_modifiers |= button_modifiers;
  base::TimeTicks event_timestamp = GetEventTimeTicks(timestamp);

  std::unique_ptr<blink::WebMouseEvent> mouse_event;

  if (type == blink::WebInputEvent::Type::kMouseWheel) {
    auto wheel_event = std::make_unique<blink::WebMouseWheelEvent>(
        type, event_modifiers, event_timestamp);
    if (!delta_x.isJust() || !delta_y.isJust()) {
      return base::unexpected(Response::InvalidParams(
          "'deltaX' and 'deltaY' are expected for mouseWheel event"));
    }
    wheel_event->delta_x = static_cast<float>(-delta_x.fromJust());
    wheel_event->delta_y = static_cast<float>(-delta_y.fromJust());
    wheel_event->phase = blink::WebMouseWheelEvent::kPhaseBegan;
    wheel_event->dispatch_type = blink::WebInputEvent::DispatchType::kBlocking;
    mouse_event = std::move(wheel_event);
  } else {
    mouse_event = std::make_unique<blink::WebMouseEvent>(type, event_modifiers,
                                                         event_timestamp);
    std::string message = ValidatePointerEventProperties(
        force.fromMaybe(0), tangential_pressure.fromMaybe(0),
        tilt_x.fromMaybe(0), tilt_y.fromMaybe(0), twist.fromMaybe(0));
    if (!message.empty()) {
      return base::unexpected(Response::InvalidParams(message));
    }
  }

  mouse_event->button = event_button;
  mouse_event->click_count = click_count.fromMaybe(0);
  mouse_event->pointer_type = GetPointerType(pointer_type.fromMaybe(""));
  mouse_event->force = force.fromMaybe(0);
  mouse_event->tangential_pressure = tangential_pressure.fromMaybe(0);
  mouse_event->tilt_x = tilt_x.fromMaybe(0);
  mouse_event->tilt_y = tilt_y.fromMaybe(0);
  mouse_event->twist = twist.fromMaybe(0);

  return mouse_event;
}

}  // namespace

class InputHandler::InputInjector
    : public RenderWidgetHost::InputEventObserver {
 public:
  InputInjector(InputHandler* owner, RenderWidgetHostImpl* widget_host)
      : owner_(owner), widget_host_(widget_host->GetWeakPtr()) {
    widget_host->AddInputEventObserver(this);
  }

  InputInjector(const InputInjector&) = delete;
  InputInjector& operator=(const InputInjector&) = delete;

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
    wheel_event->dispatch_type =
        blink::WebInputEvent::DispatchType::kEventNonBlocking;
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
                           Maybe<Array<std::string>> commands,
                           std::unique_ptr<DispatchKeyEventCallback> callback) {
    if (!widget_host_) {
      callback->sendFailure(Response::InternalError());
      return;
    }

    widget_host_->Focus();
    input_queued_ = false;
    pending_key_callbacks_.push_back(std::move(callback));
    ui::LatencyInfo latency;
    std::vector<blink::mojom::EditCommandPtr> edit_commands;
    if (commands.isJust()) {
      for (const std::string& command : *commands.fromJust())
        edit_commands.push_back(blink::mojom::EditCommand::New(command, ""));
    }
    // This may close the target, for example, if pressing Ctrl+W.
    base::WeakPtr<InputHandler::InputInjector> weak_this =
        weak_ptr_factory_.GetWeakPtr();
    widget_host_->ForwardKeyboardEventWithCommands(keyboard_event, latency,
                                                   std::move(edit_commands));
    if (!weak_this)
      return;
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

  void OnInputEventAck(blink::mojom::InputEventResultSource source,
                       blink::mojom::InputEventResultState state,
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
         event.GetType() == blink::WebInputEvent::Type::kMouseWheel) &&
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
  base::WeakPtrFactory<InputHandler::InputInjector> weak_ptr_factory_{this};
};

InputHandler::InputHandler(bool allow_file_access,
                           bool allow_sending_input_to_browser)
    : DevToolsDomainHandler(Input::Metainfo::domainName),
      host_(nullptr),
      last_id_(0),
      allow_file_access_(allow_file_access),
      allow_sending_input_to_browser_(allow_sending_input_to_browser) {}

InputHandler::~InputHandler() = default;

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

  auto* old_web_contents = WebContentsImpl::FromRenderFrameHostImpl(host_);
  host_ = frame_host;
  web_contents_ = WebContentsImpl::FromRenderFrameHostImpl(host_);

  if (ignore_input_events_ && old_web_contents != web_contents_) {
    if (old_web_contents)
      old_web_contents->SetIgnoreInputEvents(false);
    if (web_contents_)
      web_contents_->SetIgnoreInputEvents(true);
  }
}

void InputHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Input::Frontend>(dispatcher->channel());
  Input::Dispatcher::wire(dispatcher, this);
}

Response InputHandler::Disable() {
  ClearInputState();
  if (web_contents_ && ignore_input_events_)
    web_contents_->SetIgnoreInputEvents(false);
  ignore_input_events_ = false;
  pointer_ids_.clear();
  touch_points_.clear();
  return Response::Success();
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
    Maybe<Array<std::string>> commands,
    std::unique_ptr<DispatchKeyEventCallback> callback) {
  blink::WebInputEvent::Type web_event_type;

  if (type == Input::DispatchKeyEvent::TypeEnum::KeyDown) {
    web_event_type = blink::WebInputEvent::Type::kKeyDown;
  } else if (type == Input::DispatchKeyEvent::TypeEnum::KeyUp) {
    web_event_type = blink::WebInputEvent::Type::kKeyUp;
  } else if (type == Input::DispatchKeyEvent::TypeEnum::Char) {
    web_event_type = blink::WebInputEvent::Type::kChar;
  } else if (type == Input::DispatchKeyEvent::TypeEnum::RawKeyDown) {
    web_event_type = blink::WebInputEvent::Type::kRawKeyDown;
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
  if (event.native_key_code && allow_sending_input_to_browser_)
    event.os_event = NativeInputEventBuilder::CreateEvent(event);
  else
    event.skip_in_browser = true;

  EnsureInjector(widget_host)
      ->InjectKeyboardEvent(event, std::move(commands), std::move(callback));
}

void InputHandler::InsertText(const std::string& text,
                              std::unique_ptr<InsertTextCallback> callback) {
  std::u16string text16 = base::UTF8ToUTF16(text);
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

  base::OnceClosure closure =
      base::BindOnce(&InsertTextCallback::sendSuccess, std::move(callback));

  widget_host->Focus();
  widget_host->GetWidgetInputHandler()->ImeCommitText(
      text16, std::vector<ui::ImeTextSpan>(), gfx::Range::InvalidRange(), 0,
      std::move(closure));
}

void InputHandler::ImeSetComposition(
    const std::string& text,
    int selection_start,
    int selection_end,
    Maybe<int> replacement_start,
    Maybe<int> replacement_end,
    std::unique_ptr<ImeSetCompositionCallback> callback) {
  std::u16string text16 = base::UTF8ToUTF16(text);
  if (!host_ || !host_->GetRenderWidgetHost() || !web_contents_) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  // Currently no DevTools target for Prerender.
  if (host_->GetLifecycleState() ==
      RenderFrameHost::LifecycleState::kPrerendering) {
    NOTREACHED();
  }
  // Portal cannot be focused.
  if (web_contents_->IsPortal()) {
    callback->sendFailure(
        Response::InvalidRequest("A Portal cannot be focused."));
    return;
  }

  // |RenderFrameHostImpl::GetRenderWidgetHost| returns the RWHImpl of the
  // nearest local root of |host_|.
  RenderWidgetHostImpl* widget_host = host_->GetRenderWidgetHost();
  if (widget_host->delegate()) {
    RenderWidgetHostImpl* target_host =
        widget_host->delegate()->GetFocusedRenderWidgetHost(widget_host);
    if (target_host)
      widget_host = target_host;
  }

  // If replacement start and end are not specified, then the range is invalid,
  // so no replacing will be done.
  gfx::Range replacement_range = gfx::Range::InvalidRange();

  // Check if replacement_start and end parameters were passed in
  if (replacement_start.isJust()) {
    replacement_range.set_start(replacement_start.fromJust());
    if (replacement_end.isJust()) {
      replacement_range.set_end(replacement_end.fromJust());
    } else {
      callback->sendFailure(Response::InvalidParams(
          "Either both replacement start/end are specified or neither."));
      return;
    }
  }

  base::OnceClosure closure = base::BindOnce(
      &ImeSetCompositionCallback::sendSuccess, std::move(callback));

  widget_host->Focus();

  widget_host->GetWidgetInputHandler()->ImeSetComposition(
      text16, std::vector<ui::ImeTextSpan>(), replacement_range,
      selection_start, selection_end, std::move(closure));
}

void InputHandler::DispatchMouseEvent(
    const std::string& event_type,
    double x,
    double y,
    Maybe<int> modifiers,
    Maybe<double> timestamp,
    Maybe<std::string> button,
    Maybe<int> buttons,
    Maybe<int> click_count,
    Maybe<double> force,
    Maybe<double> tangential_pressure,
    Maybe<int> tilt_x,
    Maybe<int> tilt_y,
    Maybe<int> twist,
    Maybe<double> delta_x,
    Maybe<double> delta_y,
    Maybe<std::string> pointer_type,
    std::unique_ptr<DispatchMouseEventCallback> callback) {
  base::expected<std::unique_ptr<blink::WebMouseEvent>, protocol::Response>
      maybe_event = CreateWebMouseEvent(
          event_type, std::move(modifiers), std::move(timestamp),
          std::move(button), std::move(buttons), std::move(click_count),
          std::move(force), std::move(tangential_pressure), std::move(tilt_x),
          std::move(tilt_y), std::move(twist), std::move(delta_x),
          std::move(delta_y), std::move(pointer_type));
  if (!maybe_event.has_value()) {
    callback->sendFailure(std::move(maybe_event).error());
    return;
  }

  std::unique_ptr<blink::WebMouseEvent> event = std::move(maybe_event).value();
  bool is_wheel_event =
      event->GetType() == blink::WebInputEvent::Type::kMouseWheel;

  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (!widget_host || !widget_host->delegate() ||
      !widget_host->delegate()->GetInputEventRouter() ||
      !widget_host->GetView()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  auto findWidgetAndDispatchEvent = base::BindOnce(
      [](base::WeakPtr<InputHandler> self,
         base::WeakPtr<RenderWidgetHostImpl> widget_host, double x, double y,
         std::unique_ptr<blink::WebMouseEvent> event,
         std::unique_ptr<DispatchMouseEventCallback> callback, bool success) {
        if (!self || !widget_host)
          return;
        widget_host->delegate()
            ->GetInputEventRouter()
            ->GetRenderWidgetHostAtPointAsynchronously(
                widget_host->GetView(),
                CssPixelsToPointF(x, y, self->ScaleFactor()),
                base::BindOnce(&InputHandler::OnWidgetForDispatchMouseEvent,
                               self, std::move(callback), std::move(event)));
      },
      weak_factory_.GetWeakPtr(), widget_host->GetWeakPtr(), x, y,
      std::move(event), std::move(callback));
  // We make sure the compositor is up to date before sending a wheel event.
  // Otherwise it wont be picked up by newly added event listeners on the main
  // thread.
  if (is_wheel_event) {
    widget_host->InsertVisualStateCallback(
        std::move(findWidgetAndDispatchEvent));
  } else {
    std::move(findWidgetAndDispatchEvent).Run(true);
  }
}

void InputHandler::DispatchDragEvent(
    const std::string& event_type,
    double x,
    double y,
    std::unique_ptr<Input::DragData> data,
    Maybe<int> modifiers,
    std::unique_ptr<DispatchDragEventCallback> callback) {
  if (!allow_file_access_ && data->HasFiles()) {
    callback->sendFailure(Response::InvalidParams("Not allowed"));
    return;
  }

  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (!widget_host || !widget_host->delegate() ||
      !widget_host->delegate()->GetInputEventRouter() ||
      !widget_host->GetView()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  widget_host->delegate()
      ->GetInputEventRouter()
      ->GetRenderWidgetHostAtPointAsynchronously(
          widget_host->GetView(), CssPixelsToPointF(x, y, ScaleFactor()),
          base::BindOnce(&InputHandler::OnWidgetForDispatchDragEvent,
                         weak_factory_.GetWeakPtr(), event_type, x, y,
                         std::move(data), std::move(modifiers),
                         std::move(callback)));
}

void InputHandler::OnWidgetForDispatchDragEvent(
    const std::string& event_type,
    double x,
    double y,
    std::unique_ptr<Input::DragData> data,
    Maybe<int> modifiers,
    std::unique_ptr<DispatchDragEventCallback> callback,
    base::WeakPtr<RenderWidgetHostViewBase> target,
    absl::optional<gfx::PointF> maybe_point) {
  if (!target || !maybe_point.has_value()) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  auto point = *maybe_point;
  RenderWidgetHostImpl* widget_host =
      RenderWidgetHostImpl::From(target->GetRenderWidgetHost());
  auto mask =
      static_cast<blink::DragOperationsMask>(data->GetDragOperationsMask());
  std::unique_ptr<DropData> drop_data =
      std::make_unique<DropData>(ProtocolDragDataToDropData(std::move(data)));
  drop_data->view_id = widget_host->GetRoutingID();
  int event_modifiers =
      GetEventModifiers(modifiers.fromMaybe(blink::WebInputEvent::kNoModifiers),
                        false, false, 0, 0);
  if (event_type == Input::DispatchDragEvent::TypeEnum::DragEnter) {
    widget_host->DragTargetDragEnter(
        *drop_data, point, point, mask, event_modifiers,
        base::BindOnce(
            [](std::unique_ptr<DispatchDragEventCallback> callback,
               ::ui::mojom::DragOperation operation) {
              callback->sendSuccess();
            },
            std::move(callback)));
  } else if (event_type == Input::DispatchDragEvent::TypeEnum::DragOver) {
    widget_host->DragTargetDragOver(
        point, point, mask, event_modifiers,
        base::BindOnce(
            [](std::unique_ptr<DispatchDragEventCallback> callback,
               ::ui::mojom::DragOperation operation) {
              callback->sendSuccess();
            },
            std::move(callback)));
  } else if (event_type == Input::DispatchDragEvent::TypeEnum::Drop) {
    widget_host->DragTargetDragOver(
        point, point, mask, event_modifiers,
        base::BindOnce(
            [](std::unique_ptr<DropData> drop_data, int event_modifiers,
               std::unique_ptr<DispatchDragEventCallback> callback,
               base::WeakPtr<RenderWidgetHostViewBase> target,
               gfx::PointF point, ui::mojom::DragOperation current_op) {
              if (!target) {
                callback->sendFailure(Response::InternalError());
                return;
              }
              RenderWidgetHostImpl* widget_host =
                  RenderWidgetHostImpl::From(target->GetRenderWidgetHost());
              widget_host->DragTargetDrop(*drop_data, point, point,
                                          event_modifiers, base::DoNothing());
              widget_host->DragSourceSystemDragEnded();
              widget_host->DragSourceEndedAt(
                  point, point, current_op,
                  base::BindOnce(
                      [](std::unique_ptr<DispatchDragEventCallback> callback) {
                        callback->sendSuccess();
                      },
                      std::move(callback)));
            },
            std::move(drop_data), event_modifiers, std::move(callback),
            std::move(target), point));

  } else if (event_type == Input::DispatchDragEvent::TypeEnum::DragCancel) {
    widget_host->DragSourceSystemDragEnded();
    widget_host->DragSourceEndedAt(
        point, point, ui::mojom::DragOperation::kNone,
        base::BindOnce(
            [](std::unique_ptr<DispatchDragEventCallback> callback) {
              callback->sendSuccess();
            },
            std::move(callback)));
  } else {
    callback->sendFailure(Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", event_type.c_str())));
  }
}

float InputHandler::ScaleFactor() {
  DCHECK(web_contents_);
  return blink::PageZoomLevelToZoomFactor(
             web_contents_->GetPendingPageZoomLevel()) *
         web_contents_->GetPrimaryPage().GetPageScaleFactor();
}

void InputHandler::StartDragging(const blink::mojom::DragData& drag_data,
                                 blink::DragOperationsMask drag_operations_mask,
                                 bool* intercepted) {
  if (!intercept_drags_ || *intercepted)
    return;
  *intercepted = true;
  auto items =
      std::make_unique<protocol::Array<protocol::Input::DragDataItem>>();
  for (const auto& item : drag_data.items) {
    if (!item->is_string())
      continue;
    const auto& string_item = item->get_string();
    auto protocol_item =
        protocol::Input::DragDataItem::Create()
            .SetMimeType(string_item->string_type)
            .SetData(base::UTF16ToUTF8(string_item->string_data))
            .Build();
    if (string_item->base_url.has_value())
      protocol_item->SetBaseURL(string_item->base_url->spec());
    if (string_item->title.has_value())
      protocol_item->SetTitle(base::UTF16ToUTF8(string_item->title.value()));
    items->push_back(std::move(protocol_item));
  }
  frontend_->DragIntercepted(protocol::Input::DragData::Create()
                                 .SetDragOperationsMask(drag_operations_mask)
                                 .SetItems(std::move(items))
                                 .Build());
}

void InputHandler::DragEnded() {}

Response InputHandler::SetInterceptDrags(bool enabled) {
  intercept_drags_ = enabled;
  return Response::Success();
}

void InputHandler::OnWidgetForDispatchMouseEvent(
    std::unique_ptr<DispatchMouseEventCallback> callback,
    std::unique_ptr<blink::WebMouseEvent> event,
    base::WeakPtr<RenderWidgetHostViewBase> target,
    absl::optional<gfx::PointF> point) {
  if (!target || !point.has_value()) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  event->SetPositionInWidget(*point);
  event->SetPositionInScreen(event->PositionInWidget());

  RenderWidgetHostImpl* widget_host =
      RenderWidgetHostImpl::From(target->GetRenderWidgetHost());
  if (event->GetType() == blink::WebInputEvent::Type::kMouseWheel) {
    EnsureInjector(widget_host)
        ->InjectWheelEvent(static_cast<blink::WebMouseWheelEvent*>(event.get()),
                           std::move(callback));
  } else {
    EnsureInjector(widget_host)->InjectMouseEvent(*event, std::move(callback));
  }
}

void InputHandler::DispatchTouchEvent(
    const std::string& event_type,
    std::unique_ptr<Array<Input::TouchPoint>> touch_points,
    protocol::Maybe<int> modifiers,
    protocol::Maybe<double> timestamp,
    std::unique_ptr<DispatchTouchEventCallback> callback) {
  if (base::FeatureList::IsEnabled(features::kSyntheticPointerActions)) {
    DispatchSyntheticPointerActionTouch(
        event_type, std::move(touch_points), std::move(modifiers),
        std::move(timestamp), std::move(callback));
    return;
  }

  DispatchWebTouchEvent(event_type, std::move(touch_points),
                        std::move(modifiers), std::move(timestamp),
                        std::move(callback));
}

void InputHandler::DispatchWebTouchEvent(
    const std::string& event_type,
    std::unique_ptr<Array<Input::TouchPoint>> touch_points,
    protocol::Maybe<int> modifiers,
    protocol::Maybe<double> timestamp,
    std::unique_ptr<DispatchTouchEventCallback> callback) {
  blink::WebInputEvent::Type type = GetTouchEventType(event_type);
  if (type == blink::WebInputEvent::Type::kUndefined) {
    callback->sendFailure(Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", event_type.c_str())));
    return;
  }

  int event_modifiers =
      GetEventModifiers(modifiers.fromMaybe(blink::WebInputEvent::kNoModifiers),
                        false, false, 0, 0);
  base::TimeTicks event_timestamp = GetEventTimeTicks(timestamp);

  if ((type == blink::WebInputEvent::Type::kTouchStart ||
       type == blink::WebInputEvent::Type::kTouchMove) &&
      touch_points->empty()) {
    callback->sendFailure(Response::InvalidParams(
        "TouchStart and TouchMove must have at least one touch point."));
    return;
  }
  if (type == blink::WebInputEvent::Type::kTouchCancel &&
      !touch_points->empty()) {
    callback->sendFailure(
        Response::InvalidParams("TouchCancel must not have any touch points."));
    return;
  }
  if (type != blink::WebInputEvent::Type::kTouchStart &&
      touch_points_.empty()) {
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
    std::string message = ValidatePointerEventProperties(
        point->GetForce(1.0), point->GetTangentialPressure(0),
        point->GetTiltX(0), point->GetTiltY(0), point->GetTwist(0));
    if (!message.empty()) {
      callback->sendFailure(Response::InvalidParams(message));
      return;
    }
    points[id].id = id;
    points[id].radius_x = point->GetRadiusX(1.0);
    points[id].radius_y = point->GetRadiusY(1.0);
    points[id].rotation_angle = point->GetRotationAngle(0.0);
    points[id].force = point->GetForce(1.0);
    points[id].pointer_type = blink::WebPointerProperties::PointerType::kTouch;
    points[id].SetPositionInWidget(point->GetX() * ScaleFactor(),
                                   point->GetY() * ScaleFactor());
    points[id].SetPositionInScreen(point->GetX() * ScaleFactor(),
                                   point->GetY() * ScaleFactor());
    points[id].tilt_x = point->GetTiltX(0);
    points[id].tilt_y = point->GetTiltY(0);
    points[id].tangential_pressure = point->GetTangentialPressure(0);
    points[id].twist = point->GetTwist(0);
  }
  if (with_id > 0 && with_id < touch_points->size()) {
    callback->sendFailure(Response::InvalidParams(
        "All or none of the provided TouchPoints must supply ids."));
    return;
  }

  std::vector<blink::WebTouchEvent> events;
  bool ok = true;
  for (auto& id_point : points) {
    if (touch_points_.find(id_point.first) != touch_points_.end() &&
        type == blink::WebInputEvent::Type::kTouchMove &&
        touch_points_[id_point.first].PositionInWidget() ==
            id_point.second.PositionInWidget()) {
      continue;
    }

    events.emplace_back(type, event_modifiers, event_timestamp);
    ok &= GenerateTouchPoints(&events.back(), type, touch_points_,
                              id_point.second);
    if (type == blink::WebInputEvent::Type::kTouchStart ||
        type == blink::WebInputEvent::Type::kTouchMove) {
      touch_points_[id_point.first] = id_point.second;
    } else if (type == blink::WebInputEvent::Type::kTouchEnd) {
      touch_points_.erase(id_point.first);
    }
  }

  if (touch_points->size() == 0 && touch_points_.size() > 0) {
    if (type == blink::WebInputEvent::Type::kTouchCancel) {
      events.emplace_back(type, event_modifiers, event_timestamp);
      ok &= GenerateTouchPoints(&events.back(), type, touch_points_,
                                touch_points_.begin()->second);
      touch_points_.clear();
    } else if (type == blink::WebInputEvent::Type::kTouchEnd) {
      for (auto it = touch_points_.begin(); it != touch_points_.end();) {
        events.emplace_back(type, event_modifiers, event_timestamp);
        ok &= GenerateTouchPoints(&events.back(), type, touch_points_,
                                  it->second);
        it = touch_points_.erase(it);
      }
    }
  }
  if (!ok) {
    callback->sendFailure(Response::ServerError(
        base::StringPrintf("Exceeded maximum touch points limit of %d",
                           blink::WebTouchEvent::kTouchesLengthCap)));
    return;
  }

  if (events.empty()) {
    callback->sendSuccess();
    return;
  }

  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (!widget_host || !widget_host->delegate() ||
      !widget_host->delegate()->GetInputEventRouter() ||
      !widget_host->GetView()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  // We make sure the compositor is up to date before
  // sending a touch event. Otherwise it wont be
  // picked up by newly added event listeners on the main thread.
  widget_host->InsertVisualStateCallback(base::BindOnce(
      [](base::WeakPtr<InputHandler> self,
         base::WeakPtr<RenderWidgetHostImpl> widget_host,
         std::vector<blink::WebTouchEvent> events,
         std::unique_ptr<DispatchTouchEventCallback> callback, bool success) {
        if (!self || !widget_host)
          return;
        gfx::PointF original(events[0].touches[0].PositionInWidget());
        widget_host->delegate()
            ->GetInputEventRouter()
            ->GetRenderWidgetHostAtPointAsynchronously(
                widget_host->GetView(), original,
                base::BindOnce(&InputHandler::OnWidgetForDispatchWebTouchEvent,
                               self, std::move(callback), std::move(events)));
      },
      weak_factory_.GetWeakPtr(), widget_host->GetWeakPtr(), std::move(events),
      std::move(callback)));
}

void InputHandler::OnWidgetForDispatchWebTouchEvent(
    std::unique_ptr<DispatchTouchEventCallback> callback,
    std::vector<blink::WebTouchEvent> events,
    base::WeakPtr<RenderWidgetHostViewBase> target,
    absl::optional<gfx::PointF> transformed) {
  if (!target || !transformed.has_value()) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  RenderWidgetHostImpl* widget_host =
      RenderWidgetHostImpl::From(target->GetRenderWidgetHost());

  gfx::PointF original(events[0].touches[0].PositionInWidget());
  gfx::Vector2dF delta = *transformed - original;
  for (auto& event : events) {
    event.dispatch_type =
        event.GetType() == blink::WebInputEvent::Type::kTouchCancel
            ? blink::WebInputEvent::DispatchType::kEventNonBlocking
            : blink::WebInputEvent::DispatchType::kBlocking;
    event.moved_beyond_slop_region = true;
    event.unique_touch_event_id = ui::GetNextTouchEventId();
    for (unsigned j = 0; j < event.touches_length; j++) {
      gfx::PointF point = event.touches[j].PositionInWidget();
      event.touches[j].SetPositionInWidget(point.x() + delta.x(),
                                           point.y() + delta.y());
      point = event.touches[j].PositionInScreen();
      event.touches[j].SetPositionInScreen(point.x() + delta.x(),
                                           point.y() + delta.y());
    }
  }
  EnsureInjector(widget_host)->InjectTouchEvents(events, std::move(callback));
}

void InputHandler::DispatchSyntheticPointerActionTouch(
    const std::string& event_type,
    std::unique_ptr<Array<Input::TouchPoint>> touch_points,
    protocol::Maybe<int> modifiers,
    protocol::Maybe<double> timestamp,
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

  int event_modifiers =
      GetEventModifiers(modifiers.fromMaybe(blink::WebInputEvent::kNoModifiers),
                        false, false, 0, 0);

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

  content::mojom::GestureSourceType gesture_source_type =
      content::mojom::GestureSourceType::kTouchInput;
  SyntheticPointerActionListParams action_list_params;
  SyntheticPointerActionListParams::ParamList param_list;
  action_list_params.gesture_source_type = gesture_source_type;
  if (pointer_action_type ==
          SyntheticPointerActionParams::PointerActionType::RELEASE ||
      pointer_action_type ==
          SyntheticPointerActionParams::PointerActionType::CANCEL) {
    for (auto it = pointer_ids_.begin(); it != pointer_ids_.end();) {
      SyntheticPointerActionParams action_params =
          PrepareSyntheticPointerActionParams(pointer_action_type, *it, 0, 0,
                                              event_modifiers);
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
            action_type, id, point->GetX(), point->GetY(), event_modifiers,
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
              SyntheticPointerActionParams::PointerActionType::RELEASE, *it, 0,
              0, event_modifiers);
      param_list.push_back(action_params);
      it = pointer_ids_.erase(it);
    }
  }
  action_list_params.PushPointerActionParamsList(param_list);

  if (!synthetic_pointer_driver_) {
    synthetic_pointer_driver_ =
        SyntheticPointerDriver::Create(gesture_source_type, true);
  }
  std::unique_ptr<SyntheticPointerAction> synthetic_gesture =
      std::make_unique<SyntheticPointerAction>(action_list_params);
  synthetic_gesture->SetSyntheticPointerDriver(
      synthetic_pointer_driver_->AsWeakPtr());

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
    double x,
    double y,
    int key_modifiers,
    float radius_x,
    float radius_y,
    float rotation_angle,
    float force) {
  SyntheticPointerActionParams action_params(pointer_action_type);
  action_params.set_pointer_id(id);
  const SyntheticPointerActionParams::Button button =
      SyntheticPointerActionParams::Button::NO_BUTTON;
  switch (pointer_action_type) {
    case SyntheticPointerActionParams::PointerActionType::PRESS:
      action_params.set_position(
          gfx::PointF(x * ScaleFactor(), y * ScaleFactor()));
      action_params.set_button(button);
      action_params.set_key_modifiers(key_modifiers);
      action_params.set_width(radius_x * 2.f);
      action_params.set_height(radius_y * 2.f);
      action_params.set_rotation_angle(rotation_angle);
      action_params.set_force(force);
      break;
    case SyntheticPointerActionParams::PointerActionType::MOVE:
      action_params.set_position(
          gfx::PointF(x * ScaleFactor(), y * ScaleFactor()));
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
                                                  Maybe<double> timestamp,
                                                  Maybe<double> delta_x,
                                                  Maybe<double> delta_y,
                                                  Maybe<int> modifiers,
                                                  Maybe<int> click_count) {
  blink::WebInputEvent::Type event_type;
  if (type == Input::EmulateTouchFromMouseEvent::TypeEnum::MouseWheel) {
    event_type = blink::WebInputEvent::Type::kMouseWheel;
    if (!delta_x.isJust() || !delta_y.isJust()) {
      return Response::InvalidParams(
          "'deltaX' and 'deltaY' are expected for mouseWheel event");
    }
  } else {
    event_type = GetMouseEventType(type);
    if (event_type == blink::WebInputEvent::Type::kUndefined) {
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
        GetEventTimeTicks(timestamp));
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
        GetEventTimeTicks(timestamp));
    event.reset(mouse_event);
  }

  mouse_event->SetPositionInWidget(x, y);
  mouse_event->button = event_button;
  mouse_event->SetPositionInScreen(x, y);
  mouse_event->click_count = click_count.fromMaybe(0);
  mouse_event->pointer_type = blink::WebPointerProperties::PointerType::kTouch;

  if (!host_ || !host_->GetRenderWidgetHost())
    return Response::InternalError();

  base::OnceCallback<void(bool)> forward_event_func;

  if (wheel_event) {
    forward_event_func = base::BindOnce(
        [](base::WeakPtr<InputHandler> self,
           base::WeakPtr<RenderWidgetHostImpl> widget_host,
           blink::WebMouseWheelEvent* event,
           ui::WebScopedInputEvent event_deleter, bool success) {
          if (!self || !widget_host)
            return;

          widget_host->ForwardWheelEvent(*event);
          // Send a synthetic wheel event with phaseEnded to finish scrolling.
          event->delta_x = 0;
          event->delta_y = 0;
          event->phase = blink::WebMouseWheelEvent::kPhaseEnded;
          event->dispatch_type =
              blink::WebInputEvent::DispatchType::kEventNonBlocking;
          widget_host->ForwardWheelEvent(*event);
        },
        weak_factory_.GetWeakPtr(), host_->GetRenderWidgetHost()->GetWeakPtr(),
        wheel_event, std::move(event));
  } else {
    forward_event_func = base::BindOnce(
        [](base::WeakPtr<InputHandler> self,
           base::WeakPtr<RenderWidgetHostImpl> widget_host,
           blink::WebMouseEvent* event, ui::WebScopedInputEvent event_deleter,
           bool success) {
          if (!self || !widget_host)
            return;
          widget_host->ForwardMouseEvent(*event);
        },
        weak_factory_.GetWeakPtr(), host_->GetRenderWidgetHost()->GetWeakPtr(),
        mouse_event, std::move(event));
  }
  // We make sure the compositor is up to date before sending a mouse event.
  // Otherwise it wont be picked up by newly added event listeners on the main
  // thread.
  host_->GetRenderWidgetHost()->InsertVisualStateCallback(
      std::move(forward_event_func));
  return Response::Success();
}

Response InputHandler::SetIgnoreInputEvents(bool ignore) {
  ignore_input_events_ = ignore;
  if (web_contents_)
    web_contents_->SetIgnoreInputEvents(ignore);
  return Response::Success();
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

  gesture_params.from_devtools_debugger = true;
  gesture_params.scale_factor = scale_factor;
  gesture_params.anchor = CssPixelsToPointF(x, y, ScaleFactor());
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
  gesture_params.from_devtools_debugger = true;
  gesture_params.granularity = ui::ScrollGranularity::kScrollByPrecisePixel;

  gesture_params.anchor = CssPixelsToPointF(x, y, ScaleFactor());
  if (!PointIsWithinContents(gesture_params.anchor)) {
    callback->sendFailure(Response::InvalidParams("Position out of bounds"));
    return;
  }

  const bool kDefaultPreventFling = true;
  const int kDefaultSpeed = 800;
  gesture_params.prevent_fling =
      prevent_fling.fromMaybe(kDefaultPreventFling);
  gesture_params.speed_in_pixels_s = speed.fromMaybe(kDefaultSpeed);

  if (x_distance.isJust() || y_distance.isJust()) {
    gesture_params.distances.push_back(CssPixelsToVector2dF(
        x_distance.fromMaybe(0), y_distance.fromMaybe(0), ScaleFactor()));
  }

  if (x_overscroll.isJust() || y_overscroll.isJust()) {
    gesture_params.distances.push_back(CssPixelsToVector2dF(
        -x_overscroll.fromMaybe(0), -y_overscroll.fromMaybe(0), ScaleFactor()));
  }

  if (!StringToGestureSourceType(
      std::move(gesture_source_type),
      gesture_params.gesture_source_type)) {
    callback->sendFailure(
        Response::InvalidParams("Unknown gestureSourceType"));
    return;
  }

  SynthesizeRepeatingScroll(gesture_params, repeat_count.fromMaybe(0),
                            base::Milliseconds(repeat_delay_ms.fromMaybe(250)),
                            interaction_marker_name.fromMaybe(""), ++last_id_,
                            std::move(callback));
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
    callback->sendFailure(Response::ServerError("Frame was detached"));
    return;
  }

  if (!interaction_marker_name.empty()) {
    // TODO(alexclarke): Can we move this elsewhere? It doesn't really fit here.
    TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN0(
        "benchmark", interaction_marker_name.c_str(),
        TRACE_ID_WITH_SCOPE(interaction_marker_name.c_str(), id));
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
    TRACE_EVENT_COPY_NESTABLE_ASYNC_END0(
        "benchmark", interaction_marker_name.c_str(),
        TRACE_ID_WITH_SCOPE(interaction_marker_name.c_str(), id));
  }

  if (repeat_count > 0) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
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

  gesture_params.position = CssPixelsToPointF(x, y, ScaleFactor());
  gesture_params.from_devtools_debugger = true;
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

RenderWidgetHostViewBase* InputHandler::GetRootView() {
  if (!host_)
    return nullptr;

  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(host_->GetView());
  if (!view)
    return nullptr;

  return view->GetRootView();
}

}  // namespace content::protocol
