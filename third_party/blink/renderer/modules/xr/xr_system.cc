// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_system.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_depth_state_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_reference_space_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_session_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_tracked_image_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_enter_fullscreen_observer.h"
#include "third_party/blink/renderer/modules/xr/xr_exit_fullscreen_observer.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_session_viewport_scaler.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

constexpr uint64_t kInvalidTraceId = -1;

const char kNavigatorDetachedError[] =
    "The navigator.xr object is no longer associated with a document.";

const char kPageNotVisible[] = "The page is not visible";

const char kFeaturePolicyBlocked[] =
    "Access to the feature \"xr\" is disallowed by permissions policy.";

const char kActiveImmersiveSession[] =
    "There is already an active, immersive XRSession.";

const char kRequestRequiresUserActivation[] =
    "The requested session requires user activation.";

const char kSessionNotSupported[] =
    "The specified session configuration is not supported.";

const char kInvalidRequiredFeatures[] =
    "The session request contains invalid requiredFeatures and could not be "
    "fullfilled.";

const char kNoDevicesMessage[] = "No XR hardware found.";

const char kImmersiveArModeNotValid[] =
    "Failed to execute '%s' on 'XRSystem': The provided value 'immersive-ar' "
    "is not a valid enum value of type XRSessionMode.";

const char kTrackedImageWidthInvalid[] =
    "trackedImages[%d].widthInMeters invalid, must be a positive number.";

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

device::mojom::blink::XRSessionMode V8EnumToSessionMode(
    V8XRSessionMode::Enum mode) {
  switch (mode) {
    case V8XRSessionMode::Enum::kInline:
      return device::mojom::blink::XRSessionMode::kInline;
    case V8XRSessionMode::Enum::kImmersiveVr:
      return device::mojom::blink::XRSessionMode::kImmersiveVr;
    case V8XRSessionMode::Enum::kImmersiveAr:
      return device::mojom::blink::XRSessionMode::kImmersiveAr;
  }
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
}

device::mojom::XRDepthUsage ParseDepthUsage(const V8XRDepthUsage& usage) {
  switch (usage.AsEnum()) {
    case V8XRDepthUsage::Enum::kCpuOptimized:
      return device::mojom::XRDepthUsage::kCPUOptimized;
    case V8XRDepthUsage::Enum::kGpuOptimized:
      return device::mojom::XRDepthUsage::kGPUOptimized;
  }
}

Vector<device::mojom::XRDepthUsage> ParseDepthUsages(
    const Vector<V8XRDepthUsage>& usages) {
  Vector<device::mojom::XRDepthUsage> result;

  base::ranges::transform(usages, std::back_inserter(result), ParseDepthUsage);

  return result;
}

device::mojom::XRDepthDataFormat ParseDepthFormat(
    const V8XRDepthDataFormat& format) {
  switch (format.AsEnum()) {
    case V8XRDepthDataFormat::Enum::kLuminanceAlpha:
      return device::mojom::XRDepthDataFormat::kLuminanceAlpha;
    case V8XRDepthDataFormat::Enum::kFloat32:
      return device::mojom::XRDepthDataFormat::kFloat32;
    case V8XRDepthDataFormat::Enum::kUnsignedShort:
      return device::mojom::XRDepthDataFormat::kUnsignedShort;
  }
}

Vector<device::mojom::XRDepthDataFormat> ParseDepthFormats(
    const Vector<V8XRDepthDataFormat>& formats) {
  Vector<device::mojom::XRDepthDataFormat> result;

  base::ranges::transform(formats, std::back_inserter(result),
                          ParseDepthFormat);

  return result;
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
    case device::mojom::XRSessionFeature::HAND_INPUT:
    case device::mojom::XRSessionFeature::SECONDARY_VIEWS:
    case device::mojom::XRSessionFeature::LAYERS:
    case device::mojom::XRSessionFeature::WEBGPU:
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
    case device::mojom::XRSessionFeature::IMAGE_TRACKING:
      if (mode != device::mojom::blink::XRSessionMode::kImmersiveAr)
        return false;
      if (!session_init->hasTrackedImages()) {
        execution_context->AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kJavaScript, error_level,
                "Must specify trackedImages in XRSessionInit"));
        return false;
      }
      return true;
    case device::mojom::XRSessionFeature::LIGHT_ESTIMATION:
    case device::mojom::XRSessionFeature::CAMERA_ACCESS:
    case device::mojom::XRSessionFeature::PLANE_DETECTION:
    case device::mojom::XRSessionFeature::FRONT_FACING:
      return mode == device::mojom::blink::XRSessionMode::kImmersiveAr;
    case device::mojom::XRSessionFeature::DEPTH:
      if (!session_init->hasDepthSensing()) {
        execution_context->AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kJavaScript, error_level,
                "Must provide a depthSensing dictionary in XRSessionInit"));
        return false;
      }
      return mode == device::mojom::blink::XRSessionMode::kImmersiveAr;
  }
}

