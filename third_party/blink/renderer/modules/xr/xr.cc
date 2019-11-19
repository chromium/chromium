// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr.h"

#include <utility>

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

const char kNavigatorDetachedError[] =
    "The navigator.xr object is no longer associated with a document.";

const char kPageNotVisible[] = "The page is not visible";

const char kFeaturePolicyBlocked[] =
    "Access to the feature \"xr\" is disallowed by feature policy.";

const char kActiveImmersiveSession[] =
    "There is already an active, immersive XRSession.";

const char kRequestRequiresUserActivation[] =
    "The requested session requires user activation.";

const char kSessionNotSupported[] =
    "The specified session configuration is not supported.";

const char kNoDevicesMessage[] = "No XR hardware found.";

const char kImmersiveArModeNotValid[] =
    "Failed to execute '%s' on 'XR': The provided value 'immersive-ar' is not "
    "a valid enum value of type XRSessionMode.";

constexpr device::mojom::XRSessionFeature kDefaultImmersiveVrFeatures[] = {
    device::mojom::XRSessionFeature::REF_SPACE_VIEWER,
    device::mojom::XRSessionFeature::REF_SPACE_LOCAL,
};

constexpr device::mojom::XRSessionFeature kDefaultImmersiveArFeatures[] = {
    device::mojom::XRSessionFeature::REF_SPACE_VIEWER,
    device::mojom::XRSessionFeature::REF_SPACE_LOCAL,
};

constexpr device::mojom::XRSessionFeature kDefaultInlineFeatures[] = {
    device::mojom::XRSessionFeature::REF_SPACE_VIEWER,
};

// Helper method to convert session mode into Mojo options.
device::mojom::blink::XRSessionOptionsPtr convertModeToMojo(
    XRSession::SessionMode mode) {
  auto session_options = device::mojom::blink::XRSessionOptions::New();
  session_options->immersive = (mode == XRSession::kModeImmersiveVR ||
                                mode == XRSession::kModeImmersiveAR);
  session_options->environment_integration =
      mode == XRSession::kModeImmersiveAR;

  return session_options;
}

XRSession::SessionMode stringToSessionMode(const String& mode_string) {
  if (mode_string == "inline") {
    return XRSession::kModeInline;
  }
  if (mode_string == "immersive-vr") {
    return XRSession::kModeImmersiveVR;
  }
  if (mode_string == "immersive-ar") {
    return XRSession::kModeImmersiveAR;
  }

  NOTREACHED();  // Only strings in the enum are allowed by IDL.
  return XRSession::kModeInline;
}

const char* SessionModeToString(const XRSession::SessionMode& session_mode) {
  switch (session_mode) {
    case XRSession::SessionMode::kModeInline:
      return "inline";
    case XRSession::SessionMode::kModeImmersiveVR:
      return "immersive-vr";
    case XRSession::SessionMode::kModeImmersiveAR:
      return "immersive-ar";
  }

  NOTREACHED();
  return "";
}

// Converts the given string to an XRSessionFeature. If the string is
// unrecognized, returns nullopt. Based on the spec:
// https://immersive-web.github.io/webxr/#feature-name
base::Optional<device::mojom::XRSessionFeature> StringToXRSessionFeature(
    const Document* doc,
    const String& feature_string) {
  if (feature_string == "viewer") {
    return device::mojom::XRSessionFeature::REF_SPACE_VIEWER;
  } else if (feature_string == "local") {
    return device::mojom::XRSessionFeature::REF_SPACE_LOCAL;
  } else if (feature_string == "local-floor") {
    return device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR;
  } else if (feature_string == "bounded-floor") {
    return device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR;
  } else if (feature_string == "unbounded") {
    return device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED;
  } else if (RuntimeEnabledFeatures::WebXRARDOMOverlayEnabled(doc) &&
             feature_string == "dom-overlay-for-handheld-ar") {
    return device::mojom::XRSessionFeature::DOM_OVERLAY_FOR_HANDHELD_AR;
  }

  return base::nullopt;
}

bool IsFeatureValidForMode(device::mojom::XRSessionFeature feature,
                           XRSession::SessionMode mode) {
  switch (feature) {
    case device::mojom::XRSessionFeature::REF_SPACE_VIEWER:
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL:
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
      return true;
    case device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
    case device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED:
      return mode == XRSession::kModeImmersiveVR ||
             mode == XRSession::kModeImmersiveAR;
    case device::mojom::XRSessionFeature::DOM_OVERLAY_FOR_HANDHELD_AR:
      return mode == XRSession::kModeImmersiveAR;
  }
}

bool HasRequiredFeaturePolicy(const Document* doc,
                              device::mojom::XRSessionFeature feature) {
  if (!doc)
    return false;

  switch (feature) {
    case device::mojom::XRSessionFeature::REF_SPACE_VIEWER:
      return true;
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL:
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
    case device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
    case device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED:
    case device::mojom::XRSessionFeature::DOM_OVERLAY_FOR_HANDHELD_AR:
      return doc->IsFeatureEnabled(mojom::FeaturePolicyFeature::kWebXr,
                                   ReportOptions::kReportOnFailure);
  }
}

