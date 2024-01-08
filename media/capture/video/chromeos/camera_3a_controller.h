// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_3A_CONTROLLER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_3A_CONTROLLER_H_

#include <unordered_set>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video/chromeos/request_manager.h"

namespace media {

// A class to control the auto-exposure, auto-focus, and auto-white-balancing
// operations and modes of the camera.  For the detailed state transitions for
// auto-exposure, auto-focus, and auto-white-balancing, see
// https://source.android.com/devices/camera/camera3_3Amodes
class CAPTURE_EXPORT Camera3AController final
    : public CaptureMetadataDispatcher::ResultMetadataObserver {
 public:
  Camera3AController() = delete;

  Camera3AController(const cros::mojom::CameraMetadataPtr& static_metadata,
                     CaptureMetadataDispatcher* capture_metadata_dispatcher,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  Camera3AController(const Camera3AController&) = delete;
  Camera3AController& operator=(const Camera3AController&) = delete;

  ~Camera3AController() final;

  // Trigger the camera to start exposure, focus, and white-balance metering and
  // lock them for still capture.
  void Stabilize3AForStillCapture(base::OnceClosure on_3a_stabilized_callback);

  // CaptureMetadataDispatcher::ResultMetadataObserver implementation.
  void OnResultMetadataAvailable(
      uint32_t frame_number,
      const cros::mojom::CameraMetadataPtr& result_metadata) final;

  // Enable the auto-focus mode suitable for still capture.
  void SetAutoFocusModeForStillCapture();

  // TODO(shik): This function is unused now.
  // Enable the auto-focus mode suitable for video recording.
  void SetAutoFocusModeForVideoRecording();

  // Set auto white balance mode.
  void SetAutoWhiteBalanceMode(cros::mojom::AndroidControlAwbMode mode);

  // Set exposure time.
  // |enable_auto| enables auto exposure mode. |exposure_time_nanoseconds| is
  // only effective if |enable_auto| is set to false
  void SetExposureTime(bool enable_auto, int64_t exposure_time_nanoseconds);

  // Set focus distance.
  // |enable_auto| enables auto focus mode. |focus_distance_diopters| is only
  // effective if |enable_auto| is set to false
  void SetFocusDistance(bool enable_auto, float focus_distance_diopters);

  bool IsPointOfInterestSupported();

  // Set point of interest. The coordinate system is based on the active
  // pixel array.
  void SetPointOfInterest(gfx::Point point);

  base::WeakPtr<Camera3AController> GetWeakPtr();

 private:
  void Set3AMode(cros::mojom::CameraMetadataTag tag, uint8_t target_mode);

  // Sometimes it might take long time to stabilize 3A.  Fire the
  // callback artificially after |time_limit| passed.
  void Set3aStabilizedCallback(base::OnceClosure callback,
                               base::TimeDelta time_limit);

  bool Is3AStabilized();

  // Intermediate steps of setting point of interest.
  void SetPointOfInterestOn3AModeSet();
  void SetPointOfInterestOn3AStabilized();
  void SetPointOfInterestUnlockAe();

  // Helper functions for manipulating metadata.
  template <typename T>
  void SetCaptureMetadata(cros::mojom::CameraMetadataTag tag, T value);

  template <typename T>
  void SetCaptureMetadata(cros::mojom::CameraMetadataTag tag,
                          const std::vector<T>& value);

  template <typename T>
  void SetRepeatingCaptureMetadata(cros::mojom::CameraMetadataTag tag, T value);

  template <typename T>
  void SetRepeatingCaptureMetadata(cros::mojom::CameraMetadataTag tag,
                                   const std::vector<T>& value);

  void ClearRepeatingCaptureMetadata();

  const raw_ref<const cros::mojom::CameraMetadataPtr> static_metadata_;
  bool ae_region_supported_;
  bool af_region_supported_;
  bool point_of_interest_supported_;
  bool zero_shutter_lag_supported_;

  raw_ptr<CaptureMetadataDispatcher> capture_metadata_dispatcher_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::unordered_set<cros::mojom::AndroidControlAfMode> available_af_modes_;
  cros::mojom::AndroidControlAfMode af_mode_;
  cros::mojom::AndroidControlAfState af_state_;
  // |af_mode_set_| is set to true when the AF mode is synchronized between
  // the HAL and the Camera3AController.
  bool af_mode_set_;

  std::unordered_set<cros::mojom::AndroidControlAeMode> available_ae_modes_;
  cros::mojom::AndroidControlAeMode ae_mode_;
  cros::mojom::AndroidControlAeState ae_state_;
  // |ae_mode_set_| is set to true when the AE mode is synchronized between
  // the HAL and the Camera3AController.
  bool ae_mode_set_;

  std::unordered_set<cros::mojom::AndroidControlAwbMode> available_awb_modes_;
  cros::mojom::AndroidControlAwbMode awb_mode_;
  cros::mojom::AndroidControlAwbState awb_state_;
  // |awb_mode_set_| is set to true when the AWB mode is synchronized between
  // the HAL and the Camera3AController.
  bool awb_mode_set_;

  bool set_point_of_interest_running_;

  bool ae_locked_for_point_of_interest_;

  int32_t request_id_ = 0;

  base::TimeDelta latest_sensor_timestamp_;

  std::unordered_set<cros::mojom::CameraMetadataTag> repeating_metadata_tags_;

  // TODO(shik): There are potential races in |on.*callback_| due to processing
  // pipeline.  Here are some possible solutions/mitgations used in GCA:
  // 1. Record the frame number.
  // 2. Wait for some frames before checking.
  // 3. Wait for more complex patterns (like TriggerStateMachine.java in GCA).
  base::OnceClosure on_3a_mode_set_callback_;

  base::OnceClosure on_3a_stabilized_callback_;
  base::TimeDelta artificial_3a_stabilized_deadline_;

  base::OnceClosure on_ae_locked_for_point_of_interest_callback_;

  base::OnceClosure on_af_trigger_cancelled_callback_;

  base::CancelableOnceClosure delayed_ae_unlock_callback_;

  base::WeakPtrFactory<Camera3AController> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_3A_CONTROLLER_H_
