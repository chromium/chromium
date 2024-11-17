// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/vr_service_impl.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/xr/metrics/session_metrics_helper.h"
#include "content/browser/xr/service/browser_xr_runtime_impl.h"
#include "content/browser/xr/service/xr_permission_results.h"
#include "content/browser/xr/service/xr_runtime_manager_impl.h"
#include "content/browser/xr/webxr_internals/mojom/webxr_internals.mojom.h"
#include "content/browser/xr/webxr_internals/webxr_internals_handler_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/cpp/features.h"
#include "device/vr/public/cpp/session_mode.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"
#include "device/vr/public/mojom/xr_device.mojom-shared.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"

namespace {

device::mojom::XRRuntimeSessionOptionsPtr GetRuntimeOptions(
    device::mojom::XRSessionOptions* options) {
  device::mojom::XRRuntimeSessionOptionsPtr runtime_options =
      device::mojom::XRRuntimeSessionOptions::New();
  runtime_options->mode = options->mode;
  return runtime_options;
}

// Helper, returns collection of permissions required for XR session creation
// for session with mode set to |mode|. The order in the result does not matter
// as the permissions API does not honor it.
std::vector<blink::PermissionType> GetRequiredPermissionsForMode(
    device::mojom::XRSessionMode mode) {
  std::vector<blink::PermissionType> permissions;

  auto mode_permission = content::XrPermissionResults::GetPermissionFor(mode);
  if (mode_permission) {
    permissions.push_back(*mode_permission);
  }

  return permissions;
}

// Helper, returns collection of permissions required for XR session creation
// for session with enabled features listed in |required_features| and
// |optional_features|. The order in the result does not matter as the
// permissions API does not honor it.
std::vector<blink::PermissionType> GetRequiredPermissionsForFeatures(
    const std::unordered_set<device::mojom::XRSessionFeature>&
        required_features,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        optional_features) {
  std::vector<blink::PermissionType> permissions;

  for (const auto& required_feature : required_features) {
    auto feature_permission =
        content::XrPermissionResults::GetPermissionFor(required_feature);
    if (feature_permission &&
        !base::Contains(permissions, *feature_permission)) {
      permissions.push_back(*feature_permission);
    }
  }

  for (const auto& optional_feature : optional_features) {
    auto feature_permission =
        content::XrPermissionResults::GetPermissionFor(optional_feature);
    if (feature_permission &&
        !base::Contains(permissions, *feature_permission)) {
      permissions.push_back(*feature_permission);
    }
  }

  return permissions;
}

// TODO(crbug.com/40930146): Replace with base::ranges::set_difference
std::unordered_set<device::mojom::XRSessionFeature> GetMissingRequiredFeatures(
    const std::unordered_set<device::mojom::XRSessionFeature>& enabled_features,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        required_features) {
  DVLOG(3) << __func__
           << ": enabled_features.size()=" << enabled_features.size();

  std::unordered_set<device::mojom::XRSessionFeature> missing_required_features;

  for (const auto& required_feature : required_features) {
    if (!base::Contains(enabled_features, required_feature)) {
      DVLOG(2) << __func__
               << ": one of the required features was not enabled on the "
                  "created session, feature: "
               << required_feature;
      missing_required_features.insert(required_feature);
    }
  }

  return missing_required_features;
}

void RejectSession(device::mojom::VRService::RequestSessionCallback callback,
                   size_t trace_id,
                   device::mojom::RequestSessionError error,
                   const std::string& failure_reason_description,
                   std::unordered_set<device::mojom::XRSessionFeature>*
                       rejected_features = nullptr) {
  DVLOG(2) << __func__
           << ": failure reason description=" << failure_reason_description;

  webxr::mojom::SessionRejectedRecordPtr session_rejected_record =
      webxr::mojom::SessionRejectedRecord::New();
  session_rejected_record->trace_id = trace_id;
  session_rejected_record->failure_reason = error;
  session_rejected_record->rejected_time = base::Time::Now();
  session_rejected_record->failure_reason_description =
      failure_reason_description;
  if (rejected_features) {
    session_rejected_record->rejected_features.assign(
        rejected_features->begin(), rejected_features->end());
  }

  auto* runtime_manager_impl = static_cast<content::XRRuntimeManagerImpl*>(
      content::XRRuntimeManager::GetInstanceIfCreated());
  if (runtime_manager_impl) {
    runtime_manager_impl->GetLoggerManager().RecordSessionRejected(
        std::move(session_rejected_record));
  }

  std::move(callback).Run(
      device::mojom::RequestSessionResult::NewFailureReason(error));
}

}  // namespace