// Ensure that the immersive session request is allowed, if not
// return which security error occurred.
// https://immersive-web.github.io/webxr/#immersive-session-request-is-allowed
const char* CheckImmersiveSessionRequestAllowed(LocalFrame* frame,
                                                Document* doc) {
  // Ensure that the session was initiated by a user gesture
  if (!LocalFrame::HasTransientUserActivation(frame)) {
    return kRequestRequiresUserActivation;
  }

  // Check that the document is "trustworthy"
  // https://immersive-web.github.io/webxr/#trustworthy
  if (!doc->IsPageVisible()) {
    return kPageNotVisible;
  }

  // Consent occurs in the Browser process.

  return nullptr;
}

}  // namespace

// Ensure that the inline session request is allowed, if not
// return which security error occurred.
// https://immersive-web.github.io/webxr/#inline-session-request-is-allowed
const char* XR::CheckInlineSessionRequestAllowed(
    LocalFrame* frame,
    const PendingRequestSessionQuery& query) {
  // Without user activation, we must reject the session if *any* features
  // (optional or required) were present, whether or not they were recognized.
  // The only exception to this is the 'viewer' feature.
  if (!LocalFrame::HasTransientUserActivation(frame)) {
    if (query.InvalidOptionalFeatures() || query.InvalidRequiredFeatures()) {
      return kRequestRequiresUserActivation;
    }

    // If any required features (besides 'viewer') were requested, reject.
    for (auto feature : query.RequiredFeatures()) {
      if (feature != device::mojom::XRSessionFeature::REF_SPACE_VIEWER) {
        return kRequestRequiresUserActivation;
      }
    }

    // If any optional features (besides 'viewer') were requested, reject.
    for (auto feature : query.OptionalFeatures()) {
      if (feature != device::mojom::XRSessionFeature::REF_SPACE_VIEWER) {
        return kRequestRequiresUserActivation;
      }
    }
  }

  return nullptr;
}

XR::PendingSupportsSessionQuery::PendingSupportsSessionQuery(
    ScriptPromiseResolver* resolver,
    XRSession::SessionMode session_mode,
    bool throw_on_unsupported)
    : resolver_(resolver),
      mode_(session_mode),
      throw_on_unsupported_(throw_on_unsupported) {}

void XR::PendingSupportsSessionQuery::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
}

void XR::PendingSupportsSessionQuery::Resolve(bool supported,
                                              ExceptionState* exception_state) {
  if (throw_on_unsupported_) {
    if (supported) {
      resolver_->Resolve();
    } else {
      RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                             kSessionNotSupported, exception_state);
    }
  } else {
    resolver_->Resolve(supported);
  }
}

void XR::PendingSupportsSessionQuery::RejectWithDOMException(
    DOMExceptionCode exception_code,
    const String& message,
    ExceptionState* exception_state) {
  DCHECK_NE(exception_code, DOMExceptionCode::kSecurityError);

  if (exception_state) {
    // The generated bindings will reject the returned promise for us.
    // Detaching the resolver prevents it from thinking we abandoned
    // the promise.
    exception_state->ThrowDOMException(exception_code, message);
    resolver_->Detach();
  } else {
    resolver_->Reject(
        MakeGarbageCollected<DOMException>(exception_code, message));
  }
}

void XR::PendingSupportsSessionQuery::RejectWithSecurityError(
    const String& sanitized_message,
    ExceptionState* exception_state) {
  if (exception_state) {
    // The generated V8 bindings will reject the returned promise for us.
    // Detaching the resolver prevents it from thinking we abandoned
    // the promise.
    exception_state->ThrowSecurityError(sanitized_message);
    resolver_->Detach();
  } else {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError, sanitized_message));
  }
}

void XR::PendingSupportsSessionQuery::RejectWithTypeError(
    const String& message,
    ExceptionState* exception_state) {
  if (exception_state) {
    // The generated bindings will reject the returned promise for us.
    // Detaching the resolver prevents it from thinking we abandoned
    // the promise.
    exception_state->ThrowTypeError(message);
    resolver_->Detach();
  } else {
    resolver_->Reject(V8ThrowException::CreateTypeError(
        resolver_->GetScriptState()->GetIsolate(), message));
  }
}

XRSession::SessionMode XR::PendingSupportsSessionQuery::mode() const {
  return mode_;
}

XR::PendingRequestSessionQuery::PendingRequestSessionQuery(
    int64_t ukm_source_id,
    ScriptPromiseResolver* resolver,
    XRSession::SessionMode session_mode,
    RequestedXRSessionFeatureSet required_features,
    RequestedXRSessionFeatureSet optional_features)
    : resolver_(resolver),
      mode_(session_mode),
      required_features_(std::move(required_features)),
      optional_features_(std::move(optional_features)),
      ukm_source_id_(ukm_source_id) {
  ParseSensorRequirement();
}

void XR::PendingRequestSessionQuery::Resolve(
    XRSession* session,
    mojo::PendingRemote<device::mojom::blink::XRSessionMetricsRecorder>
        metrics_recorder) {
  resolver_->Resolve(session);
  ReportRequestSessionResult(SessionRequestStatus::kSuccess, session,
                             std::move(metrics_recorder));
}

