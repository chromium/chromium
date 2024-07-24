// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_sw_privacy_switch_state_observer.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace media {

CrosCameraSWPrivacySwitchStateObserver::CrosCameraSWPrivacySwitchStateObserver(
    OnSWPrivacySwitchStateChangedCallback
        on_sw_privacy_switch_state_changed_callback)
    : on_sw_privacy_switch_state_changed_callback_(
          std::move(on_sw_privacy_switch_state_changed_callback)) {
  mojo_service_manager_observer_ = MojoServiceManagerObserver::Create(
      chromeos::mojo_services::kCrosCameraService,
      base::BindRepeating(
          &CrosCameraSWPrivacySwitchStateObserver::ConnectToCameraService,
          weak_factory_.GetWeakPtr()),
      base::DoNothing());
}

CrosCameraSWPrivacySwitchStateObserver::
    ~CrosCameraSWPrivacySwitchStateObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CrosCameraSWPrivacySwitchStateObserver::CameraDeviceActivityChange(
    int32_t camera_id,
    bool opened,
    cros::mojom::CameraClientType type) {}

void CrosCameraSWPrivacySwitchStateObserver::CameraPrivacySwitchStateChange(
    cros::mojom::CameraPrivacySwitchState state,
    int32_t camera_id) {}

void CrosCameraSWPrivacySwitchStateObserver::CameraSWPrivacySwitchStateChange(
    cros::mojom::CameraPrivacySwitchState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_sw_privacy_switch_state_changed_callback_.Run(state);
}

void CrosCameraSWPrivacySwitchStateObserver::CameraEffectChange(
    cros::mojom::EffectsConfigPtr config) {}

void CrosCameraSWPrivacySwitchStateObserver::AutoFramingStateChange(
    cros::mojom::CameraAutoFramingState state) {}

void CrosCameraSWPrivacySwitchStateObserver::ConnectToCameraService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ash::mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosCameraService, std::nullopt,
      camera_service_.BindNewPipeAndPassReceiver().PassPipe());
  camera_service_.set_disconnect_handler(base::BindOnce(
      &CrosCameraSWPrivacySwitchStateObserver::OnCameraServiceConnectionError,
      weak_factory_.GetWeakPtr()));
  camera_service_->AddCrosCameraServiceObserver(
      camera_service_observer_receiver_.BindNewPipeAndPassRemote());
}

void CrosCameraSWPrivacySwitchStateObserver::OnCameraServiceConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  camera_service_.reset();
  camera_service_observer_receiver_.reset();
}

}  // namespace media
