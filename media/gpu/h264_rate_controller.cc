// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/h264_rate_controller.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "media/gpu/h264_rate_control_util.h"

namespace media {
namespace {
// Base temporal layer index.
constexpr int kBaseLayerIndex = 0;

// Delta QP between layers in Fixed Delta QP mode. It is arbitrary chosen value.
constexpr int kFixedLayerDeltaQP = 4;

// Maximum FPS used in the tradeoff calculation between FPS and maximum QP.
constexpr float kFpsMax = 60;

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

// Arbitrarily chosen value for the minimum QP of the first encoded intra frame.
constexpr uint32_t kMinFirstFrameQP = 34u;

// The constants kRDSlope and kRDYIntercept are the slope and Y-intercept of the
// linear approximation in the expression
// log2(bpp) = a * log2(mad / q_step) + b.
// a - kRDSlope
// b - kRDYIntercept
// The optimal values for kRDSlope and kRDYIntercept are derived from the
// analysis of rate and distortion values over a large set of data.
constexpr float kRDSlope = 0.91f;
constexpr float kRDYIntercept = -6.0f;

// The arrays define line segments in the tradeoff function between FPS and
// maximum QP .
constexpr struct {
  float fps;
  float qp;
} kFPS2QPTradeoffs[] = {{0.0f, 51.0f},
                        {5.0f, 42.0f},
                        {10.0f, 41.0f},
                        {15.0f, 40.0f},
                        {30.0f, 37.0f},
                        {kFpsMax, 37.0f},
                        {std::numeric_limits<float>::max(), 20.0f}};

// Window size in number of frames for the Moving Window. The average framerate
// is based on the last received frames within the window.
constexpr int kWindowFrameCount = 3;

// Returns a budget in bytes per frame for the given frame rate and average
// bitrate. The budget represents the amount of data equally distributed among
// frames.
size_t GetRateBudget(float frame_rate, uint32_t avg_bitrate) {
  return static_cast<size_t>(avg_bitrate / 8.0f / frame_rate);
}

// Returns the FPS value related to the Max QP value. The function is
// represented by line segments defined in the array `kFPS2QPTradeoffs`.
float Fps2MaxQP(float fps) {
  size_t num_elems = sizeof(kFPS2QPTradeoffs) / sizeof(kFPS2QPTradeoffs[0]);
  for (size_t i = 0; i < num_elems - 1; ++i) {
    if (fps >= kFPS2QPTradeoffs[i].fps && fps < kFPS2QPTradeoffs[i + 1].fps) {
      return h264_rate_control_util::ClampedLinearInterpolation(
          fps, kFPS2QPTradeoffs[i].fps, kFPS2QPTradeoffs[i + 1].fps,
          kFPS2QPTradeoffs[i].qp, kFPS2QPTradeoffs[i + 1].qp);
    }
  }
  NOTREACHED_IN_MIGRATION();
  return 0.0f;
}

// Returns the FPS value related to the Max QP value. The returned value is
// a constant value obtained from the `kFPS2QPTradeoffs` array.
float MaxQP2Fps(int max_qp) {
  size_t num_elems = sizeof(kFPS2QPTradeoffs) / sizeof(kFPS2QPTradeoffs[0]);
  for (size_t i = 0; i < num_elems - 1; ++i) {
    if (max_qp <= kFPS2QPTradeoffs[i].qp &&
        max_qp > kFPS2QPTradeoffs[i + 1].qp) {
      // Do not use linear interpolation to be less aggressive on FPS changes.
      return kFPS2QPTradeoffs[i + 1].fps;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return 0.0f;
}
}  // namespace

H264RateControllerSettings::H264RateControllerSettings() = default;
H264RateControllerSettings::~H264RateControllerSettings() = default;
H264RateControllerSettings::H264RateControllerSettings(
    const H264RateControllerSettings&) = default;
H264RateControllerSettings& H264RateControllerSettings::operator=(
    const H264RateControllerSettings&) = default;

std::partial_ordering H264RateControllerSettings::operator<=>(
    const H264RateControllerSettings& other) const {
  if (auto res = frame_size.width() <=> other.frame_size.width(); res != 0) {
    return res;
  }
  if (auto res = frame_size.height() <=> other.frame_size.height(); res != 0) {
    return res;
  }
  if (auto res = fixed_delta_qp <=> other.fixed_delta_qp; res != 0) {
    return res;
  }
  if (auto res = frame_rate_max <=> other.frame_rate_max; res != 0) {
    return res;
  }
  if (auto res = num_temporal_layers <=> other.num_temporal_layers; res != 0) {
    return res;
  }
  if (auto res = gop_max_duration.InMilliseconds() <=>
                 other.gop_max_duration.InMilliseconds();
      res != 0) {
    return res;
  }
  return std::lexicographical_compare_three_way(
      layer_settings.begin(), layer_settings.end(),
      other.layer_settings.begin(), other.layer_settings.end());
}

H264RateController::Layer::Layer(H264RateControllerLayerSettings settings,
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

size_t H264RateController::Layer::EstimateLongTermFrameSize(
    uint32_t qp,
    uint32_t qp_prev) const {
  return long_term_estimator_.Estimate(qp, qp_prev);
}

uint32_t H264RateController::Layer::EstimateShortTermQP(
    size_t target_frame_bytes,
    uint32_t qp_prev) const {
  return short_term_estimator_.InverseEstimate(target_frame_bytes, qp_prev);
}

uint32_t H264RateController::Layer::EstimateLongTermQP(
    size_t target_frame_bytes,
    uint32_t qp_prev) const {
  return long_term_estimator_.InverseEstimate(target_frame_bytes, qp_prev);
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
    H264RateControllerLayerSettings settings) const {
  // The initial size correction is set to 0.3 x frame budget. The multiplier is
  // chosen arbitrarily.
  float bytes_per_frame_avg = settings.avg_bitrate / (8 * settings.frame_rate);
  return 0.3f * bytes_per_frame_avg;
}

H264RateController::H264RateController(H264RateControllerSettings settings)
    : target_fps_(GetTargetFps(settings)),
      frame_rate_max_(settings.frame_rate_max),
      frame_size_(settings.frame_size),
      fixed_delta_qp_(settings.fixed_delta_qp),
      num_temporal_layers_(settings.num_temporal_layers),
      gop_max_duration_(settings.gop_max_duration),
      content_type_(settings.content_type) {
  DCHECK_GT(settings.num_temporal_layers, 0u);
  DCHECK_LE(settings.num_temporal_layers,
            h264_rate_control_util::kMaxNumTemporalLayers);
  DCHECK_GT(target_fps_, 1.0f);
  DCHECK_GT(frame_rate_max_, 1.0f);
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
  H264RateController::Layer& base_layer = *temporal_layers_[kBaseLayerIndex];

  ++frame_number_;

  // Update the frame rate statistics.
  base_layer.AddFrameTimestamp(frame_timestamp);

  if (0 == frame_number_) {
    target_fps_ = std::min(target_fps_, frame_rate_max_);
  }

  // Estimating the target intra frame encoded frame size.
  size_t target_bytes_frame = GetTargetBytesForIntraFrame(frame_timestamp);

  // Applying Rate-Distortion model.
  const float bpp =
      target_bytes_frame * 8.0f / (frame_size_.width() * frame_size_.height());
  const float q_step =
      kIntraFrameMAD /
      (std::pow(bpp / std::pow(2, kRDYIntercept), 1 / kRDSlope));

  uint32_t curr_qp = std::clamp(h264_rate_control_util::QStepSize2QP(q_step),
                                h264_rate_control_util::kQPMin,
                                h264_rate_control_util::kQPMax);

  if (0 == frame_number_) {
    // The initial long term QP. The subtracted value is chosen arbitrarily.
    base_layer.update_long_term_qp(curr_qp - 3);

    // Limit minimum QP value for the first IDR.
    curr_qp = std::max(curr_qp, kMinFirstFrameQP);
  } else if (frame_number_ > 0) {
    // Prevent quality flickering.
    // If the previous frame was dropped, make sure QP will increase.
    if (base_layer.is_buffer_full()) {
      // base_layer.curr_frame_qp should point to the QP used for dropped
      // frame.
      if (curr_qp > base_layer.curr_frame_qp() + 2) {
        curr_qp = (curr_qp + base_layer.curr_frame_qp() + 2) / 2;
      } else {
        curr_qp = base_layer.curr_frame_qp() + 2;
      }
    } else if (base_layer.last_frame_type() ==
               H264RateController::FrameType::kPFrame) {
      // Limit QP for IDR frames based on the QP estimated for the previous P
      // frame. The offset for the minimum value is a constant, while the offset
      // for the maximum value is calclated as a linear function of the frame
      // rate. The constants are chosen arbitrarily, based on the analysis of
      // the real use cases.
      constexpr float kMinQPOffsetForIDR = -3.0f;
      constexpr float kMaxQPOffsetForIDRLowerLimit = 6.0f;
      constexpr float kMaxQPOffsetForIDRUpperLimit = 15.0f;
      constexpr float kFrameRateToMaxQPSlope = -0.67f;
      constexpr float kFrameRateToMaxQPYIntercept = 16.0f;
      float max_qp_offset_for_idr =
          kFrameRateToMaxQPSlope * base_layer.GetFrameRateMean() +
          kFrameRateToMaxQPYIntercept;
      max_qp_offset_for_idr =
          std::clamp(max_qp_offset_for_idr, kMaxQPOffsetForIDRLowerLimit,
                     kMaxQPOffsetForIDRUpperLimit);
      const float last_qp =
          std::max(base_layer.long_term_qp(), base_layer.last_frame_qp());
      curr_qp = static_cast<uint32_t>(
          std::clamp(static_cast<float>(curr_qp), last_qp + kMinQPOffsetForIDR,
                     last_qp + max_qp_offset_for_idr));
    } else if (base_layer.last_frame_type() ==
               H264RateController::FrameType::kIFrame) {
      curr_qp = std::clamp(curr_qp, base_layer.last_frame_qp() - 1,
                           base_layer.last_frame_qp() + 3);
    }
  }

  // Limit highest possible quality.
  base_layer.update_curr_frame_qp(
      std::clamp(curr_qp, base_layer.min_qp(), base_layer.max_qp()));

  base_layer.update_long_term_qp(std::clamp(base_layer.long_term_qp(),
                                            base_layer.min_qp(),
                                            h264_rate_control_util::kQPMax));

  last_idr_timestamp_ = frame_timestamp;
}

void H264RateController::EstimateInterFrameQP(size_t temporal_id,
                                              base::TimeDelta frame_timestamp) {
  H264RateController::Layer& curr_layer = *temporal_layers_[temporal_id];
  H264RateController::Layer& base_layer = *temporal_layers_[kBaseLayerIndex];

  ++frame_number_;

  // Update the frame rate statistics.
  curr_layer.AddFrameTimestamp(frame_timestamp);

  // Compute a baselayer QP that together with layer delta QP's fit the channel
  // rates.
  if (frame_number_ > 2) {
    base_layer.update_long_term_qp(GetInterFrameLongTermQP(temporal_id));
  }

  curr_layer.update_long_term_qp(std::clamp(curr_layer.long_term_qp(),
                                            curr_layer.min_qp(),
                                            h264_rate_control_util::kQPMax));

  // The enhancement layer QP in Fixed Delta QP mode is calculated by adding a
  // fixed difference to the base layer's QP. In the case of buffer overflow, a
  // statistical model is employed for QP estimation.
  if (fixed_delta_qp_ && temporal_id > kBaseLayerIndex &&
      !curr_layer.is_buffer_full()) {
    int delta_qp = kFixedLayerDeltaQP;
    // delta_qp is reduced if the QP estimation for the last base layer frame is
    // lower than the minimum QP.
    if (base_layer.undershoot_delta_qp() > 0) {
      delta_qp = std::max(delta_qp - base_layer.undershoot_delta_qp(), 0);
    }
    curr_layer.update_curr_frame_qp(base_layer.curr_frame_qp() + delta_qp);
    return;
  }

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

  uint32_t curr_qp = GetInterFrameShortTermQP(temporal_id, frame_size_target);

  curr_qp = ClipInterFrameQP(curr_qp, temporal_id, frame_timestamp);

  // Don't use post-fill here because estimated error can be inaccurate (scene
  // change) and bias the decision.
  const bool hrd_buffer_is_full =
      buffer_level_current >= static_cast<int>(buffer_size);
  if (hrd_buffer_is_full) {
    // HRD buffer is already full: use max QP to limit the damage.
    curr_qp = curr_layer.max_qp();
  }

  // Limit the quality.
  curr_layer.update_curr_frame_qp(
      std::clamp(curr_qp, curr_layer.min_qp(), curr_layer.max_qp()));

  curr_layer.update_pred_p_frame_size(curr_layer.EstimateShortTermFrameSize(
      curr_layer.curr_frame_qp(), curr_layer.last_frame_qp()));
}

void H264RateController::FinishIntraFrame(size_t access_unit_bytes,
                                          base::TimeDelta frame_timestamp) {
  FinishLayerData(kBaseLayerIndex, FrameType::kIFrame, access_unit_bytes,
                  frame_timestamp);

  FinishLayerPreviousFrameTimestamp(kBaseLayerIndex, frame_timestamp);

  last_idr_timestamp_ = frame_timestamp;

  if (0 == frame_number_) {
    // To minimize risks of HRD violation on first P frames, first frame QP is
    // used to readjust target FPS.
    float buffer_level_norm =
        static_cast<float>(
            temporal_layers_[kBaseLayerIndex]->last_frame_buffer_bytes()) /
        temporal_layers_[kBaseLayerIndex]->buffer_size();

    if (0.5f < buffer_level_norm) {
      const float max_qp_from_fps = Fps2MaxQP(target_fps_);
      if (temporal_layers_[kBaseLayerIndex]->long_term_qp() > max_qp_from_fps) {
        target_fps_ = MaxQP2Fps(static_cast<int>(
            temporal_layers_[kBaseLayerIndex]->long_term_qp()));
      }
    }
  }

  SetLastTsOvershootingFrame(kBaseLayerIndex, frame_timestamp);
}

void H264RateController::FinishInterFrame(size_t temporal_id,
                                          size_t access_unit_bytes,
                                          base::TimeDelta frame_timestamp) {
  FinishLayerData(temporal_id, FrameType::kPFrame, access_unit_bytes,
                  frame_timestamp);

  H264RateController::Layer& curr_layer = *temporal_layers_[temporal_id];

  const base::TimeDelta elapsed_time =
      h264_rate_control_util::ClampedTimestampDiff(
          frame_timestamp, curr_layer.previous_frame_timestamp());

  curr_layer.UpdateFrameSizeEstimator(access_unit_bytes,
                                      curr_layer.curr_frame_qp(),
                                      curr_layer.last_frame_qp(), elapsed_time);

  FinishLayerPreviousFrameTimestamp(temporal_id, frame_timestamp);

  SetLastTsOvershootingFrame(temporal_id, frame_timestamp);
}

void H264RateController::UpdateFrameSize(const gfx::Size& frame_size) {
  frame_size_ = frame_size;
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
                                         FrameType frame_type,
                                         size_t frame_bytes,
                                         base::TimeDelta frame_timestamp) {
  // Update HRDs for all temporal leyars.
  for (size_t tl = temporal_id; tl < num_temporal_layers_; ++tl) {
    temporal_layers_[tl]->AddFrameBytes(frame_bytes, frame_timestamp);
    temporal_layers_[tl]->update_last_frame_qp(
        temporal_layers_[tl]->curr_frame_qp());
    temporal_layers_[tl]->update_last_frame_type(frame_type);
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

void H264RateController::SetLastTsOvershootingFrame(
    size_t temporal_id,
    base::TimeDelta frame_timestamp) {
  for (size_t tl = temporal_id; tl < num_temporal_layers_; ++tl) {
    bool check_overshoot = !fixed_delta_qp_ || tl == num_temporal_layers_ - 1;
    if (!check_overshoot || !temporal_layers_[tl]->is_buffer_full()) {
      last_ts_overshooting_frame_ = base::TimeDelta::Max();
    } else if (last_ts_overshooting_frame_ == base::TimeDelta::Max()) {
      last_ts_overshooting_frame_ = frame_timestamp;
    }
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
    DCHECK_EQ(num_temporal_layers_,
              h264_rate_control_util::kMaxNumTemporalLayers);
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

uint32_t H264RateController::GetInterFrameShortTermQP(
    size_t temporal_id,
    size_t frame_size_target) {
  uint32_t curr_qp = temporal_layers_[temporal_id]->EstimateShortTermQP(
      frame_size_target, temporal_layers_[temporal_id]->last_frame_qp());
  curr_qp = std::clamp(curr_qp, h264_rate_control_util::kQPMin,
                       h264_rate_control_util::kQPMax);
  if (fixed_delta_qp_) {
    temporal_layers_[temporal_id]->update_undershoot_delta_qp(
        static_cast<int>(temporal_layers_[temporal_id]->min_qp()) -
        static_cast<int>(curr_qp));
  }

  return curr_qp;
}

uint32_t H264RateController::GetInterFrameLongTermQP(size_t temporal_id) {
  float target_rate_bytes_per_sec = static_cast<float>(
      temporal_layers_[kBaseLayerIndex]->average_bitrate() / 8);
  float frame_rate = temporal_layers_[kBaseLayerIndex]->GetFrameRateMean();
  size_t target_frame_bytes =
      static_cast<uint32_t>(target_rate_bytes_per_sec / frame_rate);
  uint32_t long_term_qp = temporal_layers_[temporal_id]->EstimateLongTermQP(
      target_frame_bytes, temporal_layers_[temporal_id]->last_frame_qp());

  // Does this baselayer QP fit the channel rate? If not, increase it.
  constexpr int kMaxQPIter = 10;
  constexpr float kBitrateThreshold = 1.1f;
  for (int i = 0; i < kMaxQPIter; i++) {
    size_t layer_bytes =
        temporal_layers_[temporal_id]->EstimateLongTermFrameSize(
            long_term_qp, temporal_layers_[temporal_id]->last_frame_qp());
    float bitrate = 8 * layer_bytes * frame_rate;
    if (bitrate >
        temporal_layers_[temporal_id]->average_bitrate() * kBitrateThreshold) {
      long_term_qp += 1;
    } else {
      break;
    }
  }

  return std::clamp(long_term_qp, h264_rate_control_util::kQPMin,
                    h264_rate_control_util::kQPMax);
}

uint32_t H264RateController::ClipInterFrameQP(uint32_t curr_qp,
                                              size_t temporal_id,
                                              base::TimeDelta frame_timestamp) {
  // Decrease the minimum QP limit by 1 when the frame rate falls below 3 fps.
  constexpr float kMinQPFrameRateThreshold = 3.0f;
  // Maximum Delta QP between consecutive layers.
  constexpr int kMaxDeltaQP = 6;

  uint32_t min_qp = h264_rate_control_util::kQPMin,
           max_qp = h264_rate_control_util::kQPMax;

  if (temporal_id == kBaseLayerIndex) {
    if (temporal_layers_[kBaseLayerIndex]->last_frame_qp() > 0) {
      min_qp = temporal_layers_[kBaseLayerIndex]->last_frame_qp() -
               (temporal_layers_[kBaseLayerIndex]->GetFrameRateMean() <
                        kMinQPFrameRateThreshold
                    ? 2
                    : 1);
      max_qp = std::max(temporal_layers_[kBaseLayerIndex]->last_frame_qp() + 3,
                        (temporal_layers_[kBaseLayerIndex]->min_qp() +
                         temporal_layers_[kBaseLayerIndex]->max_qp()) /
                            2);
    }
  } else {
    min_qp = temporal_layers_[kBaseLayerIndex]->curr_frame_qp();
    max_qp = temporal_layers_[kBaseLayerIndex]->curr_frame_qp() + kMaxDeltaQP;
  }

  // QP coupling between temporal layers.
  // Raise base QP if enhancement layer buffer is in danger.
  if (!fixed_delta_qp_ && num_temporal_layers_ > 1) {
    std::array<int, 2> buffer_fullness_array = {0, 0};
    base::span<int> buffer_fullness_values(buffer_fullness_array);
    GetHRDBufferFullness(buffer_fullness_values, frame_timestamp);
    int enhance_buffer_fullness = buffer_fullness_values[kBaseLayerIndex + 1];
    if (limit_base_qp_ && temporal_id == kBaseLayerIndex) {
      uint32_t enhance_qp =
          temporal_layers_[kBaseLayerIndex + 1]->curr_frame_qp();
      uint32_t min_base_qp;
      if (enhance_buffer_fullness > 95) {
        min_base_qp = enhance_qp - 2;
      } else if (enhance_buffer_fullness > 90) {
        min_base_qp = enhance_qp - 3;
      } else if (enhance_buffer_fullness > 80) {
        min_base_qp = enhance_qp - 4;
      } else if (enhance_buffer_fullness > 70) {
        min_base_qp = enhance_qp - 5;
      } else {
        min_base_qp = enhance_qp - 6;
      }
      min_base_qp = std::max(min_base_qp, enhance_qp - kMaxDeltaQP);
      min_qp = std::max(min_qp, min_base_qp);
    } else if (temporal_id > kBaseLayerIndex) {
      int layer_delta =
          static_cast<int>(curr_qp) -
          static_cast<int>(temporal_layers_[kBaseLayerIndex]->curr_frame_qp());
      int qp_trend =
          static_cast<int>(curr_qp) -
          static_cast<int>(temporal_layers_[temporal_id]->last_frame_qp());
      if (layer_delta >= kMaxDeltaQP) {
        if (enhance_buffer_fullness > 60 && qp_trend > 0) {
          limit_base_qp_ = true;
        }
      } else {
        if (limit_base_qp_) {
          if (enhance_buffer_fullness < 35 && qp_trend < 0) {
            limit_base_qp_ = false;
          }
        }
      }
    }
  } else if (num_temporal_layers_ > 1 && temporal_id == kBaseLayerIndex) {
    if (temporal_layers_[kBaseLayerIndex + 1]->curr_frame_qp() >
        temporal_layers_[kBaseLayerIndex]->curr_frame_qp() +
            kFixedLayerDeltaQP) {
      // Delta QP more than 4 means enhancement layer QP has been raised due to
      // HRD overflow. Make sure the following base layer QP follows.
      min_qp = std::max(min_qp,
                        temporal_layers_[kBaseLayerIndex + 1]->curr_frame_qp() -
                            kFixedLayerDeltaQP);
    }
  }

  // Raise min QP if previous frame has been dropped.
  if (temporal_layers_[temporal_id]->is_buffer_full()) {
    // curr_frame_qp should point to the QP used for the dropped frame.
    uint32_t lower_bound =
        std::min(temporal_layers_[temporal_id]->curr_frame_qp() + 2,
                 h264_rate_control_util::kQPMax);
    min_qp = std::clamp(min_qp, lower_bound, h264_rate_control_util::kQPMax);
  }

  // Min QP may have been raised. Need to make sure max_qp >= min_qp. Also,
  // avoid too low maximum QP value. The lowest maximum QP value is chosen
  // arbitrarily.
  constexpr uint32_t kMaxQPLowestValue = 28u;
  max_qp = std::clamp(max_qp, min_qp, h264_rate_control_util::kQPMax);
  max_qp =
      std::clamp(max_qp, kMaxQPLowestValue, h264_rate_control_util::kQPMax);

  // QP range continues growing as long as frames overshoot. Out of order
  // timestamps are ignored.
  constexpr int kQPStepDuration = 33;
  if (last_ts_overshooting_frame_ != base::TimeDelta::Max() &&
      frame_timestamp > last_ts_overshooting_frame_) {
    base::TimeDelta delta_ts_overshooting_frame =
        frame_timestamp - last_ts_overshooting_frame_;
    uint32_t delta_qp =
        std::max(delta_ts_overshooting_frame.InMilliseconds() / kQPStepDuration,
                 delta_ts_overshooting_frame.InMilliseconds() *
                     delta_ts_overshooting_frame.InMilliseconds() /
                     (kQPStepDuration * kQPStepDuration));
    max_qp += delta_qp;
    min_qp += delta_qp;
  }

  return std::clamp(curr_qp, min_qp, max_qp);
}

float H264RateController::GetTargetFps(
    H264RateControllerSettings settings) const {
  DCHECK_EQ(settings.layer_settings.size(), settings.num_temporal_layers);
  return settings.layer_settings[settings.num_temporal_layers - 1].frame_rate;
}

}  // namespace media