void XR::PendingRequestSessionQuery::RejectWithDOMException(
    DOMExceptionCode exception_code,
    const String& message,
    ExceptionState* exception_state) {
  DCHECK_NE(exception_code, DOMExceptionCode::kSecurityError);

  if (exception_state) {
    exception_state->ThrowDOMException(exception_code, message);
    resolver_->Detach();
  } else {
    resolver_->Reject(
        MakeGarbageCollected<DOMException>(exception_code, message));
  }

  ReportRequestSessionResult(SessionRequestStatus::kOtherError);
}

void XR::PendingRequestSessionQuery::RejectWithSecurityError(
    const String& sanitized_message,
    ExceptionState* exception_state) {
  if (exception_state) {
    exception_state->ThrowSecurityError(sanitized_message);
    resolver_->Detach();
  } else {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError, sanitized_message));
  }

  ReportRequestSessionResult(SessionRequestStatus::kOtherError);
}

void XR::PendingRequestSessionQuery::RejectWithTypeError(
    const String& message,
    ExceptionState* exception_state) {
  if (exception_state) {
    exception_state->ThrowTypeError(message);
    resolver_->Detach();
  } else {
    resolver_->Reject(V8ThrowException::CreateTypeError(
        GetScriptState()->GetIsolate(), message));
  }

  ReportRequestSessionResult(SessionRequestStatus::kOtherError);
}

device::mojom::XRSessionFeatureRequestStatus
XR::PendingRequestSessionQuery::GetFeatureRequestStatus(
    device::mojom::XRSessionFeature feature,
    const XRSession* session) const {
  using device::mojom::XRSessionFeatureRequestStatus;

  if (RequiredFeatures().Contains(feature)) {
    // In the case of required features, accepted/rejected state is
    // the same as the entire session.
    return XRSessionFeatureRequestStatus::kRequired;
  }

  if (OptionalFeatures().Contains(feature)) {
    if (!session || !session->IsFeatureEnabled(feature)) {
      return XRSessionFeatureRequestStatus::kOptionalRejected;
    }

    return XRSessionFeatureRequestStatus::kOptionalAccepted;
  }

  return XRSessionFeatureRequestStatus::kNotRequested;
}

void XR::PendingRequestSessionQuery::ReportRequestSessionResult(
    SessionRequestStatus status,
    XRSession* session,
    mojo::PendingRemote<device::mojom::blink::XRSessionMetricsRecorder>
        metrics_recorder) {
  using device::mojom::XRSessionFeature;

  LocalFrame* frame = resolver_->GetFrame();
  Document* doc = frame ? frame->GetDocument() : nullptr;
  if (!doc)
    return;

  auto feature_request_viewer =
      GetFeatureRequestStatus(XRSessionFeature::REF_SPACE_VIEWER, session);
  auto feature_request_local =
      GetFeatureRequestStatus(XRSessionFeature::REF_SPACE_LOCAL, session);
  auto feature_request_local_floor =
      GetFeatureRequestStatus(XRSessionFeature::REF_SPACE_LOCAL_FLOOR, session);
  auto feature_request_bounded_floor = GetFeatureRequestStatus(
      XRSessionFeature::REF_SPACE_BOUNDED_FLOOR, session);
  auto feature_request_unbounded =
      GetFeatureRequestStatus(XRSessionFeature::REF_SPACE_UNBOUNDED, session);

  ukm::builders::XR_WebXR_SessionRequest(ukm_source_id_)
      .SetMode(static_cast<int64_t>(mode_))
      .SetStatus(static_cast<int64_t>(status))
      .SetFeature_Viewer(static_cast<int64_t>(feature_request_viewer))
      .SetFeature_Local(static_cast<int64_t>(feature_request_local))
      .SetFeature_LocalFloor(static_cast<int64_t>(feature_request_local_floor))
      .SetFeature_BoundedFloor(
          static_cast<int64_t>(feature_request_bounded_floor))
      .SetFeature_Unbounded(static_cast<int64_t>(feature_request_unbounded))
      .Record(doc->UkmRecorder());

  if (session && metrics_recorder) {
    mojo::Remote<device::mojom::blink::XRSessionMetricsRecorder> recorder(
        std::move(metrics_recorder));
    session->SetMetricsReporter(
        std::make_unique<XRSession::MetricsReporter>(std::move(recorder)));
  }
}

XRSession::SessionMode XR::PendingRequestSessionQuery::mode() const {
  return mode_;
}

const XRSessionFeatureSet& XR::PendingRequestSessionQuery::RequiredFeatures()
    const {
  return required_features_.valid_features;
}

const XRSessionFeatureSet& XR::PendingRequestSessionQuery::OptionalFeatures()
    const {
  return optional_features_.valid_features;
}

bool XR::PendingRequestSessionQuery::InvalidRequiredFeatures() const {
  return required_features_.invalid_features;
}

bool XR::PendingRequestSessionQuery::InvalidOptionalFeatures() const {
  return optional_features_.invalid_features;
}

ScriptState* XR::PendingRequestSessionQuery::GetScriptState() const {
  return resolver_->GetScriptState();
}

