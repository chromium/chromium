// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_orientation_controller.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event_pump.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

DeviceOrientationController::DeviceOrientationController(LocalDOMWindow& window)
    : DeviceSingleWindowEventController(window),
      Supplement<LocalDOMWindow>(window) {}

DeviceOrientationController::~DeviceOrientationController() = default;

void DeviceOrientationController::DidUpdateData() {
  if (override_orientation_data_)
    return;
  DispatchDeviceEvent(LastEvent());
}

const char DeviceOrientationController::kSupplementName[] =
    "DeviceOrientationController";

DeviceOrientationController& DeviceOrientationController::From(
    LocalDOMWindow& window) {
  DeviceOrientationController* controller =
      Supplement<LocalDOMWindow>::From<DeviceOrientationController>(window);
  if (!controller) {
    controller = MakeGarbageCollected<DeviceOrientationController>(window);
    ProvideTo(window, controller);
  }
  return *controller;
}

void DeviceOrientationController::DidAddEventListener(
    LocalDOMWindow* window,
    const AtomicString& event_type) {
  if (event_type != EventTypeName())
    return;

  // The window could be detached, e.g. if it is the `contentWindow` of an
  // <iframe> that has been removed from the DOM of its parent frame.
  if (GetWindow().IsContextDestroyed())
    return;

  // The API is not exposed to Workers or Worklets, so if the current realm
  // execution context is valid, it must have a responsible browsing context.
  SECURITY_CHECK(GetWindow().GetFrame());

  // The event handler property on `window` is restricted to [SecureContext],
  // but nothing prevents a site from calling `window.addEventListener(...)`
  // from a non-secure browsing context.
  if (!GetWindow().IsSecureContext())
    return;

  UseCounter::Count(GetWindow(), WebFeature::kDeviceOrientationSecureOrigin);

  if (!has_event_listener_) {
    if (!CheckPolicyFeatures(
            {mojom::blink::PermissionsPolicyFeature::kAccelerometer,
             mojom::blink::PermissionsPolicyFeature::kGyroscope})) {
      LogToConsolePolicyFeaturesDisabled(*GetWindow().GetFrame(),
                                         EventTypeName());
      return;
    }
  }

  DeviceSingleWindowEventController::DidAddEventListener(window, event_type);
}

DeviceOrientationData* DeviceOrientationController::LastData() const {
  return override_orientation_data_
             ? override_orientation_data_.Get()
             : orientation_event_pump_
                   ? orientation_event_pump_->LatestDeviceOrientationData()
                   : nullptr;
}

bool DeviceOrientationController::HasLastData() {
  return LastData();
}

void DeviceOrientationController::RegisterWithDispatcher() {
  RegisterWithOrientationEventPump(false /* absolute */);
}

void DeviceOrientationController::UnregisterWithDispatcher() {
  if (orientation_event_pump_)
    orientation_event_pump_->RemoveController();
}

Event* DeviceOrientationController::LastEvent() const {
  return DeviceOrientationEvent::Create(EventTypeName(), LastData());
}

bool DeviceOrientationController::IsNullEvent(Event* event) const {
  auto* orientation_event = To<DeviceOrientationEvent>(event);
  return !orientation_event->Orientation()->CanProvideEventData();
}

const AtomicString& DeviceOrientationController::EventTypeName() const {
  return event_type_names::kDeviceorientation;
}

void DeviceOrientationController::SetOverride(
    DeviceOrientationData* device_orientation_data) {
  DCHECK(device_orientation_data);
  override_orientation_data_ = device_orientation_data;
  DispatchDeviceEvent(LastEvent());
}

void DeviceOrientationController::ClearOverride() {
  if (!override_orientation_data_)
    return;
  override_orientation_data_.Clear();
  if (LastData())
    DidUpdateData();
}

void DeviceOrientationController::Trace(Visitor* visitor) const {
  visitor->Trace(override_orientation_data_);
  visitor->Trace(orientation_event_pump_);
  DeviceSingleWindowEventController::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

void DeviceOrientationController::RegisterWithOrientationEventPump(
    bool absolute) {
  if (!orientation_event_pump_) {
    orientation_event_pump_ = MakeGarbageCollected<DeviceOrientationEventPump>(
        *GetWindow().GetFrame(), absolute);
  }
  orientation_event_pump_->SetController(this);
}

// static
void DeviceOrientationController::LogToConsolePolicyFeaturesDisabled(
    LocalFrame& frame,
    const AtomicString& event_name) {
  const String& message = String::Format(
      "The %s events are blocked by permissions policy. "
      "See "
      "https://github.com/w3c/webappsec-permissions-policy/blob/master/"
      "features.md#sensor-features",
      event_name.Ascii().c_str());
  auto* console_message = MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kWarning, std::move(message));
  frame.Console().AddMessage(console_message);
}

}  // namespace blink
