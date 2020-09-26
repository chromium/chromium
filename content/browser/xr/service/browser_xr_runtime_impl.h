// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_SERVICE_BROWSER_XR_RUNTIME_IMPL_H_
#define CONTENT_BROWSER_XR_SERVICE_BROWSER_XR_RUNTIME_IMPL_H_

#include <set>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "content/browser/xr/service/vr_service_impl.h"
#include "content/public/browser/browser_xr_runtime.h"
#include "content/public/browser/render_frame_host.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class XrInstallHelper;
}  // namespace content

namespace content {
// This class wraps a physical device's interfaces, and registers for events.
// There is one BrowserXRRuntimeImpl per physical device runtime.  It manages
// browser-side handling of state, like which VRServiceImpl is listening for
// device activation.
class BrowserXRRuntimeImpl : public content::BrowserXRRuntime,
                             public device::mojom::XRRuntimeEventListener {
 public:
  using RequestSessionCallback =
      base::OnceCallback<void(device::mojom::XRSessionPtr)>;
  explicit BrowserXRRuntimeImpl(
      device::mojom::XRDeviceId id,
      device::mojom::XRDeviceDataPtr device_data,
      mojo::PendingRemote<device::mojom::XRRuntime> runtime,
      device::mojom::VRDisplayInfoPtr info);
  ~BrowserXRRuntimeImpl() override;

  void ExitActiveImmersiveSession();
  bool SupportsFeature(device::mojom::XRSessionFeature feature) const;
  bool SupportsAllFeatures(
      const std::vector<device::mojom::XRSessionFeature>& features) const;

  bool SupportsCustomIPD() const;
  bool SupportsNonEmulatedHeight() const;

  device::mojom::XRRuntime* GetRuntime() { return runtime_.get(); }

  // Methods called by VRServiceImpl to interact with the runtime's device.
  void OnServiceAdded(VRServiceImpl* service);
  void OnServiceRemoved(VRServiceImpl* service);
  void ExitPresent(VRServiceImpl* service,
                   VRServiceImpl::ExitPresentCallback on_exited);
  void SetFramesThrottled(const VRServiceImpl* service, bool throttled);
  void RequestSession(VRServiceImpl* service,
                      const device::mojom::XRRuntimeSessionOptionsPtr& options,
                      RequestSessionCallback callback);
  void EnsureInstalled(int render_process_id,
                       int render_frame_id,
                       base::OnceCallback<void(bool)> install_callback);
  VRServiceImpl* GetServiceWithActiveImmersiveSession() {
    return presenting_service_;
  }

  device::mojom::VRDisplayInfoPtr GetVRDisplayInfo() {
    return display_info_.Clone();
  }

  device::mojom::XRDeviceId GetId() const { return id_; }

#if defined(OS_WIN)
  base::Optional<LUID> GetLuid() const;
#endif

  // BrowserXRRuntime
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Called to allow the runtime to conduct any cleanup it needs to do before it
  // is removed.
  void BeforeRuntimeRemoved();

 private:
  // device::XRRuntimeEventListener
  void OnDisplayInfoChanged(
      device::mojom::VRDisplayInfoPtr vr_device_info) override;
  void OnExitPresent() override;
  void OnVisibilityStateChanged(
      device::mojom::XRVisibilityState visibility_state) override;

  void StopImmersiveSession(VRServiceImpl::ExitPresentCallback on_exited);
  void OnRequestSessionResult(
      base::WeakPtr<VRServiceImpl> service,
      device::mojom::XRRuntimeSessionOptionsPtr options,
      RequestSessionCallback callback,
      device::mojom::XRSessionPtr session,
      mojo::PendingRemote<device::mojom::XRSessionController>
          immersive_session_controller);
  void OnImmersiveSessionError();
  void OnInstallFinished(bool succeeded);

  device::mojom::XRDeviceId id_;
  device::mojom::XRDeviceDataPtr device_data_;
  mojo::Remote<device::mojom::XRRuntime> runtime_;
  mojo::Remote<device::mojom::XRSessionController>
      immersive_session_controller_;

  std::set<VRServiceImpl*> services_;
  device::mojom::VRDisplayInfoPtr display_info_;

  VRServiceImpl* presenting_service_ = nullptr;

  mojo::AssociatedReceiver<device::mojom::XRRuntimeEventListener> receiver_{
      this};

  base::ObserverList<Observer> observers_;
  std::unique_ptr<content::XrInstallHelper> install_helper_;
  base::OnceCallback<void(bool)> install_finished_callback_;

  base::WeakPtrFactory<BrowserXRRuntimeImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_SERVICE_BROWSER_XR_RUNTIME_IMPL_H_
