// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webusb/usb.h"

#include <utility>

#include "device/usb/public/mojom/device.mojom-blink.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/webusb/usb_connection_event.h"
#include "third_party/blink/renderer/modules/webusb/usb_device.h"
#include "third_party/blink/renderer/modules/webusb/usb_device_filter.h"
#include "third_party/blink/renderer/modules/webusb/usb_device_request_options.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using device::mojom::blink::UsbDeviceFilterPtr;
using device::mojom::blink::UsbDeviceInfoPtr;
using device::mojom::blink::UsbDevicePtr;

namespace blink {
namespace {

const char kFeaturePolicyBlocked[] =
    "Access to the feature \"usb\" is disallowed by feature policy.";
const char kNoDeviceSelected[] = "No device selected.";

void RejectWithTypeError(const String& error_details,
                         ScriptPromiseResolver* resolver) {
  ScriptState::Scope scope(resolver->GetScriptState());
  v8::Isolate* isolate = resolver->GetScriptState()->GetIsolate();
  resolver->Reject(V8ThrowException::CreateTypeError(isolate, error_details));
}

UsbDeviceFilterPtr ConvertDeviceFilter(const USBDeviceFilter& filter,
                                       ScriptPromiseResolver* resolver) {
  auto mojo_filter = device::mojom::blink::UsbDeviceFilter::New();
  mojo_filter->has_vendor_id = filter.hasVendorId();
  if (mojo_filter->has_vendor_id)
    mojo_filter->vendor_id = filter.vendorId();
  mojo_filter->has_product_id = filter.hasProductId();
  if (mojo_filter->has_product_id) {
    if (!mojo_filter->has_vendor_id) {
      RejectWithTypeError(
          "A filter containing a productId must also contain a vendorId.",
          resolver);
      return nullptr;
    }
    mojo_filter->product_id = filter.productId();
  }
  mojo_filter->has_class_code = filter.hasClassCode();
  if (mojo_filter->has_class_code)
    mojo_filter->class_code = filter.classCode();
  mojo_filter->has_subclass_code = filter.hasSubclassCode();
  if (mojo_filter->has_subclass_code) {
    if (!mojo_filter->has_class_code) {
      RejectWithTypeError(
          "A filter containing a subclassCode must also contain a classCode.",
          resolver);
      return nullptr;
    }
    mojo_filter->subclass_code = filter.subclassCode();
  }
  mojo_filter->has_protocol_code = filter.hasProtocolCode();
  if (mojo_filter->has_protocol_code) {
    if (!mojo_filter->has_subclass_code) {
      RejectWithTypeError(
          "A filter containing a protocolCode must also contain a "
          "subclassCode.",
          resolver);
      return nullptr;
    }
    mojo_filter->protocol_code = filter.protocolCode();
  }
  if (filter.hasSerialNumber())
    mojo_filter->serial_number = filter.serialNumber();
  return mojo_filter;
}

}  // namespace

USB::USB(ExecutionContext& context)
    : ContextLifecycleObserver(&context), client_binding_(this) {}

USB::~USB() {
  // |service_| may still be valid but there should be no more outstanding
  // requests to them because each holds a persistent handle to this object.
  DCHECK(get_devices_requests_.IsEmpty());
  DCHECK(get_permission_requests_.IsEmpty());
}

void USB::Dispose() {
  // The pipe to this object must be closed when is marked unreachable to
  // prevent messages from being dispatched before lazy sweeping.
  client_binding_.Close();
}

ScriptPromise USB::getDevices(ScriptState* script_state) {
  if (!IsContextSupported()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kNotSupportedError));
  }
  if (!IsFeatureEnabled()) {
    ExecutionContext* execution_context = ExecutionContext::From(script_state);
    if (auto* document = DynamicTo<Document>(execution_context)) {
      document->ReportFeaturePolicyViolation(mojom::FeaturePolicyFeature::kUsb);
    }
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                           kFeaturePolicyBlocked));
  }

  EnsureServiceConnection();
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  get_devices_requests_.insert(resolver);
  service_->GetDevices(WTF::Bind(&USB::OnGetDevices, WrapPersistent(this),
                                 WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise USB::requestDevice(ScriptState* script_state,
                                 const USBDeviceRequestOptions& options) {
  LocalFrame* frame = GetFrame();
  if (!frame || !frame->GetDocument()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kNotSupportedError));
  }

  if (!frame->GetDocument()->IsFeatureEnabled(
          mojom::FeaturePolicyFeature::kUsb, ReportOptions::kReportOnFailure)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                           kFeaturePolicyBlocked));
  }

  EnsureServiceConnection();

  if (!LocalFrame::HasTransientUserActivation(frame)) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            DOMExceptionCode::kSecurityError,
            "Must be handling a user gesture to show a permission request."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  Vector<UsbDeviceFilterPtr> filters;
  if (options.hasFilters()) {
    filters.ReserveCapacity(options.filters().size());
    for (const auto& filter : options.filters()) {
      UsbDeviceFilterPtr converted_filter =
          ConvertDeviceFilter(filter, resolver);
      if (!converted_filter)
        return promise;
      filters.push_back(std::move(converted_filter));
    }
  }

  DCHECK(options.filters().size() == filters.size());
  get_permission_requests_.insert(resolver);
  service_->GetPermission(std::move(filters),
                          WTF::Bind(&USB::OnGetPermission, WrapPersistent(this),
                                    WrapPersistent(resolver)));
  return promise;
}

