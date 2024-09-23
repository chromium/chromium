// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_posture/device_posture.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"

namespace blink {

namespace {

String PostureToString(mojom::blink::DevicePostureType posture) {
  switch (posture) {
    case mojom::blink::DevicePostureType::kContinuous:
      return "continuous";
    case mojom::blink::DevicePostureType::kFolded:
      return "folded";
  }
}

}  // namespace

DevicePosture::DevicePosture(LocalDOMWindow* window)
    : ExecutionContextClient(window), receiver_(this, GetExecutionContext()) {}

DevicePosture::~DevicePosture() = default;

String DevicePosture::type() {
  EnsureServiceConnection();
  return PostureToString(posture_);
}

void DevicePosture::OnPostureChanged(mojom::blink::DevicePostureType posture) {
  if (posture_ == posture)
    return;

  posture_ = posture;
  DispatchEvent(*Event::CreateBubble(event_type_names::kChange));
}

void DevicePosture::EnsureServiceConnection() {
  LocalDOMWindow* window = DomWindow();
  if (!window) {
    return;
  }

  if (receiver_.is_bound()) {
    return;
  }

  mojom::blink::DevicePostureProvider* service =
      window->GetFrame()->GetDevicePostureProvider();
  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  service->AddListenerAndGetCurrentPosture(
      receiver_.BindNewPipeAndPassRemote(task_runner),
      WTF::BindOnce(&DevicePosture::OnPostureChanged, WrapPersistent(this)));
}

void DevicePosture::AddedEventListener(const AtomicString& event_type,
                                       RegisteredEventListener& listener) {
  EventTarget::AddedEventListener(event_type, listener);

  if (event_type != event_type_names::kChange)
    return;

  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return;

  EnsureServiceConnection();
}

ExecutionContext* DevicePosture::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

const AtomicString& DevicePosture::InterfaceName() const {
  return event_target_names::kDevicePosture;
}

void DevicePosture::Trace(blink::Visitor* visitor) const {
  visitor->Trace(receiver_);
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
