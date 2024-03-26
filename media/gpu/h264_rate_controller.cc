// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/h264_rate_controller.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "media/gpu/h264_rate_control_util.h"

namespace media {
namespace {
// Base temporal layer index.
constexpr int kBaseLayerIndex = 0;

// The set of dummy QP values used to simulate QP estimation.
constexpr uint32_t kDummyQpValues[] = {22, 24, 26, 28, 26, 24};

// Base layer to enhancement layer data rate ratio. It is used in fixed delta QP
// mode only.
constexpr float kLayerRateRatio = 0.8f;

// Initial QP size value used in initialization of the estimators. The value is
// chosen arbitrarily bases on common values for QP and P frame size.
constexpr float kInitQPSize = 100000.0f;

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

H264RateController::Layer::Layer(LayerSettings settings,
                                 float expected_fps,
                                 base::TimeDelta short_term_window_size,
                                 base::TimeDelta long_term_window_size)
    : hrd_buffer_(settings.hrd_buffer_size, settings.avg_bitrate),
      src_frame_rate_(kWindowFrameCount),
      expected_fps_(expected_fps),
      min_qp_(settings.min_qp),
      max_qp_(settings.max_qp),
      short_term_estimator_(short_term_window_size,
                            kInitQPSize,
                            GetInitialSizeCorrection(settings)),
      long_term_estimator_(long_term_window_size,
                           kInitQPSize,
                           GetInitialSizeCorrection(settings)),
      estimator_error_(long_term_window_size) {
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
  return static_cast<size_t>(hrd_buffer_.GetBytesAtTime(timestamp));
}

size_t H264RateController::Layer::GetBufferBytesRemainingAtTime(
    base::TimeDelta timestamp) const {
  return static_cast<size_t>(hrd_buffer_.GetBytesRemainingAtTime(timestamp));
}

float H264RateController::Layer::GetFrameRateMean() const {
  // Return the default value until the buffer is filled up.
  if (src_frame_rate_.Count() < kWindowFrameCount) {
    return expected_fps_;
  }

  base::TimeDelta timestamp_min = src_frame_rate_.Min();
  base::TimeDelta timestamp_max = src_frame_rate_.Max();
  base::TimeDelta duration = timestamp_max - timestamp_min;

  // Return the default value if the duration is too small or too big. Limiting
  // values are chosen arbitrarily.
  if (duration <= base::Milliseconds(1) || duration > base::Minutes(5)) {
    return expected_fps_;
  }

  return (kWindowFrameCount - 1) / duration.InSecondsF();
}

size_t H264RateController::Layer::EstimateShortTermFrameSize(
    uint32_t qp,
    uint32_t qp_prev) const {
  return short_term_estimator_.Estimate(qp, qp_prev);
}

size_t H264RateController::Layer::GetFrameSizeEstimatorError() const {
  return static_cast<size_t>(estimator_error_.GetStdDeviation());
}

void H264RateController::Layer::UpdateFrameSizeEstimator(
    size_t frame_bytes,
    uint32_t qp,
    uint32_t qp_prev,
    base::TimeDelta elapsed_time) {
  short_term_estimator_.Update(frame_bytes, qp, qp_prev, elapsed_time);
  long_term_estimator_.Update(frame_bytes, qp, qp_prev, elapsed_time);

  // Compute the per-frame rate prediction error.
  estimator_error_.AddValue(
      pred_p_frame_size_ - static_cast<float>(frame_bytes), elapsed_time);
}

float H264RateController::Layer::GetInitialSizeCorrection(
    LayerSettings settings) const {
  // The initial size correction is set to 0.3 x frame budget. The multiplier is
  // chosen arbitrarily.
  float bytes_per_frame_avg = settings.avg_bitrate / (8 * settings.frame_rate);
  return 0.3f * bytes_per_frame_avg;
}

H264RateController::ControllerSettings::ControllerSettings() = default;
H264RateController::ControllerSettings::~ControllerSettings() = default;
H264RateController::ControllerSettings::ControllerSettings(
    const ControllerSettings&) = default;
H264RateController::ControllerSettings&
H264RateController::ControllerSettings::operator=(const ControllerSettings&) =
    default;

H264RateController::H264RateController(ControllerSettings settings)
    : target_fps_(GetTargetFps(settings)),
      frame_size_(settings.frame_size),
      fixed_delta_qp_(settings.fixed_delta_qp),
      num_temporal_layers_(settings.num_temporal_layers),
      gop_max_duration_(settings.gop_max_duration),
      content_type_(settings.content_type) {
  DCHECK_GT(settings.num_temporal_layers, 0u);
  DCHECK_LE(settings.num_temporal_layers, kMaxNumTemporalLayers);
  // Short-term window is 5 x frame duration with the lowest value limited at
  // 300 ms. The values are chosen arbitrarily.
  base::TimeDelta short_term_window_size = base::Milliseconds(std::max(
      static_cast<int>(5.0f * base::Time::kMillisecondsPerSecond / target_fps_),
      300));
  // Set long-term window to 3 x HRD buffer size. Use uint64_t, as it might
  // overflow uint32_t.
  base::TimeDelta long_term_window_size = base::Milliseconds(
      3 *
      static_cast<uint64_t>(
          settings.layer_settings[kBaseLayerIndex].hrd_buffer_size * 8) *
      base::Time::kMillisecondsPerSecond /
      settings.layer_settings[kBaseLayerIndex].avg_bitrate);
  for (auto& tls : settings.layer_settings) {
    temporal_layers_.emplace_back(std::make_unique<Layer>(
        tls, target_fps_, short_term_window_size, long_term_window_size));
  }
}

H264RateController::~H264RateController() = default;

void H264RateController::EstimateIntraFrameQP(base::TimeDelta frame_timestamp) {
  // Estimating the target intra frame encoded frame size.
  size_t target_bytes_frame = GetTargetBytesForIntraFrame(frame_timestamp);

  // Applying Rate-Distortion model.
  const float bpp =
      target_bytes_frame * 8.0f / (frame_size_.width() * frame_size_.height());
  const float q_step =
      kIntraFrameMAD /
      (std::pow(bpp / std::pow(2, kRDYIntercept), 1 / kRDSlope));

  temporal_layers_[kBaseLayerIndex]->update_curr_frame_qp(std::clamp(
      h264_rate_control_util::QStepSize2QP(q_step),
      h264_rate_control_util::kQPMin, h264_rate_control_util::kQPMax));
}

void H264RateController::EstimateInterFrameQP(size_t temporal_id,
                                              base::TimeDelta frame_timestamp) {
  H264RateController::Layer& curr_layer = *temporal_layers_[temporal_id];

  // The QP for P frames is taken form a set of constant values. This will be
  // implemented in the subsequent CLs.
  uint32_t curr_qp = kDummyQpValues[frame_number_++ % (sizeof(kDummyQpValues) /
                                                       sizeof(uint32_t))];

  // Update the frame rate statistics.
  curr_layer.AddFrameTimestamp(frame_timestamp);

  curr_layer.update_long_term_qp(curr_qp);

  // For the fixed delta QP, take the buffer parameters from the topmost layer.
  const size_t buffer_layer_id =
      fixed_delta_qp_ ? num_temporal_layers_ - 1 : temporal_id;
  uint32_t max_rate_bytes_per_sec =
      temporal_layers_[buffer_layer_id]->average_bitrate() / 8;
  size_t buffer_size = temporal_layers_[buffer_layer_id]->buffer_size();
  int buffer_level_current =
      temporal_layers_[buffer_layer_id]->GetBufferBytesAtTime(frame_timestamp);

  size_t frame_size_target = GetTargetBytesForInterFrame(
      temporal_id, max_rate_bytes_per_sec, buffer_size, buffer_level_current,
      frame_timestamp);
  curr_layer.update_last_frame_size_target(frame_size_target);

  DVLOG(1) << "Estimated QP: " << curr_qp;

  curr_layer.update_curr_frame_qp(curr_qp);

  // Limit the quality.
  curr_layer.update_curr_frame_qp(std::clamp(
      curr_layer.curr_frame_qp(), curr_layer.min_qp(), curr_layer.max_qp()));

  curr_layer.update_pred_p_frame_size(curr_layer.EstimateShortTermFrameSize(
      curr_layer.curr_frame_qp(), curr_layer.last_frame_qp()));
}

void H264RateController::FinishIntraFrame(size_t access_unit_bytes,
                                          base::TimeDelta frame_timestamp) {
  FinishLayerData(kBaseLayerIndex, access_unit_bytes, frame_timestamp);

  FinishLayerPreviousFrameTimestamp(kBaseLayerIndex, frame_timestamp);

  last_idr_timestamp_ = frame_timestamp;
}

void H264RateController::FinishInterFrame(size_t temporal_id,
                                          size_t access_unit_bytes,
                                          base::TimeDelta frame_timestamp) {
  FinishLayerData(temporal_id, access_unit_bytes, frame_timestamp);

  H264RateController::Layer& l = *temporal_layers_[temporal_id];

  const base::TimeDelta elapsed_time =
      h264_rate_control_util::ClampedTimestampDiff(
          frame_timestamp, l.previous_frame_timestamp());

  l.UpdateFrameSizeEstimator(access_unit_bytes, l.curr_frame_qp(),
                             l.last_frame_qp(), elapsed_time);

  FinishLayerPreviousFrameTimestamp(temporal_id, frame_timestamp);
}

void H264RateController::GetHRDBufferFullness(
    base::span<int> buffer_fullness,
    base::TimeDelta frame_timestamp) const {
  for (size_t tl = kBaseLayerIndex;
       tl < std::min(buffer_fullness.size(), num_temporal_layers_); ++tl) {
    buffer_fullness[tl] =
        (100 * temporal_layers_[tl]->GetBufferBytesAtTime(frame_timestamp)) /
        static_cast<int>(temporal_layers_[tl]->buffer_size());
  }
}

void H264RateController::FinishLayerData(size_t temporal_id,
                                         size_t frame_bytes,
                                         base::TimeDelta frame_timestamp) {
  // Update HRDs for all temporal leyars.
  for (size_t tl = temporal_id; tl < num_temporal_layers_; ++tl) {
    temporal_layers_[tl]->AddFrameBytes(frame_bytes, frame_timestamp);
    temporal_layers_[tl]->update_last_frame_qp(
        temporal_layers_[tl]->curr_frame_qp());
    temporal_layers_[tl]->update_last_frame_type(FrameType::kPFrame);
  }
}

void H264RateController::FinishLayerPreviousFrameTimestamp(
    size_t temporal_id,
    base::TimeDelta frame_timestamp) {
  // Update timestamps for all tamporal layers.
  for (size_t tl = temporal_id; tl < num_temporal_layers_; ++tl) {
    temporal_layers_[tl]->update_previous_frame_timestamp(frame_timestamp);
  }
}

size_t H264RateController::GetTargetBytesForIntraFrame(
    base::TimeDelta frame_timestamp) const {
  // Find the layer with the minimum buffer bytes remaining. The remaining
  // bytes are used to estimate the target bytes for the intra frame. Since
  // the intra frame is encoded in the base layer, the intra frame bytes are
  // added to the buffers of all upper layers. Thats's why the intra encoded
  // frame size is estimated based on the fullest buffer among all layers.
  const size_t starting_layer_id =
      fixed_delta_qp_ ? num_temporal_layers_ - 1 : kBaseLayerIndex;
  size_t min_bytes_remaining_layer_id = kBaseLayerIndex;
  int bytes_remaining = INT32_MAX;
  for (size_t tl = starting_layer_id; tl < num_temporal_layers_; ++tl) {
    int bytes_remaining_tl =
        temporal_layers_[tl]->GetBufferBytesRemainingAtTime(frame_timestamp);
    if (bytes_remaining > bytes_remaining_tl) {
      bytes_remaining = bytes_remaining_tl;
      min_bytes_remaining_layer_id = tl;
    }
  }

  const size_t buffer_bytes =
      temporal_layers_[min_bytes_remaining_layer_id]->GetBufferBytesAtTime(
          frame_timestamp);
  const size_t hrd_buffer_size =
      temporal_layers_[min_bytes_remaining_layer_id]->buffer_size();

  // The minimum target intra frame fill up is 0.5 x HRD size.
  size_t min_bytes_target = 0;
  if (hrd_buffer_size / 2 >= buffer_bytes) {
    min_bytes_target = hrd_buffer_size / 2 - buffer_bytes;
  }

  // The target fill up should be above the minimum value. The minimum value is
  // calculated by multiplying the average budget of the encoded frame by a
  // value from the range 1 to 4. The multiplier is 4 x frame_budget for 15fps
  // (and above) and 1x for 3.75 fps (and below). It is 4x for the desktop
  // video source. The boundary values are chosen arbitrarily.
  float intra_frame_multiplier =
      (content_type_ == VideoEncodeAccelerator::Config::ContentType::kDisplay)
          ? 4.0f
          : std::clamp(
                temporal_layers_[starting_layer_id]->GetFrameRateMean() / 3.75f,
                1.0f, 4.0f);
  size_t bytes_target =
      std::max(min_bytes_target,
               static_cast<size_t>(
                   GetRateBudget(
                       temporal_layers_[starting_layer_id]->GetFrameRateMean(),
                       temporal_layers_[starting_layer_id]->average_bitrate()) *
                   intra_frame_multiplier));
  bytes_target = std::min(bytes_target, hrd_buffer_size);

  return bytes_target;
}

size_t H264RateController::GetTargetBytesForInterFrame(
    size_t temporal_id,
    uint32_t max_rate_bytes_per_sec,
    size_t buffer_size,
    int buffer_level_current,
    base::TimeDelta frame_timestamp) const {
  // The long-term frame size is calculated based on short-term stats and
  // long-term QP parameters.
  size_t frame_size_long_term =
      temporal_layers_[temporal_id]->EstimateShortTermFrameSize(
          temporal_layers_[temporal_id]->long_term_qp(),
          temporal_layers_[temporal_id]->last_frame_qp());

  // Calculate bitrate allocated for the current layer. This value doesn't
  // include the bitrate of the lower layers. In case of fixed delta QP, the
  // the bitrate ratio between layers is fixed.
  uint32_t curr_layer_bitrate;
  if (fixed_delta_qp_ && num_temporal_layers_ > 1) {
    DCHECK_EQ(num_temporal_layers_, kMaxNumTemporalLayers);
    curr_layer_bitrate = static_cast<uint32_t>(
        temporal_layers_[kBaseLayerIndex + 1]->average_bitrate() *
        kLayerRateRatio);
  } else {
    uint32_t lower_layer_bitrate =
        temporal_id == kBaseLayerIndex
            ? 0u
            : temporal_layers_[temporal_id - 1]->average_bitrate();
    curr_layer_bitrate =
        temporal_layers_[temporal_id]->average_bitrate() - lower_layer_bitrate;
  }

  float frame_rate = std::clamp(
      temporal_layers_[temporal_id]->GetFrameRateMean(), 1.0f, target_fps_);

  size_t frame_size_budget =
      static_cast<size_t>(curr_layer_bitrate / 8 / frame_rate);

  float frame_size_deviation =
      static_cast<float>(fabs(static_cast<int>(frame_size_long_term) -
                              static_cast<int>(frame_size_budget))) /
      frame_size_budget;
  float frame_size_compress =
      h264_rate_control_util::ClampedLinearInterpolation(
          frame_size_deviation, 0.5f, 3.0f, 0.1f, 0.9f);

  int frame_size_target =
      static_cast<int>(static_cast<int>(frame_size_budget) +
                       (static_cast<int>(frame_size_long_term) -
                        static_cast<int>(frame_size_budget)) *
                           (1 - frame_size_compress));

  DCHECK_GT(frame_size_target, 0);

  // Correct the target frame size based on current buffer level.
  size_t buffer_target_low = frame_size_budget;
  size_t buffer_target_high = std::max(frame_size_budget, buffer_size / 5);

  // The remaining time to the end of GOP.
  base::TimeDelta frame_remaining_gop = base::Milliseconds(800);
  if (gop_max_duration_ > base::TimeDelta() &&
      buffer_target_high > buffer_target_low) {
    frame_remaining_gop =
        last_idr_timestamp_ - frame_timestamp + gop_max_duration_;
  }

  // Size correction window is a linear transformation of the remaining time in
  // GOP.
  uint32_t size_correction_window =
      static_cast<uint32_t>(h264_rate_control_util::ClampedLinearInterpolation(
          static_cast<float>(frame_remaining_gop.InMilliseconds()), 0.0f,
          2000.0f, 200.0f, 800.0f));

  base::TimeDelta buffer_duration = base::Milliseconds(
      static_cast<float>(buffer_size) / max_rate_bytes_per_sec *
      base::Time::kMillisecondsPerSecond);

  int size_correction = 0;
  if (buffer_level_current + frame_size_target >
      static_cast<int>(buffer_target_high)) {
    // Windowed overshoot prevention.
    uint32_t win = buffer_duration.InMilliseconds() * 2;
    size_correction_window = std::min(size_correction_window, win);
    size_correction = static_cast<int>(
        -(static_cast<int>(buffer_level_current) + frame_size_target -
          static_cast<int>(buffer_target_high)) /
        static_cast<float>(size_correction_window) / frame_rate *
        base::Time::kMillisecondsPerSecond);
  } else if (buffer_level_current + frame_size_target <
             static_cast<int>(buffer_target_low)) {
    // Windowed undershoot prevention.
    uint32_t win = buffer_duration.InMilliseconds();
    size_correction_window = std::min(size_correction_window, win);
    size_correction =
        static_cast<int>((static_cast<int>(buffer_target_low) -
                          buffer_level_current - frame_size_target) /
                         static_cast<float>(size_correction_window) /
                         frame_rate * base::Time::kMillisecondsPerSecond);
  }

  frame_size_target = std::clamp(frame_size_target + size_correction,
                                 frame_size_target / 5, frame_size_target * 5);

  size_t frame_size_error =
      temporal_layers_[temporal_id]->GetFrameSizeEstimatorError();

  // Instantaneous undershoot prevention (buffer should not be empty after
  // the frame is removed).
  int buf_level_pre_fill_next_frame = buffer_level_current + frame_size_target -
                                      static_cast<int>(frame_size_budget);
  if (buf_level_pre_fill_next_frame - static_cast<int>(frame_size_error) < 0) {
    frame_size_target -= buf_level_pre_fill_next_frame;
    frame_size_target += frame_size_error;
  }

  // Instantaneous overshoot prevention (buffer should not overshoot after
  // the frame is added).
  int buf_level_post_fill = buffer_level_current + frame_size_target;

  if (buf_level_post_fill + frame_size_error > buffer_size) {
    frame_size_target -= buf_level_post_fill - static_cast<int>(buffer_size);
    frame_size_target -= frame_size_error;
  }

  frame_size_target =
      std::max(frame_size_target, static_cast<int>(frame_size_budget / 5));

  return static_cast<size_t>(frame_size_target);
}

float H264RateController::GetTargetFps(ControllerSettings settings) const {
  DCHECK_EQ(settings.layer_settings.size(), settings.num_temporal_layers);
  return settings.layer_settings[settings.num_temporal_layers - 1].frame_rate;
}

}  // namespace media
