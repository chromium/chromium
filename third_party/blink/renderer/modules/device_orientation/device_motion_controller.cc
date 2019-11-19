// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_motion_controller.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/hosts_using_features.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_pump.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_controller.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

DeviceMotionController::DeviceMotionController(Document& document)
    : DeviceSingleWindowEventController(document),
      Supplement<Document>(document) {}

DeviceMotionController::~DeviceMotionController() = default;

const char DeviceMotionController::kSupplementName[] = "DeviceMotionController";

DeviceMotionController& DeviceMotionController::From(Document& document) {
  DeviceMotionController* controller =
      Supplement<Document>::From<DeviceMotionController>(document);
  if (!controller) {
    controller = MakeGarbageCollected<DeviceMotionController>(document);
    ProvideTo(document, controller);
  }
  return *controller;
}

void DeviceMotionController::DidAddEventListener(
    LocalDOMWindow* window,
    const AtomicString& event_type) {
  if (event_type != EventTypeName())
    return;

  // The document could be detached, e.g. if it is the `contentDocument` of an
  // <iframe> that has been removed from the DOM of its parent frame.
  if (GetDocument().IsContextDestroyed())
    return;

  // The API is not exposed to Workers or Worklets, so if the current realm
  // execution context is valid, it must have a responsible browsing context.
  SECURITY_CHECK(GetDocument().GetFrame());

  // The event handler property on `window` is restricted to [SecureContext],
  // but nothing prevents a site from calling `window.addEventListener(...)`
  // from a non-secure browsing context.
  if (!GetDocument().IsSecureContext())
    return;

  UseCounter::Count(GetDocument(), WebFeature::kDeviceMotionSecureOrigin);

  if (!has_event_listener_) {
    if (!IsSameSecurityOriginAsMainFrame()) {
      Platform::Current()->RecordRapporURL(
          "DeviceSensors.DeviceMotionCrossOrigin", WebURL(GetDocument().Url()));
    }

    if (!CheckPolicyFeatures({mojom::FeaturePolicyFeature::kAccelerometer,
                              mojom::FeaturePolicyFeature::kGyroscope})) {
      DeviceOrientationController::LogToConsolePolicyFeaturesDisabled(
          GetDocument().GetFrame(), EventTypeName());
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
    LocalFrame* frame = GetDocument().GetFrame();
    if (!frame)
      return;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        frame->GetTaskRunner(TaskType::kSensor);
    motion_event_pump_ =
        MakeGarbageCollected<DeviceMotionEventPump>(task_runner);
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
  DeviceMotionEvent* motion_event = ToDeviceMotionEvent(event);
  return !motion_event->GetDeviceMotionData()->CanProvideEventData();
}

const AtomicString& DeviceMotionController::EventTypeName() const {
  return event_type_names::kDevicemotion;
}

void DeviceMotionController::Trace(blink::Visitor* visitor) {
  DeviceSingleWindowEventController::Trace(visitor);
  visitor->Trace(motion_event_pump_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
