// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/vr_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/trace_event/common/trace_event_common.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/xr/metrics/session_metrics_helper.h"
#include "content/browser/xr/service/browser_xr_runtime_impl.h"
#include "content/browser/xr/service/xr_runtime_manager_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/cpp/session_mode.h"

namespace {

device::mojom::XRRuntimeSessionOptionsPtr GetRuntimeOptions(
    device::mojom::XRSessionOptions* options) {
  device::mojom::XRRuntimeSessionOptionsPtr runtime_options =
      device::mojom::XRRuntimeSessionOptions::New();
  runtime_options->mode = options->mode;
  return runtime_options;
}

std::vector<content::PermissionType> GetRequiredPermissions(
    device::mojom::XRSessionMode mode,
    const std::set<device::mojom::XRSessionFeature>& enabled_features) {
  std::vector<content::PermissionType> permissions;

  switch (mode) {
    case device::mojom::XRSessionMode::kInline:
      permissions.push_back(content::PermissionType::SENSORS);
      break;
    case device::mojom::XRSessionMode::kImmersiveVr:
      permissions.push_back(content::PermissionType::VR);
      break;
    case device::mojom::XRSessionMode::kImmersiveAr:
      permissions.push_back(content::PermissionType::AR);
      break;
  }

  if (base::Contains(enabled_features,
                     device::mojom::XRSessionFeature::CAMERA_ACCESS)) {
    permissions.push_back(content::PermissionType::VIDEO_CAPTURE);
  }

  return permissions;
}

}  // namespace

namespace content {

VRServiceImpl::SessionRequestData::SessionRequestData(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::RequestSessionCallback callback,
    std::set<device::mojom::XRSessionFeature> enabled_features,
    device::mojom::XRDeviceId runtime_id)
    : options(std::move(options)),
      callback(std::move(callback)),
      enabled_features(std::move(enabled_features)),
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
VRServiceImpl::VRServiceImpl(util::PassKey<XRRuntimeManagerTest>)
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
    device::mojom::XRSessionPtr session,
    mojo::PendingRemote<device::mojom::XRSessionController>
        pending_controller) {
  if (!session) {
    std::move(request.callback)
        .Run(device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::UNKNOWN_RUNTIME_ERROR));
    return;
  }

  mojo::Remote<device::mojom::XRSessionController> controller(
      std::move(pending_controller));
  // Start giving out magic window data if we are focused.
  controller->SetFrameDataRestricted(!in_focused_frame_);

  auto id = magic_window_controllers_.Add(std::move(controller));
  DVLOG(2) << __func__ << ": session_id=" << id.GetUnsafeValue()
           << " runtime_id=" << request.runtime_id;

  mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
      session_metrics_recorder = GetSessionMetricsHelper()->StartInlineSession(
          *(request.options), request.enabled_features, id.GetUnsafeValue());

  OnSessionCreated(std::move(request), std::move(session),
                   std::move(session_metrics_recorder));
}

void VRServiceImpl::OnImmersiveSessionCreated(
    SessionRequestData request,
    device::mojom::XRSessionPtr session) {
  DCHECK(request.options);
  if (!session) {
    std::move(request.callback)
        .Run(device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::UNKNOWN_RUNTIME_ERROR));
    return;
  }

  // Get the metrics tracker for the new immersive session
  mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
      session_metrics_recorder =
          GetSessionMetricsHelper()->StartImmersiveSession(
              *(request.options), request.enabled_features);

  OnSessionCreated(std::move(request), std::move(session),
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

  mojo::Remote<device::mojom::XRSessionClient> client;
  session->client_receiver = client.BindNewPipeAndPassReceiver();

  session->enabled_features.clear();
  for (const auto& feature : request.enabled_features) {
    session->enabled_features.push_back(feature);
  }

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

  // GetRuntimeForOptions should only return a device that supports all required
  // features.
  std::set<device::mojom::XRSessionFeature> requested_features;
  for (const auto& feature : options->required_features) {
    DVLOG(2) << __func__ << ": required_feature=" << feature;
    requested_features.insert(feature);
  }

  // The consent flow cannot differentiate between optional and required
  // features, but we don't need to block creation if an optional feature is
  // not supported. Add all requested features to the set of supported features.
  for (const auto& feature : options->optional_features) {
    if (runtime->SupportsFeature(feature)) {
      requested_features.insert(feature);
    }
  }

  SessionRequestData request(std::move(options), std::move(callback),
                             std::move(requested_features), runtime->GetId());

  GetPermissionStatus(std::move(request), runtime);
}

