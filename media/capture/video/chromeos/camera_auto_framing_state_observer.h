// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_AUTO_FRAMING_STATE_OBSERVER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_AUTO_FRAMING_STATE_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/mojo_service_manager_observer.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// CrosCameraAutoFramingStateObserver is used to observe auto framing state
// changed on cros-camera service. This class lives on the thread which the mojo
// service manager lives.
class CAPTURE_EXPORT CrosCameraAutoFramingStateObserver
    : public cros::mojom::CrosCameraServiceObserver {
 public:
  using OnAutoFramingStateChangedCallback =
      base::RepeatingCallback<void(cros::mojom::CameraAutoFramingState)>;

  // When the auto framing state changes,
  // |on_auto_framing_state_changed_callback| will be invoked on the ui thread.
  explicit CrosCameraAutoFramingStateObserver(
      OnAutoFramingStateChangedCallback on_auto_framing_state_changed_callback);

  ~CrosCameraAutoFramingStateObserver() override;

  CrosCameraAutoFramingStateObserver(
      const CrosCameraAutoFramingStateObserver&) = delete;
  CrosCameraAutoFramingStateObserver& operator=(
      const CrosCameraAutoFramingStateObserver&) = delete;

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

  const OnAutoFramingStateChangedCallback
      on_auto_framing_state_changed_callback_;

  std::unique_ptr<MojoServiceManagerObserver> mojo_service_manager_observer_;

  mojo::Remote<cros::mojom::CrosCameraService> camera_service_;

  mojo::Receiver<cros::mojom::CrosCameraServiceObserver>
      camera_service_observer_receiver_{this};

  base::WeakPtrFactory<CrosCameraAutoFramingStateObserver> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_AUTO_FRAMING_STATE_OBSERVER_H_