bool HasRequiredPermissionsPolicy(ExecutionContext* context,
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
    case device::mojom::XRSessionFeature::PLANE_DETECTION:
    case device::mojom::XRSessionFeature::DEPTH:
    case device::mojom::XRSessionFeature::IMAGE_TRACKING:
    case device::mojom::XRSessionFeature::HAND_INPUT:
    case device::mojom::XRSessionFeature::SECONDARY_VIEWS:
    case device::mojom::XRSessionFeature::LAYERS:
    case device::mojom::XRSessionFeature::FRONT_FACING:
    case device::mojom::XRSessionFeature::WEBGPU:
      return context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kWebXr,
          ReportOptions::kReportOnFailure);
    case device::mojom::XRSessionFeature::CAMERA_ACCESS:
      return context->IsFeatureEnabled(
                 mojom::blink::PermissionsPolicyFeature::kWebXr,
                 ReportOptions::kReportOnFailure) &&
             context->IsFeatureEnabled(
                 mojom::blink::PermissionsPolicyFeature::kCamera,
                 ReportOptions::kReportOnFailure);
  }
}

// Ensure that the immersive session request is allowed, if not
// return which security error occurred.
// https://immersive-web.github.io/webxr/#immersive-session-request-is-allowed
const char* CheckImmersiveSessionRequestAllowed(LocalDOMWindow* window) {
  // Ensure that the session was initiated by a user gesture
  if (!LocalFrame::HasTransientUserActivation(window->GetFrame())) {
    return kRequestRequiresUserActivation;
  }

  // Check that the document is "trustworthy"
  // https://immersive-web.github.io/webxr/#trustworthy
  if (!window->document()->IsPageVisible()) {
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

bool IsImmersiveArAllowedBySettings(LocalDOMWindow* window) {
  // If we're unable to get the settings for any reason, we'll treat the AR as
  // enabled.
  if (!window->GetFrame()) {
    return true;
  }

  return window->GetFrame()->GetSettings()->GetWebXRImmersiveArAllowed();
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
    ScriptPromiseResolverBase* resolver,
    device::mojom::blink::XRSessionMode session_mode,
    bool throw_on_unsupported)
    : resolver_(resolver),
      mode_(session_mode),
      trace_id_(base::trace_event::GetNextGlobalTraceId()),
      throw_on_unsupported_(throw_on_unsupported) {
  TRACE_EVENT("xr", "PendingSupportsSessionQuery::PendingSupportsSessionQuery",
              "session_mode", session_mode, perfetto::Flow::Global(trace_id_));
}

void XRSystem::PendingSupportsSessionQuery::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
}

void XRSystem::PendingSupportsSessionQuery::Resolve(
    bool supported,
    ExceptionState* exception_state) {
  TRACE_EVENT("xr", "PendingSupportsSessionQuery::Resolve", "supported",
              supported, perfetto::TerminatingFlow::Global(trace_id_));

  if (throw_on_unsupported_) {
    if (supported) {
      resolver_->DowncastTo<IDLUndefined>()->Resolve();
    } else {
      DVLOG(2) << __func__ << ": session is unsupported - throwing exception";
      RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                             kSessionNotSupported, exception_state);
    }
  } else {
    static_cast<ScriptPromiseResolver<IDLBoolean>*>(resolver_.Get())
        ->Resolve(supported);
  }
}

void XRSystem::PendingSupportsSessionQuery::RejectWithDOMException(
    DOMExceptionCode exception_code,
    const String& message,
    ExceptionState* exception_state) {
  DCHECK_NE(exception_code, DOMExceptionCode::kSecurityError);

  TRACE_EVENT("xr", "PendingSupportsSessionQuery::RejectWithDOMException",
              "exception_code", exception_code, "message", message,
              perfetto::TerminatingFlow::Global(trace_id_));

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
    const String& message,
    ExceptionState* exception_state) {
  TRACE_EVENT("xr", "PendingSupportsSessionQuery::RejectWithSecurityError",
              "message", message, perfetto::TerminatingFlow::Global(trace_id_));

  if (exception_state) {
    // The generated V8 bindings will reject the returned promise for us.
    // Detaching the resolver prevents it from thinking we abandoned
    // the promise.
    exception_state->ThrowSecurityError(message);
    resolver_->Detach();
  } else {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError, message));
  }
}

void XRSystem::PendingSupportsSessionQuery::RejectWithTypeError(
    const String& message,
    ExceptionState* exception_state) {
  TRACE_EVENT("xr", "PendingSupportsSessionQuery::RejectWithTypeError",
              "message", message, perfetto::TerminatingFlow::Global(trace_id_));

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
    ScriptPromiseResolver<XRSession>* resolver,
    device::mojom::blink::XRSessionMode session_mode,
    RequestedXRSessionFeatureSet required_features,
    RequestedXRSessionFeatureSet optional_features)
    : resolver_(resolver),
      mode_(session_mode),
      required_features_(std::move(required_features)),
      optional_features_(std::move(optional_features)),
      ukm_source_id_(ukm_source_id),
      trace_id_(base::trace_event::GetNextGlobalTraceId()) {
  TRACE_EVENT("xr", "PendingRequestSessionQuery::PendingRequestSessionQuery",
              "Session mode", session_mode, perfetto::Flow::Global(trace_id_));

  ParseSensorRequirement();
}

