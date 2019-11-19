// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_orientation_controller.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/hosts_using_features.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event_pump.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

DeviceOrientationController::DeviceOrientationController(Document& document)
    : DeviceSingleWindowEventController(document),
      Supplement<Document>(document) {}

DeviceOrientationController::~DeviceOrientationController() = default;

void DeviceOrientationController::DidUpdateData() {
  if (override_orientation_data_)
    return;
  DispatchDeviceEvent(LastEvent());
}

const char DeviceOrientationController::kSupplementName[] =
    "DeviceOrientationController";

DeviceOrientationController& DeviceOrientationController::From(
    Document& document) {
  DeviceOrientationController* controller =
      Supplement<Document>::From<DeviceOrientationController>(document);
  if (!controller) {
    controller = MakeGarbageCollected<DeviceOrientationController>(document);
    ProvideTo(document, controller);
  }
  return *controller;
}

void DeviceOrientationController::DidAddEventListener(
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

  UseCounter::Count(GetDocument(), WebFeature::kDeviceOrientationSecureOrigin);

  if (!has_event_listener_) {
    if (!IsSameSecurityOriginAsMainFrame()) {
      Platform::Current()->RecordRapporURL(
          "DeviceSensors.DeviceOrientationCrossOrigin",
          WebURL(GetDocument().Url()));
    }

    if (!CheckPolicyFeatures({mojom::FeaturePolicyFeature::kAccelerometer,
                              mojom::FeaturePolicyFeature::kGyroscope})) {
      LogToConsolePolicyFeaturesDisabled(GetDocument().GetFrame(),
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
  DeviceOrientationEvent* orientation_event = ToDeviceOrientationEvent(event);
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

void DeviceOrientationController::Trace(blink::Visitor* visitor) {
  visitor->Trace(override_orientation_data_);
  visitor->Trace(orientation_event_pump_);
  DeviceSingleWindowEventController::Trace(visitor);
  Supplement<Document>::Trace(visitor);
}

void DeviceOrientationController::RegisterWithOrientationEventPump(
    bool absolute) {
  // The document's frame may be null if the document was already shut down.
  LocalFrame* frame = GetDocument().GetFrame();
  if (!orientation_event_pump_) {
    if (!frame)
      return;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        frame->GetTaskRunner(TaskType::kSensor);
    orientation_event_pump_ =
        MakeGarbageCollected<DeviceOrientationEventPump>(task_runner, absolute);
  }
  // TODO(crbug.com/850619): Ensure a valid frame is passed.
  orientation_event_pump_->SetController(this);
}

// static
void DeviceOrientationController::LogToConsolePolicyFeaturesDisabled(
    LocalFrame* frame,
    const AtomicString& event_name) {
  if (!frame)
    return;
  const String& message = String::Format(
      "The %s events are blocked by feature policy. "
      "See "
      "https://github.com/WICG/feature-policy/blob/master/"
      "features.md#sensor-features",
      event_name.Ascii().c_str());
  ConsoleMessage* console_message = ConsoleMessage::Create(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kWarning, std::move(message));
  frame->Console().AddMessage(console_message);
}

}  // namespace blink
