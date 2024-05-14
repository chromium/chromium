// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_3a_controller.h"

#include <algorithm>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/typed_macros.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "media/capture/video/chromeos/camera_trace_utils.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace media {

namespace {

template <typename EntryType>
bool Get3AEntry(const cros::mojom::CameraMetadataPtr& metadata,
                cros::mojom::CameraMetadataTag control,
                EntryType* result) {
  const auto* entry = GetMetadataEntry(metadata, control);
  if (entry) {
    *result = static_cast<EntryType>((*entry)->data[0]);
    return true;
  } else {
    return false;
  }
}

}  // namespace

Camera3AController::Camera3AController(
    const cros::mojom::CameraMetadataPtr& static_metadata,
    CaptureMetadataDispatcher* capture_metadata_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : static_metadata_(static_metadata),
      capture_metadata_dispatcher_(capture_metadata_dispatcher),
      task_runner_(std::move(task_runner)),
      af_mode_(cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_OFF),
      af_state_(cros::mojom::AndroidControlAfState::
                    ANDROID_CONTROL_AF_STATE_INACTIVE),
      af_mode_set_(false),
      ae_mode_(cros::mojom::AndroidControlAeMode::ANDROID_CONTROL_AE_MODE_ON),
      ae_state_(cros::mojom::AndroidControlAeState::
                    ANDROID_CONTROL_AE_STATE_INACTIVE),
      ae_mode_set_(false),
      awb_mode_(
          cros::mojom::AndroidControlAwbMode::ANDROID_CONTROL_AWB_MODE_AUTO),
      awb_state_(cros::mojom::AndroidControlAwbState::
                     ANDROID_CONTROL_AWB_STATE_INACTIVE),
      awb_mode_set_(false),
      set_point_of_interest_running_(false),
      ae_locked_for_point_of_interest_(false) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  capture_metadata_dispatcher_->AddResultMetadataObserver(this);

  auto max_regions = GetMetadataEntryAsSpan<int32_t>(
      *static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_MAX_REGIONS);
  if (max_regions.empty()) {
    ae_region_supported_ = false;
    af_region_supported_ = false;
  } else {
    DCHECK_EQ(max_regions.size(), 3u);
    ae_region_supported_ = max_regions[0] > 0;
    af_region_supported_ = max_regions[2] > 0;
  }

  auto* af_modes = GetMetadataEntry(
      *static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_AVAILABLE_MODES);
  if (af_modes) {
    for (const auto& m : (*af_modes)->data) {
      available_af_modes_.insert(
          static_cast<cros::mojom::AndroidControlAfMode>(m));
    }
  }
  auto* ae_modes = GetMetadataEntry(
      *static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_AVAILABLE_MODES);
  if (ae_modes) {
    for (const auto& m : (*ae_modes)->data) {
      available_ae_modes_.insert(
          static_cast<cros::mojom::AndroidControlAeMode>(m));
    }
  }
  auto* awb_modes = GetMetadataEntry(
      *static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_AVAILABLE_MODES);
  if (awb_modes) {
    for (const auto& m : (*awb_modes)->data) {
      available_awb_modes_.insert(
          static_cast<cros::mojom::AndroidControlAwbMode>(m));
    }
  }

  point_of_interest_supported_ = [&]() {
    // Because of line wrapping, multiple if-statements is more readable than a
    // super long boolean expression.
    auto available_modes = GetMetadataEntryAsSpan<uint8_t>(
        *static_metadata_,
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AVAILABLE_MODES);
    if (available_modes.empty()) {
      return false;
    }
    if (!base::Contains(
            available_modes,
            base::checked_cast<uint8_t>(
                cros::mojom::AndroidControlMode::ANDROID_CONTROL_MODE_AUTO))) {
      return false;
    }
    if (!available_ae_modes_.count(
            cros::mojom::AndroidControlAeMode::ANDROID_CONTROL_AE_MODE_ON)) {
      return false;
    }
    if (!available_af_modes_.count(
            cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_AUTO)) {
      return false;
    }
    if (!ae_region_supported_ && !af_region_supported_) {
      return false;
    }
    auto ae_lock_available = GetMetadataEntryAsSpan<uint8_t>(
        *static_metadata_,
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_LOCK_AVAILABLE);
    if (ae_lock_available.empty()) {
      return false;
    }
    DCHECK_EQ(ae_lock_available.size(), 1u);
    if (ae_lock_available[0] !=
        base::checked_cast<uint8_t>(
            cros::mojom::AndroidControlAeLockAvailable::
                ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE)) {
      return false;
    }
    return true;
  }();

  // Enable AF if supported.  MODE_AUTO is always supported on auto-focus camera
  // modules; fixed focus camera modules always has MODE_OFF.
  if (available_af_modes_.count(
          cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_AUTO)) {
    af_mode_ = cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_AUTO;
  }
  // AE should always be MODE_ON unless we enable manual sensor control.  Since
  // we don't have flash on any of our devices we don't care about the
  // flash-related AE modes.
  //
  // AWB should always be MODE_AUTO unless we enable manual sensor control.
  Set3AMode(cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE,
            base::checked_cast<uint8_t>(af_mode_));
  Set3AMode(cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_MODE,
            base::checked_cast<uint8_t>(ae_mode_));
  Set3AMode(cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_MODE,
            base::checked_cast<uint8_t>(awb_mode_));

  // Enable face detection if it's available.
  auto face_modes = GetMetadataEntryAsSpan<uint8_t>(
      static_metadata, cros::mojom::CameraMetadataTag::
                           ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES);
  // We don't need face landmarks and ids, so using SIMPLE mode instead of FULL
  // mode should be enough.
  const auto face_mode_simple = cros::mojom::AndroidStatisticsFaceDetectMode::
      ANDROID_STATISTICS_FACE_DETECT_MODE_SIMPLE;
  if (base::Contains(face_modes,
                     base::checked_cast<uint8_t>(face_mode_simple))) {
    SetRepeatingCaptureMetadata(
        cros::mojom::CameraMetadataTag::ANDROID_STATISTICS_FACE_DETECT_MODE,
        face_mode_simple);
  }

  auto request_keys = GetMetadataEntryAsSpan<int32_t>(
      *static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS);
  zero_shutter_lag_supported_ = base::Contains(
      request_keys,
      static_cast<int32_t>(
          cros::mojom::CameraMetadataTag::ANDROID_CONTROL_ENABLE_ZSL));
}

