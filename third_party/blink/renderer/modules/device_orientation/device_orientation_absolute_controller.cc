// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_orientation_absolute_controller.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event_pump.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

DeviceOrientationAbsoluteController::DeviceOrientationAbsoluteController(
    Document& document)
    : DeviceOrientationController(document) {}

DeviceOrientationAbsoluteController::~DeviceOrientationAbsoluteController() =
    default;

const char DeviceOrientationAbsoluteController::kSupplementName[] =
    "DeviceOrientationAbsoluteController";

DeviceOrientationAbsoluteController& DeviceOrientationAbsoluteController::From(
    Document& document) {
  DeviceOrientationAbsoluteController* controller =
      Supplement<Document>::From<DeviceOrientationAbsoluteController>(document);
  if (!controller) {
    controller =
        MakeGarbageCollected<DeviceOrientationAbsoluteController>(document);
    Supplement<Document>::ProvideTo(document, controller);
  }
  return *controller;
}

void DeviceOrientationAbsoluteController::DidAddEventListener(
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

  UseCounter::Count(GetDocument(),
                    WebFeature::kDeviceOrientationAbsoluteSecureOrigin);

  if (!has_event_listener_) {
    // TODO: add rappor url logging as in DeviceOrientationController.

    if (!CheckPolicyFeatures({mojom::FeaturePolicyFeature::kAccelerometer,
                              mojom::FeaturePolicyFeature::kGyroscope,
                              mojom::FeaturePolicyFeature::kMagnetometer})) {
      LogToConsolePolicyFeaturesDisabled(GetDocument().GetFrame(),
                                         EventTypeName());
      return;
    }
  }

  DeviceSingleWindowEventController::DidAddEventListener(window, event_type);
}

const AtomicString& DeviceOrientationAbsoluteController::EventTypeName() const {
  return event_type_names::kDeviceorientationabsolute;
}

void DeviceOrientationAbsoluteController::Trace(blink::Visitor* visitor) {
  DeviceOrientationController::Trace(visitor);
}

void DeviceOrientationAbsoluteController::RegisterWithDispatcher() {
  RegisterWithOrientationEventPump(true /* absolute */);
}

}  // namespace blink