namespace content {

VRServiceImpl::SessionRequestData::SessionRequestData(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::RequestSessionCallback callback,
    device::mojom::XRDeviceId runtime_id)
    : callback(std::move(callback)),
      required_features(options->required_features.begin(),
                        options->required_features.end()),
      optional_features(options->optional_features.begin(),
                        options->optional_features.end()),
      options(std::move(options)),
      runtime_id(runtime_id) {}

VRServiceImpl::SessionRequestData::~SessionRequestData() {
  // In some cases, we may get dropped before the VRService pipe is closed. In
  // these cases we need to try to ensure that the callback is run or else we
  // hit DCHECKs for dropping the callback without closing the pipe.
  // This most often occurs when the Permissions prompt is dismissed.
  if (callback) {
    RejectSession(std::move(callback), options->trace_id,
                  device::mojom::RequestSessionError::UNKNOWN_FAILURE,
                  "SessionRequestData destroyed without running callback.");
  }
}

VRServiceImpl::SessionRequestData::SessionRequestData(SessionRequestData&&) =
    default;

VRServiceImpl::XrCompatibleCallback::XrCompatibleCallback(
    device::mojom::VRService::MakeXrCompatibleCallback callback)
    : callback(std::move(callback)) {}

VRServiceImpl::XrCompatibleCallback::XrCompatibleCallback(
    XrCompatibleCallback&& wrapper) {
  this->callback = std::move(wrapper.callback);
}

VRServiceImpl::XrCompatibleCallback::~XrCompatibleCallback() {
  if (!callback.is_null())
    std::move(callback).Run(
        device::mojom::XrCompatibleResult::kNoDeviceAvailable);
}

VRServiceImpl::VRServiceImpl(content::RenderFrameHost* render_frame_host)
    : WebContentsObserver(
          content::WebContents::FromRenderFrameHost(render_frame_host)),
      render_frame_host_(render_frame_host),
      in_focused_frame_(render_frame_host->GetView()->HasFocus()) {
  DCHECK(render_frame_host_);
  DVLOG(2) << __func__;

  runtime_manager_ =
      XRRuntimeManagerImpl::GetOrCreateInstance(*GetWebContents());
  runtime_manager_->AddService(this);

  magic_window_controllers_.set_disconnect_handler(base::BindRepeating(
      &VRServiceImpl::OnInlineSessionDisconnected,
      base::Unretained(this)));  // Unretained is OK since the collection is
                                 // owned by VRServiceImpl.
}

// Constructor for testing.
VRServiceImpl::VRServiceImpl(base::PassKey<XRRuntimeManagerTest>)
    : render_frame_host_(nullptr) {
  DVLOG(2) << __func__;
  runtime_manager_ = XRRuntimeManagerImpl::GetOrCreateInstanceForTesting();
  runtime_manager_->AddService(this);
}

VRServiceImpl::~VRServiceImpl() {
  DVLOG(2) << __func__;
  // Ensure that any active magic window sessions are disconnected to avoid
  // collisions when a new session starts. See https://crbug.com/1017959, the
  // disconnect handler doesn't get called automatically on page navigation.
  for (auto it = magic_window_controllers_.begin();
       it != magic_window_controllers_.end(); ++it) {
    OnInlineSessionDisconnected(it.id());
  }

  if (on_exit_present_) {
    std::move(on_exit_present_).Run();
  }

  runtime_manager_->RemoveService(this);
}

void VRServiceImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<device::mojom::VRService> receiver) {
  DVLOG(2) << __func__;
  std::unique_ptr<VRServiceImpl> vr_service_impl =
      std::make_unique<VRServiceImpl>(render_frame_host);

  VRServiceImpl* impl = vr_service_impl.get();
  impl->receiver_ = mojo::MakeSelfOwnedReceiver(std::move(vr_service_impl),
                                                std::move(receiver));
}

void VRServiceImpl::InitializationComplete() {
  // After initialization has completed, we can correctly answer
  // supportsSession, and can provide correct display capabilities.
  DVLOG(2) << __func__;
  initialization_complete_ = true;

  ResolvePendingRequests();
}