void XR::PendingRequestSessionQuery::ParseSensorRequirement() {
  // All modes other than inline require sensors.
  if (mode_ != XRSession::kModeInline) {
    sensor_requirement_ = SensorRequirement::kRequired;
    return;
  }

  // If any required features require sensors, then sensors are required.
  for (const auto& feature : RequiredFeatures()) {
    if (feature != device::mojom::XRSessionFeature::REF_SPACE_VIEWER) {
      sensor_requirement_ = SensorRequirement::kRequired;
      return;
    }
  }

  // If any optional features require sensors, then sensors are optional.
  for (const auto& feature : OptionalFeatures()) {
    if (feature != device::mojom::XRSessionFeature::REF_SPACE_VIEWER) {
      sensor_requirement_ = SensorRequirement::kOptional;
      return;
    }
  }

  // By this point any situation that requires sensors should have returned.
  sensor_requirement_ = kNone;
}

void XR::PendingRequestSessionQuery::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
}

device::mojom::blink::XRSessionOptionsPtr XR::XRSessionOptionsFromQuery(
    const PendingRequestSessionQuery& query) {
  device::mojom::blink::XRSessionOptionsPtr session_options =
      convertModeToMojo(query.mode());

  CopyToVector(query.RequiredFeatures(), session_options->required_features);
  CopyToVector(query.OptionalFeatures(), session_options->optional_features);

  return session_options;
}

XR::XR(LocalFrame& frame, int64_t ukm_source_id)
    : ContextLifecycleObserver(frame.GetDocument()),
      FocusChangedObserver(frame.GetPage()),
      ukm_source_id_(ukm_source_id),
      navigation_start_(
          frame.Loader().GetDocumentLoader()->GetTiming().NavigationStart()),
      feature_handle_for_scheduler_(frame.GetFrameScheduler()->RegisterFeature(
          SchedulingPolicy::Feature::kWebXR,
          {SchedulingPolicy::RecordMetricsForBackForwardCache()})) {
  // See https://bit.ly/2S0zRAS for task types.
  DCHECK(frame.IsAttached());
  frame.GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(
          frame.GetTaskRunner(TaskType::kMiscPlatformAPI)));
  service_.set_disconnect_handler(WTF::Bind(
      &XR::Dispose, WrapWeakPersistent(this), DisposeType::kDisconnected));
}

void XR::FocusedFrameChanged() {
  // Tell all sessions that focus changed.
  for (const auto& session : sessions_) {
    session->OnFocusChanged();
  }

  if (frame_provider_)
    frame_provider_->OnFocusChanged();
}

bool XR::IsFrameFocused() {
  return FocusChangedObserver::IsFrameFocused(GetFrame());
}

ExecutionContext* XR::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& XR::InterfaceName() const {
  return event_target_names::kXR;
}

XRFrameProvider* XR::frameProvider() {
  if (!frame_provider_) {
    frame_provider_ = MakeGarbageCollected<XRFrameProvider>(this);
  }

  return frame_provider_;
}

const mojo::AssociatedRemote<
    device::mojom::blink::XREnvironmentIntegrationProvider>&
XR::xrEnvironmentProviderRemote() {
  return environment_provider_;
}

void XR::AddEnvironmentProviderErrorHandler(
    EnvironmentProviderErrorCallback callback) {
  environment_provider_error_callbacks_.push_back(std::move(callback));
}

void XR::ExitPresent(base::OnceClosure on_exited) {
  DVLOG(1) << __func__;
  if (service_) {
    service_->ExitPresent(std::move(on_exited));
  } else {
    // The service was already shut down, run the callback immediately.
    std::move(on_exited).Run();
  }

  // If the document was potentially being shown in a DOM overlay via
  // fullscreened elements, make sure to clear any fullscreen states on exiting
  // the session. This avoids a race condition:
  // - browser side ends session and exits fullscreen (i.e. back button)
  // - renderer processes WebViewImpl::ExitFullscreen via ChromeClient
  // - JS application sets a new element to fullscreen, this is allowed
  //   because doc->IsImmersiveArOverlay() is still true at this point
  // - renderer processes XR session shutdown (this method)
  // - browser re-enters fullscreen unexpectedly
  LocalFrame* frame = GetFrame();
  if (!frame)
    return;

  Document* doc = frame->GetDocument();
  DCHECK(doc);
  if (doc->IsImmersiveArOverlay()) {
    doc->SetIsImmersiveArOverlay(false);
    Element* fullscreen_element = Fullscreen::FullscreenElementFrom(*doc);
    if (fullscreen_element) {
      // "ua_originated" means that the browser process already exited
      // fullscreen. Set it to false because we need the browser process
      // to get notified that it needs to exit fullscreen. Use
      // FullyExitFullscreen to ensure that we return to non-fullscreen mode.
      // ExitFullscreen only unfullscreens a single element, potentially
      // leaving others in fullscreen mode.
      constexpr bool kUaOriginated = false;
      Fullscreen::FullyExitFullscreen(*doc, kUaOriginated);
    }
    // Restore the FrameView background color that was changed in
    // OnRequestSessionReturned.
    auto* frame_view = doc->GetLayoutView()->GetFrameView();
    // SetBaseBackgroundColor updates composited layer mappings.
    // That DCHECKs IsAllowedToQueryCompositingState which requires
    // DocumentLifecycle >= kInCompositingUpdate.
    frame_view->UpdateLifecycleToCompositingInputsClean();
    frame_view->SetBaseBackgroundColor(original_base_background_color_);
  }
}

