// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_device.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_presentation_context.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

namespace {

const char kActiveImmersiveSession[] =
    "XRDevice already has an active, immersive session";

const char kNoOutputContext[] =
    "Non-immersive sessions must be created with an outputContext.";

const char kRequestRequiresUserActivation[] =
    "The requested session requires user activation.";

const char kSessionNotSupported[] =
    "The specified session configuration is not supported.";

}  // namespace

XRDevice::XRDevice(XR* xr, device::mojom::blink::XRDevicePtr device)
    : xr_(xr), device_ptr_(std::move(device)) {}

const char* XRDevice::checkSessionSupport(
    const XRSessionCreationOptions& options) const {
  if (!options.immersive()) {
    // Validation for non-immersive sessions. Validation for immersive sessions
    // happens browser side.
    if (!options.hasOutputContext()) {
      return kNoOutputContext;
    }
  }

  return nullptr;
}

ScriptPromise XRDevice::supportsSession(
    ScriptState* script_state,
    const XRSessionCreationOptions& options) {
  // Check to see if the device is capable of supporting the requested session
  // options. Note that reporting support here does not guarantee that creating
  // a session with those options will succeed, as other external and
  // time-sensitve factors (focus state, existence of another immersive session,
  // etc.) may prevent the creation of a session as well.
  const char* reject_reason = checkSessionSupport(options);
  if (reject_reason) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                           reject_reason));
  }

  // If the above checks pass, resolve without a value. Future API iterations
  // may specify a value to be returned here.
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  device::mojom::blink::XRSessionOptionsPtr session_options =
      device::mojom::blink::XRSessionOptions::New();
  session_options->immersive = options.immersive();

  device_ptr_->SupportsSession(
      std::move(session_options),
      WTF::Bind(&XRDevice::OnSupportsSessionReturned, WrapPersistent(this),
                WrapPersistent(resolver)));

  return promise;
}

void XRDevice::OnSupportsSessionReturned(ScriptPromiseResolver* resolver,
                                         bool supports_session) {
  supports_session
      ? resolver->Resolve()
      : resolver->Reject(DOMException::Create(
            DOMExceptionCode::kNotSupportedError, kSessionNotSupported));
}

int64_t XRDevice::GetSourceId() const {
  return xr_->GetSourceId();
}

ScriptPromise XRDevice::requestSession(
    ScriptState* script_state,
    const XRSessionCreationOptions& options) {
  Document* doc = To<Document>(ExecutionContext::From(script_state));

  if (options.immersive() && !did_log_request_immersive_session_ && doc) {
    ukm::builders::XR_WebXR(GetSourceId())
        .SetDidRequestPresentation(1)
        .Record(doc->UkmRecorder());
    did_log_request_immersive_session_ = true;
  }

  // Check first to see if the device is capable of supporting the requested
  // options.
  const char* reject_reason = checkSessionSupport(options);
  if (reject_reason) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                           reject_reason));
  }

  // TODO(ijamardo): Should we just exit if there is not document?
  bool has_user_activation =
      LocalFrame::HasTransientUserActivation(doc ? doc->GetFrame() : nullptr);

  // Check if the current page state prevents the requested session from being
  // created.
  if (options.immersive()) {
    if (frameProvider()->immersive_session()) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               kActiveImmersiveSession));
    }

    if (!has_user_activation) {
      return ScriptPromise::RejectWithDOMException(
          script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                             kRequestRequiresUserActivation));
    }
  }

  // All AR sessions require a user gesture.
  if (options.environmentIntegration()) {
    if (!has_user_activation) {
      return ScriptPromise::RejectWithDOMException(
          script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                             kRequestRequiresUserActivation));
    }
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  device::mojom::blink::XRSessionOptionsPtr session_options =
      device::mojom::blink::XRSessionOptions::New();
  session_options->immersive = options.immersive();
  session_options->provide_passthrough_camera =
      options.environmentIntegration();
  session_options->has_user_activation = has_user_activation;

  XRPresentationContext* output_context =
      options.hasOutputContext() ? options.outputContext() : nullptr;

  // TODO(http://crbug.com/826899) Once device activation is sorted out for
  // WebXR, either pass in the correct value for metrics to know whether
  // this was triggered by device activation, or remove the value as soon as
  // legacy API has been removed.
  device_ptr_->RequestSession(
      std::move(session_options), false /* triggered by display activate */,
      WTF::Bind(&XRDevice::OnRequestSessionReturned, WrapWeakPersistent(this),
                WrapPersistent(resolver), WrapPersistent(output_context),
                options.environmentIntegration(), options.immersive()));
  return promise;
}

void XRDevice::OnRequestSessionReturned(
    ScriptPromiseResolver* resolver,
    XRPresentationContext* output_context,
    bool environment_integration,
    bool immersive,
    device::mojom::blink::XRSessionPtr session_ptr) {
  // TODO(https://crbug.com/872316) Improve the error messaging to indicate why
  // a request failed.
  if (!session_ptr) {
    DOMException* exception = DOMException::Create(
        DOMExceptionCode::kNotSupportedError, kSessionNotSupported);
    resolver->Reject(exception);
    return;
  }

  XRSession::EnvironmentBlendMode blend_mode = XRSession::kBlendModeOpaque;
  if (environment_integration)
    blend_mode = XRSession::kBlendModeAlphaBlend;

  XRSession* session =
      new XRSession(this, std::move(session_ptr->client_request), immersive,
                    environment_integration, output_context, blend_mode);
  // immersive sessions must supply display info.
  DCHECK(!immersive || session_ptr->display_info);
  if (session_ptr->display_info)
    session->SetXRDisplayInfo(std::move(session_ptr->display_info));
  sessions_.insert(session);

  if (immersive) {
    frameProvider()->BeginImmersiveSession(session, std::move(session_ptr));
  } else {
    magic_window_provider_.Bind(std::move(session_ptr->data_provider));
    environment_provider_.Bind(std::move(session_ptr->environment_provider));
  }

  resolver->Resolve(session);
}

void XRDevice::OnFrameFocusChanged() {
  OnFocusChanged();
}

void XRDevice::OnFocusChanged() {
  // Tell all sessions that focus changed.
  for (const auto& session : sessions_) {
    session->OnFocusChanged();
  }

  if (frame_provider_)
    frame_provider_->OnFocusChanged();
}

bool XRDevice::IsFrameFocused() {
  return xr_->IsFrameFocused();
}

XRFrameProvider* XRDevice::frameProvider() {
  if (!frame_provider_) {
    frame_provider_ = new XRFrameProvider(this);
  }

  return frame_provider_;
}

void XRDevice::Dispose() {
  if (frame_provider_)
    frame_provider_->Dispose();
}

void XRDevice::Trace(blink::Visitor* visitor) {
  visitor->Trace(xr_);
  visitor->Trace(frame_provider_);
  visitor->Trace(sessions_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
