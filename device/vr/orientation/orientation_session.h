// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ORIENTATION_ORIENTATION_SESSION_H_
#define DEVICE_VR_ORIENTATION_ORIENTATION_SESSION_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace device {

class VROrientationDevice;

// VR device process implementation of a XRFrameDataProvider within a session
// that exposes device orientation sensors.
// VROrientationSession objects are owned by their respective
// VROrientationDevice instances.
class COMPONENT_EXPORT(VR_ORIENTATION) VROrientationSession
    : public mojom::XRFrameDataProvider,
      public mojom::XRSessionController {
 public:
  VROrientationSession(VROrientationDevice* device,
                       mojo::PendingReceiver<mojom::XRFrameDataProvider>,
                       mojo::PendingReceiver<mojom::XRSessionController>);
  ~VROrientationSession() override;

  void GetEnvironmentIntegrationProvider(
      mojo::PendingAssociatedReceiver<mojom::XREnvironmentIntegrationProvider>
          environment_provider) override;
  void SetInputSourceButtonListener(
      mojo::PendingAssociatedRemote<device::mojom::XRInputSourceButtonListener>)
      override;

  // Accessible to tests.
 protected:
  // mojom::XRFrameDataProvider
  void GetFrameData(mojom::XRFrameDataRequestOptionsPtr options,
                    GetFrameDataCallback callback) override;

  // mojom::XRSessionController
  void SetFrameDataRestricted(bool frame_data_restricted) override;

  void OnMojoConnectionError();

  mojo::Receiver<mojom::XRFrameDataProvider> magic_window_receiver_;
  mojo::Receiver<mojom::XRSessionController> session_controller_receiver_;
  device::VROrientationDevice* device_;
  bool restrict_frame_data_ = true;

  // This must be the last member
  base::WeakPtrFactory<VROrientationSession> weak_ptr_factory_{this};
};

}  // namespace device

#endif  //  DEVICE_VR_ORIENTATION_ORIENTATION_SESSION_H_
