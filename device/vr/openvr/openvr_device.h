// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_OPENVR_DEVICE_H_
#define DEVICE_VR_OPENVR_OPENVR_DEVICE_H_

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "device/vr/openvr/openvr_api_wrapper.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace device {

class XRCompositorCommon;

class DEVICE_VR_EXPORT OpenVRDevice
    : public VRDeviceBase,
      public mojom::XRSessionController,
      public mojom::XRCompositorHost {
 public:
  OpenVRDevice();
  ~OpenVRDevice() override;

  static bool IsHwAvailable();
  static bool IsApiAvailable();
  static void RecordRuntimeAvailability();

  void Shutdown();

  // VRDeviceBase
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;

  void OnPollingEvents();

  void OnRequestSessionResult(mojom::XRRuntime::RequestSessionCallback callback,
                              bool result,
                              mojom::XRSessionPtr session);

  bool IsAvailable();

  mojo::PendingRemote<mojom::XRCompositorHost> BindCompositorHost();

 private:
  // XRSessionController
  void SetFrameDataRestricted(bool restricted) override;

  // XRCompositorHost
  void CreateImmersiveOverlay(
      mojo::PendingReceiver<mojom::ImmersiveOverlay> overlay_receiver) override;

  void OnPresentingControllerMojoConnectionError();
  void OnPresentationEnded();
  bool EnsureValidDisplayInfo();

  int outstanding_session_requests_count_ = 0;
  bool have_real_display_info_ = false;
  std::unique_ptr<XRCompositorCommon> render_loop_;
  std::unique_ptr<OpenVRWrapper> openvr_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  mojo::Receiver<mojom::XRSessionController> exclusive_controller_receiver_{
      this};

  mojo::Receiver<mojom::XRCompositorHost> compositor_host_receiver_{this};
  mojo::PendingReceiver<mojom::ImmersiveOverlay> overlay_receiver_;

  base::WeakPtrFactory<OpenVRDevice> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OpenVRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_OPENVR_OPENVR_DEVICE_H_
