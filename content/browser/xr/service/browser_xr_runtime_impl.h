// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_SERVICE_BROWSER_XR_RUNTIME_IMPL_H_
#define CONTENT_BROWSER_XR_SERVICE_BROWSER_XR_RUNTIME_IMPL_H_

#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "content/browser/xr/service/vr_service_impl.h"
#include "content/public/browser/browser_xr_runtime.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/xr_integration_client.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "device/vr/public/mojom/xr_device.mojom-forward.h"
#include "device/vr/public/mojom/xr_session.mojom-forward.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

struct CHROME_LUID;

namespace content {
class XrInstallHelper;

// This class wraps a physical device's interfaces, and registers for events.
// There is one BrowserXRRuntimeImpl per physical device runtime. It manages
// browser-side handling of state, like which VRServiceImpl is listening for
// device activation.
class BrowserXRRuntimeImpl : public content::BrowserXRRuntime,
                             public device::mojom::XRRuntimeEventListener {
 public:
  using RequestSessionCallback =
      base::OnceCallback<void(device::mojom::XRRuntimeSessionResultPtr)>;
  explicit BrowserXRRuntimeImpl(
      device::mojom::XRDeviceId id,
      device::mojom::XRDeviceDataPtr device_data,
      mojo::PendingRemote<device::mojom::XRRuntime> runtime);
  ~BrowserXRRuntimeImpl() override;

  void ExitActiveImmersiveSession();
  bool SupportsFeature(device::mojom::XRSessionFeature feature) const;
  bool SupportsAllFeatures(
      const std::vector<device::mojom::XRSessionFeature>& features) const;

  bool SupportsCustomIPD() const;
  bool SupportsNonEmulatedHeight() const;
  bool SupportsArBlendMode();

  device::mojom::XRRuntime* GetRuntime() { return runtime_.get(); }

  // Methods called by VRServiceImpl to interact with the runtime's device.
  void OnServiceAdded(VRServiceImpl* service);
  void OnServiceRemoved(VRServiceImpl* service);
  void ExitPresent(VRServiceImpl* service);
  void SetFramesThrottled(const VRServiceImpl* service, bool throttled);

  // Both of these will forward the RequestSession call onto the runtime, the
  // main difference is that when Requesting an Immersive session we will bind
  // the XRSessionController rather than passing it back to the VrService, as
  // well as setting the appropriate presenting state.
  void RequestImmersiveSession(
      VRServiceImpl* service,
      device::mojom::XRRuntimeSessionOptionsPtr options,
      RequestSessionCallback callback);
  void RequestInlineSession(
      device::mojom::XRRuntimeSessionOptionsPtr options,
      device::mojom::XRRuntime::RequestSessionCallback callback);

  void EnsureInstalled(int render_process_id,
                       int render_frame_id,
                       base::OnceCallback<void(bool)> install_callback);
  VRServiceImpl* GetServiceWithActiveImmersiveSession() {
    return presenting_service_;
  }

  bool HasPendingImmersiveSessionRequest() {
    return has_pending_immersive_session_request_;
  }

  device::mojom::XRDeviceId GetId() const { return id_; }

#if BUILDFLAG(IS_WIN)
  std::optional<CHROME_LUID> GetLuid() const;
#endif

  // BrowserXRRuntime
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Called to allow the runtime to conduct any cleanup it needs to do before it
  // is removed.
  void BeforeRuntimeRemoved();

  std::vector<device::mojom::XRSessionFeature> GetSupportedFeatures();

 private:
  // device::XRRuntimeEventListener
  void OnExitPresent() override;
  void OnVisibilityStateChanged(
      device::mojom::XRVisibilityState visibility_state) override;

  void StopImmersiveSession();
  void OnRequestSessionResult(
      base::WeakPtr<VRServiceImpl> service,
      device::mojom::XRRuntimeSessionOptionsPtr options,
      RequestSessionCallback callback,
      device::mojom::XRRuntimeSessionResultPtr session_result);
  void OnImmersiveSessionError();
  void OnInstallFinished(bool succeeded);

  device::mojom::XRDeviceId id_;
  device::mojom::XRDeviceDataPtr device_data_;
  mojo::Remote<device::mojom::XRRuntime> runtime_;
  mojo::Remote<device::mojom::XRSessionController>
      immersive_session_controller_;
  bool immersive_session_has_camera_access_ = false;

  std::set<raw_ptr<VRServiceImpl, SetExperimental>> services_;

  raw_ptr<VRServiceImpl> presenting_service_ = nullptr;

  mojo::AssociatedReceiver<device::mojom::XRRuntimeEventListener> receiver_{
      this};

  base::ObserverList<Observer> observers_;
  std::unique_ptr<XrInstallHelper> install_helper_;
  std::unique_ptr<BrowserXRRuntime::Observer> runtime_observer_;
  std::unique_ptr<VrUiHost> vr_ui_host_;
  base::OnceCallback<void(bool)> install_finished_callback_;
  bool has_pending_immersive_session_request_ = false;

  base::WeakPtrFactory<BrowserXRRuntimeImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_SERVICE_BROWSER_XR_RUNTIME_IMPL_H_
