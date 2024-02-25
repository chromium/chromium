// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/h264_rate_controller.h"

#include "base/time/time.h"
#include "media/gpu/h264_rate_control_util.h"

namespace media {
namespace {
// The constant kIntraFrameMAD is the average MAD between the original and
// predicted pixels for intra frames in H.264 video. The average is calculated
// over a set of frames with a common complexity level.
constexpr float kIntraFrameMAD = 768.0f;

// The maximum number of temporal layers in the stream.
constexpr size_t kMaxNumTemporalLayers = 2u;

// The constants kRDSlope and kRDYIntercept are the slope and Y-intercept of the
// linear approximation in the expression
// log2(bpp) = a * log2(mad / q_step) + b.
// a - kRDSlope
// b - kRDYIntercept
// The optimal values for kRDSlope and kRDYIntercept are derived from the
// analysis of rate and distortion values over a large set of data.
constexpr float kRDSlope = 0.91f;
constexpr float kRDYIntercept = -6.0f;

// Window size in number of frames for the Moving Window. The average framerate
// is based on the last received frames within the window.
constexpr int kWindowFrameCount = 3;

// Returns a budget in bytes per frame for the given frame rate and average
// bitrate. The budget represents the amount of data equally distributed among
// frames.
size_t GetRateBudget(float frame_rate, uint32_t avg_bitrate) {
  return static_cast<size_t>(avg_bitrate / 8.0f / frame_rate);
}

}  // namespace

H264RateController::Layer::Layer(LayerSettings settings, float expected_fps)
    : hrd_buffer_(settings.hrd_buffer_size, settings.avg_bitrate),
      src_frame_rate_(kWindowFrameCount),
      settings_(settings),
      expected_fps_(expected_fps) {
  DCHECK_GT(settings.hrd_buffer_size, 0u);
  DCHECK_GT(settings.avg_bitrate, 0u);
  DCHECK_GT(expected_fps, 0.0f);
}

H264RateController::Layer::~Layer() = default;

void H264RateController::Layer::ShrinkHRDBuffer(base::TimeDelta timestamp) {
  hrd_buffer_.Shrink(timestamp);
}

void H264RateController::Layer::AddFrameBytes(size_t frame_bytes,
                                              base::TimeDelta frame_timestamp) {
  hrd_buffer_.AddFrameBytes(frame_bytes, frame_timestamp);
}

void H264RateController::Layer::AddFrameTimestamp(
    base::TimeDelta frame_timestamp) {
  src_frame_rate_.AddSample(frame_timestamp);
}

void H264RateController::Layer::SetBufferParameters(size_t buffer_size,
                                                    uint32_t avg_bitrate,
                                                    uint32_t peak_bitrate,
                                                    bool ease_hrd_reduction) {
  hrd_buffer_.SetParameters(buffer_size, avg_bitrate, peak_bitrate,
                            ease_hrd_reduction);
}

size_t H264RateController::Layer::GetBufferBytesAtTime(
    base::TimeDelta timestamp) const {
  return hrd_buffer_.GetBytesAtTime(timestamp);
}

size_t H264RateController::Layer::GetBufferBytesRemainingAtTime(
    base::TimeDelta timestamp) const {
  return hrd_buffer_.GetBytesRemainingAtTime(timestamp);
}

float H264RateController::Layer::GetFrameRateMean() const {
  // Return the default value until the buffer is filled up.
  if (src_frame_rate_.Count() < kWindowFrameCount) {
    return expected_fps_;
  }

  base::TimeDelta timestamp_min = src_frame_rate_.Min();
  base::TimeDelta timestamp_max = src_frame_rate_.Max();
  base::TimeDelta duration = timestamp_max - timestamp_min;

  // Return the default value if the duration is too small. 1 ms is arbitrarily
  // chosen value.
  if (duration.InMilliseconds() <= 1) {
    return expected_fps_;
  }

  return (kWindowFrameCount - 1) / duration.InSecondsF();
}

H264RateController::ControllerSettings::ControllerSettings() = default;
H264RateController::ControllerSettings::~ControllerSettings() = default;
H264RateController::ControllerSettings::ControllerSettings(
    const ControllerSettings&) = default;
H264RateController::ControllerSettings&
H264RateController::ControllerSettings::operator=(const ControllerSettings&) =
    default;

H264RateController::H264RateController(ControllerSettings settings)
    : settings_(settings) {
  DCHECK_GT(settings_.num_temporal_layers, 0u);
  DCHECK_LE(settings_.num_temporal_layers, kMaxNumTemporalLayers);
  DCHECK_EQ(settings_.layers.size(), settings_.num_temporal_layers);
  for (auto& tl : settings_.layers) {
    temporal_layers_.emplace_back(std::make_unique<Layer>(
        tl, settings_.layers[settings_.num_temporal_layers - 1].frame_rate));
  }
}

H264RateController::~H264RateController() = default;

uint32_t H264RateController::EstimateIntraFrameQP(
    base::TimeDelta picture_timestamp) const {
  // Estimating the target intra frame encoded frame size.
  size_t target_bytes_frame = GetTargetBytesForIntraFrame(picture_timestamp);

  // Applying Rate-Distortion model.
  const float bpp =
      target_bytes_frame * 8.0f /
      (settings_.frame_size.width() * settings_.frame_size.height());
  const float q_step =
      kIntraFrameMAD /
      (std::pow(bpp / std::pow(2, kRDYIntercept), 1 / kRDSlope));

  return std::clamp(h264_rate_control_util::QStepSize2QP(q_step),
                    h264_rate_control_util::kQPMin,
                    h264_rate_control_util::kQPMax);
}

size_t H264RateController::GetTargetBytesForIntraFrame(
    base::TimeDelta picture_timestamp) const {
  // Find the layer with the minimum buffer bytes remaining. The remaining
  // bytes are used to estimate the target bytes for the intra frame. Since
  // the intra frame is encoded in the base layer, the intra frame bytes are
  // added to the buffers of all upper layers. Thats's why the intra encoded
  // frame size is estimated based on the fullest buffer among all layers.
  const size_t starting_layer_id =
      settings_.fixed_delta_qp ? settings_.num_temporal_layers - 1 : 0;
  size_t min_bytes_remaining_layer_id = 0;
  int bytes_remaining = INT32_MAX;
  for (size_t tl = starting_layer_id; tl < settings_.num_temporal_layers;
       ++tl) {
    int bytes_remaining_tl =
        temporal_layers_[tl]->GetBufferBytesRemainingAtTime(picture_timestamp);
    if (bytes_remaining > bytes_remaining_tl) {
      bytes_remaining = bytes_remaining_tl;
      min_bytes_remaining_layer_id = tl;
    }
  }

  const size_t buffer_bytes =
      temporal_layers_[min_bytes_remaining_layer_id]->GetBufferBytesAtTime(
          picture_timestamp);
  const size_t hrd_buffer_size =
      settings_.layers[min_bytes_remaining_layer_id].hrd_buffer_size;

  // The minimum target intra frame fill up is 0.5 x HRD size.
  size_t min_bytes_target = hrd_buffer_size / 2 - buffer_bytes;

  // The target fill up should be above the minimum value. The minimum value is
  // calculated by multiplying the average budget of the encoded frame by a
  // value from the range 1 to 4. The multiplier is 4 x frame_budget for 15fps
  // (and above) and 1x for 3.75 fps (and below). It is 4x for the desktop
  // video source. The boundary values are chosen arbitrarily.
  float intra_frame_multiplier =
      (settings_.content_type ==
       VideoEncodeAccelerator::Config::ContentType::kDisplay)
          ? 4.0f
          : std::clamp(
                temporal_layers_[starting_layer_id]->GetFrameRateMean() / 3.75f,
                1.0f, 4.0f);
  size_t bytes_target = std::max(
      min_bytes_target,
      static_cast<size_t>(
          GetRateBudget(temporal_layers_[starting_layer_id]->GetFrameRateMean(),
                        settings_.layers[starting_layer_id].avg_bitrate) *
          intra_frame_multiplier));
  bytes_target = std::min(bytes_target, hrd_buffer_size);

  return bytes_target;
}

}  // namespace media