void XRSystem::PendingRequestSessionQuery::Resolve(
    XRSession* session,
    mojo::PendingRemote<device::mojom::blink::XRSessionMetricsRecorder>
        metrics_recorder) {
  TRACE_EVENT("xr", "PendingRequestSessionQuery::Resolve",
              perfetto::TerminatingFlow::Global(trace_id_));

  resolver_->Resolve(session);
  ReportRequestSessionResult(SessionRequestStatus::kSuccess, session,
                             std::move(metrics_recorder));
}

void XRSystem::PendingRequestSessionQuery::RejectWithDOMException(
    DOMExceptionCode exception_code,
    const String& message,
    ExceptionState* exception_state) {
  DCHECK_NE(exception_code, DOMExceptionCode::kSecurityError);

  TRACE_EVENT("xr", "PendingRequestSessionQuery::RejectWithDOMException",
              "exception_code", exception_code, "message", message,
              perfetto::TerminatingFlow::Global(trace_id_));

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
    const String& message,
    ExceptionState* exception_state) {
  TRACE_EVENT("xr", "PendingRequestSessionQuery::RejectWithSecurityError",
              "message", message, perfetto::TerminatingFlow::Global(trace_id_));

  if (exception_state) {
    exception_state->ThrowSecurityError(message);
    resolver_->Detach();
  } else {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError, message));
  }

  ReportRequestSessionResult(SessionRequestStatus::kOtherError);
}