Camera3AController::~Camera3AController() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  ClearRepeatingCaptureMetadata();
  capture_metadata_dispatcher_->RemoveResultMetadataObserver(this);
}

void Camera3AController::Stabilize3AForStillCapture(
    base::OnceClosure on_3a_stabilized_callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  auto track = GetTraceTrack(CameraTraceEvent::kStabilize3A, request_id_);
  TRACE_EVENT_BEGIN("camera", "Stabilize3AForStillCapture", track);
  on_3a_stabilized_callback = base::BindOnce(
      [](base::OnceClosure callback, perfetto::Track track) {
        TRACE_EVENT_END("camera", std::move(track));
        std::move(callback).Run();
      },
      std::move(on_3a_stabilized_callback), std::move(track));
  ++request_id_;

  if (set_point_of_interest_running_) {
    // Use the settings from point of interest.
    if (!on_ae_locked_for_point_of_interest_callback_) {
      on_ae_locked_for_point_of_interest_callback_ =
          std::move(on_3a_stabilized_callback);
    }
    return;
  }

  if (on_3a_stabilized_callback_ || on_3a_mode_set_callback_) {
    // Already stabilizing 3A.
    return;
  }

  if (Is3AStabilized() || zero_shutter_lag_supported_) {
    std::move(on_3a_stabilized_callback).Run();
    return;
  }

  // Wait until all the 3A modes are set in the HAL; otherwise the AF trigger
  // and AE precapture trigger may be invalidated during mode transition.
  if (!af_mode_set_ || !ae_mode_set_ || !awb_mode_set_) {
    on_3a_mode_set_callback_ =
        base::BindOnce(&Camera3AController::Stabilize3AForStillCapture,
                       GetWeakPtr(), std::move(on_3a_stabilized_callback));
    return;
  }

  Set3aStabilizedCallback(std::move(on_3a_stabilized_callback),
                          base::Seconds(2));

  if (af_mode_ !=
      cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_OFF) {
    DVLOG(1) << "Start AF trigger to lock focus";
    SetCaptureMetadata(
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER,
        cros::mojom::AndroidControlAfTrigger::ANDROID_CONTROL_AF_TRIGGER_START);
  }

  if (ae_mode_ !=
      cros::mojom::AndroidControlAeMode::ANDROID_CONTROL_AE_MODE_OFF) {
    DVLOG(1) << "Start AE precapture trigger to converge exposure";
    SetCaptureMetadata(
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
        cros::mojom::AndroidControlAePrecaptureTrigger::
            ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START);
  }
}

