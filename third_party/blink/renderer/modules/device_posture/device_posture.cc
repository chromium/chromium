// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_posture/device_posture.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"

namespace blink {

namespace {

String PostureToString(
    mojom::blink::DevicePostureType posture) {
  switch (posture) {
    case mojom::blink::DevicePostureType::kNoFold:
      return "no-fold";
    case mojom::blink::DevicePostureType::kLaptop:
      return "laptop";
    case mojom::blink::DevicePostureType::kFlat:
      return "flat";
    case mojom::blink::DevicePostureType::kTent:
      return "tent";
    case mojom::blink::DevicePostureType::kTablet:
      return "tablet";
    case mojom::blink::DevicePostureType::kBook:
      return "book";
  }
  NOTREACHED();
  return "no-fold";
}

}  // namespace

DevicePosture::DevicePosture(LocalDOMWindow* window)
    : ExecutionContextClient(window) {}

DevicePosture::~DevicePosture() = default;

String DevicePosture::type() const {
  return PostureToString(posture_);
}

ExecutionContext* DevicePosture::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

const AtomicString& DevicePosture::InterfaceName() const {
  return event_target_names::kDevicePosture;
}

void DevicePosture::Trace(blink::Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