void XRSystem::PendingRequestSessionQuery::RejectWithTypeError(
    const String& message,
    ExceptionState* exception_state) {
  TRACE_EVENT("xr", "PendingRequestSessionQuery::RejectWithTypeError",
              "message", message, perfetto::TerminatingFlow::Global(trace_id_));

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
  auto* execution_context = resolver_->GetExecutionContext();
  if (!execution_context) {
    return;
  }

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
  auto feature_request_plane_detection =
      GetFeatureRequestStatus(XRSessionFeature::PLANE_DETECTION, session);
  auto feature_request_image_tracking =
      GetFeatureRequestStatus(XRSessionFeature::IMAGE_TRACKING, session);

  ukm::builders::XR_WebXR_SessionRequest(ukm_source_id_)
      .SetMode(static_cast<int64_t>(mode_))
      .SetStatus(static_cast<int64_t>(status))
      .SetFeature_Viewer(static_cast<int64_t>(feature_request_viewer))
      .SetFeature_Local(static_cast<int64_t>(feature_request_local))
      .SetFeature_LocalFloor(static_cast<int64_t>(feature_request_local_floor))
      .SetFeature_BoundedFloor(
          static_cast<int64_t>(feature_request_bounded_floor))
      .SetFeature_Unbounded(static_cast<int64_t>(feature_request_unbounded))
      .Record(execution_context->UkmRecorder());

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

  if (session && status == SessionRequestStatus::kSuccess &&
      IsFeatureRequested(feature_request_plane_detection)) {
    DVLOG(2) << __func__
             << ": plane detection was requested, logging a UseCounter";
    UseCounter::Count(session->GetExecutionContext(),
                      WebFeature::kXRPlaneDetection);
  }

  if (session && status == SessionRequestStatus::kSuccess &&
      IsFeatureRequested(feature_request_image_tracking)) {
    DVLOG(2) << __func__
             << ": image tracking was requested, logging a UseCounter";
    UseCounter::Count(session->GetExecutionContext(),
                      WebFeature::kXRImageTracking);
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

device::mojom::blink::XRSessionOptionsPtr XRSystem::XRSessionOptionsFromQuery(
    const PendingRequestSessionQuery& query) {
  device::mojom::blink::XRSessionOptionsPtr session_options =
      device::mojom::blink::XRSessionOptions::New();
  session_options->mode = query.mode();

  session_options->required_features.assign(query.RequiredFeatures());
  session_options->optional_features.assign(query.OptionalFeatures());

  session_options->tracked_images.resize(query.TrackedImages().size());
  for (unsigned i = 0; i < query.TrackedImages().size(); ++i) {
    session_options->tracked_images[i] =
        device::mojom::blink::XRTrackedImage::New();
    *session_options->tracked_images[i] = query.TrackedImages()[i];
  }

  if (query.HasFeature(device::mojom::XRSessionFeature::DEPTH)) {
    session_options->depth_options =
        device::mojom::blink::XRDepthOptions::New();
    session_options->depth_options->usage_preferences = query.PreferredUsage();
    session_options->depth_options->data_format_preferences =
        query.PreferredFormat();
  }

  session_options->trace_id = query.TraceId();

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
  DVLOG(2) << __func__;

  return document.domWindow() ? xr(*document.domWindow()->navigator())
                              : nullptr;
}

XRSystem* XRSystem::xr(Navigator& navigator) {
  DVLOG(2) << __func__;

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
              ->RegisterFeature(SchedulingPolicy::Feature::kWebXR,
                                {SchedulingPolicy::DisableBackForwardCache()})),
      webxr_internals_renderer_listener_(GetExecutionContext()) {}

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
  return FocusChangedObserver::IsFrameFocused(
      DomWindow() ? DomWindow()->GetFrame() : nullptr);
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

  return frame_provider_.Get();
}

device::mojom::blink::XREnvironmentIntegrationProvider*
XRSystem::xrEnvironmentProviderRemote() {
  return environment_provider_.get();
}

device::mojom::blink::VRService* XRSystem::BrowserService() {
  return service_.get();
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
  if (LocalDOMWindow* window = DomWindow()) {
    Document* doc = window->document();
    DVLOG(3) << __func__ << ": doc->IsXrOverlay()=" << doc->IsXrOverlay();
    if (doc->IsXrOverlay()) {
      Element* fullscreen_element = Fullscreen::FullscreenElementFrom(*doc);
      DVLOG(3) << __func__ << ": fullscreen_element=" << fullscreen_element;
      if (fullscreen_element) {
        fullscreen_exit_observer_ =
            MakeGarbageCollected<XrExitFullscreenObserver>();
        // Once we exit fullscreen, we'll need to come back here to finish
        // shutting down the session.
        fullscreen_exit_observer_->ExitFullscreen(
            doc, WTF::BindOnce(&XRSystem::ExitPresent, WrapWeakPersistent(this),
                               std::move(on_exited)));
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

ScriptPromise<IDLUndefined> XRSystem::supportsSession(
    ScriptState* script_state,
    const V8XRSessionMode& mode,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  InternalIsSessionSupported(resolver, mode, exception_state, true);
  return promise;
}

ScriptPromise<IDLBoolean> XRSystem::isSessionSupported(
    ScriptState* script_state,
    const V8XRSessionMode& mode,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  InternalIsSessionSupported(resolver, mode, exception_state, false);
  return promise;
}

void XRSystem::AddConsoleMessage(mojom::blink::ConsoleMessageLevel error_level,
                                 const String& message) {
  DVLOG(2) << __func__ << ": error_level=" << error_level
           << ", message=" << message;

  if ((error_level == mojom::blink::ConsoleMessageLevel::kError ||
       error_level == mojom::blink::ConsoleMessageLevel::kWarning) &&
      frameProvider()->immersive_session()) {
    AddWebXrInternalsMessage(message);
  }
  GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript, error_level, message));
}

void XRSystem::AddWebXrInternalsMessage(const String& message) {
  if (webxr_internals_renderer_listener_) {
    device::mojom::blink::XrLogMessagePtr xr_logging_statistics =
        device::mojom::blink::XrLogMessage::New();

    xr_logging_statistics->message = message;
    xr_logging_statistics->trace_id =
        frameProvider()->immersive_session()->GetTraceId();

    webxr_internals_renderer_listener_->OnConsoleLog(
        std::move(xr_logging_statistics));
  }
}

void XRSystem::InternalIsSessionSupported(ScriptPromiseResolverBase* resolver,
                                          const V8XRSessionMode& mode,
                                          ExceptionState& exception_state,
                                          bool throw_on_unsupported) {
  if (!GetExecutionContext()) {
    // Reject if the context is inaccessible.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kNavigatorDetachedError);
    return;  // Promise will be rejected by generated bindings
  }

  device::mojom::blink::XRSessionMode session_mode =
      V8EnumToSessionMode(mode.AsEnum());
  PendingSupportsSessionQuery* query =
      MakeGarbageCollected<PendingSupportsSessionQuery>(resolver, session_mode,
                                                        throw_on_unsupported);

  if (session_mode == device::mojom::blink::XRSessionMode::kImmersiveAr &&
      !IsImmersiveArAllowed()) {
    DVLOG(2) << __func__
             << ": Immersive AR session is only supported if WebXRARModule "
                "feature is enabled by a runtime feature and web settings";
    query->Resolve(false);
    return;
  }

  if (session_mode == device::mojom::blink::XRSessionMode::kInline) {
    // inline sessions are always supported.
    query->Resolve(true);
    return;
  }

  if (!GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kWebXr,
          ReportOptions::kReportOnFailure)) {
    // Only allow the call to be made if the appropriate permissions policy is
    // in place.
    query->RejectWithSecurityError(kFeaturePolicyBlocked, &exception_state);
    return;
  }

  // If TryEnsureService() doesn't set |service_|, then we don't have any WebXR
  // hardware, so we need to reject as being unsupported.
  TryEnsureService();
  if (!service_.is_bound()) {
    query->Resolve(false, &exception_state);
    return;
  }

  device::mojom::blink::XRSessionOptionsPtr session_options =
      device::mojom::blink::XRSessionOptions::New();
  session_options->mode = query->mode();
  session_options->trace_id = query->TraceId();

  outstanding_support_queries_.insert(query);
  service_->SupportsSession(
      std::move(session_options),
      WTF::BindOnce(&XRSystem::OnSupportsSessionReturned, WrapPersistent(this),
                    WrapPersistent(query)));
}

void XRSystem::RequestSessionInternal(
    device::mojom::blink::XRSessionMode session_mode,
    PendingRequestSessionQuery* query,
    ExceptionState* exception_state) {
  // The various session request methods may have other checks that would reject
  // before needing to create the vr service, so we don't try to create it here.
  switch (session_mode) {
    case device::mojom::blink::XRSessionMode::kImmersiveVr:
    case device::mojom::blink::XRSessionMode::kImmersiveAr:
      RequestImmersiveSession(query, exception_state);
      break;
    case device::mojom::blink::XRSessionMode::kInline:
      RequestInlineSession(query, exception_state);
      break;
  }
}

void XRSystem::RequestImmersiveSession(PendingRequestSessionQuery* query,
                                       ExceptionState* exception_state) {
  DVLOG(2) << __func__;
  // Log an immersive session request if we haven't already
  if (!did_log_request_immersive_session_) {
    ukm::builders::XR_WebXR(DomWindow()->UkmSourceID())
        .SetDidRequestPresentation(1)
        .Record(DomWindow()->UkmRecorder());
    did_log_request_immersive_session_ = true;
  }

  // Make sure the request is allowed
  auto* immersive_session_request_error =
      CheckImmersiveSessionRequestAllowed(DomWindow());
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
                                  kInvalidRequiredFeatures, exception_state);
    return;
  }

  // Reworded from spec 'pending immersive session'
  has_outstanding_immersive_request_ = true;

  // Submit the request to VrServiceImpl in the Browser process
  outstanding_request_queries_.insert(query);
  auto session_options = XRSessionOptionsFromQuery(*query);

  // If we're already in fullscreen mode, we need to exit and re-enter
  // fullscreen mode to properly apply the is_xr_overlay property and reset the
  // existing navigationUI options that may be conflicting with what we want.
  // Request a fullscreen exit, and continue with the session request once that
  // completes.
  Document* doc = DomWindow()->document();
  if (Fullscreen::FullscreenElementFrom(*doc)) {
    fullscreen_exit_observer_ =
        MakeGarbageCollected<XrExitFullscreenObserver>();

    base::OnceClosure callback =
        WTF::BindOnce(&XRSystem::DoRequestSession, WrapWeakPersistent(this),
                      WrapPersistent(query), std::move(session_options));
    fullscreen_exit_observer_->ExitFullscreen(doc, std::move(callback));
    return;
  }

  DoRequestSession(std::move(query), std::move(session_options));
}