void VRServiceImpl::SetClient(
    mojo::PendingRemote<device::mojom::VRServiceClient> service_client) {
  if (service_client_) {
    mojo::ReportBadMessage("ServiceClient should only be set once.");
    return;
  }

  DVLOG(2) << __func__;
  service_client_.Bind(std::move(service_client));
}

void VRServiceImpl::ResolvePendingRequests() {
  DVLOG(2) << __func__
           << ": pending_requests_.size()=" << pending_requests_.size();
  for (auto& callback : pending_requests_) {
    std::move(callback).Run();
  }
  pending_requests_.clear();
}

void VRServiceImpl::RuntimesChanged() {
  DVLOG(2) << __func__;
  if (service_client_) {
    service_client_->OnDeviceChanged();
  }
}

void VRServiceImpl::OnWebContentsFocused(content::RenderWidgetHost* host) {
  OnWebContentsFocusChanged(host, true);
}

void VRServiceImpl::OnWebContentsLostFocus(content::RenderWidgetHost* host) {
  OnWebContentsFocusChanged(host, false);
}

void VRServiceImpl::RenderFrameDeleted(content::RenderFrameHost* host) {
  DVLOG(2) << __func__;
  if (host != render_frame_host_)
    return;

  // Receiver should always be live here, as this is a SelfOwnedReceiver.
  // Close the receiver (and delete this VrServiceImpl) when the RenderFrameHost
  // is deleted.
  DCHECK(receiver_.get());
  receiver_->Close();
}

void VRServiceImpl::OnWebContentsFocusChanged(content::RenderWidgetHost* host,
                                              bool focused) {
  if (!render_frame_host_->GetView() ||
      render_frame_host_->GetView()->GetRenderWidgetHost() != host) {
    return;
  }

  in_focused_frame_ = focused;

  for (const auto& controller : magic_window_controllers_)
    controller->SetFrameDataRestricted(!focused);
}

void VRServiceImpl::OnInlineSessionCreated(
    SessionRequestData request,
    device::mojom::XRRuntimeSessionResultPtr session_result) {
  if (!session_result) {
    TRACE_EVENT("xr",
                "VRServiceImpl::OnInlineSessionCreated: no session_result",
                perfetto::Flow::Global(request.options->trace_id));

    RejectSession(std::move(request.callback), request.options->trace_id,
                  device::mojom::RequestSessionError::UNKNOWN_RUNTIME_ERROR,
                  "Runtime did not provide a session.");
    return;
  }

  mojo::Remote<device::mojom::XRSessionController> controller(
      std::move(session_result->controller));
  // Start giving out magic window data if we are focused.
  controller->SetFrameDataRestricted(!in_focused_frame_);

  auto id = magic_window_controllers_.Add(std::move(controller));
  DVLOG(2) << __func__ << ": session_id=" << id.GetUnsafeValue()
           << " runtime_id=" << request.runtime_id;

  auto* session = session_result->session.get();
  std::unordered_set<device::mojom::XRSessionFeature> enabled_features(
      session->enabled_features.begin(), session->enabled_features.end());

  auto missing_required_features =
      GetMissingRequiredFeatures(enabled_features, request.required_features);
  if (!missing_required_features.empty()) {
    // UNKNOWN_FAILURE since a runtime should not return a session if there
    // exists a required feature that was not enabled - this would signify a bug
    // in the runtime.

    TRACE_EVENT(
        "xr",
        "VRServiceImpl::OnInlineSessionCreated: required feature not granted",
        perfetto::Flow::Global(request.options->trace_id));

    RejectSession(std::move(request.callback), request.options->trace_id,
                  device::mojom::RequestSessionError::UNKNOWN_FAILURE,
                  "Required feature not granted.", &missing_required_features);
    return;
  }

  mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
      session_metrics_recorder = GetSessionMetricsHelper()->StartInlineSession(
          *(request.options), enabled_features, id.GetUnsafeValue());

  OnSessionCreated(
      std::move(request), std::move(session_result->session),
      std::move(session_metrics_recorder),
      mojo::PendingRemote<device::mojom::WebXrInternalsRendererListener>());
}

