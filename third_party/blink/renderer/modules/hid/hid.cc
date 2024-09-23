// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid.h"

#include <utility>

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_hid_device_filter.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_hid_device_request_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/hid/hid_connection_event.h"
#include "third_party/blink/renderer/modules/hid/hid_device.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

const char kContextGone[] = "Script context has shut down.";
const char kFeaturePolicyBlocked[] =
    "Access to the feature \"hid\" is disallowed by permissions policy.";

bool IsContextSupported(ExecutionContext* context) {
  // Since WebHID on Web Workers is in the process of being implemented, we
  // check here if the runtime flag for the appropriate worker is enabled.
  // TODO(https://crbug.com/365932453): Remove this check once the feature has
  // shipped.
  if (!context) {
    return false;
  }

  DCHECK(context->IsWindow() || context->IsDedicatedWorkerGlobalScope() ||
         context->IsServiceWorkerGlobalScope());
  DCHECK(!context->IsDedicatedWorkerGlobalScope() ||
         RuntimeEnabledFeatures::WebHIDOnDedicatedWorkersEnabled());
  DCHECK(!context->IsServiceWorkerGlobalScope() ||
         RuntimeEnabledFeatures::WebHIDOnServiceWorkersEnabled());

  return true;
}

// Carries out basic checks for the web-exposed APIs, to make sure the minimum
// requirements for them to be served are met. Returns true if any conditions
// fail to be met, generating an appropriate exception as well. Otherwise,
// returns false to indicate the call should be allowed.
bool ShouldBlockHidServiceCall(LocalDOMWindow* window,
                               ExecutionContext* context,
                               ExceptionState* exception_state) {
  if (!IsContextSupported(context)) {
    if (exception_state) {
      exception_state->ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                         kContextGone);
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
          "Access to the WebHID API is denied from contexts where the "
          "top-level "
          "document has an opaque origin.");
    }
    return true;
  }

  if (!context->IsFeatureEnabled(mojom::blink::PermissionsPolicyFeature::kHid,
                                 ReportOptions::kReportOnFailure)) {
    if (exception_state) {
      exception_state->ThrowSecurityError(kFeaturePolicyBlocked);
    }
    return true;
  }

  return false;
}

void RejectWithTypeError(const String& message,
                         ScriptPromiseResolverBase* resolver) {
  ScriptState::Scope scope(resolver->GetScriptState());
  v8::Isolate* isolate = resolver->GetScriptState()->GetIsolate();
  resolver->Reject(V8ThrowException::CreateTypeError(isolate, message));
}

}  // namespace

const char HID::kSupplementName[] = "HID";

HID* HID::hid(NavigatorBase& navigator) {
  HID* hid = Supplement<NavigatorBase>::From<HID>(navigator);
  if (!hid) {
    hid = MakeGarbageCollected<HID>(navigator);
    ProvideTo(navigator, hid);
  }
  return hid;
}

