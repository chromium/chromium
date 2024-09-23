// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webusb/usb.h"

#include <utility>

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/usb_device.mojom-blink.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_device_filter.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_device_request_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/webusb/usb_connection_event.h"
#include "third_party/blink/renderer/modules/webusb/usb_device.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using device::mojom::blink::UsbDevice;
using device::mojom::blink::UsbDeviceFilterPtr;
using device::mojom::blink::UsbDeviceInfoPtr;

namespace blink {
namespace {

const char kFeaturePolicyBlocked[] =
    "Access to the feature \"usb\" is disallowed by permissions policy.";
const char kNoDeviceSelected[] = "No device selected.";

void RejectWithTypeError(const String& error_details,
                         ScriptPromiseResolverBase* resolver) {
  ScriptState::Scope scope(resolver->GetScriptState());
  v8::Isolate* isolate = resolver->GetScriptState()->GetIsolate();
  resolver->Reject(V8ThrowException::CreateTypeError(isolate, error_details));
}

UsbDeviceFilterPtr ConvertDeviceFilter(const USBDeviceFilter* filter,
                                       ScriptPromiseResolverBase* resolver) {
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

bool IsContextSupported(ExecutionContext* context) {
  // Since WebUSB on Web Workers is in the process of being implemented, we
  // check here if the runtime flag for the appropriate worker is enabled.
  // TODO(https://crbug.com/837406): Remove this check once the feature has
  // shipped.
  if (!context) {
    return false;
  }

  DCHECK(context->IsWindow() || context->IsDedicatedWorkerGlobalScope() ||
         context->IsServiceWorkerGlobalScope());
  DCHECK(!context->IsDedicatedWorkerGlobalScope() ||
         RuntimeEnabledFeatures::WebUSBOnDedicatedWorkersEnabled());
  DCHECK(!context->IsServiceWorkerGlobalScope() ||
         RuntimeEnabledFeatures::WebUSBOnServiceWorkersEnabled());

  return true;
}

// Carries out basic checks for the web-exposed APIs, to make sure the minimum
// requirements for them to be served are met. Returns true if any conditions
// fail to be met, generating an appropriate exception as well. Otherwise,
// returns false to indicate the call should be allowed.
bool ShouldBlockUsbServiceCall(LocalDOMWindow* window,
                               ExecutionContext* context,
                               ExceptionState* exception_state) {
  if (!IsContextSupported(context)) {
    if (exception_state) {
      exception_state->ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "The implementation did not support the requested type of object or "
          "operation.");
    }
    return true;
  }
  // For window and dedicated workers, reject the request if the top-level frame
  // has an opaque origin. For Service Workers, we use their security origin
  // directly as they do not use delegated permissions.
  const SecurityOrigin* security_origin = nullptr;
  if (context->IsWindow()) {
    security_origin =
        window->GetFrame()->Top()->GetSecurityContext()->GetSecurityOrigin();
  } else if (context->IsDedicatedWorkerGlobalScope()) {
    security_origin = static_cast<WorkerGlobalScope*>(context)
                          ->top_level_frame_security_origin();
  } else if (context->IsServiceWorkerGlobalScope()) {
    security_origin = context->GetSecurityOrigin();
  } else {
    NOTREACHED();
  }
  if (security_origin->IsOpaque()) {
    if (exception_state) {
      exception_state->ThrowSecurityError(
          "Access to the WebUSB API is denied from contexts where the "
          "top-level document has an opaque origin.");
    }
    return true;
  }

  if (!context->IsFeatureEnabled(mojom::blink::PermissionsPolicyFeature::kUsb,
                                 ReportOptions::kReportOnFailure)) {
    if (exception_state) {
      exception_state->ThrowSecurityError(kFeaturePolicyBlocked);
    }
    return true;
  }

  return false;
}

}  // namespace

const char USB::kSupplementName[] = "USB";

USB* USB::usb(NavigatorBase& navigator) {
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
  DCHECK(get_devices_requests_.empty());
  DCHECK(get_permission_requests_.empty());
}

ScriptPromise<IDLSequence<USBDevice>> USB::getDevices(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (ShouldBlockUsbServiceCall(GetSupplementable()->DomWindow(),
                                GetExecutionContext(), &exception_state)) {
    return ScriptPromise<IDLSequence<USBDevice>>();
  }

  EnsureServiceConnection();
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<USBDevice>>>(
          script_state, exception_state.GetContext());
  get_devices_requests_.insert(resolver);
  service_->GetDevices(WTF::BindOnce(&USB::OnGetDevices, WrapPersistent(this),
                                     WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise<USBDevice> USB::requestDevice(
    ScriptState* script_state,
    const USBDeviceRequestOptions* options,
    ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The implementation did not support the requested type of object or "
        "operation.");
    return EmptyPromise();
  }

  if (ShouldBlockUsbServiceCall(GetSupplementable()->DomWindow(),
                                GetExecutionContext(), &exception_state)) {
    return EmptyPromise();
  }

  EnsureServiceConnection();

  if (!LocalFrame::HasTransientUserActivation(DomWindow()->GetFrame())) {
    exception_state.ThrowSecurityError(
        "Must be handling a user gesture to show a permission request.");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<USBDevice>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  auto mojo_options = mojom::blink::WebUsbRequestDeviceOptions::New();
  if (options->hasFilters()) {
    mojo_options->filters.reserve(options->filters().size());
    for (const auto& filter : options->filters()) {
      UsbDeviceFilterPtr converted_filter =
          ConvertDeviceFilter(filter, resolver);
      if (!converted_filter)
        return promise;
      mojo_options->filters.push_back(std::move(converted_filter));
    }
  }
  mojo_options->exclusion_filters.reserve(options->exclusionFilters().size());
  for (const auto& filter : options->exclusionFilters()) {
    UsbDeviceFilterPtr converted_filter = ConvertDeviceFilter(filter, resolver);
    if (!converted_filter) {
      return promise;
    }
    mojo_options->exclusion_filters.push_back(std::move(converted_filter));
  }

  DCHECK(options->filters().size() == mojo_options->filters.size());
  DCHECK(options->exclusionFilters().size() ==
         mojo_options->exclusion_filters.size());
  get_permission_requests_.insert(resolver);
  service_->GetPermission(std::move(mojo_options),
                          resolver->WrapCallbackInScriptScope(WTF::BindOnce(
                              &USB::OnGetPermission, WrapPersistent(this))));
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
  auto it = device_cache_.find(device_info->guid);
  if (it != device_cache_.end()) {
    return it->value.Get();
  }

  String guid = device_info->guid;
  mojo::PendingRemote<UsbDevice> pipe;
  service_->GetDevice(guid, pipe.InitWithNewPipeAndPassReceiver());
  USBDevice* device = MakeGarbageCollected<USBDevice>(
      this, std::move(device_info), std::move(pipe), GetExecutionContext());
  device_cache_.insert(guid, device);
  return device;
}

void USB::ForgetDevice(
    const String& device_guid,
    mojom::blink::WebUsbService::ForgetDeviceCallback callback) {
  EnsureServiceConnection();
  service_->ForgetDevice(device_guid, std::move(callback));
}

void USB::OnGetDevices(ScriptPromiseResolver<IDLSequence<USBDevice>>* resolver,
                       Vector<UsbDeviceInfoPtr> device_infos) {
  DCHECK(get_devices_requests_.Contains(resolver));

  HeapVector<Member<USBDevice>> devices;
  for (auto& device_info : device_infos)
    devices.push_back(GetOrCreateDevice(std::move(device_info)));
  resolver->Resolve(devices);
  get_devices_requests_.erase(resolver);
}

void USB::OnGetPermission(ScriptPromiseResolver<USBDevice>* resolver,
                          UsbDeviceInfoPtr device_info) {
  DCHECK(get_permission_requests_.Contains(resolver));

  EnsureServiceConnection();

  if (service_.is_bound() && device_info) {
    resolver->Resolve(GetOrCreateDevice(std::move(device_info)));
  } else {
    resolver->RejectWithDOMException(DOMExceptionCode::kNotFoundError,
                                     kNoDeviceSelected);
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
  USBDevice* device = nullptr;
  const auto it = device_cache_.find(guid);
  if (it != device_cache_.end()) {
    device = it->value;
  } else {
    device = MakeGarbageCollected<USBDevice>(this, std::move(device_info),
                                             mojo::NullRemote(),
                                             GetExecutionContext());
  }
  DispatchEvent(
      *USBConnectionEvent::Create(event_type_names::kDisconnect, device));
  device_cache_.erase(guid);
}

void USB::OnServiceConnectionError() {
  service_.reset();
  client_receiver_.reset();

  // This loop is resolving promises with a value and so it is possible for
  // script to be executed in the process of determining if the value is a
  // thenable. Move the set to a local variable to prevent such execution from
  // invalidating the iterator used by the loop.
  HeapHashSet<Member<ScriptPromiseResolver<IDLSequence<USBDevice>>>>
      get_devices_requests;
  get_devices_requests.swap(get_devices_requests_);
  for (auto& resolver : get_devices_requests)
    resolver->Resolve(HeapVector<Member<USBDevice>>(0));

  // Similar protection is unnecessary when rejecting a promise.
  for (auto& resolver : get_permission_requests_) {
    ScriptState* resolver_script_state = resolver->GetScriptState();
    if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                       resolver_script_state)) {
      continue;
    }
    ScriptState::Scope script_state_scope(resolver_script_state);
    resolver->RejectWithDOMException(DOMExceptionCode::kNotFoundError,
                                     kNoDeviceSelected);
  }
}

void USB::AddedEventListener(const AtomicString& event_type,
                             RegisteredEventListener& listener) {
  EventTarget::AddedEventListener(event_type, listener);
  if (event_type != event_type_names::kConnect &&
      event_type != event_type_names::kDisconnect) {
    return;
  }

  auto* context = GetExecutionContext();
  if (ShouldBlockUsbServiceCall(GetSupplementable()->DomWindow(), context,
                                nullptr)) {
    return;
  }

  if (context->IsServiceWorkerGlobalScope()) {
    auto* service_worker_global_scope =
        static_cast<ServiceWorkerGlobalScope*>(context);
    if (service_worker_global_scope->did_evaluate_script()) {
      String message = String::Format(
          "Event handler of '%s' event must be added on the initial evaluation "
          "of worker script. More info: "
          "https://developer.chrome.com/docs/extensions/mv3/service_workers/"
          "events/",
          event_type.Utf8().c_str());
      GetExecutionContext()->AddConsoleMessage(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning, message);
    }
  }

  EnsureServiceConnection();
}

void USB::EnsureServiceConnection() {
  if (service_.is_bound())
    return;

  DCHECK(IsContextSupported(GetExecutionContext()));
  DCHECK(IsFeatureEnabled(ReportOptions::kDoNotReport));
  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(task_runner));
  service_.set_disconnect_handler(
      WTF::BindOnce(&USB::OnServiceConnectionError, WrapWeakPersistent(this)));

  DCHECK(!client_receiver_.is_bound());

  service_->SetClient(
      client_receiver_.BindNewEndpointAndPassRemote(task_runner));
}

bool USB::IsFeatureEnabled(ReportOptions report_options) const {
  return GetExecutionContext()->IsFeatureEnabled(
      mojom::blink::PermissionsPolicyFeature::kUsb, report_options);
}

void USB::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  visitor->Trace(get_devices_requests_);
  visitor->Trace(get_permission_requests_);
  visitor->Trace(client_receiver_);
  visitor->Trace(device_cache_);
  EventTarget::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