void VRServiceImpl::OnImmersiveSessionCreated(
    SessionRequestData request,
    device::mojom::XRRuntimeSessionResultPtr session_result) {
  DCHECK(request.options);
  if (!session_result) {
    TRACE_EVENT("xr",
                "VRServiceImpl::OnImmersiveSessionCreated: no session_result",
                perfetto::Flow::Global(request.options->trace_id));

    RejectSession(std::move(request.callback), request.options->trace_id,
                  device::mojom::RequestSessionError::UNKNOWN_RUNTIME_ERROR,
                  "Runtime did not provide a session.");
    return;
  }

  auto* session = session_result->session.get();
  std::unordered_set<device::mojom::XRSessionFeature> enabled_features(
      session->enabled_features.begin(), session->enabled_features.end());

  auto missing_required_features =
      GetMissingRequiredFeatures(enabled_features, request.required_features);
  if (!missing_required_features.empty()) {
    // UNKNOWN_FAILURE since a runtime should not return a session if there
    // exists a required feature that was not enabled - this would signify a bug
    // in the runtime.

    TRACE_EVENT("xr",
                "VRServiceImpl::OnImmersiveSessionCreated: required feature "
                "not granted",
                perfetto::Flow::Global(request.options->trace_id));

    RejectSession(std::move(request.callback), request.options->trace_id,
                  device::mojom::RequestSessionError::UNKNOWN_FAILURE,
                  "Required feature not granted.", &missing_required_features);
    return;
  }

  // Get the metrics tracker for the new immersive session
  mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
      session_metrics_recorder =
          GetSessionMetricsHelper()->StartImmersiveSession(
              request.runtime_id, *(request.options), enabled_features);

  // If the session specified a FrameSinkId that means that it is handling its
  // own compositing in a way that we should notify the WebContents about.
  if (session_result->frame_sink_id) {
    if (session_result->frame_sink_id->is_valid()) {
      static_cast<WebContentsImpl*>(GetWebContents())
          ->OnXrHasRenderTarget(*session_result->frame_sink_id);
    } else {
      DLOG(ERROR) << __func__ << " frame_sink_id was specified but was invalid";
    }
  }

  OnSessionCreated(std::move(request), std::move(session_result->session),
                   std::move(session_metrics_recorder),
                   runtime_manager_->GetLoggerManager().BindRenderListener());
}

void VRServiceImpl::OnInlineSessionDisconnected(
    mojo::RemoteSetElementId session_id) {
  DVLOG(2) << __func__ << ": session_id=" << session_id.GetUnsafeValue();
  // Notify metrics helper that inline session was stopped.
  auto* metrics_helper = GetSessionMetricsHelper();
  metrics_helper->StopAndRecordInlineSession(session_id.GetUnsafeValue());
}

SessionMetricsHelper* VRServiceImpl::GetSessionMetricsHelper() {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  SessionMetricsHelper* metrics_helper =
      SessionMetricsHelper::FromWebContents(web_contents);
  if (!metrics_helper) {
    // This will only happen if we are not already in VR; set start params
    // accordingly.
    metrics_helper = SessionMetricsHelper::CreateForWebContents(web_contents);
  }

  return metrics_helper;
}

void VRServiceImpl::OnSessionCreated(
    SessionRequestData request,
    device::mojom::XRSessionPtr session,
    mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
        session_metrics_recorder,
    mojo::PendingRemote<device::mojom::WebXrInternalsRendererListener>
        xr_internals_listener) {
  DVLOG(2) << __func__ << ": session_runtime_id=" << request.runtime_id;

  // Not checking for validity of |session|, since that's done by
  // |OnInlineSessionCreated| and |OnImmersiveSessionCreated|.

  UMA_HISTOGRAM_ENUMERATION("XR.RuntimeUsed", request.runtime_id);

  TRACE_EVENT("xr", "VRServiceImpl::OnSessionCreated: succeeded",
              perfetto::Flow::Global(request.options->trace_id));

  mojo::Remote<device::mojom::XRSessionClient> client;
  session->client_receiver = client.BindNewPipeAndPassReceiver();

  client->OnVisibilityStateChanged(visibility_state_);
  session_clients_.Add(std::move(client));

  auto success = device::mojom::RequestSessionSuccess::New();
  success->session = std::move(session);
  success->metrics_recorder = std::move(session_metrics_recorder);
  success->trace_id = request.options->trace_id;
  success->xr_internals_listener = std::move(xr_internals_listener);

  std::move(request.callback)
      .Run(device::mojom::RequestSessionResult::NewSuccess(std::move(success)));
}

