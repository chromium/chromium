// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_SW_PRIVACY_SWITCH_STATE_OBSERVER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_SW_PRIVACY_SWITCH_STATE_OBSERVER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/mojo_service_manager_observer.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// CrosCameraSWPrivacySwitchStateObserver is used to observe software privacy
// switch state changed on cros-camera service. This class has to live on the
// main thread.
class CAPTURE_EXPORT CrosCameraSWPrivacySwitchStateObserver
    : public cros::mojom::CrosCameraServiceObserver {
 public:
  using OnSWPrivacySwitchStateChangedCallback = base::RepeatingCallback<void(
      cros::mojom::CameraPrivacySwitchState state)>;

  // When the sw privacy switch state changes,
  // |on_sw_privacy_switch_state_changed_callback_| will be invoked on the main
  // thread.
  explicit CrosCameraSWPrivacySwitchStateObserver(
      OnSWPrivacySwitchStateChangedCallback
          on_sw_privacy_switch_state_changed_callback);

  ~CrosCameraSWPrivacySwitchStateObserver() override;

  CrosCameraSWPrivacySwitchStateObserver(
      const CrosCameraSWPrivacySwitchStateObserver&) = delete;
  CrosCameraSWPrivacySwitchStateObserver(
      CrosCameraSWPrivacySwitchStateObserver&&) = delete;
  CrosCameraSWPrivacySwitchStateObserver& operator=(
      const CrosCameraSWPrivacySwitchStateObserver&) = delete;
  CrosCameraSWPrivacySwitchStateObserver& operator=(
      CrosCameraSWPrivacySwitchStateObserver&&) = delete;

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

  const OnSWPrivacySwitchStateChangedCallback
      on_sw_privacy_switch_state_changed_callback_;

  std::unique_ptr<MojoServiceManagerObserver> mojo_service_manager_observer_;

  mojo::Remote<cros::mojom::CrosCameraService> camera_service_;

  mojo::Receiver<cros::mojom::CrosCameraServiceObserver>
      camera_service_observer_receiver_{this};

  base::WeakPtrFactory<CrosCameraSWPrivacySwitchStateObserver> weak_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_SW_PRIVACY_SWITCH_STATE_OBSERVER_H_
