// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/vr_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/trace_event/common/trace_event_common.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/xr/metrics/session_metrics_helper.h"
#include "content/browser/xr/service/browser_xr_runtime_impl.h"
#include "content/browser/xr/service/xr_runtime_manager_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "device/base/features.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/cpp/session_mode.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace {

device::mojom::XRRuntimeSessionOptionsPtr GetRuntimeOptions(
    device::mojom::XRSessionOptions* options) {
  device::mojom::XRRuntimeSessionOptionsPtr runtime_options =
      device::mojom::XRRuntimeSessionOptions::New();
  runtime_options->mode = options->mode;
  return runtime_options;
}

std::vector<blink::PermissionType> GetRequiredPermissions(
    device::mojom::XRSessionMode mode,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        required_features,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        optional_features) {
  std::vector<blink::PermissionType> permissions;

  switch (mode) {
    case device::mojom::XRSessionMode::kInline:
      permissions.push_back(blink::PermissionType::SENSORS);
      break;
    case device::mojom::XRSessionMode::kImmersiveVr:
      permissions.push_back(blink::PermissionType::VR);
      break;
    case device::mojom::XRSessionMode::kImmersiveAr:
      permissions.push_back(blink::PermissionType::AR);
      break;
  }

  if (base::Contains(required_features,
                     device::mojom::XRSessionFeature::CAMERA_ACCESS) ||
      base::Contains(optional_features,
                     device::mojom::XRSessionFeature::CAMERA_ACCESS)) {
    permissions.push_back(blink::PermissionType::VIDEO_CAPTURE);
  }

  return permissions;
}

bool AreAllRequiredFeaturesEnabled(
    const std::unordered_set<device::mojom::XRSessionFeature>& enabled_features,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        required_features) {
  DVLOG(3) << __func__
           << ": enabled_features.size()=" << enabled_features.size();
  return base::ranges::all_of(required_features, [&enabled_features](
                                                     const auto&
                                                         required_feature) {
    if (!base::Contains(enabled_features, required_feature)) {
      DVLOG(2)
          << __func__
          << ": one of the required features was not enabled on the created "
             "session, feature: "
          << required_feature;
      return false;
    }

    return true;
  });
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
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::UNKNOWN_FAILURE));
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

  runtime_manager_ = XRRuntimeManagerImpl::GetOrCreateInstance();
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
  runtime_manager_ = XRRuntimeManagerImpl::GetOrCreateInstance();
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

void VRServiceImpl::OnDisplayInfoChanged() {
  device::mojom::VRDisplayInfoPtr display_info =
      runtime_manager_->GetCurrentVRDisplayInfo(this);
  if (display_info) {
    for (auto& client : session_clients_)
      client->OnChanged(display_info.Clone());
  }
}

void VRServiceImpl::RuntimesChanged() {
  DVLOG(2) << __func__;
  OnDisplayInfoChanged();

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

    std::move(request.callback)
        .Run(device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::UNKNOWN_RUNTIME_ERROR));
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

  if (!AreAllRequiredFeaturesEnabled(enabled_features,
                                     request.required_features)) {
    // UNKNOWN_FAILURE since a runtime should not return a session if there
    // exists a required feature that was not enabled - this would signify a bug
    // in the runtime.

    TRACE_EVENT(
        "xr",
        "VRServiceImpl::OnInlineSessionCreated: required feature not granted",
        perfetto::Flow::Global(request.options->trace_id));

    std::move(request.callback)
        .Run(device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::UNKNOWN_FAILURE));
    return;
  }

  mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
      session_metrics_recorder = GetSessionMetricsHelper()->StartInlineSession(
          *(request.options), enabled_features, id.GetUnsafeValue());

  OnSessionCreated(std::move(request), std::move(session_result->session),
                   std::move(session_metrics_recorder));
}

void VRServiceImpl::OnImmersiveSessionCreated(
    SessionRequestData request,
    device::mojom::XRRuntimeSessionResultPtr session_result) {
  DCHECK(request.options);
  if (!session_result) {
    TRACE_EVENT("xr",
                "VRServiceImpl::OnImmersiveSessionCreated: no session_result",
                perfetto::Flow::Global(request.options->trace_id));

    std::move(request.callback)
        .Run(device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::UNKNOWN_RUNTIME_ERROR));
    return;
  }

  auto* session = session_result->session.get();
  std::unordered_set<device::mojom::XRSessionFeature> enabled_features(
      session->enabled_features.begin(), session->enabled_features.end());

  if (!AreAllRequiredFeaturesEnabled(enabled_features,
                                     request.required_features)) {
    // UNKNOWN_FAILURE since a runtime should not return a session if there
    // exists a required feature that was not enabled - this would signify a bug
    // in the runtime.

    TRACE_EVENT("xr",
                "VRServiceImpl::OnImmersiveSessionCreated: required feature "
                "not granted",
                perfetto::Flow::Global(request.options->trace_id));

    std::move(request.callback)
        .Run(device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::UNKNOWN_FAILURE));
    return;
  }

  // Get the metrics tracker for the new immersive session
  mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
      session_metrics_recorder =
          GetSessionMetricsHelper()->StartImmersiveSession(*(request.options),
                                                           enabled_features);

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
                   std::move(session_metrics_recorder));
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
        session_metrics_recorder) {
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

  std::move(request.callback)
      .Run(device::mojom::RequestSessionResult::NewSuccess(std::move(success)));
}