void VRServiceImpl::RequestSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::RequestSessionCallback callback) {
  DVLOG(2) << __func__;
  DCHECK(options);

  webxr::mojom::SessionRequestedRecordPtr session_requested_record =
      webxr::mojom::SessionRequestedRecord::New();
  session_requested_record->options = options->Clone();
  session_requested_record->requested_time = base::Time::Now();
  runtime_manager_->GetLoggerManager().RecordSessionRequested(
      std::move(session_requested_record));

  // Queue the request to get to when initialization has completed.
  if (!initialization_complete_) {
    DVLOG(2) << __func__ << ": initialization not yet complete, defer request";
    pending_requests_.push_back(
        base::BindOnce(&VRServiceImpl::RequestSession, base::Unretained(this),
                       std::move(options), std::move(callback)));
    return;
  }

  if (runtime_manager_->IsOtherClientPresenting(this) ||
      runtime_manager_->HasPendingImmersiveRequest()) {
    DVLOG(2) << __func__
             << ": can't create sessions while an immersive session exists";

    // Can't create sessions while an immersive session exists.
    RejectSession(
        std::move(callback), options->trace_id,
        device::mojom::RequestSessionError::EXISTING_IMMERSIVE_SESSION,
        "There is an existing immersive session.");
    return;
  }

  auto* runtime = runtime_manager_->GetRuntimeForOptions(options.get());
  if (!runtime) {
    RejectSession(std::move(callback), options->trace_id,
                  device::mojom::RequestSessionError::NO_RUNTIME_FOUND,
                  "No runtime found for the given session options.");
    return;
  }

  const bool has_user_activation =
      render_frame_host_->HasTransientUserActivation();
  if (!has_user_activation) {
    // User activation is verified blink-side, so this should never fail
    // (everything that happens up to this point should not take enough time for
    // the user activation to expire). Treat lack of user activation as unknown
    // failure:
    RejectSession(std::move(callback), options->trace_id,
                  device::mojom::RequestSessionError::UNKNOWN_FAILURE,
                  "Missing user activation.");
    return;
  }

  // The consent flow cannot differentiate between optional and required
  // features, but we don't need to block creation if an optional feature is
  // not supported. Remove all unsupported optional features from the
  // optional_features collection before handing it off.
  std::erase_if(options->optional_features, [runtime](auto& feature) {
    return !runtime->SupportsFeature(feature);
  });

  SessionRequestData request(std::move(options), std::move(callback),
                             runtime->GetId());

  GetPermissionStatus(std::move(request), runtime);
}

void VRServiceImpl::DoRequestPermissions(
    const std::vector<blink::PermissionType> request_permissions,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        result_callback) {
  PermissionController* permission_controller =
      GetWebContents()->GetBrowserContext()->GetPermissionController();
  CHECK(permission_controller);

  permission_controller->RequestPermissionsFromCurrentDocument(
      render_frame_host_,
      PermissionRequestDescription(request_permissions,
                                   /*user_gesture=*/true),
      std::move(result_callback));
}

void VRServiceImpl::GetPermissionStatus(SessionRequestData request,
                                        BrowserXRRuntimeImpl* runtime) {
  DVLOG(2) << __func__;
  DCHECK(request.options);
  DCHECK(runtime);
  DCHECK_EQ(runtime->GetId(), request.runtime_id);

#if BUILDFLAG(ENABLE_OPENXR)
  if (request.options->mode == device::mojom::XRSessionMode::kImmersiveAr &&
      runtime->GetId() == device::mojom::XRDeviceId::OPENXR_DEVICE_ID) {
    DCHECK(device::features::IsOpenXrArEnabled());
  }
#endif

  // Need to calculate the permissions before the call below, as otherwise
  // std::move nulls options out before `GetRequiredPermissions()` runs.
  const std::vector<blink::PermissionType> permissions_for_mode =
      GetRequiredPermissionsForMode(request.options->mode);

  DoRequestPermissions(
      permissions_for_mode,
      base::BindOnce(&VRServiceImpl::OnPermissionResultsForMode,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     permissions_for_mode));
}

