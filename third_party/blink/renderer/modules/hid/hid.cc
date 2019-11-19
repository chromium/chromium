// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/hid/hid_connection_event.h"
#include "third_party/blink/renderer/modules/hid/hid_device.h"
#include "third_party/blink/renderer/modules/hid/hid_device_request_options.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

const char kContextGone[] = "Script context has shut down.";
const char kFeaturePolicyBlocked[] =
    "Access to the feature \"hid\" is disallowed by feature policy.";
const char kNoDeviceSelected[] = "No device selected.";

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

HID::HID(ExecutionContext& context) : ContextLifecycleObserver(&context) {}

HID::~HID() {
  DCHECK(get_devices_promises_.IsEmpty());
  DCHECK(request_device_promises_.IsEmpty());
}

ExecutionContext* HID::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& HID::InterfaceName() const {
  return event_target_names::kHID;
}

void HID::AddedEventListener(const AtomicString& event_type,
                             RegisteredEventListener& listener) {
  EventTargetWithInlineData::AddedEventListener(event_type, listener);
  // TODO(mattreynolds): Connect to the HID service and register for connect
  // and disconnect events.
}

ScriptPromise HID::getDevices(ScriptState* script_state,
                              ExceptionState& exception_state) {
  auto* context = GetExecutionContext();
  if (!context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kContextGone);
    return ScriptPromise();
  }

  if (!context->GetSecurityContext().IsFeatureEnabled(
          mojom::FeaturePolicyFeature::kHid, ReportOptions::kReportOnFailure)) {
    exception_state.ThrowSecurityError(kFeaturePolicyBlocked);
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  get_devices_promises_.insert(resolver);

  EnsureServiceConnection();
  service_->GetDevices(WTF::Bind(&HID::FinishGetDevices, WrapPersistent(this),
                                 WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise HID::requestDevice(ScriptState* script_state,
                                 const HIDDeviceRequestOptions* options,
                                 ExceptionState& exception_state) {
  auto* frame = GetFrame();
  if (!frame || !frame->GetDocument()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kContextGone);
    return ScriptPromise();
  }

  if (!frame->GetDocument()->IsFeatureEnabled(
          mojom::FeaturePolicyFeature::kHid, ReportOptions::kReportOnFailure)) {
    exception_state.ThrowSecurityError(kFeaturePolicyBlocked);
    return ScriptPromise();
  }

  if (!LocalFrame::HasTransientUserActivation(frame)) {
    exception_state.ThrowSecurityError(
        "Must be handling a user gesture to show a permission request.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  request_device_promises_.insert(resolver);

  Vector<mojom::blink::HidDeviceFilterPtr> mojo_filters;
  if (options->hasFilters()) {
    mojo_filters.ReserveCapacity(options->filters().size());
    for (const auto& filter : options->filters()) {
      auto mojo_filter = ConvertDeviceFilter(*filter, resolver);
      if (!mojo_filter)
        return promise;
      mojo_filters.push_back(std::move(mojo_filter));
    }
  }
  DCHECK_EQ(options->filters().size(), mojo_filters.size());

  EnsureServiceConnection();
  service_->RequestDevice(
      std::move(mojo_filters),
      WTF::Bind(&HID::FinishRequestDevice, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

void HID::Connect(
    const String& device_guid,
    mojo::PendingRemote<device::mojom::blink::HidConnectionClient> client,
    device::mojom::blink::HidManager::ConnectCallback callback) {
  EnsureServiceConnection();
  service_->Connect(device_guid, std::move(client), std::move(callback));
}

HIDDevice* HID::GetOrCreateDevice(device::mojom::blink::HidDeviceInfoPtr info) {
  const String guid = info->guid;
  HIDDevice* device = device_cache_.at(guid);
  if (!device) {
    device = MakeGarbageCollected<HIDDevice>(this, std::move(info),
                                             GetExecutionContext());
    device_cache_.insert(guid, device);
  }
  return device;
}

void HID::FinishGetDevices(
    ScriptPromiseResolver* resolver,
    Vector<device::mojom::blink::HidDeviceInfoPtr> device_infos) {
  DCHECK(get_devices_promises_.Contains(resolver));
  get_devices_promises_.erase(resolver);

  HeapVector<Member<HIDDevice>> devices;
  for (auto& device_info : device_infos)
    devices.push_back(GetOrCreateDevice(std::move(device_info)));

  resolver->Resolve(devices);
}

void HID::FinishRequestDevice(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::HidDeviceInfoPtr device_info) {
  DCHECK(request_device_promises_.Contains(resolver));
  request_device_promises_.erase(resolver);

  if (device_info) {
    resolver->Resolve(GetOrCreateDevice(std::move(device_info)));
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoDeviceSelected));
  }
  request_device_promises_.erase(resolver);
}

void HID::EnsureServiceConnection() {
  DCHECK(GetExecutionContext());

  if (service_)
    return;

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(task_runner));
  service_.set_disconnect_handler(
      WTF::Bind(&HID::OnServiceConnectionError, WrapWeakPersistent(this)));
}

void HID::OnServiceConnectionError() {
  service_.reset();

  // Script may execute during a call to Resolve(). Swap these sets to prevent
  // concurrent modification.
  HeapHashSet<Member<ScriptPromiseResolver>> get_devices_promises;
  get_devices_promises_.swap(get_devices_promises);
  for (ScriptPromiseResolver* resolver : get_devices_promises)
    resolver->Resolve(HeapVector<Member<HIDDevice>>());

  HeapHashSet<Member<ScriptPromiseResolver>> request_device_promises;
  request_device_promises_.swap(request_device_promises);
  for (ScriptPromiseResolver* resolver : request_device_promises) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoDeviceSelected));
  }
}

void HID::Trace(blink::Visitor* visitor) {
  visitor->Trace(get_devices_promises_);
  visitor->Trace(request_device_promises_);
  visitor->Trace(device_cache_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
