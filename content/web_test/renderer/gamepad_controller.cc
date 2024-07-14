// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/web_test/renderer/gamepad_controller.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8.h"

using device::Gamepad;
using device::Gamepads;

namespace content {

namespace {

// Set button.pressed if the button value is above a threshold. The threshold is
// chosen to match XInput's trigger deadzone.
constexpr float kButtonPressedThreshold = 30.f / 255.f;

int64_t CurrentTimeInMicroseconds() {
  return base::TimeTicks::Now().since_origin().InMicroseconds();
}

}  // namespace

class GamepadControllerBindings
    : public gin::Wrappable<GamepadControllerBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  GamepadControllerBindings(const GamepadControllerBindings&) = delete;
  GamepadControllerBindings& operator=(const GamepadControllerBindings&) =
      delete;

  static void Install(base::WeakPtr<GamepadController> controller,
                      blink::WebLocalFrame* frame);

 private:
  explicit GamepadControllerBindings(
      base::WeakPtr<GamepadController> controller);
  ~GamepadControllerBindings() override;

  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  void Connect(int index);
  void DispatchConnected(int index);
  void Disconnect(int index);
  void SetId(int index, const std::string& src);
  void SetButtonCount(int index, int buttons);
  void SetButtonData(int index, int button, double data);
  void SetAxisCount(int index, int axes);
  void SetAxisData(int index, int axis, double data);
  void SetDualRumbleVibrationActuator(int index, bool enabled);
  void SetTriggerRumbleVibrationActuator(int index, bool enabled);
  void SetTouchCount(int index, int touches);
  void SetTouchData(int index,
                    int touch,
                    unsigned int touch_id,
                    float position_x,
                    float position_y);

  base::WeakPtr<GamepadController> controller_;
};

gin::WrapperInfo GamepadControllerBindings::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void GamepadControllerBindings::Install(
    base::WeakPtr<GamepadController> controller,
    blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<GamepadControllerBindings> bindings =
      gin::CreateHandle(isolate, new GamepadControllerBindings(controller));
  if (bindings.IsEmpty())
    return;
  v8::Local<v8::Object> global = context->Global();
  global
      ->Set(context, gin::StringToV8(isolate, "gamepadController"),
            bindings.ToV8())
      .Check();
}

GamepadControllerBindings::GamepadControllerBindings(
    base::WeakPtr<GamepadController> controller)
    : controller_(controller) {}

GamepadControllerBindings::~GamepadControllerBindings() {}

gin::ObjectTemplateBuilder GamepadControllerBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<GamepadControllerBindings>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("connect", &GamepadControllerBindings::Connect)
      .SetMethod("dispatchConnected",
                 &GamepadControllerBindings::DispatchConnected)
      .SetMethod("disconnect", &GamepadControllerBindings::Disconnect)
      .SetMethod("setId", &GamepadControllerBindings::SetId)
      .SetMethod("setButtonCount", &GamepadControllerBindings::SetButtonCount)
      .SetMethod("setButtonData", &GamepadControllerBindings::SetButtonData)
      .SetMethod("setAxisCount", &GamepadControllerBindings::SetAxisCount)
      .SetMethod("setAxisData", &GamepadControllerBindings::SetAxisData)
      .SetMethod("setDualRumbleVibrationActuator",
                 &GamepadControllerBindings::SetDualRumbleVibrationActuator)
      .SetMethod("setTriggerRumbleVibrationActuator",
                 &GamepadControllerBindings::SetTriggerRumbleVibrationActuator)
      .SetMethod("setTouchCount", &GamepadControllerBindings::SetTouchCount)
      .SetMethod("setTouchData", &GamepadControllerBindings::SetTouchData);
}

void GamepadControllerBindings::Connect(int index) {
  if (controller_)
    controller_->Connect(index);
}

void GamepadControllerBindings::DispatchConnected(int index) {
  if (controller_)
    controller_->DispatchConnected(index);
}

void GamepadControllerBindings::Disconnect(int index) {
  if (controller_)
    controller_->Disconnect(index);
}

