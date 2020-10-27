// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_system.h"

#include <utility>

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fullscreen_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/fullscreen/scoped_allow_fullscreen.h"
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
    "Failed to execute '%s' on 'XRSystem': The provided value 'immersive-ar' "
    "is not a valid enum value of type XRSessionMode.";

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

device::mojom::blink::XRSessionMode stringToSessionMode(
    const String& mode_string) {
  if (mode_string == "inline") {
    return device::mojom::blink::XRSessionMode::kInline;
  }
  if (mode_string == "immersive-vr") {
    return device::mojom::blink::XRSessionMode::kImmersiveVr;
  }
  if (mode_string == "immersive-ar") {
    return device::mojom::blink::XRSessionMode::kImmersiveAr;
  }

  NOTREACHED();  // Only strings in the enum are allowed by IDL.
  return device::mojom::blink::XRSessionMode::kInline;
}

const char* SessionModeToString(device::mojom::blink::XRSessionMode mode) {
  switch (mode) {
    case device::mojom::blink::XRSessionMode::kInline:
      return "inline";
    case device::mojom::blink::XRSessionMode::kImmersiveVr:
      return "immersive-vr";
    case device::mojom::blink::XRSessionMode::kImmersiveAr:
      return "immersive-ar";
  }

  NOTREACHED();
  return "";
}

// Converts the given string to an XRSessionFeature. If the string is
// unrecognized, returns nullopt. Based on the spec:
// https://immersive-web.github.io/webxr/#feature-name
base::Optional<device::mojom::XRSessionFeature> StringToXRSessionFeature(
    const ExecutionContext* context,
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
  } else if (RuntimeEnabledFeatures::WebXRHitTestEnabled(context) &&
             feature_string == "hit-test") {
    return device::mojom::XRSessionFeature::HIT_TEST;
  } else if (RuntimeEnabledFeatures::WebXRAnchorsEnabled(context) &&
             feature_string == "anchors") {
    return device::mojom::XRSessionFeature::ANCHORS;
  } else if (feature_string == "dom-overlay") {
    return device::mojom::XRSessionFeature::DOM_OVERLAY;
  } else if (RuntimeEnabledFeatures::WebXRLightEstimationEnabled(context) &&
             feature_string == "light-estimation") {
    return device::mojom::XRSessionFeature::LIGHT_ESTIMATION;
  } else if (RuntimeEnabledFeatures::WebXRCameraAccessEnabled(context) &&
             feature_string == "camera-access") {
    return device::mojom::XRSessionFeature::CAMERA_ACCESS;
  } else if (RuntimeEnabledFeatures::WebXRPlaneDetectionEnabled(context) &&
             feature_string == "plane-detection") {
    return device::mojom::XRSessionFeature::PLANE_DETECTION;
  } else if (RuntimeEnabledFeatures::WebXRDepthEnabled(context) &&
             feature_string == "depth-sensing") {
    return device::mojom::XRSessionFeature::DEPTH;
  }

  return base::nullopt;
}

bool IsFeatureValidForMode(device::mojom::XRSessionFeature feature,
                           device::mojom::blink::XRSessionMode mode,
                           XRSessionInit* session_init,
                           ExecutionContext* execution_context,
                           mojom::blink::ConsoleMessageLevel error_level) {
  switch (feature) {
    case device::mojom::XRSessionFeature::REF_SPACE_VIEWER:
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL:
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
      return true;
    case device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
    case device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED:
    case device::mojom::XRSessionFeature::HIT_TEST:
    case device::mojom::XRSessionFeature::ANCHORS:
      return mode == device::mojom::blink::XRSessionMode::kImmersiveVr ||
             mode == device::mojom::blink::XRSessionMode::kImmersiveAr;
    case device::mojom::XRSessionFeature::DOM_OVERLAY:
      if (mode != device::mojom::blink::XRSessionMode::kImmersiveAr)
        return false;
      if (!session_init->hasDomOverlay()) {
        execution_context->AddConsoleMessage(MakeGarbageCollected<
                                             ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript, error_level,
            "Must specify a valid domOverlay.root element in XRSessionInit"));
        return false;
      }
      return true;
    case device::mojom::XRSessionFeature::LIGHT_ESTIMATION:
    case device::mojom::XRSessionFeature::CAMERA_ACCESS:
    case device::mojom::XRSessionFeature::PLANE_DETECTION:
    case device::mojom::XRSessionFeature::DEPTH:
      // Fallthrough - light estimation, camera access, plane detection and
      // depth APIs are all valid only for immersive AR mode for now.
      return mode == device::mojom::blink::XRSessionMode::kImmersiveAr;
  }
}

