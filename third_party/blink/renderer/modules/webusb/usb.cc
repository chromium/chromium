// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webusb/usb.h"

#include <utility>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/usb_device.mojom-blink.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom-blink.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_device_filter.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_device_request_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/webusb/usb_connection_event.h"
#include "third_party/blink/renderer/modules/webusb/usb_device.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using device::mojom::blink::UsbDevice;
using device::mojom::blink::UsbDeviceFilterPtr;
using device::mojom::blink::UsbDeviceInfoPtr;

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

UsbDeviceFilterPtr ConvertDeviceFilter(const USBDeviceFilter* filter,
                                       ScriptPromiseResolver* resolver) {
  auto mojo_filter = device::mojom::blink::UsbDeviceFilter::New();
  mojo_filter->has_vendor_id = filter->hasVendorId();
  if (mojo_filter->has_vendor_id)
    mojo_filter->vendor_id = filter->vendorId();
  mojo_filter->has_product_id = filter->hasProductId();
  if (mojo_filter->has_product_id) {
    if (!mojo_filter->has_vendor_id) {
      RejectWithTypeError(
          "A filter containing a productId must also contain a vendorId.",
          resolver);
      return nullptr;
    }
    mojo_filter->product_id = filter->productId();
  }
  mojo_filter->has_class_code = filter->hasClassCode();
  if (mojo_filter->has_class_code)
    mojo_filter->class_code = filter->classCode();
  mojo_filter->has_subclass_code = filter->hasSubclassCode();
  if (mojo_filter->has_subclass_code) {
    if (!mojo_filter->has_class_code) {
      RejectWithTypeError(
          "A filter containing a subclassCode must also contain a classCode.",
          resolver);
      return nullptr;
    }
    mojo_filter->subclass_code = filter->subclassCode();
  }
  mojo_filter->has_protocol_code = filter->hasProtocolCode();
  if (mojo_filter->has_protocol_code) {
    if (!mojo_filter->has_subclass_code) {
      RejectWithTypeError(
          "A filter containing a protocolCode must also contain a "
          "subclassCode.",
          resolver);
      return nullptr;
    }
    mojo_filter->protocol_code = filter->protocolCode();
  }
  if (filter->hasSerialNumber())
    mojo_filter->serial_number = filter->serialNumber();
  return mojo_filter;
}

}  // namespace

const char USB::kSupplementName[] = "USB";

USB* USB::usb(NavigatorBase& navigator) {
  ExecutionContext* context = navigator.GetExecutionContext();
  if (!context)
    return nullptr;
  if (!context->IsWindow() &&
      (!context->IsDedicatedWorkerGlobalScope() ||
       !RuntimeEnabledFeatures::WebUSBOnDedicatedWorkersEnabled())) {
    // A bug in the WebIDL compiler causes this attribute to be incorrectly
    // exposed in the other worker contexts if one of the RuntimeEnabled flags
    // is enabled. Therefore, we will just return the empty usb_ member if the
    // appropriate flag is not enabled for the current context, or if the
    // current context is a ServiceWorkerGlobalScope.
    // TODO(https://crbug.com/839117): Once this attribute stops being
    // incorrectly exposed to the worker contexts, remove these checks.
    return nullptr;
  }

  USB* usb = Supplement<NavigatorBase>::From<USB>(navigator);
  if (!usb) {
    usb = MakeGarbageCollected<USB>(navigator);
    ProvideTo(navigator, usb);
  }
  return usb;
}