void GamepadControllerBindings::SetId(int index, const std::string& src) {
  if (controller_)
    controller_->SetId(index, src);
}

void GamepadControllerBindings::SetButtonCount(int index, int buttons) {
  if (controller_)
    controller_->SetButtonCount(index, buttons);
}

void GamepadControllerBindings::SetButtonData(int index,
                                              int button,
                                              double data) {
  if (controller_)
    controller_->SetButtonData(index, button, data);
}

void GamepadControllerBindings::SetAxisCount(int index, int axes) {
  if (controller_)
    controller_->SetAxisCount(index, axes);
}

void GamepadControllerBindings::SetAxisData(int index, int axis, double data) {
  if (controller_)
    controller_->SetAxisData(index, axis, data);
}

void GamepadControllerBindings::SetDualRumbleVibrationActuator(int index,
                                                               bool enabled) {
  if (controller_)
    controller_->SetDualRumbleVibrationActuator(index, enabled);
}

void GamepadControllerBindings::SetTriggerRumbleVibrationActuator(
    int index,
    bool enabled) {
  if (controller_)
    controller_->SetTriggerRumbleVibrationActuator(index, enabled);
}

void GamepadControllerBindings::SetTouchData(int index,
                                             int touch,
                                             unsigned int touch_id,
                                             float position_x,
                                             float position_y) {
  if (controller_) {
    controller_->SetTouchData(index, touch, touch_id, position_x, position_y);
  }
}

void GamepadControllerBindings::SetTouchCount(int index, int touches) {
  if (controller_) {
    controller_->SetTouchCount(index, touches);
  }
}

GamepadController::MonitorImpl::MonitorImpl(
    GamepadController* controller,
    mojo::PendingReceiver<device::mojom::GamepadMonitor> receiver)
    : controller_(controller) {
  receiver_.Bind(std::move(receiver));
}

GamepadController::MonitorImpl::~MonitorImpl() = default;

bool GamepadController::MonitorImpl::HasPendingConnect(int index) {
  return missed_dispatches_.test(index);
}

void GamepadController::MonitorImpl::GamepadStartPolling(
    GamepadStartPollingCallback callback) {
  std::move(callback).Run(controller_->GetSharedMemoryRegion());
}

void GamepadController::MonitorImpl::GamepadStopPolling(
    GamepadStopPollingCallback callback) {
  std::move(callback).Run();
}

void GamepadController::MonitorImpl::SetObserver(
    mojo::PendingRemote<device::mojom::GamepadObserver> observer) {
  observer_remote_.Bind(std::move(observer));
  observer_remote_.set_disconnect_handler(
      base::BindOnce(&GamepadController::OnConnectionError,
                     base::Unretained(controller_), base::Unretained(this)));

  // Notify the new observer of any GamepadConnected RPCs that it missed because
  // the SetObserver RPC wasn't processed in time. This happens during layout
  // tests because SetObserver is async, so the test can continue to the
  // DispatchConnected call before the SetObserver RPC was processed. This isn't
  // an issue in the real implementation because the 'gamepadconnected' event
  // doesn't fire until user input is detected, so even if a GamepadConnected
  // event is missed, another will be picked up after the next user input.
  controller_->NotifyForMissedDispatches(this);
  missed_dispatches_.reset();
}

void GamepadController::MonitorImpl::DispatchConnected(
    int index,
    const device::Gamepad& pad) {
  if (observer_remote_) {
    observer_remote_->GamepadConnected(index, pad);
  } else {
    // Record that there wasn't an observer to get the GamepadConnected RPC so
    // we can send it when SetObserver gets called.
    missed_dispatches_.set(index);
  }
}

void GamepadController::MonitorImpl::DispatchDisconnected(
    int index,
    const device::Gamepad& pad) {
  if (observer_remote_)
    observer_remote_->GamepadDisconnected(index, pad);
}

void GamepadController::MonitorImpl::Reset() {
  missed_dispatches_.reset();
}