bool HasRequiredFeaturePolicy(const ExecutionContext* context,
                              device::mojom::XRSessionFeature feature) {
  if (!context)
    return false;

  switch (feature) {
    case device::mojom::XRSessionFeature::REF_SPACE_VIEWER:
      return true;
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL:
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
    case device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
    case device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED:
    case device::mojom::XRSessionFeature::DOM_OVERLAY:
    case device::mojom::XRSessionFeature::HIT_TEST:
    case device::mojom::XRSessionFeature::LIGHT_ESTIMATION:
    case device::mojom::XRSessionFeature::ANCHORS:
    case device::mojom::XRSessionFeature::CAMERA_ACCESS:
    case device::mojom::XRSessionFeature::PLANE_DETECTION:
    case device::mojom::XRSessionFeature::DEPTH:
      return context->IsFeatureEnabled(
          mojom::blink::FeaturePolicyFeature::kWebXr,
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

// Helper method to convert the mojom error code into text for displaying in the
// console. The console message will have the format of:
// "Could not create a session because: <this value>"
const char* GetConsoleMessage(device::mojom::RequestSessionError error) {
  switch (error) {
    case device::mojom::RequestSessionError::EXISTING_IMMERSIVE_SESSION:
      return "There is already an existing immersive session";
    case device::mojom::RequestSessionError::INVALID_CLIENT:
      return "An error occurred while querying for runtime support";
    case device::mojom::RequestSessionError::USER_DENIED_CONSENT:
      return "The user denied some part of the requested configuration";
    case device::mojom::RequestSessionError::NO_RUNTIME_FOUND:
      return "No runtimes supported the requested configuration";
    case device::mojom::RequestSessionError::UNKNOWN_RUNTIME_ERROR:
      return "Something went wrong initializing the session in the runtime";
    case device::mojom::RequestSessionError::RUNTIME_INSTALL_FAILURE:
      return "The runtime for this configuration could not be installed";
    case device::mojom::RequestSessionError::RUNTIMES_CHANGED:
      return "The supported runtimes changed while initializing the session";
    case device::mojom::RequestSessionError::FULLSCREEN_ERROR:
      return "An error occurred while initializing fullscreen support";
    case device::mojom::RequestSessionError::UNKNOWN_FAILURE:
      return "An unknown error occurred";
  }
}

bool IsFeatureRequested(
    device::mojom::XRSessionFeatureRequestStatus requestStatus) {
  switch (requestStatus) {
    case device::mojom::XRSessionFeatureRequestStatus::kOptionalAccepted:
    case device::mojom::XRSessionFeatureRequestStatus::kRequired:
      return true;
    case device::mojom::XRSessionFeatureRequestStatus::kNotRequested:
    case device::mojom::XRSessionFeatureRequestStatus::kOptionalRejected:
      return false;
  }
}

}  // namespace

// Ensure that the inline session request is allowed, if not
// return which security error occurred.
// https://immersive-web.github.io/webxr/#inline-session-request-is-allowed
const char* XRSystem::CheckInlineSessionRequestAllowed(
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

XRSystem::PendingSupportsSessionQuery::PendingSupportsSessionQuery(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::XRSessionMode session_mode,
    bool throw_on_unsupported)
    : resolver_(resolver),
      mode_(session_mode),
      throw_on_unsupported_(throw_on_unsupported) {}

void XRSystem::PendingSupportsSessionQuery::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
}

void XRSystem::PendingSupportsSessionQuery::Resolve(
    bool supported,
    ExceptionState* exception_state) {
  if (throw_on_unsupported_) {
    if (supported) {
      resolver_->Resolve();
    } else {
      DVLOG(2) << __func__ << ": session is unsupported - throwing exception";
      RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                             kSessionNotSupported, exception_state);
    }
  } else {
    resolver_->Resolve(supported);
  }
}

void XRSystem::PendingSupportsSessionQuery::RejectWithDOMException(
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

void XRSystem::PendingSupportsSessionQuery::RejectWithSecurityError(
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

void XRSystem::PendingSupportsSessionQuery::RejectWithTypeError(
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

device::mojom::blink::XRSessionMode
XRSystem::PendingSupportsSessionQuery::mode() const {
  return mode_;
}

XRSystem::PendingRequestSessionQuery::PendingRequestSessionQuery(
    int64_t ukm_source_id,
    ScriptPromiseResolver* resolver,
    device::mojom::blink::XRSessionMode session_mode,
    RequestedXRSessionFeatureSet required_features,
    RequestedXRSessionFeatureSet optional_features)
    : resolver_(resolver),
      mode_(session_mode),
      required_features_(std::move(required_features)),
      optional_features_(std::move(optional_features)),
      ukm_source_id_(ukm_source_id) {
  ParseSensorRequirement();
}

void XRSystem::PendingRequestSessionQuery::Resolve(
    XRSession* session,
    mojo::PendingRemote<device::mojom::blink::XRSessionMetricsRecorder>
        metrics_recorder) {
  resolver_->Resolve(session);
  ReportRequestSessionResult(SessionRequestStatus::kSuccess, session,
                             std::move(metrics_recorder));
}

void XRSystem::PendingRequestSessionQuery::RejectWithDOMException(
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

void XRSystem::PendingRequestSessionQuery::RejectWithSecurityError(
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

void XRSystem::PendingRequestSessionQuery::RejectWithTypeError(
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
XRSystem::PendingRequestSessionQuery::GetFeatureRequestStatus(
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

void XRSystem::PendingRequestSessionQuery::ReportRequestSessionResult(
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
  auto feature_request_dom_overlay =
      GetFeatureRequestStatus(XRSessionFeature::DOM_OVERLAY, session);
  auto feature_request_depth_sensing =
      GetFeatureRequestStatus(XRSessionFeature::DEPTH, session);

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

  // If the session was successfully created and DOM overlay was requested,
  // count this as a use of the DOM overlay feature.
  if (session && status == SessionRequestStatus::kSuccess &&
      IsFeatureRequested(feature_request_dom_overlay)) {
    DVLOG(2) << __func__ << ": DOM overlay was requested, logging a UseCounter";
    UseCounter::Count(session->GetExecutionContext(),
                      WebFeature::kXRDOMOverlay);
  }

  // If the session was successfully created and depth-sensing was requested,
  // count this as a use of depth sensing feature.
  if (session && status == SessionRequestStatus::kSuccess &&
      IsFeatureRequested(feature_request_depth_sensing)) {
    DVLOG(2) << __func__
             << ": depth sensing was requested, logging a UseCounter";
    UseCounter::Count(session->GetExecutionContext(),
                      WebFeature::kXRDepthSensing);
  }

  if (session && metrics_recorder) {
    mojo::Remote<device::mojom::blink::XRSessionMetricsRecorder> recorder(
        std::move(metrics_recorder));
    session->SetMetricsReporter(
        std::make_unique<XRSession::MetricsReporter>(std::move(recorder)));
  }
}

device::mojom::blink::XRSessionMode XRSystem::PendingRequestSessionQuery::mode()
    const {
  return mode_;
}

const XRSessionFeatureSet&
XRSystem::PendingRequestSessionQuery::RequiredFeatures() const {
  return required_features_.valid_features;
}

const XRSessionFeatureSet&
XRSystem::PendingRequestSessionQuery::OptionalFeatures() const {
  return optional_features_.valid_features;
}

bool XRSystem::PendingRequestSessionQuery::HasFeature(
    device::mojom::XRSessionFeature feature) const {
  return RequiredFeatures().Contains(feature) ||
         OptionalFeatures().Contains(feature);
}

bool XRSystem::PendingRequestSessionQuery::InvalidRequiredFeatures() const {
  return required_features_.invalid_features;
}

bool XRSystem::PendingRequestSessionQuery::InvalidOptionalFeatures() const {
  return optional_features_.invalid_features;
}

ScriptState* XRSystem::PendingRequestSessionQuery::GetScriptState() const {
  return resolver_->GetScriptState();
}

void XRSystem::PendingRequestSessionQuery::ParseSensorRequirement() {
  // All modes other than inline require sensors.
  if (mode_ != device::mojom::blink::XRSessionMode::kInline) {
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

void XRSystem::PendingRequestSessionQuery::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
  visitor->Trace(dom_overlay_element_);
}

XRSystem::OverlayFullscreenEventManager::OverlayFullscreenEventManager(
    XRSystem* xr,
    XRSystem::PendingRequestSessionQuery* query,
    device::mojom::blink::RequestSessionResultPtr result)
    : xr_(xr), query_(query), result_(std::move(result)) {
  DVLOG(2) << __func__;
}

XRSystem::OverlayFullscreenEventManager::~OverlayFullscreenEventManager() =
    default;

void XRSystem::OverlayFullscreenEventManager::Invoke(
    ExecutionContext* execution_context,
    Event* event) {
  DVLOG(2) << __func__ << ": event type=" << event->type();

  // This handler should only be called once, it's unregistered after use.
  DCHECK(query_);
  DCHECK(result_);

  Element* element = query_->DOMOverlayElement();
  element->GetDocument().removeEventListener(
      event_type_names::kFullscreenchange, this, true);
  element->GetDocument().removeEventListener(event_type_names::kFullscreenerror,
                                             this, true);

  if (event->type() == event_type_names::kFullscreenchange) {
    // Succeeded, proceed with session creation.
    element->GetDocument().GetViewportData().SetExpandIntoDisplayCutout(true);
    xr_->OnRequestSessionReturned(query_, std::move(result_));
  }

  if (event->type() == event_type_names::kFullscreenerror) {
    // Failed, reject the session
    xr_->OnRequestSessionReturned(
        query_, device::mojom::blink::RequestSessionResult::NewFailureReason(
                    device::mojom::RequestSessionError::FULLSCREEN_ERROR));
  }
}

void XRSystem::OverlayFullscreenEventManager::RequestFullscreen() {
  Element* element = query_->DOMOverlayElement();
  DCHECK(element);

  bool wait_for_fullscreen_change = true;

  if (element == Fullscreen::FullscreenElementFrom(element->GetDocument())) {
    // It's possible that the requested element is already fullscreen, in which
    // case we must not wait for a fullscreenchange event since it won't arrive.
    // This can happen if the site used Fullscreen API to place the element into
    // fullscreen mode before requesting the session, and if the session can
    // proceed without needing a permission prompt. (Showing a dialog exits
    // fullscreen mode.)
    //
    // We still need to do the RequestFullscreen call to apply the kForXrOverlay
    // property which sets the background transparent.
    DVLOG(2) << __func__ << ": requested element already fullscreen";
    wait_for_fullscreen_change = false;
  }

  if (wait_for_fullscreen_change) {
    // Set up event listeners for success and failure.
    element->GetDocument().addEventListener(event_type_names::kFullscreenchange,
                                            this, true);
    element->GetDocument().addEventListener(event_type_names::kFullscreenerror,
                                            this, true);
  }

  // Use the event-generating unprefixed version of RequestFullscreen to ensure
  // that the fullscreen event listener is informed once this completes.
  FullscreenOptions* options = FullscreenOptions::Create();
  options->setNavigationUI("hide");

  // Grant fullscreen API permission for the following call. Requesting the
  // immersive session had required a user activation state, but that may have
  // expired by now due to the user taking time to respond to the consent
  // prompt.
  ScopedAllowFullscreen scope(ScopedAllowFullscreen::kXrOverlay);

  Fullscreen::RequestFullscreen(*element, options,
                                FullscreenRequestType::kUnprefixed |
                                    FullscreenRequestType::kForXrOverlay);

  if (!wait_for_fullscreen_change) {
    // Element was already fullscreen, proceed with session creation.
    xr_->OnRequestSessionReturned(query_, std::move(result_));
  }
}

void XRSystem::OverlayFullscreenEventManager::Trace(Visitor* visitor) const {
  visitor->Trace(xr_);
  visitor->Trace(query_);
  EventListener::Trace(visitor);
}

XRSystem::OverlayFullscreenExitObserver::OverlayFullscreenExitObserver(
    XRSystem* xr)
    : xr_(xr) {
  DVLOG(2) << __func__;
}

XRSystem::OverlayFullscreenExitObserver::~OverlayFullscreenExitObserver() =
    default;

void XRSystem::OverlayFullscreenExitObserver::Invoke(
    ExecutionContext* execution_context,
    Event* event) {
  DVLOG(2) << __func__ << ": event type=" << event->type();

  document_->removeEventListener(event_type_names::kFullscreenchange, this,
                                 true);

  if (event->type() == event_type_names::kFullscreenchange) {
    // Succeeded, proceed with session shutdown. Expanding into the fullscreen
    // cutout is only valid for fullscreen mode which we just exited (cf.
    // MediaControlsDisplayCutoutDelegate::DidExitFullscreen), so we can
    // unconditionally turn this off here.
    document_->GetViewportData().SetExpandIntoDisplayCutout(false);
    xr_->ExitPresent(std::move(on_exited_));
  }
}

void XRSystem::OverlayFullscreenExitObserver::ExitFullscreen(
    Document* document,
    base::OnceClosure on_exited) {
  DVLOG(2) << __func__;
  document_ = document;
  on_exited_ = std::move(on_exited);

  document->addEventListener(event_type_names::kFullscreenchange, this, true);
  // "ua_originated" means that the browser process already exited
  // fullscreen. Set it to false because we need the browser process
  // to get notified that it needs to exit fullscreen. Use
  // FullyExitFullscreen to ensure that we return to non-fullscreen mode.
  // ExitFullscreen only unfullscreens a single element, potentially
  // leaving others in fullscreen mode.
  constexpr bool kUaOriginated = false;

  Fullscreen::FullyExitFullscreen(*document, kUaOriginated);
}

void XRSystem::OverlayFullscreenExitObserver::Trace(Visitor* visitor) const {
  visitor->Trace(xr_);
  visitor->Trace(document_);
  EventListener::Trace(visitor);
}

device::mojom::blink::XRSessionOptionsPtr XRSystem::XRSessionOptionsFromQuery(
    const PendingRequestSessionQuery& query) {
  device::mojom::blink::XRSessionOptionsPtr session_options =
      device::mojom::blink::XRSessionOptions::New();
  session_options->mode = query.mode();

  CopyToVector(query.RequiredFeatures(), session_options->required_features);
  CopyToVector(query.OptionalFeatures(), session_options->optional_features);

  return session_options;
}

const char XRSystem::kSupplementName[] = "XRSystem";

XRSystem* XRSystem::FromIfExists(Document& document) {
  if (!document.domWindow())
    return nullptr;
  return Supplement<Navigator>::From<XRSystem>(
      document.domWindow()->navigator());
}

XRSystem* XRSystem::From(Document& document) {
  return document.domWindow() ? xr(*document.domWindow()->navigator())
                              : nullptr;
}

XRSystem* XRSystem::xr(Navigator& navigator) {
  LocalDOMWindow* window = navigator.DomWindow();
  if (!window)
    return nullptr;

  XRSystem* xr = Supplement<Navigator>::From<XRSystem>(navigator);
  if (!xr) {
    xr = MakeGarbageCollected<XRSystem>(navigator);
    ProvideTo(navigator, xr);

    ukm::builders::XR_WebXR(window->UkmSourceID())
        .SetDidUseNavigatorXR(1)
        .Record(window->UkmRecorder());
  }
  return xr;
}

XRSystem::XRSystem(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      ExecutionContextLifecycleObserver(navigator.DomWindow()),
      FocusChangedObserver(navigator.DomWindow()->GetFrame()->GetPage()),
      service_(navigator.DomWindow()),
      environment_provider_(navigator.DomWindow()),
      receiver_(this, navigator.DomWindow()),
      navigation_start_(navigator.DomWindow()
                            ->document()
                            ->Loader()
                            ->GetTiming()
                            .NavigationStart()),
      feature_handle_for_scheduler_(
          navigator.DomWindow()
              ->GetFrame()
              ->GetFrameScheduler()
              ->RegisterFeature(
                  SchedulingPolicy::Feature::kWebXR,
                  {SchedulingPolicy::RecordMetricsForBackForwardCache()})) {}

void XRSystem::FocusedFrameChanged() {
  // Tell all sessions that focus changed.
  // Since this eventually dispatches an event to the page, the page could
  // create a new session which would invalidate our iterators; so iterate over
  // a copy of the session map.
  HeapHashSet<WeakMember<XRSession>> processing_sessions = sessions_;
  for (const auto& session : processing_sessions) {
    session->OnFocusChanged();
  }

  if (frame_provider_)
    frame_provider_->OnFocusChanged();
}

bool XRSystem::IsFrameFocused() {
  return FocusChangedObserver::IsFrameFocused(GetFrame());
}

ExecutionContext* XRSystem::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& XRSystem::InterfaceName() const {
  return event_target_names::kXR;
}

XRFrameProvider* XRSystem::frameProvider() {
  if (!frame_provider_) {
    frame_provider_ = MakeGarbageCollected<XRFrameProvider>(this);
  }

  return frame_provider_;
}

device::mojom::blink::XREnvironmentIntegrationProvider*
XRSystem::xrEnvironmentProviderRemote() {
  return environment_provider_.get();
}

void XRSystem::AddEnvironmentProviderErrorHandler(
    EnvironmentProviderErrorCallback callback) {
  environment_provider_error_callbacks_.push_back(std::move(callback));
}

void XRSystem::ExitPresent(base::OnceClosure on_exited) {
  DVLOG(1) << __func__;

  // If the document was potentially being shown in a DOM overlay via
  // fullscreened elements, make sure to clear any fullscreen states on exiting
  // the session. This avoids a race condition:
  // - browser side ends session and exits fullscreen (i.e. back button)
  // - renderer processes WebViewImpl::ExitFullscreen via ChromeClient
  // - JS application sets a new element to fullscreen, this is allowed
  //   because doc->IsXrOverlay() is still true at this point
  // - renderer processes XR session shutdown (this method)
  // - browser re-enters fullscreen unexpectedly
  LocalFrame* frame = GetFrame();
  if (frame) {
    Document* doc = frame->GetDocument();
    DCHECK(doc);
    DVLOG(3) << __func__ << ": doc->IsXrOverlay()=" << doc->IsXrOverlay();
    if (doc->IsXrOverlay()) {
      Element* fullscreen_element = Fullscreen::FullscreenElementFrom(*doc);
      DVLOG(3) << __func__ << ": fullscreen_element=" << fullscreen_element;

      // Restore the FrameView background color that was changed in
      // OnRequestSessionReturned. The layout view can be null on navigation.
      auto* layout_view = doc->GetLayoutView();
      if (layout_view) {
        auto* frame_view = layout_view->GetFrameView();
        // SetBaseBackgroundColor updates composited layer mappings.
        // That DCHECKs IsAllowedToQueryCompositingState which requires
        // DocumentLifecycle >= kInCompositingUpdate.
        frame_view->UpdateLifecycleToCompositingInputsClean(
            DocumentUpdateReason::kBaseColor);
        frame_view->SetBaseBackgroundColor(original_base_background_color_);
      }

      if (fullscreen_element) {
        fullscreen_exit_observer_ =
            MakeGarbageCollected<OverlayFullscreenExitObserver>(this);
        fullscreen_exit_observer_->ExitFullscreen(doc, std::move(on_exited));
        return;
      }
    }
  }

  if (service_.is_bound()) {
    service_->ExitPresent(std::move(on_exited));
  } else {
    // The service was already shut down, run the callback immediately.
    std::move(on_exited).Run();
  }
}

void XRSystem::SetFramesThrottled(const XRSession* session, bool throttled) {
  // The service only cares if the immersive session is throttling frames.
  if (session->immersive()) {
    // If we have an immersive session, we should have a service.
    DCHECK(service_.is_bound());
    service_->SetFramesThrottled(throttled);
  }
}

ScriptPromise XRSystem::supportsSession(ScriptState* script_state,
                                        const String& mode,
                                        ExceptionState& exception_state) {
  return InternalIsSessionSupported(script_state, mode, exception_state, true);
}

ScriptPromise XRSystem::isSessionSupported(ScriptState* script_state,
                                           const String& mode,
                                           ExceptionState& exception_state) {
  return InternalIsSessionSupported(script_state, mode, exception_state, false);
}

void XRSystem::AddConsoleMessage(mojom::blink::ConsoleMessageLevel error_level,
                                 const String& message) {
  DVLOG(2) << __func__ << ": error_level=" << error_level
           << ", message=" << message;

  GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript, error_level, message));
}

ScriptPromise XRSystem::InternalIsSessionSupported(
    ScriptState* script_state,
    const String& mode,
    ExceptionState& exception_state,
    bool throw_on_unsupported) {
  if (!GetExecutionContext()) {
    // Reject if the context is inaccessible.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kNavigatorDetachedError);
    return ScriptPromise();  // Will be rejected by generated bindings
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  device::mojom::blink::XRSessionMode session_mode = stringToSessionMode(mode);
  PendingSupportsSessionQuery* query =
      MakeGarbageCollected<PendingSupportsSessionQuery>(resolver, session_mode,
                                                        throw_on_unsupported);

  if (session_mode == device::mojom::blink::XRSessionMode::kImmersiveAr &&
      !RuntimeEnabledFeatures::WebXRARModuleEnabled(GetExecutionContext())) {
    DVLOG(2) << __func__
             << ": Immersive AR session is only supported if WebXRARModule "
                "feature is enabled";
    query->Resolve(false);
    return promise;
  }

  if (session_mode == device::mojom::blink::XRSessionMode::kInline) {
    // inline sessions are always supported.
    query->Resolve(true);
    return promise;
  }

  if (!GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::FeaturePolicyFeature::kWebXr,
          ReportOptions::kReportOnFailure)) {
    // Only allow the call to be made if the appropriate feature policy is in
    // place.
    query->RejectWithSecurityError(kFeaturePolicyBlocked, &exception_state);
    return promise;
  }

  // If TryEnsureService() doesn't set |service_|, then we don't have any WebXR
  // hardware, so we need to reject as being unsupported.
  TryEnsureService();
  if (!service_.is_bound()) {
    query->Resolve(false, &exception_state);
    return promise;
  }

  device::mojom::blink::XRSessionOptionsPtr session_options =
      device::mojom::blink::XRSessionOptions::New();
  session_options->mode = query->mode();

  outstanding_support_queries_.insert(query);
  service_->SupportsSession(
      std::move(session_options),
      WTF::Bind(&XRSystem::OnSupportsSessionReturned, WrapPersistent(this),
                WrapPersistent(query)));

  return promise;
}

void XRSystem::RequestImmersiveSession(LocalFrame* frame,
                                       Document* doc,
                                       PendingRequestSessionQuery* query,
                                       ExceptionState* exception_state) {
  DVLOG(2) << __func__;
  // Log an immersive session request if we haven't already
  if (!did_log_request_immersive_session_) {
    ukm::builders::XR_WebXR(doc->UkmSourceID())
        .SetDidRequestPresentation(1)
        .Record(doc->UkmRecorder());
    did_log_request_immersive_session_ = true;
  }

  // Make sure the request is allowed
  auto* immersive_session_request_error =
      CheckImmersiveSessionRequestAllowed(frame, doc);
  if (immersive_session_request_error) {
    DVLOG(2) << __func__
             << ": rejecting session - immersive session not allowed, reason: "
             << immersive_session_request_error;
    query->RejectWithSecurityError(immersive_session_request_error,
                                   exception_state);
    return;
  }

  // Ensure there are no other immersive sessions currently pending or active
  if (has_outstanding_immersive_request_ ||
      frameProvider()->immersive_session()) {
    DVLOG(2) << __func__
             << ": rejecting session - immersive session request is already "
                "pending or an immersive session is already active";
    query->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                  kActiveImmersiveSession, exception_state);
    return;
  }

  // If TryEnsureService() doesn't set |service_|, then we don't have any WebXR
  // hardware.
  TryEnsureService();
  if (!service_.is_bound()) {
    DVLOG(2) << __func__ << ": rejecting session - service is not bound";
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kNoDevicesMessage, exception_state);
    return;
  }

  // Reject session if any of the required features were invalid.
  if (query->InvalidRequiredFeatures()) {
    DVLOG(2) << __func__ << ": rejecting session - invalid required features";
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kSessionNotSupported, exception_state);
    return;
  }

  // Reworded from spec 'pending immersive session'
  has_outstanding_immersive_request_ = true;

  // Submit the request to VrServiceImpl in the Browser process
  outstanding_request_queries_.insert(query);
  auto session_options = XRSessionOptionsFromQuery(*query);

  // If using DOM Overlay, if we're already in fullscreen mode, and if this
  // frame has a remote ancestor (OOPIF with the ancestor in another process),
  // we need to exit and re-enter fullscreen mode to properly apply the
  // is_xr_overlay property. Request a fullscreen exit, and continue with
  // the session request once that completes.
  if (query->DOMOverlayElement() && Fullscreen::FullscreenElementFrom(*doc)) {
    bool has_remote_ancestor = false;
    for (Frame* f = GetFrame(); f; f = f->Tree().Parent()) {
      if (f->IsRemoteFrame()) {
        has_remote_ancestor = true;
        break;
      }
    }
    DVLOG(2) << __func__ << ": has_remote_ancestor=" << has_remote_ancestor;
    if (has_remote_ancestor) {
      fullscreen_exit_observer_ =
          MakeGarbageCollected<OverlayFullscreenExitObserver>(this);

      base::OnceClosure callback =
          WTF::Bind(&XRSystem::DoRequestSession, WrapWeakPersistent(this),
                    WrapPersistent(query), std::move(session_options));
      fullscreen_exit_observer_->ExitFullscreen(doc, std::move(callback));
      return;
    }
  }
  DoRequestSession(std::move(query), std::move(session_options));
}

void XRSystem::DoRequestSession(
    PendingRequestSessionQuery* query,
    device::mojom::blink::XRSessionOptionsPtr session_options) {
  // In DOM overlay mode, there's an additional step before an immersive-ar
  // session can start, we need to enter fullscreen mode by setting the
  // appropriate element as fullscreen from the Renderer, then waiting for the
  // browser side to send an event indicating success or failure.
  auto callback =
      query->DOMOverlayElement()
          ? WTF::Bind(&XRSystem::OnRequestSessionSetupForDomOverlay,
                      WrapWeakPersistent(this), WrapPersistent(query))
          : WTF::Bind(&XRSystem::OnRequestSessionReturned,
                      WrapWeakPersistent(this), WrapPersistent(query));
  service_->RequestSession(std::move(session_options), std::move(callback));
}

void XRSystem::RequestInlineSession(LocalFrame* frame,
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
    DVLOG(2) << __func__ << ": rejecting session - invalid required features";
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kSessionNotSupported, exception_state);
    return;
  }

  auto sensor_requirement = query->GetSensorRequirement();

  // Try to get the service now. If we can't get it, then we know that we can
  // only support a sensorless session. But if we *can* get it, then we need to
  // check if we have any hardware that supports the requested features.
  TryEnsureService();

  // If no sensors are requested, or if we don't have a service and sensors are
  // not required, then just create a sensorless session.
  if (sensor_requirement == SensorRequirement::kNone ||
      (!service_.is_bound() &&
       sensor_requirement != SensorRequirement::kRequired)) {
    query->Resolve(CreateSensorlessInlineSession());
    return;
  }

  // If we don't have a service, then we don't have any WebXR hardware.
  // If we didn't already create a sensorless session, we can't create a session
  // without hardware, so just reject now.
  if (!service_.is_bound()) {
    DVLOG(2) << __func__ << ": rejecting session - no service";
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kSessionNotSupported, exception_state);
    return;
  }

  // Submit the request to VrServiceImpl in the Browser process
  outstanding_request_queries_.insert(query);
  auto session_options = XRSessionOptionsFromQuery(*query);
  service_->RequestSession(
      std::move(session_options),
      WTF::Bind(&XRSystem::OnRequestSessionReturned, WrapWeakPersistent(this),
                WrapPersistent(query)));
}

XRSystem::RequestedXRSessionFeatureSet XRSystem::ParseRequestedFeatures(
    const HeapVector<ScriptValue>& features,
    const device::mojom::blink::XRSessionMode& session_mode,
    XRSessionInit* session_init,
    mojom::blink::ConsoleMessageLevel error_level) {
  DVLOG(2) << __func__ << ": features.size()=" << features.size()
           << ", session_mode=" << session_mode;
  RequestedXRSessionFeatureSet result;

  // Iterate over all requested features, even if intermediate
  // elements are found to be invalid.
  for (const auto& feature : features) {
    String feature_string;
    if (feature.ToString(feature_string)) {
      auto feature_enum =
          StringToXRSessionFeature(GetExecutionContext(), feature_string);

      if (!feature_enum) {
        AddConsoleMessage(error_level,
                          "Unrecognized feature requested: " + feature_string);
        result.invalid_features = true;
      } else if (!IsFeatureValidForMode(feature_enum.value(), session_mode,
                                        session_init, GetExecutionContext(),
                                        error_level)) {
        AddConsoleMessage(error_level, "Feature '" + feature_string +
                                           "' is not supported for mode: " +
                                           SessionModeToString(session_mode));
        result.invalid_features = true;
      } else if (!HasRequiredFeaturePolicy(GetExecutionContext(),
                                           feature_enum.value())) {
        AddConsoleMessage(error_level,
                          "Feature '" + feature_string +
                              "' is not permitted by feature policy");
        result.invalid_features = true;
      } else {
        DVLOG(3) << __func__ << ": Adding feature " << feature_string
                 << " to valid_features.";
        result.valid_features.insert(feature_enum.value());
      }
    } else {
      AddConsoleMessage(error_level, "Unrecognized feature value");
      result.invalid_features = true;
    }
  }

  DVLOG(2) << __func__
           << ": result.invalid_features=" << result.invalid_features
           << ", result.valid_features.size()=" << result.valid_features.size();
  return result;
}

ScriptPromise XRSystem::requestSession(ScriptState* script_state,
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

  device::mojom::blink::XRSessionMode session_mode = stringToSessionMode(mode);

  // If the request is for immersive-ar, ensure that feature is enabled.
  if (session_mode == device::mojom::blink::XRSessionMode::kImmersiveAr &&
      !RuntimeEnabledFeatures::WebXRARModuleEnabled(GetExecutionContext())) {
    exception_state.ThrowTypeError(
        String::Format(kImmersiveArModeNotValid, "requestSession"));

    // We haven't created the query yet, so we can't use it to implicitly log
    // our metrics for us, so explicitly log it here, as the query requires the
    // features to be parsed before it can be built.
    ukm::builders::XR_WebXR_SessionRequest(doc->UkmSourceID())
        .SetMode(static_cast<int64_t>(session_mode))
        .SetStatus(static_cast<int64_t>(SessionRequestStatus::kOtherError))
        .Record(doc->UkmRecorder());
    return ScriptPromise();
  }

  // Parse required feature strings
  RequestedXRSessionFeatureSet required_features;
  if (session_init && session_init->hasRequiredFeatures()) {
    required_features = ParseRequestedFeatures(
        session_init->requiredFeatures(), session_mode, session_init,
        mojom::blink::ConsoleMessageLevel::kError);
  }

  // Parse optional feature strings
  RequestedXRSessionFeatureSet optional_features;
  if (session_init && session_init->hasOptionalFeatures()) {
    optional_features = ParseRequestedFeatures(
        session_init->optionalFeatures(), session_mode, session_init,
        mojom::blink::ConsoleMessageLevel::kWarning);
  }

  // Certain session modes imply default features.
  // Add those default features as required features now.
  base::span<const device::mojom::XRSessionFeature> default_features;
  switch (session_mode) {
    case device::mojom::blink::XRSessionMode::kImmersiveVr:
      default_features = kDefaultImmersiveVrFeatures;
      break;
    case device::mojom::blink::XRSessionMode::kImmersiveAr:
      default_features = kDefaultImmersiveArFeatures;
      break;
    case device::mojom::blink::XRSessionMode::kInline:
      default_features = kDefaultInlineFeatures;
      break;
  }

  for (const auto& feature : default_features) {
    if (HasRequiredFeaturePolicy(GetExecutionContext(), feature)) {
      required_features.valid_features.insert(feature);
    } else {
      DVLOG(2) << __func__
               << ": feature policy not satisfied for a default feature: "
               << feature;
      required_features.invalid_features = true;
    }
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  PendingRequestSessionQuery* query =
      MakeGarbageCollected<PendingRequestSessionQuery>(
          doc->UkmSourceID(), resolver, session_mode,
          std::move(required_features), std::move(optional_features));

  if (query->HasFeature(device::mojom::XRSessionFeature::DOM_OVERLAY)) {
    // Prerequisites were checked by IsFeatureValidForMode and IDL.
    DCHECK(session_init);
    DCHECK(session_init->hasDomOverlay());
    DCHECK(session_init->domOverlay()->hasRoot()) << "required in IDL";
    query->SetDOMOverlayElement(session_init->domOverlay()->root());
  }

  // The various session request methods may have other checks that would reject
  // before needing to create the vr service, so we don't try to create it here.
  switch (session_mode) {
    case device::mojom::blink::XRSessionMode::kImmersiveVr:
    case device::mojom::blink::XRSessionMode::kImmersiveAr:
      RequestImmersiveSession(frame, doc, query, &exception_state);
      break;
    case device::mojom::blink::XRSessionMode::kInline:
      RequestInlineSession(frame, query, &exception_state);
      break;
  }

  return promise;
}

void XRSystem::MakeXrCompatibleAsync(
    device::mojom::blink::VRService::MakeXrCompatibleCallback callback) {
  if (!GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::FeaturePolicyFeature::kWebXr)) {
    std::move(callback).Run(
        device::mojom::XrCompatibleResult::kWebXrFeaturePolicyBlocked);
    return;
  }

  TryEnsureService();
  if (service_.is_bound()) {
    service_->MakeXrCompatible(std::move(callback));
  } else {
    std::move(callback).Run(
        device::mojom::XrCompatibleResult::kNoDeviceAvailable);
  }
}

void XRSystem::MakeXrCompatibleSync(
    device::mojom::XrCompatibleResult* xr_compatible_result) {
  if (!GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::FeaturePolicyFeature::kWebXr)) {
    *xr_compatible_result =
        device::mojom::XrCompatibleResult::kWebXrFeaturePolicyBlocked;
    return;
  }
  *xr_compatible_result = device::mojom::XrCompatibleResult::kNoDeviceAvailable;

  TryEnsureService();
  if (service_.is_bound())
    service_->MakeXrCompatible(xr_compatible_result);
}

// This will be called when the XR hardware or capabilities have potentially
// changed. For example, if a new physical device was connected to the system,
// it might be able to support immersive sessions, where it couldn't before.
void XRSystem::OnDeviceChanged() {
  ExecutionContext* context = GetExecutionContext();
  if (context &&
      context->IsFeatureEnabled(mojom::blink::FeaturePolicyFeature::kWebXr)) {
    DispatchEvent(*blink::Event::Create(event_type_names::kDevicechange));
  }
}

void XRSystem::OnSupportsSessionReturned(PendingSupportsSessionQuery* query,
                                         bool supports_session) {
  // The session query has returned and we're about to resolve or reject the
  // promise, so remove it from our outstanding list.
  DCHECK(outstanding_support_queries_.Contains(query));
  outstanding_support_queries_.erase(query);
  query->Resolve(supports_session);
}

void XRSystem::OnRequestSessionSetupForDomOverlay(
    PendingRequestSessionQuery* query,
    device::mojom::blink::RequestSessionResultPtr result) {
  DCHECK(query->DOMOverlayElement());
  if (result->is_success()) {
    // Success. Now request fullscreen mode and continue with
    // OnRequestSessionReturned once that completes.
    fullscreen_event_manager_ =
        MakeGarbageCollected<OverlayFullscreenEventManager>(this, query,
                                                            std::move(result));
    fullscreen_event_manager_->RequestFullscreen();
  } else {
    // Session request failed, continue processing that normally.
    OnRequestSessionReturned(query, std::move(result));
  }
}

void XRSystem::OnRequestSessionReturned(
    PendingRequestSessionQuery* query,
    device::mojom::blink::RequestSessionResultPtr result) {
  // The session query has returned and we're about to resolve or reject the
  // promise, so remove it from our outstanding list.
  DCHECK(outstanding_request_queries_.Contains(query));
  outstanding_request_queries_.erase(query);
  if (query->mode() == device::mojom::blink::XRSessionMode::kImmersiveVr ||
      query->mode() == device::mojom::blink::XRSessionMode::kImmersiveAr) {
    DCHECK(has_outstanding_immersive_request_);
    has_outstanding_immersive_request_ = false;
  }

  // Clean up the fullscreen event manager which may have been added for
  // DOM overlay setup. We're done with it, and it contains a reference
  // to the query and the DOM overlay element.
  fullscreen_event_manager_ = nullptr;

  if (!result->is_success()) {
    // |service_| does not support the requested mode. Attempt to create a
    // sensorless session.
    if (query->GetSensorRequirement() != SensorRequirement::kRequired) {
      DVLOG(2) << __func__ << ": session creation failed - creating sensorless";
      XRSession* session = CreateSensorlessInlineSession();
      query->Resolve(session);
      return;
    }

    String error_message =
        String::Format("Could not create a session because: %s",
                       GetConsoleMessage(result->get_failure_reason()));
    AddConsoleMessage(mojom::blink::ConsoleMessageLevel::kError, error_message);
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kSessionNotSupported, nullptr);
    return;
  }

  auto session_ptr = std::move(result->get_success()->session);
  auto metrics_recorder = std::move(result->get_success()->metrics_recorder);

  bool environment_integration =
      query->mode() == device::mojom::blink::XRSessionMode::kImmersiveAr;

  // immersive sessions must supply display info.
  DCHECK(session_ptr->display_info);
  DVLOG(2) << __func__
           << ": environment_integration=" << environment_integration;

  XRSessionFeatureSet enabled_features;
  for (const auto& feature : session_ptr->enabled_features) {
    DVLOG(2) << __func__ << ": feature " << feature << " will be enabled";
    enabled_features.insert(feature);
  }

  XRSession* session = CreateSession(
      query->mode(), session_ptr->enviroment_blend_mode,
      session_ptr->interaction_mode, std::move(session_ptr->client_receiver),
      std::move(session_ptr->display_info),
      std::move(session_ptr->device_config), enabled_features);

  frameProvider()->OnSessionStarted(session, std::move(session_ptr));

  if (query->mode() == device::mojom::blink::XRSessionMode::kImmersiveVr ||
      query->mode() == device::mojom::blink::XRSessionMode::kImmersiveAr) {
    if (environment_integration) {
      // See Task Sources spreadsheet for more information:
      // https://docs.google.com/spreadsheets/d/1b-dus1Ug3A8y0lX0blkmOjJILisUASdj8x9YN_XMwYc/view
      frameProvider()
          ->GetImmersiveDataProvider()
          ->GetEnvironmentIntegrationProvider(
              environment_provider_.BindNewEndpointAndPassReceiver(
                  GetExecutionContext()->GetTaskRunner(
                      TaskType::kMiscPlatformAPI)));
      environment_provider_.set_disconnect_handler(
          WTF::Bind(&XRSystem::OnEnvironmentProviderDisconnect,
                    WrapWeakPersistent(this)));

      session->OnEnvironmentProviderCreated();

      LocalFrame* frame = GetFrame();
      DCHECK(frame);

      if (query->HasFeature(device::mojom::XRSessionFeature::DOM_OVERLAY)) {
        DCHECK(query->DOMOverlayElement());
        // The session is using DOM overlay mode. At this point the overlay
        // element is already in fullscreen mode, and the session can
        // proceed.
        Document* doc = frame->GetDocument();
        DCHECK(doc);
        session->SetDOMOverlayElement(query->DOMOverlayElement());

        // Save the current base background color (restored in ExitPresent),
        // and set a transparent background for the FrameView.
        auto* frame_view = doc->GetLayoutView()->GetFrameView();
        // SetBaseBackgroundColor updates composited layer mappings.
        // That DCHECKs IsAllowedToQueryCompositingState which requires
        // DocumentLifecycle >= kInCompositingUpdate.
        frame_view->UpdateLifecycleToCompositingInputsClean(
            DocumentUpdateReason::kBaseColor);
        original_base_background_color_ = frame_view->BaseBackgroundColor();
        frame_view->SetBaseBackgroundColor(Color::kTransparent);
      }
    }

    if (query->mode() == device::mojom::blink::XRSessionMode::kImmersiveVr &&
        session->UsesInputEventing()) {
      frameProvider()->GetImmersiveDataProvider()->SetInputSourceButtonListener(
          session->GetInputClickListener());
    }
  }

  UseCounter::Count(ExecutionContext::From(query->GetScriptState()),
                    WebFeature::kWebXrSessionCreated);

  query->Resolve(session, std::move(metrics_recorder));
}

