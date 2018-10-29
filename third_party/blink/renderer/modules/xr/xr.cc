// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_device.h"
namespace blink {

namespace {

const char kNavigatorDetachedError[] =
    "The navigator.xr object is no longer associated with a document.";

const char kFeaturePolicyBlocked[] =
    "Access to the feature \"xr\" is disallowed by feature policy.";

const char kNoDevicesMessage[] = "No devices found.";

}  // namespace

XR::XR(LocalFrame& frame, int64_t ukm_source_id)
    : ContextLifecycleObserver(frame.GetDocument()),
      FocusChangedObserver(frame.GetPage()),
      ukm_source_id_(ukm_source_id),
      binding_(this) {
  frame.GetInterfaceProvider().GetInterface(mojo::MakeRequest(&service_));
  service_.set_connection_error_handler(
      WTF::Bind(&XR::Dispose, WrapWeakPersistent(this)));
}

void XR::FocusedFrameChanged() {
  // Tell device that focus changed.
  if (device_)
    device_->OnFrameFocusChanged();
}

bool XR::IsFrameFocused() {
  return FocusChangedObserver::IsFrameFocused(GetFrame());
}

ExecutionContext* XR::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& XR::InterfaceName() const {
  return EventTargetNames::XR;
}

ScriptPromise XR::requestDevice(ScriptState* script_state) {
  LocalFrame* frame = GetFrame();
  if (!frame) {
    // Reject if the frame is inaccessible.
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kNavigatorDetachedError));
  }

  if (!did_log_requestDevice_ && frame->GetDocument()) {
    ukm::builders::XR_WebXR(ukm_source_id_)
        .SetDidRequestAvailableDevices(1)
        .Record(frame->GetDocument()->UkmRecorder());
    did_log_requestDevice_ = true;
  }

  if (!frame->GetDocument()->IsFeatureEnabled(
          mojom::FeaturePolicyFeature::kWebVr,
          ReportOptions::kReportOnFailure)) {
    // Only allow the call to be made if the appropriate feature policy is in
    // place.
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                           kFeaturePolicyBlocked));
  }

  // If we're still waiting for a previous call to resolve return that promise
  // again.
  if (pending_devices_resolver_) {
    return pending_devices_resolver_->Promise();
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // If we no longer have a valid service connection reject the request.
  if (!service_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                          kNoDevicesMessage));
    return promise;
  }

  // If we already have a device, use that.
  if (device_) {
    resolver->Resolve(device_);
    return promise;
  }

  // Otherwise wait for device request callback.
  pending_devices_resolver_ = resolver;

  // If we're waiting for sync, then request device is already underway.
  if (pending_sync_) {
    return promise;
  }

  service_->RequestDevice(
      WTF::Bind(&XR::OnRequestDeviceReturned, WrapPersistent(this)));
  pending_sync_ = true;

  return promise;
}

// This will call when the XRDevice and its capabilities has potentially
// changed. For example, if a new physical device was connected to the system,
// the XRDevice might now be able to support immersive sessions, where it
// couldn't before.
void XR::OnDeviceChanged() {
  DispatchEvent(*blink::Event::Create(EventTypeNames::devicechange));
}

void XR::OnRequestDeviceReturned(device::mojom::blink::XRDevicePtr device) {
  pending_sync_ = false;
  if (device) {
    device_ = new XRDevice(this, std::move(device));
  }
  ResolveRequestDevice();
}

// Called when details for every connected XRDevice has been received.
void XR::ResolveRequestDevice() {
  if (pending_devices_resolver_) {
    if (!device_) {
      pending_devices_resolver_->Reject(DOMException::Create(
          DOMExceptionCode::kNotFoundError, kNoDevicesMessage));
    } else {
      // Log metrics
      if (!did_log_returned_device_ || !did_log_supports_immersive_) {
        Document* doc = GetFrame() ? GetFrame()->GetDocument() : nullptr;
        if (doc) {
          ukm::builders::XR_WebXR ukm_builder(ukm_source_id_);
          ukm_builder.SetReturnedDevice(1);
          did_log_returned_device_ = true;

          ukm_builder.Record(doc->UkmRecorder());

          device::mojom::blink::XRSessionOptionsPtr session_options =
              device::mojom::blink::XRSessionOptions::New();
          session_options->immersive = true;

          // TODO(http://crbug.com/872086) This shouldn't need to be called.
          // This information should be logged on the browser side.
          device_->xrDevicePtr()->SupportsSession(
              std::move(session_options),
              WTF::Bind(&XR::ReportImmersiveSupported, WrapPersistent(this)));
        }
      }

      // Return the device.
      pending_devices_resolver_->Resolve(device_);
    }

    pending_devices_resolver_ = nullptr;
  }
}

void XR::ReportImmersiveSupported(bool supported) {
  Document* doc = GetFrame() ? GetFrame()->GetDocument() : nullptr;
  if (doc && !did_log_supports_immersive_ && supported) {
    ukm::builders::XR_WebXR ukm_builder(ukm_source_id_);
    ukm_builder.SetReturnedPresentationCapableDevice(1);
    ukm_builder.Record(doc->UkmRecorder());
    did_log_supports_immersive_ = true;
  }
}

void XR::AddedEventListener(const AtomicString& event_type,
                            RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);

  // If we don't have device and there is no sync pending, then request the
  // device to ensure devices have been enumerated and register as a listener
  // for changes.
  if (event_type == EventTypeNames::devicechange && !device_ &&
      !pending_sync_) {
    device::mojom::blink::VRServiceClientPtr client;
    binding_.Bind(mojo::MakeRequest(&client));

    service_->RequestDevice(
        WTF::Bind(&XR::OnRequestDeviceReturned, WrapPersistent(this)));
    service_->SetClient(std::move(client));

    pending_sync_ = true;
  }
};

void XR::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

void XR::Dispose() {
  // If the document context was destroyed, shut down the client connection
  // and never call the mojo service again.
  service_.reset();
  binding_.Close();

  // Shutdown device's message pipe.
  if (device_)
    device_->Dispose();

  // Ensure that any outstanding requestDevice promises are resolved. They will
  // receive a null result.
  ResolveRequestDevice();
}

void XR::Trace(blink::Visitor* visitor) {
  visitor->Trace(device_);
  visitor->Trace(pending_devices_resolver_);
  ContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