USB::USB(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      ExecutionContextLifecycleObserver(navigator.GetExecutionContext()),
      service_(navigator.GetExecutionContext()),
      client_receiver_(this, navigator.GetExecutionContext()) {}

USB::~USB() {
  // |service_| may still be valid but there should be no more outstanding
  // requests to them because each holds a persistent handle to this object.
  DCHECK(get_devices_requests_.IsEmpty());
  DCHECK(get_permission_requests_.IsEmpty());
}

ScriptPromise USB::getDevices(ScriptState* script_state,
                              ExceptionState& exception_state) {
  if (!IsContextSupported()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The implementation did not support the requested type of object or "
        "operation.");
    return ScriptPromise();
  }

  if (!IsFeatureEnabled(ReportOptions::kReportOnFailure)) {
    exception_state.ThrowSecurityError(kFeaturePolicyBlocked);
    return ScriptPromise();
  }

  EnsureServiceConnection();
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  get_devices_requests_.insert(resolver);
  service_->GetDevices(WTF::Bind(&USB::OnGetDevices, WrapPersistent(this),
                                 WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise USB::requestDevice(ScriptState* script_state,
                                 const USBDeviceRequestOptions* options,
                                 ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The implementation did not support the requested type of object or "
        "operation.");
    return ScriptPromise();
  }

  if (!IsFeatureEnabled(ReportOptions::kReportOnFailure)) {
    exception_state.ThrowSecurityError(kFeaturePolicyBlocked);
    return ScriptPromise();
  }

  EnsureServiceConnection();

  if (!LocalFrame::HasTransientUserActivation(DomWindow()->GetFrame())) {
    exception_state.ThrowSecurityError(
        "Must be handling a user gesture to show a permission request.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  Vector<UsbDeviceFilterPtr> filters;
  if (options->hasFilters()) {
    filters.ReserveCapacity(options->filters().size());
    for (const auto& filter : options->filters()) {
      UsbDeviceFilterPtr converted_filter =
          ConvertDeviceFilter(filter, resolver);
      if (!converted_filter)
        return promise;
      filters.push_back(std::move(converted_filter));
    }
  }

  DCHECK(options->filters().size() == filters.size());
  get_permission_requests_.insert(resolver);
  service_->GetPermission(std::move(filters),
                          WTF::Bind(&USB::OnGetPermission, WrapPersistent(this),
                                    WrapPersistent(resolver)));
  return promise;
}

ExecutionContext* USB::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& USB::InterfaceName() const {
  return event_target_names::kUSB;
}

void USB::ContextDestroyed() {
  get_devices_requests_.clear();
  get_permission_requests_.clear();
}

USBDevice* USB::GetOrCreateDevice(UsbDeviceInfoPtr device_info) {
  USBDevice* device = device_cache_.at(device_info->guid);
  if (!device) {
    String guid = device_info->guid;
    mojo::PendingRemote<UsbDevice> pipe;
    service_->GetDevice(guid, pipe.InitWithNewPipeAndPassReceiver());
    device = MakeGarbageCollected<USBDevice>(
        std::move(device_info), std::move(pipe), GetExecutionContext());
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

  if (service_.is_bound() && device_info) {
    resolver->Resolve(GetOrCreateDevice(std::move(device_info)));
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoDeviceSelected));
  }
  get_permission_requests_.erase(resolver);
}

void USB::OnDeviceAdded(UsbDeviceInfoPtr device_info) {
  if (!service_.is_bound())
    return;

  DispatchEvent(*USBConnectionEvent::Create(
      event_type_names::kConnect, GetOrCreateDevice(std::move(device_info))));
}

void USB::OnDeviceRemoved(UsbDeviceInfoPtr device_info) {
  String guid = device_info->guid;
  USBDevice* device = device_cache_.at(guid);
  if (!device) {
    device = MakeGarbageCollected<USBDevice>(
        std::move(device_info), mojo::NullRemote(), GetExecutionContext());
  }
  DispatchEvent(
      *USBConnectionEvent::Create(event_type_names::kDisconnect, device));
  device_cache_.erase(guid);
}

void USB::OnServiceConnectionError() {
  service_.reset();
  client_receiver_.reset();

  // Move the set to a local variable to prevent script execution in Resolve()
  // from invalidating the iterator used by the loop.
  HeapHashSet<Member<ScriptPromiseResolver>> get_devices_requests;
  get_devices_requests.swap(get_devices_requests_);
  for (auto& resolver : get_devices_requests)
    resolver->Resolve(HeapVector<Member<USBDevice>>(0));

  // Move the set to a local variable to prevent script execution in Reject()
  // from invalidating the iterator used by the loop.
  HeapHashSet<Member<ScriptPromiseResolver>> get_permission_requests;
  get_permission_requests.swap(get_permission_requests_);
  for (auto& resolver : get_permission_requests) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoDeviceSelected));
  }
}

void USB::AddedEventListener(const AtomicString& event_type,
                             RegisteredEventListener& listener) {
  EventTargetWithInlineData::AddedEventListener(event_type, listener);
  if (event_type != event_type_names::kConnect &&
      event_type != event_type_names::kDisconnect) {
    return;
  }

  if (!IsContextSupported() || !IsFeatureEnabled(ReportOptions::kDoNotReport))
    return;

  EnsureServiceConnection();
}

void USB::EnsureServiceConnection() {
  if (service_.is_bound())
    return;

  DCHECK(IsContextSupported());
  DCHECK(IsFeatureEnabled(ReportOptions::kDoNotReport));
  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(task_runner));
  service_.set_disconnect_handler(
      WTF::Bind(&USB::OnServiceConnectionError, WrapWeakPersistent(this)));

  DCHECK(!client_receiver_.is_bound());

  service_->SetClient(
      client_receiver_.BindNewEndpointAndPassRemote(task_runner));
}

bool USB::IsContextSupported() const {
  // Since WebUSB on Web Workers is in the process of being implemented, we
  // check here if the runtime flag for the appropriate worker is enabled..
  // TODO(https://crbug.com/837406): Remove this check once the feature has
  // shipped.
  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return false;

  DCHECK(context->IsWindow() || context->IsDedicatedWorkerGlobalScope());
  DCHECK(!context->IsDedicatedWorkerGlobalScope() ||
         RuntimeEnabledFeatures::WebUSBOnDedicatedWorkersEnabled());

  return true;
}

bool USB::IsFeatureEnabled(ReportOptions report_options) const {
  return GetExecutionContext()->IsFeatureEnabled(
      mojom::blink::FeaturePolicyFeature::kUsb, report_options);
}

void USB::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  visitor->Trace(get_devices_requests_);
  visitor->Trace(get_permission_requests_);
  visitor->Trace(client_receiver_);
  visitor->Trace(device_cache_);
  EventTargetWithInlineData::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