void Camera3AController::OnResultMetadataAvailable(
    uint32_t frame_number,
    const cros::mojom::CameraMetadataPtr& result_metadata) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  auto sensor_timestamp = GetMetadataEntryAsSpan<int64_t>(
      result_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_SENSOR_TIMESTAMP);
  if (!sensor_timestamp.empty()) {
    DCHECK_EQ(sensor_timestamp.size(), 1u);
    // The sensor timestamp might not be monotonically increasing. The result
    // metadata from zero-shutter-lag request may be out of order compared to
    // previous regular requests.
    // https://developer.android.com/reference/android/hardware/camera2/CaptureResult#CONTROL_ENABLE_ZSL
    latest_sensor_timestamp_ = std::max(latest_sensor_timestamp_,
                                        base::Nanoseconds(sensor_timestamp[0]));
  }

  if (!af_mode_set_) {
    cros::mojom::AndroidControlAfMode af_mode;
    if (Get3AEntry(result_metadata,
                   cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE,
                   &af_mode)) {
      af_mode_set_ = (af_mode == af_mode_);
    } else {
      DVLOG(2) << "AF mode is not available in the metadata";
    }
  }

  if (!Get3AEntry(result_metadata,
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_STATE,
                  &af_state_)) {
    DVLOG(2) << "AF state is not available in the metadata";
  }

  if (!ae_mode_set_) {
    cros::mojom::AndroidControlAeMode ae_mode;
    if (Get3AEntry(result_metadata,
                   cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_MODE,
                   &ae_mode)) {
      ae_mode_set_ = (ae_mode == ae_mode_);
    } else {
      DVLOG(2) << "AE mode is not available in the metadata";
    }
  }
  if (!Get3AEntry(result_metadata,
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_STATE,
                  &ae_state_)) {
    DVLOG(2) << "AE state is not available in the metadata";
  }

  if (!awb_mode_set_) {
    cros::mojom::AndroidControlAwbMode awb_mode;
    if (Get3AEntry(result_metadata,
                   cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_MODE,
                   &awb_mode)) {
      awb_mode_set_ = (awb_mode == awb_mode_);
    } else {
      DVLOG(2) << "AWB mode is not available in the metadata";
    }
  }
  if (!Get3AEntry(result_metadata,
                  cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_STATE,
                  &awb_state_)) {
    DVLOG(2) << "AWB state is not available in the metadata";
  }

  DVLOG(2) << "AF mode: " << af_mode_;
  DVLOG(2) << "AF state: " << af_state_;
  DVLOG(2) << "AE mode: " << ae_mode_;
  DVLOG(2) << "AE state: " << ae_state_;
  DVLOG(2) << "AWB mode: " << awb_mode_;
  DVLOG(2) << "AWB state: " << awb_state_;

  if (on_3a_mode_set_callback_ && af_mode_set_ && ae_mode_set_ &&
      awb_mode_set_) {
    task_runner_->PostTask(FROM_HERE, std::move(on_3a_mode_set_callback_));
  }

  bool should_run_3a_stabilized_callback = [&]() {
    if (!on_3a_stabilized_callback_) {
      return false;
    }
    if (Is3AStabilized()) {
      return true;
    }
    if (latest_sensor_timestamp_ > artificial_3a_stabilized_deadline_) {
      LOG(WARNING)
          << "Timed out stabilizing 3A, fire the callback artificially";
      return true;
    }
    return false;
  }();
  if (should_run_3a_stabilized_callback) {
    std::move(on_3a_stabilized_callback_).Run();
  }

  bool should_run_trigger_cancelled_callback = [&]() {
    if (!on_af_trigger_cancelled_callback_) {
      return false;
    }
    auto af_trigger = GetMetadataEntryAsSpan<uint8_t>(
        result_metadata,
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER);
    if (af_trigger.empty()) {
      return false;
    }
    return af_trigger[0] ==
           base::checked_cast<uint8_t>(cros::mojom::AndroidControlAfTrigger::
                                           ANDROID_CONTROL_AF_TRIGGER_CANCEL);
  }();
  if (should_run_trigger_cancelled_callback) {
    std::move(on_af_trigger_cancelled_callback_).Run();
  }
}

