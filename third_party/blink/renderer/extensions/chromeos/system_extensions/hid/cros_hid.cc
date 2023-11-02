// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/hid/cros_hid.h"

#include "services/device/public/mojom/hid.mojom-blink.h"
#include "third_party/blink/public/mojom/chromeos/system_extensions/hid/cros_hid.mojom-blink.h"
#include "third_party/blink/public/mojom/hid/hid.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_hid_device_filter.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_hid_device_request_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/hid/hid.h"
#include "third_party/blink/renderer/modules/hid/hid_device.h"

namespace blink {

namespace {

void RejectWithTypeError(const String& message,
                         ScriptPromiseResolver* resolver) {
  ScriptState::Scope scope(resolver->GetScriptState());
  v8::Isolate* isolate = resolver->GetScriptState()->GetIsolate();
  resolver->Reject(V8ThrowException::CreateTypeError(isolate, message));
}

}  // namespace

const char CrosHID::kSupplementName[] = "CrosHID";

CrosHID& CrosHID::From(ExecutionContext& execution_context) {
  CHECK(!execution_context.IsContextDestroyed());
  auto* supplement =
      Supplement<ExecutionContext>::From<CrosHID>(execution_context);
  if (!supplement) {
    supplement = MakeGarbageCollected<CrosHID>(execution_context);
    ProvideTo(execution_context, supplement);
  }
  return *supplement;
}

CrosHID::CrosHID(ExecutionContext& execution_context)
    : Supplement(execution_context),
      ExecutionContextClient(&execution_context),
      cros_hid_(&execution_context) {}

void CrosHID::Trace(Visitor* visitor) const {
  visitor->Trace(cros_hid_);
  visitor->Trace(device_cache_);
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

mojom::blink::CrosHID* CrosHID::GetCrosHIDOrNull() {
  auto* execution_context = GetExecutionContext();
  if (!execution_context) {
    return nullptr;
  }

  if (!cros_hid_.is_bound()) {
    auto receiver = cros_hid_.BindNewPipeAndPassReceiver(
        execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        std::move(receiver));
  }
  return cros_hid_.get();
}

ScriptPromise CrosHID::accessDevice(ScriptState* script_state,
                                    const HIDDeviceRequestOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto* cros_hid = GetCrosHIDOrNull();

  if (cros_hid) {
    Vector<mojom::blink::HidDeviceFilterPtr> mojo_filters;
    if (options->hasFilters()) {
      mojo_filters.reserve(options->filters().size());
      for (const auto& filter : options->filters()) {
        absl::optional<String> error_message =
            HID::CheckDeviceFilterValidity(*filter);
        if (error_message) {
          RejectWithTypeError(error_message.value(), resolver);
          return resolver->Promise();
        }
        mojo_filters.push_back(HID::ConvertDeviceFilter(*filter));
      }
      DCHECK_EQ(options->filters().size(), mojo_filters.size());

      cros_hid->AccessDevices(
          std::move(mojo_filters),
          WTF::BindOnce(&CrosHID::OnAccessDevicesResponse, WrapPersistent(this),
                        WrapPersistent(resolver)));
    }
  }

  return resolver->Promise();
}

void CrosHID::Connect(
    const String& device_guid,
    mojo::PendingRemote<device::mojom::blink::HidConnectionClient>
        connection_client,
    device::mojom::blink::HidManager::ConnectCallback callback) {
  auto* cros_hid = GetCrosHIDOrNull();

  if (!cros_hid) {
    std::move(callback).Run({});
    return;
  }

  cros_hid_->Connect(device_guid, std::move(connection_client),
                     std::move(callback));
}

void CrosHID::Forget(device::mojom::blink::HidDeviceInfoPtr device_info,
                     mojom::blink::HidService::ForgetCallback callback) {
  // CrosHID::Forget is a no-op method, as System Extension does not control
  // HID devices behind a permission system at the moment. This method is
  // implemented because it is need to override
  // HIDDevice::ServiceInterface::Forget, which is in turn needed by
  // HIDDevice::forget.
}

void CrosHID::OnAccessDevicesResponse(
    ScriptPromiseResolver* resolver,
    WTF::Vector<device::mojom::blink::HidDeviceInfoPtr> device_infos) {
  HeapVector<Member<HIDDevice>> devices;
  for (auto& device_info : device_infos)
    devices.push_back(GetOrCreateDevice(std::move(device_info)));

  resolver->Resolve(devices);
}

HIDDevice* CrosHID::GetOrCreateDevice(
    device::mojom::blink::HidDeviceInfoPtr info) {
  auto it = device_cache_.find(info->guid);
  if (it != device_cache_.end()) {
    return it->value;
  }

  const String guid = info->guid;
  HIDDevice* device = MakeGarbageCollected<HIDDevice>(this, std::move(info),
                                                      GetExecutionContext());
  device_cache_.insert(guid, device);
  return device;
}

}  // namespace blink