void XR::SetFramesThrottled(const XRSession* session, bool throttled) {
  // The service only cares if the immersive session is throttling frames.
  if (session->immersive()) {
    // If we have an immersive session, we should have a service.
    DCHECK(service_);
    service_->SetFramesThrottled(throttled);
  }
}

ScriptPromise XR::supportsSession(ScriptState* script_state,
                                  const String& mode,
                                  ExceptionState& exception_state) {
  return InternalIsSessionSupported(script_state, mode, exception_state, true);
}

ScriptPromise XR::isSessionSupported(ScriptState* script_state,
                                     const String& mode,
                                     ExceptionState& exception_state) {
  return InternalIsSessionSupported(script_state, mode, exception_state, false);
}

ScriptPromise XR::InternalIsSessionSupported(ScriptState* script_state,
                                             const String& mode,
                                             ExceptionState& exception_state,
                                             bool throw_on_unsupported) {
  LocalFrame* frame = GetFrame();
  Document* doc = frame ? frame->GetDocument() : nullptr;
  if (!doc) {
    // Reject if the frame or document is inaccessible.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kNavigatorDetachedError);
    return ScriptPromise();  // Will be rejected by generated bindings
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  XRSession::SessionMode session_mode = stringToSessionMode(mode);
  PendingSupportsSessionQuery* query =
      MakeGarbageCollected<PendingSupportsSessionQuery>(resolver, session_mode,
                                                        throw_on_unsupported);

  if (session_mode == XRSession::kModeImmersiveAR &&
      !RuntimeEnabledFeatures::WebXRARModuleEnabled(doc)) {
    query->RejectWithTypeError(
        String::Format(kImmersiveArModeNotValid, "supportsSession"),
        &exception_state);
    return promise;
  }

  if (session_mode == XRSession::kModeInline) {
    // inline sessions are always supported.
    query->Resolve(true);
    return promise;
  }

  if (!doc->IsFeatureEnabled(mojom::FeaturePolicyFeature::kWebXr,
                             ReportOptions::kReportOnFailure)) {
    // Only allow the call to be made if the appropriate feature policy is in
    // place.
    query->RejectWithSecurityError(kFeaturePolicyBlocked, &exception_state);
    return promise;
  }

  if (!service_) {
    // If we don't have a service at the time we reach this call it indicates
    // that there's no WebXR hardware. Reject as not supported.
    query->Resolve(false, &exception_state);
    return promise;
  }

  device::mojom::blink::XRSessionOptionsPtr session_options =
      convertModeToMojo(query->mode());

  outstanding_support_queries_.insert(query);
  service_->SupportsSession(
      std::move(session_options),
      WTF::Bind(&XR::OnSupportsSessionReturned, WrapPersistent(this),
                WrapPersistent(query)));

  return promise;
}

void XR::RequestImmersiveSession(LocalFrame* frame,
                                 Document* doc,
                                 PendingRequestSessionQuery* query,
                                 ExceptionState* exception_state) {
  DVLOG(2) << __func__;
  // Log an immersive session request if we haven't already
  if (!did_log_request_immersive_session_) {
    ukm::builders::XR_WebXR(GetSourceId())
        .SetDidRequestPresentation(1)
        .Record(doc->UkmRecorder());
    did_log_request_immersive_session_ = true;
  }

  // Make sure the request is allowed
  auto* immersive_session_request_error =
      CheckImmersiveSessionRequestAllowed(frame, doc);
  if (immersive_session_request_error) {
    query->RejectWithSecurityError(immersive_session_request_error,
                                   exception_state);
    return;
  }

  // Ensure there are no other immersive sessions currently pending or active
  if (has_outstanding_immersive_request_ ||
      frameProvider()->immersive_session()) {
    query->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                  kActiveImmersiveSession, exception_state);
    return;
  }

  // If we don't have a service by the time we reach this call, there is no XR
  // hardware.
  if (!service_) {
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kNoDevicesMessage, exception_state);
    return;
  }

  // Reject session if any of the required features were invalid.
  if (query->InvalidRequiredFeatures()) {
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kSessionNotSupported, exception_state);
    return;
  }

  // Reworded from spec 'pending immersive session'
  has_outstanding_immersive_request_ = true;

  // Submit the request to VrServiceImpl in the Browser process
  outstanding_request_queries_.insert(query);
  auto session_options = XRSessionOptionsFromQuery(*query);
  service_->RequestSession(
      std::move(session_options),
      WTF::Bind(&XR::OnRequestSessionReturned, WrapWeakPersistent(this),
                WrapPersistent(query)));
}

