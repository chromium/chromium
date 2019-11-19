// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/orientation/orientation_session.h"

#include <utility>

#include "base/bind.h"
#include "device/vr/orientation/orientation_device.h"

namespace device {

VROrientationSession::VROrientationSession(
    VROrientationDevice* device,
    mojo::PendingReceiver<mojom::XRFrameDataProvider> magic_window_receiver,
    mojo::PendingReceiver<mojom::XRSessionController> session_receiver)
    : magic_window_receiver_(this, std::move(magic_window_receiver)),
      session_controller_receiver_(this, std::move(session_receiver)),
      device_(device) {
  magic_window_receiver_.set_disconnect_handler(
      base::BindOnce(&VROrientationSession::OnMojoConnectionError,
                     weak_ptr_factory_.GetWeakPtr()));
  session_controller_receiver_.set_disconnect_handler(
      base::BindOnce(&VROrientationSession::OnMojoConnectionError,
                     weak_ptr_factory_.GetWeakPtr()));
}

VROrientationSession::~VROrientationSession() = default;

// Gets frame data for sessions.
void VROrientationSession::GetFrameData(
    mojom::XRFrameDataRequestOptionsPtr options,
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  // Orientation sessions should never be exclusive or presenting.
  DCHECK(!device_->HasExclusiveSession());

  if (restrict_frame_data_) {
    std::move(callback).Run(nullptr);
    return;
  }

  device_->GetInlineFrameData(std::move(callback));
}

void VROrientationSession::GetEnvironmentIntegrationProvider(
    mojo::PendingAssociatedReceiver<mojom::XREnvironmentIntegrationProvider>
        environment_provider) {
  // Environment integration is not supported. This call should not
  // be made on this device.
  mojo::ReportBadMessage("Environment integration is not supported.");
}

void VROrientationSession::SetInputSourceButtonListener(
    mojo::PendingAssociatedRemote<device::mojom::XRInputSourceButtonListener>) {
  // Input eventing is not supported. This call should not
  // be made on this device.
  mojo::ReportBadMessage("Input eventing is not supported.");
}

// XRSessionController
void VROrientationSession::SetFrameDataRestricted(bool frame_data_restricted) {
  restrict_frame_data_ = frame_data_restricted;
}

void VROrientationSession::OnMojoConnectionError() {
  magic_window_receiver_.reset();
  session_controller_receiver_.reset();
  device_->EndMagicWindowSession(this);  // This call will destroy us.
}

}  // namespace device
