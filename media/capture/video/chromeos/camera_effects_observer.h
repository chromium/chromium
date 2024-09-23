// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_EFFECTS_OBSERVER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_EFFECTS_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/mojo_service_manager_observer.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// CrosCameraEffectsObserver is used to observe camera effects changed on
// cros-camera service. This class lives on the thread which the mojo service
// manager lives.
class CAPTURE_EXPORT CrosCameraEffectsObserver
    : public cros::mojom::CrosCameraServiceObserver {
 public:
  using OnCameraEffectsChangedCallback =
      base::RepeatingCallback<void(cros::mojom::EffectsConfigPtr)>;

  // When the camera effect changes, |on_camera_effects_changed_callback| will
  // be invoked on the ui thread.
  explicit CrosCameraEffectsObserver(
      OnCameraEffectsChangedCallback on_camera_effects_changed_callback);

  ~CrosCameraEffectsObserver() override;

  CrosCameraEffectsObserver(const CrosCameraEffectsObserver&) = delete;
  CrosCameraEffectsObserver& operator=(const CrosCameraEffectsObserver&) =
      delete;

 private:
  // CrosCameraServiceObserver implementations.
  void CameraDeviceActivityChange(int32_t camera_id,
                                  bool opened,
                                  cros::mojom::CameraClientType type) override;
  void CameraPrivacySwitchStateChange(
      cros::mojom::CameraPrivacySwitchState state,
      int32_t camera_id) override;
  void CameraSWPrivacySwitchStateChange(
      cros::mojom::CameraPrivacySwitchState state) override;

  void CameraEffectChange(cros::mojom::EffectsConfigPtr config) override;
  void AutoFramingStateChange(
      cros::mojom::CameraAutoFramingState state) override;

  void ConnectToCameraService();

  void OnCameraServiceConnectionError();

  SEQUENCE_CHECKER(sequence_checker_);

  const OnCameraEffectsChangedCallback on_camera_effects_changed_callback_;

  std::unique_ptr<MojoServiceManagerObserver> mojo_service_manager_observer_;

  mojo::Remote<cros::mojom::CrosCameraService> camera_service_;

  mojo::Receiver<cros::mojom::CrosCameraServiceObserver>
      camera_service_observer_receiver_{this};

  base::WeakPtrFactory<CrosCameraEffectsObserver> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_EFFECTS_OBSERVER_H_