void VRServiceImpl::OnPermissionResultsForMode(
    SessionRequestData request,
    const std::vector<blink::PermissionType>& permissions,
    const std::vector<blink::mojom::PermissionStatus>& permission_statuses) {
  DVLOG(2) << __func__ << ": permissions.size()=" << permissions.size();
  DCHECK_EQ(permissions.size(), permission_statuses.size());

  // Prolong the user activation since the user may have taken long enough to
  // answer the permission prompts that the transient user activation expired.
  // This is fine to do here, since we enforce that the activation existed prior
  // to requesting permissions.
  DVLOG(3) << __func__ << ": prolonging user activation, current status="
           << render_frame_host_->HasTransientUserActivation();
  render_frame_host_->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kInteraction);

  const XrPermissionResults permission_results(permissions,
                                               permission_statuses);

  bool is_consent_granted =
      permission_results.HasPermissionsFor(request.options->mode);
  DVLOG(2) << __func__ << ": is_consent_granted=" << is_consent_granted;

  if (!is_consent_granted) {
    RejectSession(std::move(request.callback), request.options->trace_id,
                  device::mojom::RequestSessionError::USER_DENIED_CONSENT,
                  "Consent was not granted for the requested mode.");
    return;
  }

  const std::vector<blink::PermissionType> permissions_for_features =
      GetRequiredPermissionsForFeatures(request.required_features,
                                        request.optional_features);

  auto result_callback =
      base::BindOnce(&VRServiceImpl::OnPermissionResultsForFeatures,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     permissions_for_features);
  if (permissions_for_features.empty()) {
    std::move(result_callback).Run({});
    return;
  }

  DoRequestPermissions(permissions_for_features, std::move(result_callback));
}

void VRServiceImpl::OnPermissionResultsForFeatures(
    SessionRequestData request,
    const std::vector<blink::PermissionType>& permissions,
    const std::vector<blink::mojom::PermissionStatus>& permission_statuses) {
  const XrPermissionResults permission_results(permissions,
                                               permission_statuses);

  std::unordered_set<device::mojom::XRSessionFeature> rejected_features;
  for (auto& required_feature : request.required_features) {
    if (!permission_results.HasPermissionsFor(required_feature)) {
      DVLOG(1) << __func__ << ": required_feature=" << required_feature
               << " lacks neccessary permissions";

      rejected_features.insert(required_feature);
    }
  }

  if (!rejected_features.empty()) {
    RejectSession(std::move(request.callback), request.options->trace_id,
                  device::mojom::RequestSessionError::USER_DENIED_CONSENT,
                  "Lacks necessary permissions for the required feature.",
                  &rejected_features);
    return;
  }

  std::unordered_set<device::mojom::XRSessionFeature> granted_optional_features;

  for (auto& optional_feature : request.optional_features) {
    if (permission_results.HasPermissionsFor(optional_feature)) {
      granted_optional_features.insert(optional_feature);
    } else {
      DVLOG(2) << __func__ << ": optional_feature=" << optional_feature
               << " lacks neccessary permissions";
    }
  }

  // Replace optional features on the request with the ones that have been
  // granted by the user:
  std::swap(request.optional_features, granted_optional_features);

  // Re-check for another client instance after a potential user consent.
  if (runtime_manager_->IsOtherClientPresenting(this)) {
    // Can't create sessions while an immersive session exists.
    RejectSession(
        std::move(request.callback), request.options->trace_id,
        device::mojom::RequestSessionError::EXISTING_IMMERSIVE_SESSION,
        "Another client started presenting while waiting for permissions.");
    return;
  }

  EnsureRuntimeInstalled(std::move(request), nullptr);
}

void VRServiceImpl::EnsureRuntimeInstalled(SessionRequestData request,
                                           BrowserXRRuntimeImpl* runtime) {
  DVLOG(2) << __func__;

  // If we were not provided the runtime, try to get it again.
  if (!runtime)
    runtime = runtime_manager_->GetRuntimeForOptions(request.options.get());

  // Ensure that it's the same runtime as the one we expect.
  if (!runtime || runtime->GetId() != request.runtime_id) {
    DVLOG(1) << __func__
             << ": failed to obtain the runtime or the runtime id does not "
                "match the expected ID, request.runtime_id="
             << request.runtime_id;

    RejectSession(std::move(request.callback), request.options->trace_id,
                  device::mojom::RequestSessionError::RUNTIMES_CHANGED,
                  "failed to obtain the runtime or the runtime id does not "
                  "match the expected ID.");
    return;
  }

  runtime->EnsureInstalled(
      render_frame_host_->GetProcess()->GetID(),
      render_frame_host_->GetRoutingID(),
      base::BindOnce(&VRServiceImpl::OnInstallResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request)));
}