void VRServiceImpl::RequestSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::RequestSessionCallback callback) {
  DVLOG(2) << __func__;
  DCHECK(options);

  // Queue the request to get to when initialization has completed.
  if (!initialization_complete_) {
    DVLOG(2) << __func__ << ": initialization not yet complete, defer request";
    pending_requests_.push_back(
        base::BindOnce(&VRServiceImpl::RequestSession, base::Unretained(this),
                       std::move(options), std::move(callback)));
    return;
  }

  if (runtime_manager_->IsOtherClientPresenting(this)) {
    DVLOG(2) << __func__
             << ": can't create sessions while an immersive session exists";
    // Can't create sessions while an immersive session exists.
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::EXISTING_IMMERSIVE_SESSION));
    return;
  }

  auto* runtime = runtime_manager_->GetRuntimeForOptions(options.get());
  if (!runtime) {
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::NO_RUNTIME_FOUND));
    return;
  }

  // The consent flow cannot differentiate between optional and required
  // features, but we don't need to block creation if an optional feature is
  // not supported. Remove all unsupported optional features from the
  // optional_features collection before handing it off.
  base::EraseIf(options->optional_features, [runtime](auto& feature) {
    return !runtime->SupportsFeature(feature);
  });

  SessionRequestData request(std::move(options), std::move(callback),
                             runtime->GetId());

  GetPermissionStatus(std::move(request), runtime);
}

void VRServiceImpl::GetPermissionStatus(SessionRequestData request,
                                        BrowserXRRuntimeImpl* runtime) {
  DVLOG(2) << __func__;
  DCHECK(request.options);
  DCHECK(runtime);
  DCHECK_EQ(runtime->GetId(), request.runtime_id);

#if BUILDFLAG(ENABLE_OPENXR)
  if (request.options->mode == device::mojom::XRSessionMode::kImmersiveAr) {
    DCHECK(
        base::FeatureList::IsEnabled(
            device::features::kOpenXrExtendedFeatureSupport));
  }
#endif

  PermissionController* permission_controller =
      GetWebContents()->GetBrowserContext()->GetPermissionController();
  DCHECK(permission_controller);

  // Need to calculate the permissions before the call below, as otherwise
  // std::move nulls options out before GetRequiredPermissions runs.
  const std::vector<blink::PermissionType> permissions =
      GetRequiredPermissions(request.options->mode, request.required_features,
                             request.optional_features);

  permission_controller->RequestPermissionsFromCurrentDocument(
      permissions, render_frame_host_, true,
      base::BindOnce(&VRServiceImpl::OnPermissionResults,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     permissions));
}

void VRServiceImpl::OnPermissionResults(
    SessionRequestData request,
    const std::vector<blink::PermissionType>& permissions,
    const std::vector<blink::mojom::PermissionStatus>& permission_statuses) {
  DVLOG(2) << __func__;
  DCHECK_EQ(permissions.size(), permission_statuses.size());

  bool is_consent_granted = true;
  for (size_t i = 0; i < permission_statuses.size(); ++i) {
    const blink::mojom::PermissionStatus& permission_status =
        permission_statuses[i];
    DVLOG(3) << __func__ << ": index=" << i
             << ", permission=" << base::to_underlying(permissions[i])
             << ", status=" << permission_status;
    if (permission_status != blink::mojom::PermissionStatus::GRANTED) {
      is_consent_granted = false;
      break;
    }
  }

  if (!is_consent_granted) {
    std::move(request.callback)
        .Run(device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::USER_DENIED_CONSENT));
    return;
  }

  // Re-check for another client instance after a potential user consent.
  if (runtime_manager_->IsOtherClientPresenting(this)) {
    // Can't create sessions while an immersive session exists.
    std::move(request.callback)
        .Run(device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::EXISTING_IMMERSIVE_SESSION));
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
    std::move(request.callback)
        .Run(device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::RUNTIMES_CHANGED));
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
    std::move(request.callback)
        .Run(device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::RUNTIME_INSTALL_FAILURE));
    return;
  }

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

    std::move(request.callback)
        .Run(device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::UNKNOWN_RUNTIME_ERROR));
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

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARCORE)
  if (request.runtime_id == device::mojom::XRDeviceId::ARCORE_DEVICE_ID) {
    runtime_options->render_process_id =
        render_frame_host_->GetProcess()->GetID();
    runtime_options->render_frame_id = render_frame_host_->GetRoutingID();
  }
#endif

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
    immersive_runtime->ExitPresent(this, std::move(on_exited));
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
