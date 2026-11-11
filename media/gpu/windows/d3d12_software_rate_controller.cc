// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_software_rate_controller.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/encoder_status.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/h264_rate_control_util.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

namespace {

void UpdateLayerSettings(H264RateControllerSettings& settings,
                         const VideoBitrateAllocation& bitrate_allocation,
                         uint32_t framerate) {
  const float peak_target_ratio =
      1.0f * bitrate_allocation.GetPeakBps() / bitrate_allocation.GetSumBps();
  uint32_t sum_bitrate = 0;
  for (size_t i = 0; i < settings.layer_settings.size(); ++i) {
    H264RateControllerLayerSettings& layer = settings.layer_settings[i];
    sum_bitrate += bitrate_allocation.GetBitrateBps(0, i);
    layer.avg_bitrate = sum_bitrate;
    layer.peak_bitrate =
        bitrate_allocation.GetMode() == Bitrate::Mode::kConstant
            ? sum_bitrate
            : base::saturated_cast<uint32_t>(sum_bitrate * peak_target_ratio);
    layer.frame_rate = framerate / static_cast<float>(1u << i);
    layer.hrd_buffer_size = GetSoftwareRateControllerHRDBufferSize(sum_bitrate);
  }
}

}  // namespace

size_t GetSoftwareRateControllerHRDBufferSize(uint32_t bitrate_bps) {
  constexpr size_t kMinHRDBufferSize = 40000;
  return std::max<size_t>(bitrate_bps / 8, kMinHRDBufferSize);
}

RateControllerTimestampMapper::RateControllerTimestampMapper() = default;

RateControllerTimestampMapper::~RateControllerTimestampMapper() = default;

base::TimeDelta RateControllerTimestampMapper::Map(
    base::TimeDelta frame_timestamp,
    base::TimeDelta frame_duration) {
  CHECK_GT(frame_duration, base::TimeDelta());

  // Clamp inputs away from TimeDelta::Min()/Max() while preserving the full
  // WebCodecs timestamp range (+/-(2^53 - 1) microseconds, ~285 years) so
  // subtracting any two clamped timestamps cannot overflow int64_t.
  constexpr base::TimeDelta kMaxInputTimestamp = base::Days(365 * 1000);
  // Matches the 5-minute upper bound in ClampedTimestampDiff() and
  // H264RateController::Layer::GetFrameRateMean().
  constexpr base::TimeDelta kMaxForwardStep = base::Minutes(5);
  constexpr base::TimeDelta kMaxBackwardJitter = base::Seconds(1);
  // Keep the mapped timeline well below TimeDelta::Max() so downstream
  // timestamp differences can never saturate to Max() - Max().
  constexpr base::TimeDelta kMaxMappedTimestamp = base::Days(365 * 100000);

  const base::TimeDelta clamped_input =
      std::clamp(frame_timestamp, -kMaxInputTimestamp, kMaxInputTimestamp);
  const base::TimeDelta step = std::min(frame_duration, kMaxForwardStep);

  if (!last_mapped_timestamp_.has_value()) {
    anchor_input_timestamp_ = clamped_input;
    anchor_mapped_timestamp_ = base::TimeDelta();
    last_mapped_timestamp_ = base::TimeDelta();
    return base::TimeDelta();
  }

  const base::TimeDelta last_mapped = *last_mapped_timestamp_;
  const base::TimeDelta anchor_lag = last_mapped - anchor_mapped_timestamp_;
  const base::TimeDelta input_delta = clamped_input - anchor_input_timestamp_;

  base::TimeDelta mapped;
  if (input_delta > anchor_lag) {
    const base::TimeDelta forward_step =
        std::min(input_delta - anchor_lag, kMaxForwardStep);
    mapped = std::min(last_mapped + forward_step, kMaxMappedTimestamp);
    anchor_input_timestamp_ = clamped_input;
    anchor_mapped_timestamp_ = mapped;
  } else {
    // Small backward steps and repeated timestamps are jitter: keep the
    // timeline moving by one nominal frame. A larger backward jump means the
    // source timestamps were reset, so rebase the anchor at this frame.
    mapped = std::min(last_mapped + step, kMaxMappedTimestamp);
    if (anchor_lag - input_delta > kMaxBackwardJitter) {
      anchor_input_timestamp_ = clamped_input;
      anchor_mapped_timestamp_ = mapped;
    }
  }

  last_mapped_timestamp_ = mapped;
  return mapped;
}

// static
bool D3D12SoftwareRateController::IsSupported(Bitrate::Mode bitrate_mode,
                                              size_t num_temporal_layers) {
  return (bitrate_mode == Bitrate::Mode::kConstant ||
          bitrate_mode == Bitrate::Mode::kVariable) &&
         num_temporal_layers > 0 &&
         num_temporal_layers <= h264_rate_control_util::kMaxNumTemporalLayers;
}