GamepadController::GamepadController() {
  size_t buffer_size = sizeof(device::GamepadHardwareBuffer);
  // Use mojo to delegate the creation of shared memory to the broker process.
  mojo::ScopedSharedBufferHandle mojo_buffer =
      mojo::SharedBufferHandle::Create(buffer_size);
  base::WritableSharedMemoryRegion writable_region =
      mojo::UnwrapWritableSharedMemoryRegion(std::move(mojo_buffer));
  shared_memory_mapping_ = writable_region.Map();
  shared_memory_region_ = base::WritableSharedMemoryRegion::ConvertToReadOnly(
      std::move(writable_region));
  if (!shared_memory_region_.IsValid()) {
    // Log an error instead of crashing, as this can flakily happen in
    // clusterfuzz. If it happened in the wild, the test would be retried.
    LOG(ERROR) << "GamepadController shared memory region is not valid";
  } else if (!shared_memory_mapping_.IsValid()) {
    // Log an error instead of crashing, as this can flakily happen in
    // clusterfuzz. If it happened in the wild, the test would be retried.
    LOG(ERROR) << "GamepadController shared memory mapping is not valid";
  } else {
    gamepads_ =
        new (shared_memory_mapping_.memory()) device::GamepadHardwareBuffer();
  }

  Reset();
}

GamepadController::~GamepadController() = default;

void GamepadController::Reset() {
  if (!gamepads_)
    return;  // Shared memory failed.

  memset(gamepads_, 0, sizeof(*gamepads_));
  for (auto& monitor : monitors_)
    monitor->Reset();
}

void GamepadController::Install(RenderFrame* frame) {
  if (!gamepads_)
    return;  // Shared memory failed.

  frame->GetBrowserInterfaceBroker().SetBinderForTesting(
      device::mojom::GamepadMonitor::Name_,
      base::BindRepeating(&GamepadController::OnInterfaceRequest,
                          base::Unretained(this)));
  GamepadControllerBindings::Install(weak_factory_.GetWeakPtr(),
                                     frame->GetWebFrame());
}

void GamepadController::OnInterfaceRequest(
    mojo::ScopedMessagePipeHandle handle) {
  monitors_.insert(std::make_unique<MonitorImpl>(
      this,
      mojo::PendingReceiver<device::mojom::GamepadMonitor>(std::move(handle))));
}

base::ReadOnlySharedMemoryRegion GamepadController::GetSharedMemoryRegion()
    const {
  return shared_memory_region_.Duplicate();
}

void GamepadController::OnConnectionError(
    GamepadController::MonitorImpl* monitor) {
  monitors_.erase(monitors_.find(monitor));
}

