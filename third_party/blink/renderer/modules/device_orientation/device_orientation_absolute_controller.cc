// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_orientation_absolute_controller.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event_pump.h"

namespace blink {

DeviceOrientationAbsoluteController::DeviceOrientationAbsoluteController(
    LocalDOMWindow& window)
    : DeviceOrientationController(window) {}

DeviceOrientationAbsoluteController::~DeviceOrientationAbsoluteController() =
    default;

const char DeviceOrientationAbsoluteController::kSupplementName[] =
    "DeviceOrientationAbsoluteController";

DeviceOrientationAbsoluteController& DeviceOrientationAbsoluteController::From(
    LocalDOMWindow& window) {
  DeviceOrientationAbsoluteController* controller =
      Supplement<LocalDOMWindow>::From<DeviceOrientationAbsoluteController>(
          window);
  if (!controller) {
    controller =
        MakeGarbageCollected<DeviceOrientationAbsoluteController>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, controller);
  }
  return *controller;
}

void DeviceOrientationAbsoluteController::DidAddEventListener(
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

  UseCounter::Count(GetWindow(),
                    WebFeature::kDeviceOrientationAbsoluteSecureOrigin);

  if (!has_event_listener_) {
    if (!CheckPolicyFeatures(
            {mojom::blink::PermissionsPolicyFeature::kAccelerometer,
             mojom::blink::PermissionsPolicyFeature::kGyroscope,
             mojom::blink::PermissionsPolicyFeature::kMagnetometer})) {
      LogToConsolePolicyFeaturesDisabled(*GetWindow().GetFrame(),
                                         EventTypeName());
      return;
    }
  }

  DeviceSingleWindowEventController::DidAddEventListener(window, event_type);
}

const AtomicString& DeviceOrientationAbsoluteController::EventTypeName() const {
  return event_type_names::kDeviceorientationabsolute;
}

void DeviceOrientationAbsoluteController::Trace(Visitor* visitor) const {
  DeviceOrientationController::Trace(visitor);
}

void DeviceOrientationAbsoluteController::RegisterWithDispatcher() {
  RegisterWithOrientationEventPump(true /* absolute */);
}

}  // namespace blink