void XRSystem::DoRequestSession(
    PendingRequestSessionQuery* query,
    device::mojom::blink::XRSessionOptionsPtr session_options) {
  service_->RequestSession(
      std::move(session_options),
      WTF::BindOnce(&XRSystem::OnRequestSessionReturned,
                    WrapWeakPersistent(this), WrapPersistent(query)));
}

void XRSystem::RequestInlineSession(PendingRequestSessionQuery* query,
                                    ExceptionState* exception_state) {
  DVLOG(2) << __func__;
  // Make sure the inline session request was allowed
  auto* inline_session_request_error =
      CheckInlineSessionRequestAllowed(DomWindow()->GetFrame(), *query);
  if (inline_session_request_error) {
    query->RejectWithSecurityError(inline_session_request_error,
                                   exception_state);
    return;
  }

  // Reject session if any of the required features were invalid.
  if (query->InvalidRequiredFeatures()) {
    DVLOG(2) << __func__ << ": rejecting session - invalid required features";
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kInvalidRequiredFeatures, exception_state);
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
                                  kNoDevicesMessage, exception_state);
    return;
  }

  // Submit the request to VrServiceImpl in the Browser process
  outstanding_request_queries_.insert(query);
  auto session_options = XRSessionOptionsFromQuery(*query);
  service_->RequestSession(
      std::move(session_options),
      WTF::BindOnce(&XRSystem::OnRequestSessionReturned,
                    WrapWeakPersistent(this), WrapPersistent(query)));
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
      auto feature_enum = StringToXRSessionFeature(feature_string);

      if (!feature_enum) {
        AddConsoleMessage(error_level,
                          "Unrecognized feature requested: " + feature_string);
        result.invalid_features = true;
      } else if (!IsFeatureEnabledForContext(feature_enum.value(),
                                             GetExecutionContext())) {
        AddConsoleMessage(error_level,
                          "Unsupported feature requested: " + feature_string);
        result.invalid_features = true;
      } else if (!IsFeatureValidForMode(feature_enum.value(), session_mode,
                                        session_init, GetExecutionContext(),
                                        error_level)) {
        AddConsoleMessage(error_level, "Feature '" + feature_string +
                                           "' is not supported for mode: " +
                                           SessionModeToString(session_mode));
        result.invalid_features = true;
      } else if (!HasRequiredPermissionsPolicy(GetExecutionContext(),
                                               feature_enum.value())) {
        AddConsoleMessage(error_level,
                          "Feature '" + feature_string +
                              "' is not permitted by permissions policy");
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

ScriptPromise<XRSession> XRSystem::requestSession(
    ScriptState* script_state,
    const V8XRSessionMode& mode,
    XRSessionInit* session_init,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__;
  // TODO(https://crbug.com/968622): Make sure we don't forget to call
  // metrics-related methods when the promise gets resolved/rejected.
  if (!DomWindow()) {
    // Reject if the window is inaccessible.
    DVLOG(1) << __func__ << ": DomWindow inaccessible";

    // Do *not* record an UKM event in this case (we won't be able to access the
    // Document to get UkmRecorder anyway).
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kNavigatorDetachedError);
    return EmptyPromise();  // Will be rejected by generated
                            // bindings
  }

  device::mojom::blink::XRSessionMode session_mode =
      V8EnumToSessionMode(mode.AsEnum());

  // If the request is for immersive-ar, ensure that feature is enabled.
  if (session_mode == device::mojom::blink::XRSessionMode::kImmersiveAr &&
      !IsImmersiveArAllowed()) {
    DVLOG(1) << __func__ << ": Immersive AR not allowed";
    exception_state.ThrowTypeError(
        String::Format(kImmersiveArModeNotValid, "requestSession"));

    // We haven't created the query yet, so we can't use it to implicitly log
    // our metrics for us, so explicitly log it here, as the query requires the
    // features to be parsed before it can be built.
    ukm::builders::XR_WebXR_SessionRequest(DomWindow()->UkmSourceID())
        .SetMode(static_cast<int64_t>(session_mode))
        .SetStatus(static_cast<int64_t>(SessionRequestStatus::kOtherError))
        .Record(DomWindow()->UkmRecorder());
    return EmptyPromise();
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
    if (HasRequiredPermissionsPolicy(GetExecutionContext(), feature)) {
      required_features.valid_features.insert(feature);
    } else {
      DVLOG(2) << __func__
               << ": permissions policy not satisfied for a default feature: "
               << feature;
      AddConsoleMessage(mojom::blink::ConsoleMessageLevel::kError,
                        "Permissions policy is not satisfied for feature '" +
                            XRSessionFeatureToString(feature) +
                            "' please ensure that appropriate permissions "
                            "policy is enabled.");
      required_features.invalid_features = true;
    }
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<XRSession>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  PendingRequestSessionQuery* query =
      MakeGarbageCollected<PendingRequestSessionQuery>(
          DomWindow()->UkmSourceID(), resolver, session_mode,
          std::move(required_features), std::move(optional_features));

  if (query->HasFeature(device::mojom::XRSessionFeature::DOM_OVERLAY)) {
    // Prerequisites were checked by IsFeatureValidForMode and IDL.
    DCHECK(session_init);
    DCHECK(session_init->hasDomOverlay());
    DCHECK(session_init->domOverlay()->hasRoot()) << "required in IDL";
    query->SetDOMOverlayElement(session_init->domOverlay()->root());
  }

  if (query->HasFeature(device::mojom::XRSessionFeature::IMAGE_TRACKING)) {
    // Prerequisites were checked by IsFeatureValidForMode.
    DCHECK(session_init);
    DCHECK(session_init->hasTrackedImages());
    DVLOG(3) << __func__ << ": set up trackedImages";
    Vector<device::mojom::blink::XRTrackedImage> images;
    int index = 0;
    for (auto& image : session_init->trackedImages()) {
      DCHECK(image->hasImage()) << "required in IDL";
      DCHECK(image->hasWidthInMeters()) << "required in IDL";
      if (std::isnan(image->widthInMeters()) ||
          image->widthInMeters() <= 0.0f) {
        String message = String::Format(kTrackedImageWidthInvalid, index);
        query->RejectWithTypeError(message, &exception_state);
        return promise;
      }
      // Extract an SkBitmap snapshot for each image.
      scoped_refptr<StaticBitmapImage> static_bitmap_image =
          image->image()->BitmapImage();
      SkBitmap sk_bitmap = static_bitmap_image->AsSkBitmapForCurrentFrame(
          kRespectImageOrientation);
      images.emplace_back(sk_bitmap, static_bitmap_image->Size(),
                          image->widthInMeters());
      ++index;
    }
    query->SetTrackedImages(images);
  }

  if (query->HasFeature(device::mojom::XRSessionFeature::DEPTH)) {
    // Prerequisites were checked by IsFeatureValidForMode and IDL.
    DCHECK(session_init);
    DCHECK(session_init->hasDepthSensing());
    DCHECK(session_init->depthSensing()->hasUsagePreference())
        << "required in IDL";
    DCHECK(session_init->depthSensing()->hasDataFormatPreference())
        << "required in IDL";

    Vector<device::mojom::XRDepthUsage> preferred_usage =
        ParseDepthUsages(session_init->depthSensing()->usagePreference());
    Vector<device::mojom::XRDepthDataFormat> preferred_format =
        ParseDepthFormats(session_init->depthSensing()->dataFormatPreference());

    query->SetDepthSensingConfiguration(preferred_usage, preferred_format);
  }

  // Defer to request the session until the prerendering page is activated.
  if (DomWindow()->document()->IsPrerendering()) {
    // Pass a nullptr instead of |exception_state| because we can't guarantee
    // this object is alive until the prerendering page is activate.
    DomWindow()->document()->AddPostPrerenderingActivationStep(WTF::BindOnce(
        &XRSystem::RequestSessionInternal, WrapWeakPersistent(this),
        session_mode, WrapPersistent(query), /*exception_state=*/nullptr));
    return promise;
  }

  RequestSessionInternal(session_mode, query, &exception_state);
  return promise;
}

void XRSystem::MakeXrCompatibleAsync(
    device::mojom::blink::VRService::MakeXrCompatibleCallback callback) {
  if (!GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kWebXr)) {
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
          mojom::blink::PermissionsPolicyFeature::kWebXr)) {
    *xr_compatible_result =
        device::mojom::XrCompatibleResult::kWebXrFeaturePolicyBlocked;
    return;
  }
  *xr_compatible_result = device::mojom::XrCompatibleResult::kNoDeviceAvailable;

  TryEnsureService();
  if (service_.is_bound())
    service_->MakeXrCompatible(xr_compatible_result);
}

