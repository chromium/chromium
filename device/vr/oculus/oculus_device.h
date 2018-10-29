// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OCULUS_DEVICE_H
#define DEVICE_VR_OCULUS_DEVICE_H

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device_base.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/libovr/src/Include/OVR_CAPI.h"

namespace device {

class XRCompositorCommon;

class DEVICE_VR_EXPORT OculusDevice
    : public VRDeviceBase,
      public mojom::XRSessionController,
      public mojom::IsolatedXRGamepadProviderFactory,
      public mojom::XRCompositorHost {
 public:
  explicit OculusDevice();
  ~OculusDevice() override;

  // VRDeviceBase
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;
  void OnMagicWindowFrameDataRequest(
      mojom::XRFrameDataProvider::GetFrameDataCallback callback) override;
  void OnRequestSessionResult(mojom::XRRuntime::RequestSessionCallback callback,
                              bool result,
                              mojom::XRSessionPtr session);

  bool IsInitialized() { return !!session_; }

  mojom::IsolatedXRGamepadProviderFactoryPtr BindGamepadFactory();
  mojom::XRCompositorHostPtr BindCompositorHost();

 private:
  // XRSessionController
  void SetFrameDataRestricted(bool restricted) override;

  void OnPresentingControllerMojoConnectionError();

  // mojom::IsolatedXRGamepadProviderFactory
  void GetIsolatedXRGamepadProvider(
      mojom::IsolatedXRGamepadProviderRequest provider_request) override;

  // XRCompositorHost
  void CreateImmersiveOverlay(
      mojom::ImmersiveOverlayRequest overlay_request) override;

  void OnPresentationEnded();
  void StartOvrSession();
  void StopOvrSession();

  std::unique_ptr<XRCompositorCommon> render_loop_;
  ovrSession session_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  mojo::Binding<mojom::XRSessionController> exclusive_controller_binding_;
  mojo::Binding<mojom::IsolatedXRGamepadProviderFactory>
      gamepad_provider_factory_binding_;
  mojom::IsolatedXRGamepadProviderRequest provider_request_;

  mojo::Binding<mojom::XRCompositorHost> compositor_host_binding_;
  mojom::ImmersiveOverlayRequest overlay_request_;

  base::WeakPtrFactory<OculusDevice> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OculusDevice);
};

}  // namespace device

#endif  // DEVICE_VR_OCULUS_DEVICE_H
