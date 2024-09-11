// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_SERVICE_VR_SERVICE_IMPL_H_
#define CONTENT_BROWSER_XR_SERVICE_VR_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "content/browser/xr/metrics/session_metrics_helper.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom-forward.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_device.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-forward.h"

namespace blink {
enum class PermissionType;
}

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace content {

class XRRuntimeManagerImpl;
class XRRuntimeManagerTest;
class BrowserXRRuntimeImpl;

// Browser process implementation of the VRService mojo interface. Instantiated
// through Mojo once the user loads a page containing WebXR.
class CONTENT_EXPORT VRServiceImpl : public device::mojom::VRService,
                                     content::WebContentsObserver {
 public:
  explicit VRServiceImpl(content::RenderFrameHost* render_frame_host);

  // Constructor for tests.
  explicit VRServiceImpl(base::PassKey<XRRuntimeManagerTest>);

  VRServiceImpl(const VRServiceImpl&) = delete;
  VRServiceImpl& operator=(const VRServiceImpl&) = delete;

  ~VRServiceImpl() override;

  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<device::mojom::VRService> receiver);

  // device::mojom::VRService implementation
  void SetClient(mojo::PendingRemote<device::mojom::VRServiceClient>
                     service_client) override;
  void RequestSession(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::VRService::RequestSessionCallback callback) override;
  void SupportsSession(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::VRService::SupportsSessionCallback callback) override;
  void ExitPresent(ExitPresentCallback on_exited) override;
  void SetFramesThrottled(bool throttled) override;
  void MakeXrCompatible(
      device::mojom::VRService::MakeXrCompatibleCallback callback) override;

  void InitializationComplete();

  // Called when inline session gets disconnected. |session_id| is the value
  // returned by |magic_window_controllers_| when adding session controller to
  // it.
  void OnInlineSessionDisconnected(mojo::RemoteSetElementId session_id);

  // Notifications/calls from BrowserXRRuntimeImpl:
  void OnExitPresent();
  void OnVisibilityStateChanged(
      device::mojom::XRVisibilityState visibility_state);
  void RuntimesChanged();
  void OnMakeXrCompatibleComplete(device::mojom::XrCompatibleResult result);

  base::WeakPtr<VRServiceImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  content::WebContents* GetWebContents();

 private:
  struct SessionRequestData {
    device::mojom::VRService::RequestSessionCallback callback;
    std::unordered_set<device::mojom::XRSessionFeature> required_features;
    std::unordered_set<device::mojom::XRSessionFeature> optional_features;
    device::mojom::XRSessionOptionsPtr options;
    device::mojom::XRDeviceId runtime_id;

    SessionRequestData(
        device::mojom::XRSessionOptionsPtr options,
        device::mojom::VRService::RequestSessionCallback callback,
        device::mojom::XRDeviceId runtime_id);
    ~SessionRequestData();
    SessionRequestData(SessionRequestData&&);

   private:
    SessionRequestData(const SessionRequestData&) = delete;
    SessionRequestData& operator=(const SessionRequestData&) = delete;
  };

  // Wrapper around MakeXrCompatibleCallback so that the callback gets executed
  // on destruction if it hasn't already. Otherwise, mojom throws a DCHECK if
  // the callback is not executed before being destroyed.
  struct XrCompatibleCallback {
    device::mojom::VRService::MakeXrCompatibleCallback callback;

    explicit XrCompatibleCallback(
        device::mojom::VRService::MakeXrCompatibleCallback callback);
    XrCompatibleCallback(XrCompatibleCallback&& wrapper);
    ~XrCompatibleCallback();
  };

  // content::WebContentsObserver implementation
  void OnWebContentsFocused(content::RenderWidgetHost* host) override;
  void OnWebContentsLostFocus(content::RenderWidgetHost* host) override;
  void RenderFrameDeleted(content::RenderFrameHost* host) override;

  void OnWebContentsFocusChanged(content::RenderWidgetHost* host, bool focused);

  void ResolvePendingRequests();

  // Returns currently active instance of SessionMetricsHelper from WebContents.
  // If the instance is not present on WebContents, it will be created with the
  // assumption that we are not already in VR.
  SessionMetricsHelper* GetSessionMetricsHelper();

  bool InternalSupportsSession(device::mojom::XRSessionOptions* options);

  void DoRequestPermissions(
      const std::vector<blink::PermissionType> request_permissions,
      base::OnceCallback<void(
          const std::vector<blink::mojom::PermissionStatus>&)> result_callback);

  // The following steps are ordered in the general flow for "RequestSession"
  // GetPermissionStatus will result in a call to OnPermissionResult which then
  // calls EnsureRuntimeInstalled (with a callback to OnInstallResult), which
  // then feeds into DoRequestSession, which will continue with OnInline or
  // OnImmersive SessionCreated depending on the type of session created.
  void GetPermissionStatus(SessionRequestData request,
                           BrowserXRRuntimeImpl* runtime);

  void OnPermissionResultsForMode(
      SessionRequestData request,
      const std::vector<blink::PermissionType>& permissions,
      const std::vector<blink::mojom::PermissionStatus>& permission_statuses);

  void OnPermissionResultsForFeatures(
      SessionRequestData request,
      const std::vector<blink::PermissionType>& permissions,
      const std::vector<blink::mojom::PermissionStatus>& permission_statuses);

  void EnsureRuntimeInstalled(SessionRequestData request,
                              BrowserXRRuntimeImpl* runtime);
  void OnInstallResult(SessionRequestData request_data, bool install_succeeded);

  void DoRequestSession(SessionRequestData request);

  void OnInlineSessionCreated(
      SessionRequestData request,
      device::mojom::XRRuntimeSessionResultPtr session_result);
  void OnImmersiveSessionCreated(
      SessionRequestData request,
      device::mojom::XRRuntimeSessionResultPtr session_result);
  void OnSessionCreated(
      SessionRequestData request,
      device::mojom::XRSessionPtr session,
      mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
          session_metrics_recorder,
      mojo::PendingRemote<device::mojom::WebXrInternalsRendererListener>
          xr_internals_listener);

  mojo::PendingRemote<device::mojom::WebXrInternalsRendererListener>
  WebXrInternalsRendererListener();

  ExitPresentCallback on_exit_present_;

  scoped_refptr<XRRuntimeManagerImpl> runtime_manager_;
  mojo::RemoteSet<device::mojom::XRSessionClient> session_clients_;
  mojo::Remote<device::mojom::VRServiceClient> service_client_;
  raw_ptr<content::RenderFrameHost> render_frame_host_;
  mojo::SelfOwnedReceiverRef<device::mojom::VRService> receiver_;
  mojo::RemoteSet<device::mojom::XRSessionController> magic_window_controllers_;
  device::mojom::XRVisibilityState visibility_state_ =
      device::mojom::XRVisibilityState::VISIBLE;

  // List of callbacks to run when initialization is completed.
  std::vector<base::OnceCallback<void()>> pending_requests_;

  bool initialization_complete_ = false;
  bool in_focused_frame_ = false;
  bool frames_throttled_ = false;

  std::vector<XrCompatibleCallback> xr_compatible_callbacks_;

  base::WeakPtrFactory<VRServiceImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_SERVICE_VR_SERVICE_IMPL_H_