void XR::RequestInlineSession(LocalFrame* frame,
                              PendingRequestSessionQuery* query,
                              ExceptionState* exception_state) {
  DVLOG(2) << __func__;
  // Make sure the inline session request was allowed
  auto* inline_session_request_error =
      CheckInlineSessionRequestAllowed(frame, *query);
  if (inline_session_request_error) {
    query->RejectWithSecurityError(inline_session_request_error,
                                   exception_state);
    return;
  }

  // Reject session if any of the required features were invalid.
  if (query->InvalidRequiredFeatures()) {
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kSessionNotSupported, exception_state);
    return;
  }

  auto sensor_requirement = query->GetSensorRequirement();

  // If no sensors are requested, or if we don't have a service and sensors are
  // not required, then just create a sensorless session.
  if (sensor_requirement == SensorRequirement::kNone ||
      (!service_ && sensor_requirement != SensorRequirement::kRequired)) {
    query->Resolve(CreateSensorlessInlineSession());
    return;
  }

  // If we don't have a service, then we don't have any WebXR hardware.
  // If we didn't already create a sensorless session, we can't create a session
  // without hardware, so just reject now.
  if (!service_) {
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kSessionNotSupported, exception_state);
    return;
  }

  // Submit the request to VrServiceImpl in the Browser process
  outstanding_request_queries_.insert(query);
  auto session_options = XRSessionOptionsFromQuery(*query);
  service_->RequestSession(
      std::move(session_options),
      WTF::Bind(&XR::OnRequestSessionReturned, WrapWeakPersistent(this),
                WrapPersistent(query)));
}

XR::RequestedXRSessionFeatureSet XR::ParseRequestedFeatures(
    Document* doc,
    const HeapVector<ScriptValue>& features,
    const XRSession::SessionMode& session_mode,
    mojom::ConsoleMessageLevel error_level) {
  RequestedXRSessionFeatureSet result;

  // Iterate over all requested features, even if intermediate
  // elements are found to be invalid.
  for (const auto& feature : features) {
    String feature_string;
    if (feature.ToString(feature_string)) {
      auto feature_enum = StringToXRSessionFeature(doc, feature_string);

      if (!feature_enum) {
        GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
            mojom::ConsoleMessageSource::kJavaScript, error_level,
            "Unrecognized feature requested: " + feature_string));
        result.invalid_features = true;
      } else if (!IsFeatureValidForMode(feature_enum.value(), session_mode)) {
        GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
            mojom::ConsoleMessageSource::kJavaScript, error_level,
            "Feature '" + feature_string + "' is not supported for mode: " +
                SessionModeToString(session_mode)));
        result.invalid_features = true;
      } else if (!HasRequiredFeaturePolicy(doc, feature_enum.value())) {
        GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
            mojom::ConsoleMessageSource::kJavaScript, error_level,
            "Feature '" + feature_string +
                "' is not permitted by feature policy"));
        result.invalid_features = true;
      } else {
        result.valid_features.insert(feature_enum.value());
      }
    } else {
      GetExecutionContext()->AddConsoleMessage(
          ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                                 error_level, "Unrecognized feature value"));
      result.invalid_features = true;
    }
  }

  return result;
}

ScriptPromise XR::requestSession(ScriptState* script_state,
                                 const String& mode,
                                 XRSessionInit* session_init,
                                 ExceptionState& exception_state) {
  DVLOG(2) << __func__;
  // TODO(https://crbug.com/968622): Make sure we don't forget to call
  // metrics-related methods when the promise gets resolved/rejected.
  LocalFrame* frame = GetFrame();
  Document* doc = frame ? frame->GetDocument() : nullptr;
  if (!doc) {
    // Reject if the frame or doc is inaccessible.

    // Do *not* record an UKM event in this case (we won't be able to access the
    // Document to get UkmRecorder anyway).
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kNavigatorDetachedError);
    return ScriptPromise();  // Will be rejected by generated bindings
  }

  XRSession::SessionMode session_mode = stringToSessionMode(mode);

  // If the request is for immersive-ar, ensure that feature is enabled.
  if (session_mode == XRSession::kModeImmersiveAR &&
      !RuntimeEnabledFeatures::WebXRARModuleEnabled(doc)) {
    exception_state.ThrowTypeError(
        String::Format(kImmersiveArModeNotValid, "requestSession"));

    // We haven't created the query yet, so we can't use it to implicitly log
    // our metrics for us, so explicitly log it here, as the query requires the
    // features to be parsed before it can be built.
    ukm::builders::XR_WebXR_SessionRequest(GetSourceId())
        .SetMode(static_cast<int64_t>(session_mode))
        .SetStatus(static_cast<int64_t>(SessionRequestStatus::kOtherError))
        .Record(doc->UkmRecorder());
    return ScriptPromise();
  }

  // Parse required feature strings
  RequestedXRSessionFeatureSet required_features;
  if (session_init && session_init->hasRequiredFeatures()) {
    required_features = ParseRequestedFeatures(
        doc, session_init->requiredFeatures(), session_mode,
        mojom::ConsoleMessageLevel::kError);
  }

  // Parse optional feature strings
  RequestedXRSessionFeatureSet optional_features;
  if (session_init && session_init->hasOptionalFeatures()) {
    optional_features = ParseRequestedFeatures(
        doc, session_init->optionalFeatures(), session_mode,
        mojom::ConsoleMessageLevel::kWarning);
  }

  // Certain session modes imply default features.
  // Add those default features as required features now.
  base::span<const device::mojom::XRSessionFeature> default_features;
  switch (session_mode) {
    case XRSession::kModeImmersiveVR:
      default_features = kDefaultImmersiveVrFeatures;
      break;
    case XRSession::kModeImmersiveAR:
      default_features = kDefaultImmersiveArFeatures;
      break;
    case XRSession::kModeInline:
      default_features = kDefaultInlineFeatures;
      break;
  }

  for (const auto& feature : default_features) {
    if (HasRequiredFeaturePolicy(doc, feature)) {
      required_features.valid_features.insert(feature);
    } else {
      required_features.invalid_features = true;
    }
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  PendingRequestSessionQuery* query =
      MakeGarbageCollected<PendingRequestSessionQuery>(
          GetSourceId(), resolver, session_mode, std::move(required_features),
          std::move(optional_features));

  switch (session_mode) {
    case XRSession::kModeImmersiveVR:
    case XRSession::kModeImmersiveAR:
      RequestImmersiveSession(frame, doc, query, &exception_state);
      break;
    case XRSession::kModeInline:
      RequestInlineSession(frame, query, &exception_state);
      break;
  }

  return promise;
}