void VRServiceImpl::OnInstallResult(SessionRequestData request,
                                    bool install_succeeded) {
  DVLOG(2) << __func__ << ": install_succeeded=" << install_succeeded;

  if (!install_succeeded) {
    RejectSession(std::move(request.callback), request.options->trace_id,
                  device::mojom::RequestSessionError::RUNTIME_INSTALL_FAILURE,
                  "Runtime installation failed.");
    return;
  }

  // Prolong the user activation since the user may have taken long enough to
  // install the runtime that the transient user activation expired. This is
  // fine to do here, since we enforce that the activation existed prior to
  // kicking off installation.
  DVLOG(3) << __func__ << ": prolonging user activation, current status="
           << render_frame_host_->HasTransientUserActivation();
  render_frame_host_->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kInteraction);

  DoRequestSession(std::move(request));
}

void VRServiceImpl::DoRequestSession(SessionRequestData request) {
  DVLOG(2) << __func__;
  // Get the runtime again, since we're running in an async context
  // and the pointer returned from `GetRuntimeForOptions` is non-owning.
  auto* runtime = runtime_manager_->GetRuntimeForOptions(request.options.get());

  // Ensure that it's the same runtime as the one we expect.
  if (!runtime || runtime->GetId() != request.runtime_id) {
    TRACE_EVENT("xr", "VRServiceImpl::DoRequestSession: mismatching runtime",
                perfetto::Flow::Global(request.options->trace_id));

    RejectSession(std::move(request.callback), request.options->trace_id,
                  device::mojom::RequestSessionError::UNKNOWN_RUNTIME_ERROR,
                  "Mismatching runtime or invalid runtime.");
    return;
  }

  TRACE_EVENT_INSTANT1("xr", "GetRuntimeForOptions", TRACE_EVENT_SCOPE_THREAD,
                       "id", request.runtime_id);

  auto runtime_options = GetRuntimeOptions(request.options.get());
  // Make the resolved enabled features available to the runtime.

  runtime_options->required_features.assign(request.required_features.begin(),
                                            request.required_features.end());
  runtime_options->optional_features.assign(request.optional_features.begin(),
                                            request.optional_features.end());

  if constexpr (BUILDFLAG(IS_ANDROID)) {
    bool send_renderer_information = false;
#if BUILDFLAG(ENABLE_ARCORE)
    send_renderer_information =
        send_renderer_information ||
        request.runtime_id == device::mojom::XRDeviceId::ARCORE_DEVICE_ID;
#endif
#if BUILDFLAG(ENABLE_CARDBOARD)
    send_renderer_information =
        send_renderer_information ||
        request.runtime_id == device::mojom::XRDeviceId::CARDBOARD_DEVICE_ID;
#endif
#if BUILDFLAG(ENABLE_OPENXR) && BUILDFLAG(IS_ANDROID)
    send_renderer_information =
        send_renderer_information ||
        request.runtime_id == device::mojom::XRDeviceId::OPENXR_DEVICE_ID;
#endif
    if (send_renderer_information) {
      runtime_options->render_process_id =
          render_frame_host_->GetProcess()->GetID();
      runtime_options->render_frame_id = render_frame_host_->GetRoutingID();
    }
  }

  bool use_dom_overlay =
      base::Contains(runtime_options->required_features,
                     device::mojom::XRSessionFeature::DOM_OVERLAY) ||
      base::Contains(runtime_options->optional_features,
                     device::mojom::XRSessionFeature::DOM_OVERLAY);

  if (use_dom_overlay) {
    // Tell RenderFrameHostImpl that we're setting up the WebXR DOM Overlay,
    // it checks for this in EnterFullscreen via HasSeenRecentXrOverlaySetup().
    render_frame_host_->SetIsXrOverlaySetup();
  }

  if (device::XRSessionModeUtils::IsImmersive(runtime_options->mode)) {
    if (!request.options->tracked_images.empty()) {
      DVLOG(3) << __func__ << ": request.options->tracked_images.size()="
               << request.options->tracked_images.size();
      runtime_options->tracked_images.resize(
          request.options->tracked_images.size());
      for (std::size_t i = 0; i < request.options->tracked_images.size(); ++i) {
        runtime_options->tracked_images[i] =
            request.options->tracked_images[i].Clone();
      }
    }

    runtime_options->depth_options = std::move(request.options->depth_options);

    auto immersive_callback =
        base::BindOnce(&VRServiceImpl::OnImmersiveSessionCreated,
                       weak_ptr_factory_.GetWeakPtr(), std::move(request));

    runtime->RequestImmersiveSession(this, std::move(runtime_options),
                                     std::move(immersive_callback));
  } else {
    auto non_immersive_callback =
        base::BindOnce(&VRServiceImpl::OnInlineSessionCreated,
                       weak_ptr_factory_.GetWeakPtr(), std::move(request));
    runtime->RequestInlineSession(std::move(runtime_options),
                                  std::move(non_immersive_callback));
  }
}

