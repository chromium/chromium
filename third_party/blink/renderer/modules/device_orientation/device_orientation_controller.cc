// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_orientation_controller.h"

#include "base/notreached.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_device_orientation_permission_state.h"
#include "third_party/blink/renderer/core/frame/dactyloscoper.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event_pump.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

DeviceOrientationController::DeviceOrientationController(LocalDOMWindow& window)
    : DeviceSingleWindowEventController(window),
      Supplement<LocalDOMWindow>(window),
      permission_service_(&window) {}

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
  Dactyloscoper::RecordDirectSurface(
      &GetWindow(), WebFeature::kDeviceOrientationSecureOrigin, String());

  if (!has_requested_permission_) {
    UseCounter::Count(
        GetWindow(),
        WebFeature::kDeviceOrientationUsedWithoutPermissionRequest);
  }

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

void DeviceOrientationController::RestartPumpIfNeeded() {
  if (!orientation_event_pump_ || !has_event_listener_) {
    return;
  }
  // We do this to make sure that existing connections to
  // device::mojom::blink::Sensor instances are dropped and GetSensor() is
  // called again, so that e.g. the virtual sensors are used when added, or the
  // real ones are used again when the virtual sensors are removed.
  StopUpdating();
  set_needs_checking_null_events(/*enabled=*/true);
  orientation_event_pump_.Clear();
  StartUpdating();
}

void DeviceOrientationController::Trace(Visitor* visitor) const {
  visitor->Trace(override_orientation_data_);
  visitor->Trace(orientation_event_pump_);
  visitor->Trace(permission_service_);
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

ScriptPromise<V8DeviceOrientationPermissionState>
DeviceOrientationController::RequestPermission(ScriptState* script_state) {
  ExecutionContext* context = GetSupplementable();
  DCHECK_EQ(context, ExecutionContext::From(script_state));

  has_requested_permission_ = true;

  if (!permission_service_.is_bound()) {
    ConnectToPermissionService(context,
                               permission_service_.BindNewPipeAndPassReceiver(
                                   context->GetTaskRunner(TaskType::kSensor)));
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<V8DeviceOrientationPermissionState>>(script_state);
  auto promise = resolver->Promise();

  permission_service_->HasPermission(
      CreatePermissionDescriptor(mojom::blink::PermissionName::SENSORS),
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          [](ScriptPromiseResolver<V8DeviceOrientationPermissionState>*
                 resolver,
             mojom::blink::PermissionStatus status) {
            switch (status) {
              case mojom::blink::PermissionStatus::GRANTED:
              case mojom::blink::PermissionStatus::DENIED:
                resolver->Resolve(*V8DeviceOrientationPermissionState::Create(
                    PermissionStatusToString(status)));
                break;
              case mojom::blink::PermissionStatus::ASK:
                // At the moment, this state is not reachable because there
                // is no "ask" or "prompt" state in the Chromium
                // permissions UI for sensors, so HasPermissionStatus() will
                // always return GRANTED or DENIED.
                NOTREACHED_IN_MIGRATION();
                break;
            }
          })));

  return promise;
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