void VRServiceImpl::GetPermissionStatus(SessionRequestData request,
                                        BrowserXRRuntimeImpl* runtime) {
  DVLOG(2) << __func__;
  DCHECK(request.options);
  DCHECK(runtime);
  DCHECK_EQ(runtime->GetId(), request.runtime_id);

#if defined(OS_WIN)
  DCHECK_NE(request.options->mode, device::mojom::XRSessionMode::kImmersiveAr);
#endif

  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(
          GetWebContents()->GetBrowserContext());
  DCHECK(permission_controller);

  // Need to calculate the permissions before the call below, as otherwise
  // std::move nulls options out before GetRequiredPermissions runs.
  const std::vector<PermissionType> permissions =
      GetRequiredPermissions(request.options->mode, request.enabled_features);
  permission_controller->RequestPermissions(
      permissions, render_frame_host_,
      render_frame_host_->GetLastCommittedURL(), true,
      base::BindOnce(&VRServiceImpl::OnPermissionResults,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request)));
}

void VRServiceImpl::OnPermissionResults(
    SessionRequestData request,
    const std::vector<blink::mojom::PermissionStatus>& permission_statuses) {
  DVLOG(2) << __func__;
  bool is_consent_granted = true;
  for (auto& permission_status : permission_statuses) {
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
    std::move(request.callback)
        .Run(device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::UNKNOWN_RUNTIME_ERROR));
    return;
  }

  TRACE_EVENT_INSTANT1("xr", "GetRuntimeForOptions", TRACE_EVENT_SCOPE_THREAD,
                       "id", request.runtime_id);

  auto runtime_options = GetRuntimeOptions(request.options.get());
  // Make the resolved enabled features available to the runtime.
  runtime_options->enabled_features.assign(request.enabled_features.begin(),
                                           request.enabled_features.end());

#if defined(OS_ANDROID) && BUILDFLAG(ENABLE_ARCORE)
  if (request.runtime_id == device::mojom::XRDeviceId::ARCORE_DEVICE_ID) {
    runtime_options->render_process_id =
        render_frame_host_->GetProcess()->GetID();
    runtime_options->render_frame_id = render_frame_host_->GetRoutingID();
  }
#endif

  bool use_dom_overlay =
      base::Contains(runtime_options->enabled_features,
                     device::mojom::XRSessionFeature::DOM_OVERLAY);

  if (use_dom_overlay) {
    // Tell RenderFrameHostImpl that we're setting up the WebXR DOM Overlay,
    // it checks for this in EnterFullscreen via HasSeenRecentXrOverlaySetup().
    render_frame_host_->SetIsXrOverlaySetup();
  }

  if (device::XRSessionModeUtils::IsImmersive(runtime_options->mode)) {
    base::OnceCallback<void(device::mojom::XRSessionPtr)> immersive_callback =
        base::BindOnce(&VRServiceImpl::OnImmersiveSessionCreated,
                       weak_ptr_factory_.GetWeakPtr(), std::move(request));
    runtime->RequestSession(this, std::move(runtime_options),
                            std::move(immersive_callback));
  } else {
    base::OnceCallback<void(
        device::mojom::XRSessionPtr,
        mojo::PendingRemote<device::mojom::XRSessionController>)>
        non_immersive_callback =
            base::BindOnce(&VRServiceImpl::OnInlineSessionCreated,
                           weak_ptr_factory_.GetWeakPtr(), std::move(request));
    runtime->GetRuntime()->RequestSession(std::move(runtime_options),
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

  GetSessionMetricsHelper()->StopAndRecordImmersiveSession();

  for (auto& client : session_clients_)
    client->OnExitPresent();

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