void VRServiceImpl::SupportsSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::SupportsSessionCallback callback) {
  if (!initialization_complete_) {
    pending_requests_.push_back(
        base::BindOnce(&VRServiceImpl::SupportsSession, base::Unretained(this),
                       std::move(options), std::move(callback)));
    return;
  }

  TRACE_EVENT("xr", "VRServiceImpl::SupportsSession: received",
              perfetto::Flow::Global(options->trace_id));

  runtime_manager_->SupportsSession(std::move(options), std::move(callback));
}

void VRServiceImpl::ExitPresent(ExitPresentCallback on_exited) {
  BrowserXRRuntimeImpl* immersive_runtime =
      runtime_manager_->GetCurrentlyPresentingImmersiveRuntime();
  DVLOG(2) << __func__ << ": !!immersive_runtime=" << !!immersive_runtime;
  if (immersive_runtime) {
    on_exit_present_ = std::move(on_exited);
    immersive_runtime->ExitPresent(this);
  } else {
    std::move(on_exited).Run();
  }
}

void VRServiceImpl::SetFramesThrottled(bool throttled) {
  if (throttled != frames_throttled_) {
    frames_throttled_ = throttled;
    BrowserXRRuntimeImpl* immersive_runtime =
        runtime_manager_->GetCurrentlyPresentingImmersiveRuntime();
    if (immersive_runtime) {
      immersive_runtime->SetFramesThrottled(this, frames_throttled_);
    }
  }
}

void VRServiceImpl::MakeXrCompatible(
    device::mojom::VRService::MakeXrCompatibleCallback callback) {
  if (!initialization_complete_) {
    pending_requests_.push_back(base::BindOnce(&VRServiceImpl::MakeXrCompatible,
                                               base::Unretained(this),
                                               std::move(callback)));
    return;
  }

  xr_compatible_callbacks_.emplace_back(std::move(callback));

  // Only request compatibility if there aren't any pending calls.
  // OnMakeXrCompatibleComplete will run all callbacks.
  if (xr_compatible_callbacks_.size() == 1)
    runtime_manager_->MakeXrCompatible();
}

void VRServiceImpl::OnMakeXrCompatibleComplete(
    device::mojom::XrCompatibleResult result) {
  for (XrCompatibleCallback& wrapper : xr_compatible_callbacks_)
    std::move(wrapper.callback).Run(result);

  xr_compatible_callbacks_.clear();
}

void VRServiceImpl::OnExitPresent() {
  DVLOG(2) << __func__;

  // Clear any XrRenderTarget that may have been set.
  viz::FrameSinkId default_frame_sink_id;
  static_cast<WebContentsImpl*>(GetWebContents())
      ->OnXrHasRenderTarget(default_frame_sink_id);

  GetSessionMetricsHelper()->StopAndRecordImmersiveSession();

  if (on_exit_present_) {
    std::move(on_exit_present_).Run();
  }

  for (auto& client : session_clients_) {
    // https://crbug.com/1160940 has a fairly generic callstack, in mojom
    // generated code, which appears to aggregate a few different actual crashes
    // into the same bug. For the crashes that appear to be our fault, the
    // common "start" is this call. By causing a CHECK here instead of in the
    // mojom generated code, we can isolate our crashes.
    CHECK(client);
    client->OnExitPresent();
  }

  // Ensure that the client list is erased to avoid "Cannot issue Interface
  // method calls on an unbound Remote" errors: https://crbug.com/991747
  session_clients_.Clear();
}

void VRServiceImpl::OnVisibilityStateChanged(
    device::mojom::XRVisibilityState visiblity_state) {
  visibility_state_ = visiblity_state;
  for (auto& client : session_clients_)
    client->OnVisibilityStateChanged(visiblity_state);
}

content::WebContents* VRServiceImpl::GetWebContents() {
  return content::WebContents::FromRenderFrameHost(render_frame_host_);
}

}  // namespace content