// This will be called when the XR hardware or capabilities have potentially
// changed. For example, if a new physical device was connected to the system,
// it might be able to support immersive sessions, where it couldn't before.
void XR::OnDeviceChanged() {
  LocalFrame* frame = GetFrame();
  Document* doc = frame ? frame->GetDocument() : nullptr;
  if (doc && doc->IsFeatureEnabled(mojom::FeaturePolicyFeature::kWebXr)) {
    DispatchEvent(*blink::Event::Create(event_type_names::kDevicechange));
  }
}

void XR::OnSupportsSessionReturned(PendingSupportsSessionQuery* query,
                                   bool supports_session) {
  // The session query has returned and we're about to resolve or reject the
  // promise, so remove it from our outstanding list.
  DCHECK(outstanding_support_queries_.Contains(query));
  outstanding_support_queries_.erase(query);
  query->Resolve(supports_session);
}

void XR::OnRequestSessionReturned(
    PendingRequestSessionQuery* query,
    device::mojom::blink::RequestSessionResultPtr result) {
  // The session query has returned and we're about to resolve or reject the
  // promise, so remove it from our outstanding list.
  DCHECK(outstanding_request_queries_.Contains(query));
  outstanding_request_queries_.erase(query);
  if (query->mode() == XRSession::kModeImmersiveVR ||
      query->mode() == XRSession::kModeImmersiveAR) {
    DCHECK(has_outstanding_immersive_request_);
    has_outstanding_immersive_request_ = false;
  }

  // TODO(https://crbug.com/872316) Improve the error messaging to indicate why
  // a request failed.
  if (!result->is_success()) {
    // |service_| does not support the requested mode. Attempt to create a
    // sensorless session.
    if (query->GetSensorRequirement() != SensorRequirement::kRequired) {
      XRSession* session = CreateSensorlessInlineSession();
      query->Resolve(session);
      return;
    }

    // TODO(http://crbug.com/961960): Report appropriate exception when the user
    // denies XR session request on consent dialog
    // TODO(https://crbug.com/872316): Improve the error messaging to indicate
    // the reason for a request failure.
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kSessionNotSupported, nullptr);
    return;
  }

  auto session_ptr = std::move(result->get_success()->session);
  auto metrics_recorder = std::move(result->get_success()->metrics_recorder);

  bool environment_integration = query->mode() == XRSession::kModeImmersiveAR;

  // immersive sessions must supply display info.
  DCHECK(session_ptr->display_info);
  DVLOG(2) << __func__
           << ": environment_integration=" << environment_integration;

  // TODO(https://crbug.com/944936): The blend mode could be "additive".
  XRSession::EnvironmentBlendMode blend_mode = XRSession::kBlendModeOpaque;
  if (environment_integration)
    blend_mode = XRSession::kBlendModeAlphaBlend;

  XRSessionFeatureSet enabled_features;
  for (const auto& feature : session_ptr->enabled_features) {
    enabled_features.insert(feature);
  }

  XRSession* session = CreateSession(
      query->mode(), blend_mode, std::move(session_ptr->client_receiver),
      std::move(session_ptr->display_info), session_ptr->uses_input_eventing,
      enabled_features);

  frameProvider()->OnSessionStarted(session, std::move(session_ptr));

  if (query->mode() == XRSession::kModeImmersiveVR ||
      query->mode() == XRSession::kModeImmersiveAR) {
    if (environment_integration) {
      // See Task Sources spreadsheet for more information:
      // https://docs.google.com/spreadsheets/d/1b-dus1Ug3A8y0lX0blkmOjJILisUASdj8x9YN_XMwYc/view
      frameProvider()
          ->GetImmersiveDataProvider()
          ->GetEnvironmentIntegrationProvider(
              environment_provider_.BindNewEndpointAndPassReceiver(
                  GetExecutionContext()->GetTaskRunner(
                      TaskType::kMiscPlatformAPI)));
      environment_provider_.set_disconnect_handler(WTF::Bind(
          &XR::OnEnvironmentProviderDisconnect, WrapWeakPersistent(this)));

      session->OnEnvironmentProviderCreated();

      LocalFrame* frame = GetFrame();
      DCHECK(frame);

      if (enabled_features.Contains(
              device::mojom::XRSessionFeature::DOM_OVERLAY_FOR_HANDHELD_AR)) {
        // The session is using DOM overlay mode.
        Document* doc = frame->GetDocument();
        DCHECK(doc);
        doc->SetIsImmersiveArOverlay(true);

        // Save the current base background color (restored in ExitPresent),
        // and set a transparent background for the FrameView.
        auto* frame_view = doc->GetLayoutView()->GetFrameView();
        // SetBaseBackgroundColor updates composited layer mappings.
        // That DCHECKs IsAllowedToQueryCompositingState which requires
        // DocumentLifecycle >= kInCompositingUpdate.
        frame_view->UpdateLifecycleToCompositingInputsClean();
        original_base_background_color_ = frame_view->BaseBackgroundColor();
        frame_view->SetBaseBackgroundColor(Color::kTransparent);

        // In DOM overlay mode, entering fullscreen mode needs to be triggered
        // from the Renderer by actually fullscreening an element. If there
        // is no current fullscreen element, fullscreen the <body> element
        // for now. The JS application can use enterFullscreen to change this.
        //
        // A TabObserver on the browser side exits the session if there's
        // no longer a fullscreen element, for example if the JS app manually
        // unfullscreens the "body" element. That ensures we don't end up in a
        // hybrid non-fullscreen AR state.
        Element* fullscreen_element = Fullscreen::FullscreenElementFrom(*doc);
        if (!fullscreen_element) {
          Element* body = doc->body();
          DCHECK(body);
          // FIXME: this is the "prefixed" version that doesn't generate a
          // fullscreenchange event and auto-hides navigation bars. Should the
          // event be generated?
          Fullscreen::RequestFullscreen(*body);
        }
      }
    }

    if (query->mode() == XRSession::kModeImmersiveVR &&
        session->UsesInputEventing()) {
      frameProvider()->GetImmersiveDataProvider()->SetInputSourceButtonListener(
          session->GetInputClickListener());
    }
  }

  UseCounter::Count(ExecutionContext::From(query->GetScriptState()),
                    WebFeature::kWebXrSessionCreated);

  query->Resolve(session, std::move(metrics_recorder));
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

  if (!service_)
    return;

  if (event_type == event_type_names::kDevicechange) {
    // Register for notifications if we haven't already.
    //
    // See https://bit.ly/2S0zRAS for task types.
    auto task_runner =
        GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
    if (!receiver_.is_bound())
      service_->SetClient(receiver_.BindNewPipeAndPassRemote(task_runner));
  }
}