void XRSystem::OnSessionEnded(XRSession* session) {
  if (session->immersive()) {
    webxr_internals_renderer_listener_.reset();
  }
}

// This will be called when the XR hardware or capabilities have potentially
// changed. For example, if a new physical device was connected to the system,
// it might be able to support immersive sessions, where it couldn't before.
void XRSystem::OnDeviceChanged() {
  ExecutionContext* context = GetExecutionContext();
  if (context && context->IsFeatureEnabled(
                     mojom::blink::PermissionsPolicyFeature::kWebXr)) {
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

void XRSystem::OnRequestSessionReturned(
    PendingRequestSessionQuery* query,
    device::mojom::blink::RequestSessionResultPtr result) {
  // If session creation failed, move straight on to processing that.
  if (!result->is_success()) {
    FinishSessionCreation(query, std::move(result));
    return;
  }

  Element* fullscreen_element = nullptr;
  const auto& enabled_features =
      result->get_success()->session->enabled_features;
  if (base::Contains(enabled_features,
                     device::mojom::XRSessionFeature::DOM_OVERLAY)) {
    fullscreen_element = query->DOMOverlayElement();
  }

  // Only setup for dom_overlay if the query actually had a DOMOverlayElement
  // and the session enabled dom_overlay. (Note that fullscreen_element will be
  // null if the feature was not enabled).
  bool setup_for_dom_overlay = !!fullscreen_element;

// On Android, due to the way the device renderer is configured, we always need
// to enter fullscreen if we're starting an AR session, so if we aren't supposed
// to enter DOMOverlay, we simply fullscreen the document body.
#if BUILDFLAG(IS_ANDROID)
  if (!fullscreen_element &&
      query->mode() == device::mojom::blink::XRSessionMode::kImmersiveAr) {
    fullscreen_element = DomWindow()->document()->body();
  }
#endif

  // If we don't need to enter fullscreen continue with session setup.
  if (!fullscreen_element) {
    FinishSessionCreation(query, std::move(result));
    return;
  }

  const bool session_has_camera_access = base::Contains(
      enabled_features, device::mojom::XRSessionFeature::CAMERA_ACCESS);

  // At this point, we know that we have an element that we need to make
  // fullscreen, so we do that before we continue setting up the session.
  fullscreen_enter_observer_ =
      MakeGarbageCollected<XrEnterFullscreenObserver>();
  fullscreen_enter_observer_->RequestFullscreen(
      fullscreen_element, setup_for_dom_overlay, session_has_camera_access,
      WTF::BindOnce(&XRSystem::OnFullscreenConfigured, WrapPersistent(this),
                    WrapPersistent(query), std::move(result)));
}

void XRSystem::OnFullscreenConfigured(
    PendingRequestSessionQuery* query,
    device::mojom::blink::RequestSessionResultPtr result,
    bool fullscreen_succeeded) {
  // At this point we no longer need the enter observer, so go ahead and destroy
  // it.
  fullscreen_enter_observer_ = nullptr;

  if (fullscreen_succeeded) {
    FinishSessionCreation(query, std::move(result));
  } else {
    FinishSessionCreation(
        query, device::mojom::blink::RequestSessionResult::NewFailureReason(
                   device::mojom::RequestSessionError::FULLSCREEN_ERROR));
  }
}

void XRSystem::FinishSessionCreation(
    PendingRequestSessionQuery* query,
    device::mojom::blink::RequestSessionResultPtr result) {
  DVLOG(2) << __func__;
  // The session query has returned and we're about to resolve or reject the
  // promise, so remove it from our outstanding list.
  DCHECK(outstanding_request_queries_.Contains(query));
  outstanding_request_queries_.erase(query);
  if (query->mode() == device::mojom::blink::XRSessionMode::kImmersiveVr ||
      query->mode() == device::mojom::blink::XRSessionMode::kImmersiveAr) {
    DCHECK(has_outstanding_immersive_request_);
    has_outstanding_immersive_request_ = false;
  }

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

  XRSessionFeatureSet enabled_features;
  for (const auto& feature : session_ptr->enabled_features) {
    DVLOG(2) << __func__ << ": feature " << feature << " will be enabled";
    enabled_features.insert(feature);
  }

  XRSession* session = CreateSession(
      query->mode(), session_ptr->enviroment_blend_mode,
      session_ptr->interaction_mode, std::move(session_ptr->client_receiver),
      std::move(session_ptr->device_config), enabled_features,
      result->get_success()->trace_id);

  frameProvider()->OnSessionStarted(session, std::move(session_ptr));

  // The session is immersive, so we need to set up the WebXR Internals
  // listener.
  if (session->immersive() && result->get_success()->xr_internals_listener) {
    webxr_internals_renderer_listener_.Bind(
        std::move(std::move(result->get_success()->xr_internals_listener)),
        GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault));
  }

  if (query->mode() == device::mojom::blink::XRSessionMode::kImmersiveVr ||
      query->mode() == device::mojom::blink::XRSessionMode::kImmersiveAr) {
    const bool anchors_enabled = base::Contains(
        enabled_features, device::mojom::XRSessionFeature::ANCHORS);
    const bool hit_test_enabled = base::Contains(
        enabled_features, device::mojom::XRSessionFeature::HIT_TEST);
    const bool environment_integration = hit_test_enabled || anchors_enabled;
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
          WTF::BindOnce(&XRSystem::OnEnvironmentProviderDisconnect,
                        WrapWeakPersistent(this)));

      session->OnEnvironmentProviderCreated();
    }

    auto dom_overlay_feature = device::mojom::XRSessionFeature::DOM_OVERLAY;
    if (query->mode() == device::mojom::blink::XRSessionMode::kImmersiveAr &&
        query->HasFeature(dom_overlay_feature) &&
        base::Contains(enabled_features, dom_overlay_feature)) {
      DCHECK(query->DOMOverlayElement());
      // The session is using DOM overlay mode. At this point the overlay
      // element is already in fullscreen mode, and the session can proceed.
      session->SetDOMOverlayElement(query->DOMOverlayElement());
    }
  }

  UseCounter::Count(ExecutionContext::From(query->GetScriptState()),
                    WebFeature::kWebXrSessionCreated);

  query->Resolve(session, std::move(metrics_recorder));
}