void GamepadController::NotifyForMissedDispatches(
    GamepadController::MonitorImpl* monitor) {
  gamepads_->seqlock.WriteBegin();
  for (size_t index = 0; index < Gamepads::kItemsLengthCap; index++) {
    if (monitor->HasPendingConnect(index))
      monitor->DispatchConnected(index, gamepads_->data.items[index]);
  }
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::Connect(int index) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  const int64_t now = CurrentTimeInMicroseconds();
  gamepads_->seqlock.WriteBegin();
  Gamepad& pad = gamepads_->data.items[index];
  pad.connected = true;
  pad.timestamp = now;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::DispatchConnected(int index) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  const Gamepad& pad = gamepads_->data.items[index];
  if (!pad.connected)
    return;
  gamepads_->seqlock.WriteBegin();
  for (auto& monitor : monitors_)
    monitor->DispatchConnected(index, pad);
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::Disconnect(int index) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  const int64_t now = CurrentTimeInMicroseconds();
  gamepads_->seqlock.WriteBegin();
  Gamepad& pad = gamepads_->data.items[index];
  pad.connected = false;
  pad.timestamp = now;
  for (auto& monitor : monitors_)
    monitor->DispatchDisconnected(index, pad);
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetId(int index, const std::string& src) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  const char* p = src.c_str();
  const int64_t now = CurrentTimeInMicroseconds();
  gamepads_->seqlock.WriteBegin();
  Gamepad& pad = gamepads_->data.items[index];
  memset(pad.id, 0, sizeof(pad.id));
  for (unsigned i = 0; *p && i < Gamepad::kIdLengthCap - 1; ++i)
    pad.id[i] = *p++;
  pad.timestamp = now;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetButtonCount(int index, int buttons) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  if (buttons < 0 || buttons >= static_cast<int>(Gamepad::kButtonsLengthCap))
    return;
  const int64_t now = CurrentTimeInMicroseconds();
  gamepads_->seqlock.WriteBegin();
  Gamepad& pad = gamepads_->data.items[index];
  pad.buttons_length = buttons;
  pad.timestamp = now;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetButtonData(int index, int button, double data) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  if (button < 0 || button >= static_cast<int>(Gamepad::kButtonsLengthCap))
    return;
  const int64_t now = CurrentTimeInMicroseconds();
  gamepads_->seqlock.WriteBegin();
  Gamepad& pad = gamepads_->data.items[index];
  pad.buttons[button].value = data;
  pad.buttons[button].pressed = data > kButtonPressedThreshold;
  pad.timestamp = now;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetAxisCount(int index, int axes) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  if (axes < 0 || axes >= static_cast<int>(Gamepad::kAxesLengthCap))
    return;
  const int64_t now = CurrentTimeInMicroseconds();
  gamepads_->seqlock.WriteBegin();
  Gamepad& pad = gamepads_->data.items[index];
  pad.axes_length = axes;
  pad.timestamp = now;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetAxisData(int index, int axis, double data) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  if (axis < 0 || axis >= static_cast<int>(Gamepad::kAxesLengthCap))
    return;
  const int64_t now = CurrentTimeInMicroseconds();
  gamepads_->seqlock.WriteBegin();
  Gamepad& pad = gamepads_->data.items[index];
  pad.axes[axis] = data;
  pad.timestamp = now;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetDualRumbleVibrationActuator(int index,
                                                       bool enabled) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  const int64_t now = CurrentTimeInMicroseconds();
  gamepads_->seqlock.WriteBegin();
  Gamepad& pad = gamepads_->data.items[index];
  pad.vibration_actuator.type = device::GamepadHapticActuatorType::kDualRumble;
  pad.vibration_actuator.not_null = enabled;
  pad.timestamp = now;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetTriggerRumbleVibrationActuator(int index,
                                                          bool enabled) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  const int64_t now = CurrentTimeInMicroseconds();
  gamepads_->seqlock.WriteBegin();
  Gamepad& pad = gamepads_->data.items[index];
  pad.vibration_actuator.type =
      device::GamepadHapticActuatorType::kTriggerRumble;
  pad.vibration_actuator.not_null = enabled;
  pad.timestamp = now;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetTouchCount(int index, int touches) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap)) {
    return;
  }
  if (touches < 0 ||
      touches >= static_cast<int>(Gamepad::kTouchEventsLengthCap)) {
    return;
  }
  const int64_t now = CurrentTimeInMicroseconds();
  gamepads_->seqlock.WriteBegin();
  Gamepad& pad = gamepads_->data.items[index];
  pad.supports_touch_events_ = true;
  pad.touch_events_length = touches;
  pad.timestamp = now;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetTouchData(int index,
                                     int touch,
                                     unsigned int touch_id,
                                     float position_x,
                                     float position_y) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap)) {
    return;
  }
  if (touch < 0 || touch >= static_cast<int>(Gamepad::kTouchEventsLengthCap)) {
    return;
  }
  const int64_t now = CurrentTimeInMicroseconds();
  gamepads_->seqlock.WriteBegin();
  Gamepad& pad = gamepads_->data.items[index];
  pad.supports_touch_events_ = true;
  pad.touch_events[touch].touch_id = touch_id;
  pad.touch_events[touch].x = position_x;
  pad.touch_events[touch].y = position_y;
  pad.timestamp = now;
  gamepads_->seqlock.WriteEnd();
}

}  // namespace content