void XR::ContextDestroyed(ExecutionContext*) {
  Dispose(DisposeType::kContextDestroyed);
}

// A session is always created and returned.
XRSession* XR::CreateSession(
    XRSession::SessionMode mode,
    XRSession::EnvironmentBlendMode blend_mode,
    mojo::PendingReceiver<device::mojom::blink::XRSessionClient>
        client_receiver,
    device::mojom::blink::VRDisplayInfoPtr display_info,
    bool uses_input_eventing,
    XRSessionFeatureSet enabled_features,
    bool sensorless_session) {
  XRSession* session = MakeGarbageCollected<XRSession>(
      this, std::move(client_receiver), mode, blend_mode, uses_input_eventing,
      sensorless_session, std::move(enabled_features));
  if (display_info)
    session->SetXRDisplayInfo(std::move(display_info));
  sessions_.insert(session);
  return session;
}

XRSession* XR::CreateSensorlessInlineSession() {
  // TODO(https://crbug.com/944936): The blend mode could be "additive".
  XRSession::EnvironmentBlendMode blend_mode = XRSession::kBlendModeOpaque;
  return CreateSession(XRSession::kModeInline, blend_mode,
                       mojo::NullReceiver() /* client receiver */,
                       nullptr /* display_info */,
                       false /* uses_input_eventing */,
                       {device::mojom::XRSessionFeature::REF_SPACE_VIEWER},
                       true /* sensorless_session */);
}

void XR::Dispose(DisposeType dispose_type) {
  switch (dispose_type) {
    case DisposeType::kContextDestroyed:
      is_context_destroyed_ = true;
      break;
    case DisposeType::kDisconnected:
      // nothing to do
      break;
  }

  // If the document context was destroyed, shut down the client connection
  // and never call the mojo service again.
  service_.reset();
  receiver_.reset();

  // Shutdown frame provider, which manages the message pipes.
  if (frame_provider_)
    frame_provider_->Dispose();

  HeapHashSet<Member<PendingSupportsSessionQuery>> support_queries =
      outstanding_support_queries_;
  for (const auto& query : support_queries) {
    OnSupportsSessionReturned(query, false);
  }
  DCHECK(outstanding_support_queries_.IsEmpty());

  HeapHashSet<Member<PendingRequestSessionQuery>> request_queries =
      outstanding_request_queries_;
  for (const auto& query : request_queries) {
    // TODO(https://crbug.com/962991): The spec should specify
    // what is returned here.
    OnRequestSessionReturned(
        query, device::mojom::blink::RequestSessionResult::NewFailureReason(
                   device::mojom::RequestSessionError::INVALID_CLIENT));
  }
  DCHECK(outstanding_support_queries_.IsEmpty());
}

void XR::OnEnvironmentProviderDisconnect() {
  for (auto& callback : environment_provider_error_callbacks_) {
    std::move(callback).Run();
  }

  environment_provider_error_callbacks_.clear();
  environment_provider_.reset();
}

void XR::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_provider_);
  visitor->Trace(sessions_);
  visitor->Trace(outstanding_support_queries_);
  visitor->Trace(outstanding_request_queries_);
  ContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