void XRSystem::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTarget::AddedEventListener(event_type, registered_listener);

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
    device::mojom::blink::XRSessionDeviceConfigPtr device_config,
    XRSessionFeatureSet enabled_features,
    uint64_t trace_id,
    bool sensorless_session) {
  XRSession* session = MakeGarbageCollected<XRSession>(
      this, std::move(client_receiver), mode, blend_mode, interaction_mode,
      std::move(device_config), sensorless_session, std::move(enabled_features),
      trace_id);
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
                       std::move(device_config),
                       {device::mojom::XRSessionFeature::REF_SPACE_VIEWER},
                       true, kInvalidTraceId /* sensorless_session */);
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
  DCHECK(outstanding_support_queries_.empty());

  HeapHashSet<Member<PendingRequestSessionQuery>> request_queries =
      outstanding_request_queries_;
  for (const auto& query : request_queries) {
    OnRequestSessionReturned(
        query, device::mojom::blink::RequestSessionResult::NewFailureReason(
                   device::mojom::RequestSessionError::INVALID_CLIENT));
  }
  DCHECK(outstanding_support_queries_.empty());
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
  if (!DomWindow()) {
    DVLOG(2) << ": current frame is not attached";
    return;
  }

  // See https://bit.ly/2S0zRAS for task types.
  DomWindow()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(
          DomWindow()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  service_.set_disconnect_handler(WTF::BindOnce(&XRSystem::Dispose,
                                                WrapWeakPersistent(this),
                                                DisposeType::kDisconnected));
}

bool XRSystem::IsImmersiveArAllowed() {
  const bool ar_allowed_in_settings =
      IsImmersiveArAllowedBySettings(DomWindow());

  DVLOG(2) << __func__ << ": ar_allowed_in_settings=" << ar_allowed_in_settings;

  return ar_allowed_in_settings;
}

void XRSystem::Trace(Visitor* visitor) const {
  visitor->Trace(frame_provider_);
  visitor->Trace(sessions_);
  visitor->Trace(service_);
  visitor->Trace(environment_provider_);
  visitor->Trace(receiver_);
  visitor->Trace(webxr_internals_renderer_listener_);
  visitor->Trace(outstanding_support_queries_);
  visitor->Trace(outstanding_request_queries_);
  visitor->Trace(fullscreen_enter_observer_);
  visitor->Trace(fullscreen_exit_observer_);
  Supplement<Navigator>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  EventTarget::Trace(visitor);
}

device::mojom::blink::WebXrInternalsRendererListener*
XRSystem::GetWebXrInternalsRendererListener() {
  if (!webxr_internals_renderer_listener_) {
    return nullptr;
  }
  return webxr_internals_renderer_listener_.get();
}

}  // namespace blink
