// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "third_party/blink/renderer/modules/device/device_service.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"

namespace blink {

namespace {

const DOMExceptionCode kDOMExceptionCode = DOMExceptionCode::kNotAllowedError;
const char kDOMExceptionMessage[] =
    "This API is available only for high trusted apps.";

}  // namespace

const char DeviceService::kSupplementName[] = "DeviceService";

DeviceService* DeviceService::device(Navigator& navigator) {
  if (!navigator.DomWindow())
    return nullptr;

  DeviceService* device_service =
      Supplement<Navigator>::From<DeviceService>(navigator);
  if (!device_service) {
    device_service = MakeGarbageCollected<DeviceService>(navigator);
    ProvideTo(navigator, device_service);
  }
  return device_service;
}

DeviceService::DeviceService(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      device_api_service_(navigator.DomWindow()) {}

ExecutionContext* DeviceService::GetExecutionContext() const {
  return GetSupplementable()->DomWindow();
}

void DeviceService::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);

  visitor->Trace(device_api_service_);
  visitor->Trace(pending_promises_);
}

mojom::blink::DeviceAPIService* DeviceService::GetService() {
  if (!device_api_service_.is_bound()) {
    GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        device_api_service_.BindNewPipeAndPassReceiver(
            GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    // The access status of Device API can change dynamically. Hence, we have to
    // properly handle cases when we are losing this access.
    device_api_service_.set_disconnect_handler(WTF::Bind(
        &DeviceService::OnServiceConnectionError, WrapWeakPersistent(this)));
  }

  return device_api_service_.get();
}

void DeviceService::OnServiceConnectionError() {
  device_api_service_.reset();
  // Resolve all pending promises with a failure.
  for (ScriptPromiseResolver* resolver : pending_promises_)
    resolver->Reject(MakeGarbageCollected<DOMException>(kDOMExceptionCode,
                                                        kDOMExceptionMessage));
}

}  // namespace blink