void XRSystem::ReportImmersiveSupported(bool supported) {
  Document* doc = GetFrame() ? GetFrame()->GetDocument() : nullptr;
  if (doc && !did_log_supports_immersive_ && supported) {
    ukm::builders::XR_WebXR ukm_builder(doc->UkmSourceID());
    ukm_builder.SetReturnedPresentationCapableDevice(1);
    ukm_builder.Record(doc->UkmRecorder());
    did_log_supports_immersive_ = true;
  }
}

void XRSystem::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);

  // If we're adding an event listener we should spin up the service, if we can,
  // so that we can actually register for notifications.
  TryEnsureService();
  if (!service_.is_bound())
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

void XRSystem::ContextDestroyed() {
  Dispose(DisposeType::kContextDestroyed);
}

// A session is always created and returned.
XRSession* XRSystem::CreateSession(
    device::mojom::blink::XRSessionMode mode,
    device::mojom::blink::XREnvironmentBlendMode blend_mode,
    device::mojom::blink::XRInteractionMode interaction_mode,
    mojo::PendingReceiver<device::mojom::blink::XRSessionClient>
        client_receiver,
    device::mojom::blink::VRDisplayInfoPtr display_info,
    device::mojom::blink::XRSessionDeviceConfigPtr device_config,
    XRSessionFeatureSet enabled_features,
    bool sensorless_session) {
  XRSession* session = MakeGarbageCollected<XRSession>(
      this, std::move(client_receiver), mode, blend_mode, interaction_mode,
      std::move(device_config), sensorless_session,
      std::move(enabled_features));
  if (display_info)
    session->SetXRDisplayInfo(std::move(display_info));
  sessions_.insert(session);
  return session;
}

