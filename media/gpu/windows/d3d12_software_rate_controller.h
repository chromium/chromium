// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_SOFTWARE_RATE_CONTROLLER_H_
#define MEDIA_GPU_WINDOWS_D3D12_SOFTWARE_RATE_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "media/base/bitrate.h"
#include "media/base/encoder_status.h"
#include "media/gpu/h264_rate_controller.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

class VideoBitrateAllocation;

// Returns the HRD buffer size in bytes for the software rate controller. The
// buffer holds one second of `bitrate_bps`, so that a single large key frame or
// scroll frame does not overflow it, with a floor for very low bitrates.
MEDIA_GPU_EXPORT size_t
GetSoftwareRateControllerHRDBufferSize(uint32_t bitrate_bps);

// Maps input frame timestamps to the timeline used by `H264RateController`.
// The rate controller estimates the frame rate and bitrate from these
// timestamps, so they must follow the real frame timing. The mapped timeline
// starts at zero, never goes backwards, and stays finite: a frame whose
// timestamp does not move forward is placed one `frame_duration` after the
// previous frame, a large backward jump (a source timestamp reset) rebases the
// timeline, and large forward jumps are bounded so extreme inputs cannot
// saturate `base::TimeDelta`.
class MEDIA_GPU_EXPORT RateControllerTimestampMapper {
 public:
  RateControllerTimestampMapper();
  ~RateControllerTimestampMapper();

  base::TimeDelta Map(base::TimeDelta frame_timestamp,
                      base::TimeDelta frame_duration);

 private:
  std::optional<base::TimeDelta> last_mapped_timestamp_;
  base::TimeDelta anchor_input_timestamp_;
  base::TimeDelta anchor_mapped_timestamp_;
};

// Software bitrate control for the D3D12 H.264 and H.265 encoders, which run
// the hardware encoder in CQP mode and pick the QP of each frame here. Wraps
// `H264RateController` (also used for H.265) and keeps its settings, the HRD
// buffer sizes, and the frame timeline in sync.
class MEDIA_GPU_EXPORT D3D12SoftwareRateController {
 public:
  // Returns whether the software rate controller can handle `bitrate_mode`
  // with `num_temporal_layers` temporal layers.
  static bool IsSupported(Bitrate::Mode bitrate_mode,
                          size_t num_temporal_layers);

  // Creates a rate controller for `config`. `bitrate_allocation` is the
  // per-layer allocation for `config`, and the QP of every frame stays within
  // [`min_qp`, `max_qp`].
  static base::expected<std::unique_ptr<D3D12SoftwareRateController>,
                        EncoderStatus>
  Create(const VideoEncodeAccelerator::Config& config,
         const VideoBitrateAllocation& bitrate_allocation,
         size_t num_temporal_layers,
         uint32_t min_qp,
         uint32_t max_qp);

  D3D12SoftwareRateController(const D3D12SoftwareRateController&) = delete;
  D3D12SoftwareRateController& operator=(const D3D12SoftwareRateController&) =
      delete;
  ~D3D12SoftwareRateController();

  // Applies a new bitrate and frame rate. Returns false if they cannot be
  // used.
  bool UpdateRateControl(const VideoBitrateAllocation& bitrate_allocation,
                         uint32_t framerate);

  // Returns the QP for the next frame. `frame_timestamp` is the timestamp of
  // the input frame.
  uint32_t ComputeQp(base::TimeDelta frame_timestamp,
                     bool is_keyframe,
                     size_t temporal_idx);

  // Reports the encoded size of the frame last passed to `ComputeQp()`.
  void FinishFrame(size_t encoded_size, bool is_keyframe, size_t temporal_idx);

 private:
  explicit D3D12SoftwareRateController(H264RateControllerSettings settings);

  H264RateControllerSettings settings_;
  H264RateController rate_controller_;
  RateControllerTimestampMapper timestamp_mapper_;
  // The rate controller timestamp of the frame being encoded.
  base::TimeDelta current_timestamp_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_SOFTWARE_RATE_CONTROLLER_H_
