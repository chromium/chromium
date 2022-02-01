// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/modules/hid/hid_device.h"

namespace blink {

namespace {

void OnAccessDevicesResponse(
    ScriptPromiseResolver* resolver,
    WTF::Vector<device::mojom::blink::HidDeviceInfoPtr> device_infos) {
  HeapVector<Member<HIDDevice>> devices;
  // TODO(b/214330822) Implement new HIDDevice constructor to work with HID
  // System Extension.

  resolver->Resolve(devices);
}

void RejectWithTypeError(const String& message,
                         ScriptPromiseResolver* resolver) {
  ScriptState::Scope scope(resolver->GetScriptState());
  v8::Isolate* isolate = resolver->GetScriptState()->GetIsolate();
  resolver->Reject(V8ThrowException::CreateTypeError(isolate, message));
}

// Converts a HID device |filter| into the equivalent Mojo type and returns it.
// If the filter is invalid, nullptr is returned and |resolver| rejects the
// promise with a TypeError.
mojom::blink::HidDeviceFilterPtr ConvertDeviceFilter(
    const HIDDeviceFilter& filter,
    ScriptPromiseResolver* resolver) {
  // TODO(b/216239205): Reuse ConvertDeviceFilter and CheckDeviceFilterValidity
  // from ::blink::HID after refactoring with
  // https://chromium-review.googlesource.com/c/chromium/src/+/3411959.

  if (!filter.hasVendorId() && !filter.hasProductId() &&
      !filter.hasUsagePage() && !filter.hasUsage()) {
    RejectWithTypeError("A filter must provide a property to filter by.",
                        resolver);
    return nullptr;
  }

  if (filter.hasProductId() && !filter.hasVendorId()) {
    RejectWithTypeError(
        "A filter containing a productId must also contain a vendorId.",
        resolver);
    return nullptr;
  }

  if (filter.hasUsage() && !filter.hasUsagePage()) {
    RejectWithTypeError(
        "A filter containing a usage must also contain a usagePage.", resolver);
    return nullptr;
  }

  auto mojo_filter = mojom::blink::HidDeviceFilter::New();
  if (filter.hasVendorId()) {
    if (filter.hasProductId()) {
      mojo_filter->device_ids =
          mojom::blink::DeviceIdFilter::NewVendorAndProduct(
              mojom::blink::VendorAndProduct::New(filter.vendorId(),
                                                  filter.productId()));
    } else {
      mojo_filter->device_ids =
          mojom::blink::DeviceIdFilter::NewVendor(filter.vendorId());
    }
  }
  if (filter.hasUsagePage()) {
    if (filter.hasUsage()) {
      mojo_filter->usage = mojom::blink::UsageFilter::NewUsageAndPage(
          device::mojom::blink::HidUsageAndPage::New(filter.usage(),
                                                     filter.usagePage()));
    } else {
      mojo_filter->usage =
          mojom::blink::UsageFilter::NewPage(filter.usagePage());
    }
  }
  return mojo_filter;
}

}  // namespace

CrosHID::CrosHID(ExecutionContext* execution_context)
    : ExecutionContextClient(execution_context), cros_hid_(execution_context) {}

void CrosHID::Trace(Visitor* visitor) const {
  visitor->Trace(cros_hid_);
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

ScriptPromise CrosHID::requestDevice(ScriptState* script_state,
                                     const HIDDeviceRequestOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto* cros_hid = GetCrosHIDOrNull();

  if (cros_hid) {
    Vector<mojom::blink::HidDeviceFilterPtr> mojo_filters;
    if (options->hasFilters()) {
      mojo_filters.ReserveCapacity(options->filters().size());
      for (const auto& filter : options->filters()) {
        auto mojo_filter = ConvertDeviceFilter(*filter, resolver);
        if (!mojo_filter)
          return resolver->Promise();
        mojo_filters.push_back(std::move(mojo_filter));
      }
      DCHECK_EQ(options->filters().size(), mojo_filters.size());

      cros_hid->AccessDevices(
          std::move(mojo_filters),
          WTF::Bind(&OnAccessDevicesResponse, WrapPersistent(resolver)));
    }
  }

  return resolver->Promise();
}

}  // namespace blink