void Camera3AController::SetAutoFocusModeForStillCapture() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (set_point_of_interest_running_ || ae_locked_for_point_of_interest_) {
    return;
  }

  SetCaptureMetadata(
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER,
      cros::mojom::AndroidControlAfTrigger::ANDROID_CONTROL_AF_TRIGGER_CANCEL);

  if (available_af_modes_.count(
          cros::mojom::AndroidControlAfMode::
              ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE)) {
    af_mode_ = cros::mojom::AndroidControlAfMode::
        ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
  }
  Set3AMode(cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE,
            base::checked_cast<uint8_t>(af_mode_));
  DVLOG(1) << "Setting AF mode to: " << af_mode_;
}

void Camera3AController::SetAutoFocusModeForVideoRecording() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (set_point_of_interest_running_ || ae_locked_for_point_of_interest_) {
    return;
  }

  SetCaptureMetadata(
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER,
      cros::mojom::AndroidControlAfTrigger::ANDROID_CONTROL_AF_TRIGGER_CANCEL);

  if (available_af_modes_.count(cros::mojom::AndroidControlAfMode::
                                    ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO)) {
    af_mode_ = cros::mojom::AndroidControlAfMode::
        ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
  }
  Set3AMode(cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE,
            base::checked_cast<uint8_t>(af_mode_));
  DVLOG(1) << "Setting AF mode to: " << af_mode_;
}

void Camera3AController::SetAutoWhiteBalanceMode(
    cros::mojom::AndroidControlAwbMode mode) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!available_awb_modes_.count(mode)) {
    LOG(WARNING) << "Don't support awb mode:" << mode;
    return;
  }

  SetCaptureMetadata(
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_LOCK,
      cros::mojom::AndroidControlAwbLock::ANDROID_CONTROL_AWB_LOCK_OFF);
  awb_mode_ = mode;
  Set3AMode(cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_MODE,
            base::checked_cast<uint8_t>(awb_mode_));
  DVLOG(1) << "Setting AWB mode to: " << awb_mode_;
}

void Camera3AController::SetExposureTime(bool enable_auto,
                                         int64_t exposure_time_nanoseconds) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (enable_auto) {
    if (!available_ae_modes_.count(
            cros::mojom::AndroidControlAeMode::ANDROID_CONTROL_AE_MODE_ON)) {
      LOG(WARNING) << "Don't support ANDROID_CONTROL_AE_MODE_ON";
      return;
    }
    ae_mode_ = cros::mojom::AndroidControlAeMode::ANDROID_CONTROL_AE_MODE_ON;
    capture_metadata_dispatcher_->UnsetRepeatingCaptureMetadata(
        cros::mojom::CameraMetadataTag::ANDROID_SENSOR_EXPOSURE_TIME);
  } else {
    if (!available_ae_modes_.count(
            cros::mojom::AndroidControlAeMode::ANDROID_CONTROL_AE_MODE_OFF)) {
      LOG(WARNING) << "Don't support ANDROID_CONTROL_AE_MODE_OFF";
      return;
    }
    ae_mode_ = cros::mojom::AndroidControlAeMode::ANDROID_CONTROL_AE_MODE_OFF;
    SetRepeatingCaptureMetadata(
        cros::mojom::CameraMetadataTag::ANDROID_SENSOR_EXPOSURE_TIME,
        exposure_time_nanoseconds);
  }

  Set3AMode(cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_MODE,
            base::checked_cast<uint8_t>(ae_mode_));
  DVLOG(1) << "Setting AE mode to: " << ae_mode_;
}

void Camera3AController::SetFocusDistance(bool enable_auto,
                                          float focus_distance_diopters) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (enable_auto) {
    if (!available_af_modes_.count(
            cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_AUTO)) {
      LOG(WARNING) << "Don't support ANDROID_CONTROL_AF_MODE_AUTO";
      return;
    }
    af_mode_ = cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_AUTO;
    capture_metadata_dispatcher_->UnsetRepeatingCaptureMetadata(
        cros::mojom::CameraMetadataTag::ANDROID_LENS_FOCUS_DISTANCE);
  } else {
    if (!available_af_modes_.count(
            cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_OFF)) {
      LOG(WARNING) << "Don't support ANDROID_CONTROL_AE_MODE_OFF";
      return;
    }
    af_mode_ = cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_OFF;
    SetRepeatingCaptureMetadata(
        cros::mojom::CameraMetadataTag::ANDROID_LENS_FOCUS_DISTANCE,
        focus_distance_diopters);
  }

  Set3AMode(cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE,
            base::checked_cast<uint8_t>(af_mode_));
  DVLOG(1) << "Setting AF mode to: " << af_mode_;
}