HID::HID(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      service_(navigator.GetExecutionContext()),
      receiver_(this, navigator.GetExecutionContext()) {
  auto* context = GetExecutionContext();
  if (context) {
    feature_handle_for_scheduler_ = context->GetScheduler()->RegisterFeature(
        SchedulingPolicy::Feature::kWebHID,
        {SchedulingPolicy::DisableBackForwardCache()});
  }
}

HID::~HID() {
  DCHECK(get_devices_promises_.empty());
  DCHECK(request_device_promises_.empty());
}

ExecutionContext* HID::GetExecutionContext() const {
  return GetSupplementable()->GetExecutionContext();
}

const AtomicString& HID::InterfaceName() const {
  return event_target_names::kHID;
}

void HID::AddedEventListener(const AtomicString& event_type,
                             RegisteredEventListener& listener) {
  EventTarget::AddedEventListener(event_type, listener);

  if (event_type != event_type_names::kConnect &&
      event_type != event_type_names::kDisconnect) {
    return;
  }

  auto* context = GetExecutionContext();
  if (ShouldBlockHidServiceCall(GetSupplementable()->DomWindow(), context,
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

void HID::DeviceAdded(device::mojom::blink::HidDeviceInfoPtr device_info) {
  auto* device = GetOrCreateDevice(std::move(device_info));

  DispatchEvent(*MakeGarbageCollected<HIDConnectionEvent>(
      event_type_names::kConnect, device));
}

void HID::DeviceRemoved(device::mojom::blink::HidDeviceInfoPtr device_info) {
  auto* device = GetOrCreateDevice(std::move(device_info));

  DispatchEvent(*MakeGarbageCollected<HIDConnectionEvent>(
      event_type_names::kDisconnect, device));
}

void HID::DeviceChanged(device::mojom::blink::HidDeviceInfoPtr device_info) {
  auto it = device_cache_.find(device_info->guid);
  if (it != device_cache_.end()) {
    it->value->UpdateDeviceInfo(std::move(device_info));
    return;
  }

  // If the GUID is not in the |device_cache_| then this is the first time we
  // have been notified for this device.
  DeviceAdded(std::move(device_info));
}

ScriptPromise<IDLSequence<HIDDevice>> HID::getDevices(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (ShouldBlockHidServiceCall(GetSupplementable()->DomWindow(),
                                GetExecutionContext(), &exception_state)) {
    return ScriptPromise<IDLSequence<HIDDevice>>();
  }

  auto* resolver = MakeGarbageCollected<HIDDeviceResolver>(
      script_state, exception_state.GetContext());
  get_devices_promises_.insert(resolver);

  EnsureServiceConnection();
  service_->GetDevices(WTF::BindOnce(
      &HID::FinishGetDevices, WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise<IDLSequence<HIDDevice>> HID::requestDevice(
    ScriptState* script_state,
    const HIDDeviceRequestOptions* options,
    ExceptionState& exception_state) {
  // requestDevice requires a window to satisfy the user activation requirement
  // and to show a chooser dialog.
  auto* window = GetSupplementable()->DomWindow();
  if (!window) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kContextGone);
    return ScriptPromise<IDLSequence<HIDDevice>>();
  }

  if (ShouldBlockHidServiceCall(window, GetExecutionContext(),
                                &exception_state)) {
    return ScriptPromise<IDLSequence<HIDDevice>>();
  }

  if (!LocalFrame::HasTransientUserActivation(window->GetFrame())) {
    exception_state.ThrowSecurityError(
        "Must be handling a user gesture to show a permission request.");
    return ScriptPromise<IDLSequence<HIDDevice>>();
  }

  auto* resolver = MakeGarbageCollected<HIDDeviceResolver>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  request_device_promises_.insert(resolver);

  Vector<mojom::blink::HidDeviceFilterPtr> mojo_filters;
  if (options->hasFilters()) {
    mojo_filters.reserve(options->filters().size());
    for (const auto& filter : options->filters()) {
      String error_message = CheckDeviceFilterValidity(*filter);
      if (error_message) {
        RejectWithTypeError(error_message, resolver);
        return promise;
      }
      mojo_filters.push_back(ConvertDeviceFilter(*filter));
    }
  }
  DCHECK_EQ(options->filters().size(), mojo_filters.size());

  Vector<mojom::blink::HidDeviceFilterPtr> mojo_exclusion_filters;
  if (options->hasExclusionFilters()) {
    if (options->exclusionFilters().size() == 0) {
      exception_state.ThrowTypeError(
          "'exclusionFilters', if present, must contain at least one filter.");
      return ScriptPromise<IDLSequence<HIDDevice>>();
    }
    mojo_exclusion_filters.reserve(options->exclusionFilters().size());
    for (const auto& exclusion_filter : options->exclusionFilters()) {
      String error_message = CheckDeviceFilterValidity(*exclusion_filter);
      if (error_message) {
        RejectWithTypeError(error_message, resolver);
        return promise;
      }
      mojo_exclusion_filters.push_back(ConvertDeviceFilter(*exclusion_filter));
    }
    DCHECK_EQ(options->exclusionFilters().size(),
              mojo_exclusion_filters.size());
  }

  EnsureServiceConnection();
  service_->RequestDevice(
      std::move(mojo_filters), std::move(mojo_exclusion_filters),
      WTF::BindOnce(&HID::FinishRequestDevice, WrapPersistent(this),
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

void HID::Forget(device::mojom::blink::HidDeviceInfoPtr device_info,
                 mojom::blink::HidService::ForgetCallback callback) {
  EnsureServiceConnection();
  service_->Forget(std::move(device_info), std::move(callback));
}

HIDDevice* HID::GetOrCreateDevice(device::mojom::blink::HidDeviceInfoPtr info) {
  auto it = device_cache_.find(info->guid);
  if (it != device_cache_.end()) {
    return it->value.Get();
  }

  const String guid = info->guid;
  HIDDevice* device = MakeGarbageCollected<HIDDevice>(this, std::move(info),
                                                      GetExecutionContext());
  device_cache_.insert(guid, device);
  return device;
}

void HID::FinishGetDevices(
    HIDDeviceResolver* resolver,
    Vector<device::mojom::blink::HidDeviceInfoPtr> device_infos) {
  DCHECK(get_devices_promises_.Contains(resolver));
  get_devices_promises_.erase(resolver);

  HeapVector<Member<HIDDevice>> devices;
  for (auto& device_info : device_infos)
    devices.push_back(GetOrCreateDevice(std::move(device_info)));

  resolver->Resolve(devices);
}

void HID::FinishRequestDevice(
    HIDDeviceResolver* resolver,
    Vector<device::mojom::blink::HidDeviceInfoPtr> device_infos) {
  DCHECK(request_device_promises_.Contains(resolver));
  request_device_promises_.erase(resolver);

  HeapVector<Member<HIDDevice>> devices;
  for (auto& device_info : device_infos) {
    auto* device = GetOrCreateDevice(std::move(device_info));
    device->ResetIsForgotten();
    devices.push_back(device);
  }

  resolver->Resolve(devices);
}

void HID::EnsureServiceConnection() {
  DCHECK(GetExecutionContext());

  if (service_.is_bound())
    return;

  DCHECK(IsContextSupported(GetExecutionContext()));

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(task_runner));
  service_.set_disconnect_handler(
      WTF::BindOnce(&HID::CloseServiceConnection, WrapWeakPersistent(this)));
  DCHECK(!receiver_.is_bound());
  service_->RegisterClient(receiver_.BindNewEndpointAndPassRemote(task_runner));
}

void HID::CloseServiceConnection() {
  service_.reset();
  receiver_.reset();

  // Script may execute during a call to Resolve(). Swap these sets to prevent
  // concurrent modification.
  HeapHashSet<Member<HIDDeviceResolver>> get_devices_promises;
  get_devices_promises_.swap(get_devices_promises);
  for (HIDDeviceResolver* resolver : get_devices_promises) {
    resolver->Resolve(HeapVector<Member<HIDDevice>>());
  }

  HeapHashSet<Member<HIDDeviceResolver>> request_device_promises;
  request_device_promises_.swap(request_device_promises);
  for (HIDDeviceResolver* resolver : request_device_promises) {
    resolver->Resolve(HeapVector<Member<HIDDevice>>());
  }
}

mojom::blink::HidDeviceFilterPtr HID::ConvertDeviceFilter(
    const HIDDeviceFilter& filter) {
  DCHECK(!CheckDeviceFilterValidity(filter));

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

String HID::CheckDeviceFilterValidity(const HIDDeviceFilter& filter) {
  if (!filter.hasVendorId() && !filter.hasProductId() &&
      !filter.hasUsagePage() && !filter.hasUsage()) {
    return "A filter must provide a property to filter by.";
  }

  if (filter.hasProductId() && !filter.hasVendorId()) {
    return "A filter containing a productId must also contain a vendorId.";
  }

  if (filter.hasUsage() && !filter.hasUsagePage()) {
    return "A filter containing a usage must also contain a usagePage.";
  }

  return String();
}

void HID::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  visitor->Trace(get_devices_promises_);
  visitor->Trace(request_device_promises_);
  visitor->Trace(device_cache_);
  visitor->Trace(receiver_);
  EventTarget::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
}

}  // namespace blink
