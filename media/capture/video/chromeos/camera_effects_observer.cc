// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_effects_observer.h"

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "media/capture/video/chromeos/mojom/effects_pipeline.mojom.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace media {

CrosCameraEffectsObserver::CrosCameraEffectsObserver(
    OnCameraEffectsChangedCallback on_camera_effects_changed_callback)
    : on_camera_effects_changed_callback_(
          std::move(on_camera_effects_changed_callback)) {
  mojo_service_manager_observer_ = MojoServiceManagerObserver::Create(
      chromeos::mojo_services::kCrosCameraService,
      base::BindRepeating(&CrosCameraEffectsObserver::ConnectToCameraService,
                          weak_factory_.GetWeakPtr()),
      base::DoNothing());
}

CrosCameraEffectsObserver::~CrosCameraEffectsObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CrosCameraEffectsObserver::CameraDeviceActivityChange(
    int32_t camera_id,
    bool opened,
    cros::mojom::CameraClientType type) {}

void CrosCameraEffectsObserver::CameraPrivacySwitchStateChange(
    cros::mojom::CameraPrivacySwitchState state,
    int32_t camera_id) {}

void CrosCameraEffectsObserver::CameraSWPrivacySwitchStateChange(
    cros::mojom::CameraPrivacySwitchState state) {}

void CrosCameraEffectsObserver::CameraEffectChange(
    cros::mojom::EffectsConfigPtr config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_camera_effects_changed_callback_.Run(std::move(config));
}

void CrosCameraEffectsObserver::AutoFramingStateChange(
    cros::mojom::CameraAutoFramingState state) {}

void CrosCameraEffectsObserver::ConnectToCameraService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ash::mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosCameraService, std::nullopt,
      camera_service_.BindNewPipeAndPassReceiver().PassPipe());
  camera_service_.set_disconnect_handler(
      base::BindOnce(&CrosCameraEffectsObserver::OnCameraServiceConnectionError,
                     weak_factory_.GetWeakPtr()));
  camera_service_->AddCrosCameraServiceObserver(
      camera_service_observer_receiver_.BindNewPipeAndPassRemote());
}

void CrosCameraEffectsObserver::OnCameraServiceConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  camera_service_.reset();
  camera_service_observer_receiver_.reset();
}

}  // namespace media