XRSession* XRSystem::CreateSensorlessInlineSession() {
  // TODO(https://crbug.com/944936): The blend mode could be "additive".
  device::mojom::blink::XREnvironmentBlendMode blend_mode =
      device::mojom::blink::XREnvironmentBlendMode::kOpaque;
  device::mojom::blink::XRInteractionMode interaction_mode =
      device::mojom::blink::XRInteractionMode::kScreenSpace;
  device::mojom::blink::XRSessionDeviceConfigPtr device_config =
      device::mojom::blink::XRSessionDeviceConfig::New();
  return CreateSession(device::mojom::blink::XRSessionMode::kInline, blend_mode,
                       interaction_mode,
                       mojo::NullReceiver() /* client receiver */,
                       nullptr /* display_info */, std::move(device_config),
                       {device::mojom::XRSessionFeature::REF_SPACE_VIEWER},
                       true /* sensorless_session */);
}

void XRSystem::Dispose(DisposeType dispose_type) {
  switch (dispose_type) {
    case DisposeType::kContextDestroyed:
      is_context_destroyed_ = true;
      break;
    case DisposeType::kDisconnected:
      did_service_ever_disconnect_ = true;
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
    OnRequestSessionReturned(
        query, device::mojom::blink::RequestSessionResult::NewFailureReason(
                   device::mojom::RequestSessionError::INVALID_CLIENT));
  }
  DCHECK(outstanding_support_queries_.IsEmpty());
}