bool Camera3AController::IsPointOfInterestSupported() {
  return point_of_interest_supported_;
}

void Camera3AController::SetPointOfInterest(gfx::Point point) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "Setting point of interest to " << point.x() << ", " << point.y();

  if (!IsPointOfInterestSupported()) {
    return;
  }

  if (set_point_of_interest_running_ || ae_locked_for_point_of_interest_ ||
      on_af_trigger_cancelled_callback_) {
    // Cancel the current running one.
    // TODO(shik): Technically we can make the still capture hang if we keep
    // clicking the app quickly enough while taking the picture.
    set_point_of_interest_running_ = false;
    on_3a_mode_set_callback_.Reset();
    on_3a_stabilized_callback_.Reset();
    delayed_ae_unlock_callback_.Cancel();
    SetPointOfInterestUnlockAe();
    SetCaptureMetadata(
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER,
        cros::mojom::AndroidControlAfTrigger::
            ANDROID_CONTROL_AF_TRIGGER_CANCEL);
    // Due to pipeline dalay, we need to wait until AF_TRIGGER_CANCEL fired to
    // prevent the race condition.
    on_af_trigger_cancelled_callback_ = base::BindOnce(
        &Camera3AController::SetPointOfInterest, GetWeakPtr(), point);
    return;
  } else if (on_3a_stabilized_callback_ || on_3a_mode_set_callback_) {
    // Already stabilizing 3A for other things.
    return;
  }

  set_point_of_interest_running_ = true;

  auto active_array_size = [&]() {
    auto rect = GetMetadataEntryAsSpan<int32_t>(
        *static_metadata_,
        cros::mojom::CameraMetadataTag::ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
    DCHECK(!rect.empty());
    // (xmin, ymin, width, height)
    return gfx::Rect(rect[0], rect[1], rect[2], rect[3]);
  }();

  // Mimic the behavior of regionForNormalizedCoord() in GCA.
  int roi_radius =
      static_cast<int>(0.06125 * std::min(active_array_size.width(),
                                          active_array_size.height()));

  // (xmin, ymin, xmax, ymax, weight)
  std::vector<int32_t> region = {
      std::clamp(point.x() - roi_radius, 0, active_array_size.width() - 1),
      std::clamp(point.y() - roi_radius, 0, active_array_size.height() - 1),
      std::clamp(point.x() + roi_radius, 0, active_array_size.width() - 1),
      std::clamp(point.y() + roi_radius, 0, active_array_size.height() - 1),
      1,
  };

  SetRepeatingCaptureMetadata(
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_MODE,
      cros::mojom::AndroidControlMode::ANDROID_CONTROL_MODE_AUTO);

  if (ae_region_supported_) {
    SetRepeatingCaptureMetadata(
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_REGIONS, region);
  }
  if (af_region_supported_) {
    SetRepeatingCaptureMetadata(
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_REGIONS, region);
  }

  ae_mode_ = cros::mojom::AndroidControlAeMode::ANDROID_CONTROL_AE_MODE_ON;
  ae_mode_set_ = false;
  SetRepeatingCaptureMetadata(
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_MODE, ae_mode_);
  af_mode_ = cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_AUTO;
  af_mode_set_ = false;
  SetRepeatingCaptureMetadata(
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE, af_mode_);
  on_3a_mode_set_callback_ = base::BindOnce(
      &Camera3AController::SetPointOfInterestOn3AModeSet, GetWeakPtr());
}

void Camera3AController::SetPointOfInterestOn3AModeSet() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  Set3aStabilizedCallback(
      base::BindOnce(&Camera3AController::SetPointOfInterestOn3AStabilized,
                     GetWeakPtr()),
      base::Seconds(2));
  SetCaptureMetadata(
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_TRIGGER,
      cros::mojom::AndroidControlAfTrigger::ANDROID_CONTROL_AF_TRIGGER_START);
}

void Camera3AController::SetPointOfInterestOn3AStabilized() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  set_point_of_interest_running_ = false;
  SetRepeatingCaptureMetadata(
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_LOCK,
      cros::mojom::AndroidControlAeLock::ANDROID_CONTROL_AE_LOCK_ON);
  ae_locked_for_point_of_interest_ = true;
  if (on_ae_locked_for_point_of_interest_callback_) {
    std::move(on_ae_locked_for_point_of_interest_callback_).Run();
  }
  delayed_ae_unlock_callback_.Reset(base::BindOnce(
      &Camera3AController::SetPointOfInterestUnlockAe, GetWeakPtr()));
  // TODO(shik): Apply different delays for image capture / video recording.
  task_runner_->PostDelayedTask(
      FROM_HERE, delayed_ae_unlock_callback_.callback(), base::Seconds(4));
}