ExecutionContext* USB::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& USB::InterfaceName() const {
  return EventTargetNames::USB;
}

void USB::ContextDestroyed(ExecutionContext*) {
  service_.reset();
  get_devices_requests_.clear();
  get_permission_requests_.clear();
}

USBDevice* USB::GetOrCreateDevice(UsbDeviceInfoPtr device_info) {
  USBDevice* device = device_cache_.at(device_info->guid);
  if (!device) {
    String guid = device_info->guid;
    UsbDevicePtr pipe;
    service_->GetDevice(guid, mojo::MakeRequest(&pipe));
    device = USBDevice::Create(std::move(device_info), std::move(pipe),
                               GetExecutionContext());
    device_cache_.insert(guid, device);
  }
  return device;
}

void USB::OnGetDevices(ScriptPromiseResolver* resolver,
                       Vector<UsbDeviceInfoPtr> device_infos) {
  DCHECK(get_devices_requests_.Contains(resolver));

  HeapVector<Member<USBDevice>> devices;
  for (auto& device_info : device_infos)
    devices.push_back(GetOrCreateDevice(std::move(device_info)));
  resolver->Resolve(devices);
  get_devices_requests_.erase(resolver);
}

void USB::OnGetPermission(ScriptPromiseResolver* resolver,
                          UsbDeviceInfoPtr device_info) {
  DCHECK(get_permission_requests_.Contains(resolver));

  EnsureServiceConnection();

  if (service_ && device_info) {
    resolver->Resolve(GetOrCreateDevice(std::move(device_info)));
  } else {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                          kNoDeviceSelected));
  }
  get_permission_requests_.erase(resolver);
}

void USB::OnDeviceAdded(UsbDeviceInfoPtr device_info) {
  if (!service_)
    return;

  DispatchEvent(*USBConnectionEvent::Create(
      EventTypeNames::connect, GetOrCreateDevice(std::move(device_info))));
}

void USB::OnDeviceRemoved(UsbDeviceInfoPtr device_info) {
  String guid = device_info->guid;
  USBDevice* device = device_cache_.at(guid);
  if (!device) {
    device = USBDevice::Create(std::move(device_info), nullptr,
                               GetExecutionContext());
  }
  DispatchEvent(
      *USBConnectionEvent::Create(EventTypeNames::disconnect, device));
  device_cache_.erase(guid);
}

void USB::OnServiceConnectionError() {
  service_.reset();
  client_binding_.Close();
  for (ScriptPromiseResolver* resolver : get_devices_requests_)
    resolver->Resolve(HeapVector<Member<USBDevice>>(0));
  get_devices_requests_.clear();

  for (ScriptPromiseResolver* resolver : get_permission_requests_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                          kNoDeviceSelected));
  }
  get_permission_requests_.clear();
}

void USB::AddedEventListener(const AtomicString& event_type,
                             RegisteredEventListener& listener) {
  EventTargetWithInlineData::AddedEventListener(event_type, listener);
  if (event_type != EventTypeNames::connect &&
      event_type != EventTypeNames::disconnect) {
    return;
  }

  if (!IsContextSupported() || !IsFeatureEnabled())
    return;

  EnsureServiceConnection();
}

void USB::EnsureServiceConnection() {
  if (service_)
    return;

  DCHECK(IsContextSupported());
  DCHECK(IsFeatureEnabled());
  GetExecutionContext()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&service_));
  service_.set_connection_error_handler(
      WTF::Bind(&USB::OnServiceConnectionError, WrapWeakPersistent(this)));

  DCHECK(!client_binding_.is_bound());

  device::mojom::blink::UsbDeviceManagerClientAssociatedPtrInfo client;
  client_binding_.Bind(mojo::MakeRequest(&client));
  service_->SetClient(std::move(client));
}

bool USB::IsContextSupported() const {
  // Since WebUSB on Web Workers is in the process of being implemented, we
  // check here if the runtime flag for the appropriate worker is enabled..
  // TODO(https://crbug.com/837406): Remove this check once the feature has
  // shipped.
  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return false;

  DCHECK(context->IsDocument() || context->IsDedicatedWorkerGlobalScope());
  DCHECK(!context->IsDedicatedWorkerGlobalScope() ||
         RuntimeEnabledFeatures::WebUSBOnDedicatedWorkersEnabled());

  return true;
}

bool USB::IsFeatureEnabled() const {
  return GetExecutionContext()->GetSecurityContext().IsFeatureEnabled(
      mojom::FeaturePolicyFeature::kUsb);
}

void USB::Trace(blink::Visitor* visitor) {
  visitor->Trace(get_devices_requests_);
  visitor->Trace(get_permission_requests_);
  visitor->Trace(device_cache_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
