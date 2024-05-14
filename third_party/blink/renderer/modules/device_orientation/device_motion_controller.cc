// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_motion_controller.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_device_orientation_permission_state.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_pump.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_controller.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

DeviceMotionController::DeviceMotionController(LocalDOMWindow& window)
    : DeviceSingleWindowEventController(window),
      Supplement<LocalDOMWindow>(window),
      permission_service_(&window) {}

DeviceMotionController::~DeviceMotionController() = default;

const char DeviceMotionController::kSupplementName[] = "DeviceMotionController";

DeviceMotionController& DeviceMotionController::From(LocalDOMWindow& window) {
  DeviceMotionController* controller =
      Supplement<LocalDOMWindow>::From<DeviceMotionController>(window);
  if (!controller) {
    controller = MakeGarbageCollected<DeviceMotionController>(window);
    ProvideTo(window, controller);
  }
  return *controller;
}

void DeviceMotionController::DidAddEventListener(
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

  UseCounter::Count(GetWindow(), WebFeature::kDeviceMotionSecureOrigin);

  if (!has_requested_permission_) {
    UseCounter::Count(GetWindow(),
                      WebFeature::kDeviceMotionUsedWithoutPermissionRequest);
  }

  if (!has_event_listener_) {
    if (!CheckPolicyFeatures(
            {mojom::blink::PermissionsPolicyFeature::kAccelerometer,
             mojom::blink::PermissionsPolicyFeature::kGyroscope})) {
      DeviceOrientationController::LogToConsolePolicyFeaturesDisabled(
          *GetWindow().GetFrame(), EventTypeName());
      return;
    }
  }

  DeviceSingleWindowEventController::DidAddEventListener(window, event_type);
}

bool DeviceMotionController::HasLastData() {
  return motion_event_pump_
             ? motion_event_pump_->LatestDeviceMotionData() != nullptr
             : false;
}

void DeviceMotionController::RegisterWithDispatcher() {
  if (!motion_event_pump_) {
    motion_event_pump_ =
        MakeGarbageCollected<DeviceMotionEventPump>(*GetWindow().GetFrame());
  }
  motion_event_pump_->SetController(this);
}

void DeviceMotionController::UnregisterWithDispatcher() {
  if (motion_event_pump_)
    motion_event_pump_->RemoveController();
}

Event* DeviceMotionController::LastEvent() const {
  return DeviceMotionEvent::Create(
      event_type_names::kDevicemotion,
      motion_event_pump_ ? motion_event_pump_->LatestDeviceMotionData()
                         : nullptr);
}

bool DeviceMotionController::IsNullEvent(Event* event) const {
  auto* motion_event = To<DeviceMotionEvent>(event);
  return !motion_event->GetDeviceMotionData()->CanProvideEventData();
}

const AtomicString& DeviceMotionController::EventTypeName() const {
  return event_type_names::kDevicemotion;
}

void DeviceMotionController::Trace(Visitor* visitor) const {
  DeviceSingleWindowEventController::Trace(visitor);
  visitor->Trace(motion_event_pump_);
  visitor->Trace(permission_service_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

ScriptPromise<V8DeviceOrientationPermissionState>
DeviceMotionController::RequestPermission(ScriptState* script_state) {
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

}  // namespace blink