void Camera3AController::SetPointOfInterestUnlockAe() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  ae_locked_for_point_of_interest_ = false;
  ClearRepeatingCaptureMetadata();
}

base::WeakPtr<Camera3AController> Camera3AController::GetWeakPtr() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  return weak_ptr_factory_.GetWeakPtr();
}

void Camera3AController::Set3AMode(cros::mojom::CameraMetadataTag tag,
                                   uint8_t target_mode) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(tag == cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE ||
         tag == cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_MODE ||
         tag == cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_MODE);

  SetRepeatingCaptureMetadata(tag, target_mode);

  switch (tag) {
    case cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE:
      af_mode_set_ = false;
      break;
    case cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_MODE:
      ae_mode_set_ = false;
      break;
    case cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_MODE:
      awb_mode_set_ = false;
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid 3A mode: " << tag;
  }
}

void Camera3AController::Set3aStabilizedCallback(base::OnceClosure callback,
                                                 base::TimeDelta time_limit) {
  on_3a_stabilized_callback_ = std::move(callback);
  // TODO(shik): If this function is called before the first capture result
  // metadata is received, |latest_sensor_timestamp_| would be zero.
  artificial_3a_stabilized_deadline_ = latest_sensor_timestamp_ + time_limit;
}

bool Camera3AController::Is3AStabilized() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (af_mode_ !=
      cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_OFF) {
    if (af_state_ != cros::mojom::AndroidControlAfState::
                         ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED &&
        af_state_ != cros::mojom::AndroidControlAfState::
                         ANDROID_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED) {
      return false;
    }
  }

  if (ae_mode_ !=
      cros::mojom::AndroidControlAeMode::ANDROID_CONTROL_AE_MODE_OFF) {
    if (ae_state_ != cros::mojom::AndroidControlAeState::
                         ANDROID_CONTROL_AE_STATE_CONVERGED &&
        ae_state_ != cros::mojom::AndroidControlAeState::
                         ANDROID_CONTROL_AE_STATE_FLASH_REQUIRED &&
        ae_state_ != cros::mojom::AndroidControlAeState::
                         ANDROID_CONTROL_AE_STATE_LOCKED) {
      return false;
    }
  }

  if (awb_mode_ ==
      cros::mojom::AndroidControlAwbMode::ANDROID_CONTROL_AWB_MODE_AUTO) {
    if (awb_state_ != cros::mojom::AndroidControlAwbState::
                          ANDROID_CONTROL_AWB_STATE_CONVERGED &&
        awb_state_ != cros::mojom::AndroidControlAwbState::
                          ANDROID_CONTROL_AWB_STATE_LOCKED) {
      return false;
    }
  }

  DVLOG(1) << "3A stabilized";
  return true;
}

template <typename T>
void Camera3AController::SetCaptureMetadata(cros::mojom::CameraMetadataTag tag,
                                            T value) {
  SetCaptureMetadata(tag, std::vector<T>{value});
}

template <typename T>
void Camera3AController::SetCaptureMetadata(cros::mojom::CameraMetadataTag tag,
                                            const std::vector<T>& value) {
  capture_metadata_dispatcher_->SetCaptureMetadata(
      tag, entry_type_of<T>::value, value.size(),
      SerializeMetadataValueFromSpan(base::make_span(value)));
}

template <typename T>
void Camera3AController::SetRepeatingCaptureMetadata(
    cros::mojom::CameraMetadataTag tag,
    T value) {
  SetRepeatingCaptureMetadata(tag, std::vector<T>{value});
}

template <typename T>
void Camera3AController::SetRepeatingCaptureMetadata(
    cros::mojom::CameraMetadataTag tag,
    const std::vector<T>& value) {
  repeating_metadata_tags_.insert(tag);
  capture_metadata_dispatcher_->SetRepeatingCaptureMetadata(
      tag, entry_type_of<T>::value, value.size(),
      SerializeMetadataValueFromSpan(base::make_span(value)));
}

void Camera3AController::ClearRepeatingCaptureMetadata() {
  for (const auto& tag : repeating_metadata_tags_) {
    capture_metadata_dispatcher_->UnsetRepeatingCaptureMetadata(tag);
  }
  repeating_metadata_tags_.clear();
}

}  // namespace media
