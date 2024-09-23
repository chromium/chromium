// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_DEVICE_H_
#define DEVICE_VR_OPENXR_OPENXR_DEVICE_H_

#include <memory>

#include "components/viz/common/gpu/context_provider.h"
#include "device/vr/openxr/context_provider_callbacks.h"
#include "device/vr/openxr/exit_xr_present_reason.h"
#include "device/vr/openxr/openxr_platform_helper.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device_base.h"
#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrRenderLoop;

class DEVICE_VR_EXPORT OpenXrDevice : public VRDeviceBase,
                                      public mojom::XRSessionController {
 public:
  OpenXrDevice(VizContextProviderFactoryAsync context_provider_factory_async,
               OpenXrPlatformHelper* platform_helper);

  OpenXrDevice(const OpenXrDevice&) = delete;
  OpenXrDevice& operator=(const OpenXrDevice&) = delete;

  ~OpenXrDevice() override;

  // VRDeviceBase
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;
  void ShutdownSession(mojom::XRRuntime::ShutdownSessionCallback) override;

 private:
  // XRSessionController
  void SetFrameDataRestricted(bool restricted) override;

  void OnCreateInstanceResult(mojom::XRRuntimeSessionOptionsPtr options,
                              XrResult result,
                              XrInstance instance);

  void OnRequestSessionResult(
      bool result,
      mojom::XRSessionPtr session,
      mojo::PendingRemote<mojom::ImmersiveOverlay> overlay);
  void ForceEndSession(ExitXrPresentReason reason);
  void OnPresentingControllerMojoConnectionError();
  bool IsArBlendModeSupported();

  XrInstance instance_{XR_NULL_HANDLE};
  std::unique_ptr<OpenXrExtensionHelper> extension_helper_;
  std::unique_ptr<OpenXrRenderLoop> render_loop_;

  mojo::Receiver<mojom::XRSessionController> exclusive_controller_receiver_{
      this};

  VizContextProviderFactoryAsync context_provider_factory_async_;

  // Owned by our creator who guarantees the lifetime.
  raw_ptr<OpenXrPlatformHelper> platform_helper_;

  mojom::XRRuntime::RequestSessionCallback request_session_callback_;

  base::WeakPtrFactory<OpenXrDevice> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_DEVICE_H_
