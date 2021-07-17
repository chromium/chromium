// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_posture/device_posture.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"

namespace blink {

namespace {

String PostureToString(device::mojom::blink::DevicePostureType posture) {
  switch (posture) {
    case device::mojom::blink::DevicePostureType::kNoFold:
      return "no-fold";
    case device::mojom::blink::DevicePostureType::kLaptop:
      return "laptop";
    case device::mojom::blink::DevicePostureType::kFlat:
      return "flat";
    case device::mojom::blink::DevicePostureType::kTent:
      return "tent";
    case device::mojom::blink::DevicePostureType::kTablet:
      return "tablet";
    case device::mojom::blink::DevicePostureType::kBook:
      return "book";
  }
  NOTREACHED();
  return "no-fold";
}

}  // namespace

DevicePosture::DevicePosture(LocalDOMWindow* window)
    : ExecutionContextClient(window),
      service_(GetExecutionContext()),
      receiver_(this, GetExecutionContext()) {}

DevicePosture::~DevicePosture() = default;

String DevicePosture::type() {
  EnsureServiceConnection();
  return PostureToString(posture_);
}

void DevicePosture::OnPostureChanged(
    device::mojom::blink::DevicePostureType posture) {
  if (posture_ == posture)
    return;

  posture_ = posture;
  DispatchEvent(*Event::CreateBubble(event_type_names::kChange));
}

void DevicePosture::EnsureServiceConnection() {
  auto* context = GetExecutionContext();
  if (!context)
    return;

  if (service_.is_bound())
    return;

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(task_runner));

  service_->SetListener(
      receiver_.BindNewPipeAndPassRemote(task_runner),
      WTF::Bind(&DevicePosture::OnPostureChanged, WrapPersistent(this)));
}

ExecutionContext* DevicePosture::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

const AtomicString& DevicePosture::InterfaceName() const {
  return event_target_names::kDevicePosture;
}

void DevicePosture::Trace(blink::Visitor* visitor) const {
  visitor->Trace(service_);
  visitor->Trace(receiver_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
