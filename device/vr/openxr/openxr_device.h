// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_DEVICE_H_
#define DEVICE_VR_OPENXR_OPENXR_DEVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/viz/common/gpu/context_provider.h"
#include "device/vr/openxr/context_provider_callbacks.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device_base.h"
#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrRenderLoop;
class OpenXrStatics;

class DEVICE_VR_EXPORT OpenXrDevice
    : public VRDeviceBase,
      public mojom::XRSessionController,
      public mojom::XRCompositorHost {
 public:
  OpenXrDevice(OpenXrStatics* openxr_statics,
               VizContextProviderFactoryAsync context_provider_factory_async);
  ~OpenXrDevice() override;

  // VRDeviceBase
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;

  mojo::PendingRemote<mojom::XRCompositorHost> BindCompositorHost();

 private:
  // XRSessionController
  void SetFrameDataRestricted(bool restricted) override;

  // XRCompositorHost
  void CreateImmersiveOverlay(
      mojo::PendingReceiver<mojom::ImmersiveOverlay> overlay_receiver) override;

  void EnsureRenderLoop();

  void OnRequestSessionResult(bool result, mojom::XRSessionPtr session);
  void OnPresentingControllerMojoConnectionError();
  bool IsArBlendModeSupported(OpenXrStatics* openxr_statics);

  XrInstance instance_;
  OpenXrExtensionHelper extension_helper_;
  std::unique_ptr<OpenXrRenderLoop> render_loop_;

  mojo::Receiver<mojom::XRSessionController> exclusive_controller_receiver_{
      this};

  mojo::Receiver<mojom::XRCompositorHost> compositor_host_receiver_{this};
  mojo::PendingReceiver<mojom::ImmersiveOverlay> overlay_receiver_;

  VizContextProviderFactoryAsync context_provider_factory_async_;

  mojom::XRRuntime::RequestSessionCallback request_session_callback_;

  base::WeakPtrFactory<OpenXrDevice> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OpenXrDevice);
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_DEVICE_H_