void XRSystem::OnEnvironmentProviderDisconnect() {
  for (auto& callback : environment_provider_error_callbacks_) {
    std::move(callback).Run();
  }

  environment_provider_error_callbacks_.clear();
  environment_provider_.reset();
}

void XRSystem::TryEnsureService() {
  DVLOG(2) << __func__;

  // If we already have a service, there's nothing to do.
  if (service_.is_bound()) {
    DVLOG(2) << __func__ << ": service already bound";
    return;
  }

  // If the service has been disconnected in the past or our context has been
  // destroyed, don't try to get the service again.
  if (did_service_ever_disconnect_ || is_context_destroyed_) {
    DVLOG(2) << __func__
             << ": service disconnected or context destroyed, "
                "did_service_ever_disconnect_="
             << did_service_ever_disconnect_
             << ", is_context_destroyed_=" << is_context_destroyed_;
    return;
  }

  // If the current frame isn't attached, don't try to get the service.
  LocalFrame* frame = GetFrame();
  if (!frame || !frame->IsAttached()) {
    DVLOG(2) << ": current frame is not attached";
    return;
  }

  // See https://bit.ly/2S0zRAS for task types.
  frame->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(
          frame->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  service_.set_disconnect_handler(WTF::Bind(&XRSystem::Dispose,
                                            WrapWeakPersistent(this),
                                            DisposeType::kDisconnected));
}

void XRSystem::Trace(Visitor* visitor) const {
  visitor->Trace(frame_provider_);
  visitor->Trace(sessions_);
  visitor->Trace(service_);
  visitor->Trace(environment_provider_);
  visitor->Trace(receiver_);
  visitor->Trace(outstanding_support_queries_);
  visitor->Trace(outstanding_request_queries_);
  visitor->Trace(fullscreen_event_manager_);
  visitor->Trace(fullscreen_exit_observer_);
  Supplement<Navigator>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