// static
base::expected<std::unique_ptr<D3D12SoftwareRateController>, EncoderStatus>
D3D12SoftwareRateController::Create(
    const VideoEncodeAccelerator::Config& config,
    const VideoBitrateAllocation& bitrate_allocation,
    size_t num_temporal_layers,
    uint32_t min_qp,
    uint32_t max_qp) {
  CHECK(IsSupported(bitrate_allocation.GetMode(), num_temporal_layers));
  if (bitrate_allocation.GetSumBps() == 0) {
    return base::unexpected(EncoderStatus(
        EncoderStatus::Codes::kEncoderUnsupportedConfig, "Bitrate is zero"));
  }
  if (config.framerate == 0) {
    return base::unexpected(EncoderStatus(
        EncoderStatus::Codes::kEncoderUnsupportedConfig, "Framerate is zero"));
  }

  H264RateControllerSettings settings;
  settings.content_type = config.content_type;
  settings.frame_size = config.input_visible_size;
  settings.frame_rate_max = config.framerate;
  // At low frame rates (such as static screen content) a frame-based GOP lasts
  // much longer in wall time than `gop_length / framerate`. Match Media
  // Foundation and VAAPI by leaving `gop_max_duration` unbounded so the rate
  // controller does not narrow its correction window mid-GOP.
  settings.gop_max_duration = base::TimeDelta::Max();
  // TODO(crbug.com/40275246): `fixed_delta_qp` is `std::optional<int>`, and the
  // legacy delegate code assigned `false` (implicitly engaging fixed-delta-QP
  // mode with delta 0). Evaluate switching to `std::nullopt` in a separate CL
  // since it changes L1T2 behavior.
  settings.fixed_delta_qp = 0;
  settings.ease_hrd_reduction = false;
  settings.num_temporal_layers = num_temporal_layers;
  H264RateControllerLayerSettings layer_settings;
  layer_settings.min_qp = min_qp;
  layer_settings.max_qp = max_qp;
  settings.layer_settings.assign(num_temporal_layers, layer_settings);
  UpdateLayerSettings(settings, bitrate_allocation, config.framerate);

  return base::WrapUnique(new D3D12SoftwareRateController(std::move(settings)));
}

D3D12SoftwareRateController::D3D12SoftwareRateController(
    H264RateControllerSettings settings)
    : settings_(std::move(settings)), rate_controller_(settings_) {}

D3D12SoftwareRateController::~D3D12SoftwareRateController() = default;

bool D3D12SoftwareRateController::UpdateRateControl(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  if (!IsSupported(bitrate_allocation.GetMode(),
                   settings_.layer_settings.size()) ||
      bitrate_allocation.GetSumBps() == 0 || framerate == 0) {
    return false;
  }

  settings_.frame_rate_max = framerate;
  UpdateLayerSettings(settings_, bitrate_allocation, framerate);
  for (size_t i = 0; i < settings_.layer_settings.size(); ++i) {
    const H264RateControllerLayerSettings& layer = settings_.layer_settings[i];
    rate_controller_.temporal_layers(i).SetBufferParameters(
        layer.hrd_buffer_size, layer.avg_bitrate, layer.peak_bitrate,
        settings_.ease_hrd_reduction);
  }
  return true;
}

uint32_t D3D12SoftwareRateController::ComputeQp(base::TimeDelta frame_timestamp,
                                                bool is_keyframe,
                                                size_t temporal_idx) {
  // Feed the rate controller the real frame timing. Screen content often
  // arrives far below the configured frame rate, and a synthetic timeline
  // would make the rate controller overestimate the bitrate.
  current_timestamp_ = timestamp_mapper_.Map(
      frame_timestamp,
      base::Seconds(1) / settings_.layer_settings[0].frame_rate);

  H264RateController::Layer& layer =
      rate_controller_.temporal_layers(temporal_idx);
  layer.ShrinkHRDBuffer(current_timestamp_);
  if (is_keyframe) {
    rate_controller_.EstimateIntraFrameQP(current_timestamp_);
  } else {
    rate_controller_.EstimateInterFrameQP(temporal_idx, current_timestamp_);
  }
  return layer.curr_frame_qp();
}

void D3D12SoftwareRateController::FinishFrame(size_t encoded_size,
                                              bool is_keyframe,
                                              size_t temporal_idx) {
  if (is_keyframe) {
    rate_controller_.FinishIntraFrame(encoded_size, current_timestamp_);
  } else {
    rate_controller_.FinishInterFrame(temporal_idx, encoded_size,
                                      current_timestamp_);
  }
}

}  // namespace media
