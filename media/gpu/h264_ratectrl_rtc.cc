// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "h264_ratectrl_rtc.h"

#include "base/logging.h"
#include "media/gpu/h264_rate_control_util.h"

namespace media {
namespace {
void CheckRateControlConfig(const H264RateControlConfigRTC& config) {
  DCHECK_GT(config.gop_max_duration, base::TimeDelta());
  DCHECK_GT(config.num_temporal_layers, 0u);
  DCHECK_LE(config.num_temporal_layers,
            h264_rate_control_util::kMaxNumTemporalLayers);
  DCHECK_GT(config.frame_rate_max, 0.0f);

  // The minimum frame rate per temporal layer. The value is arbitrarily chosen.
  constexpr float kMinimumFrameRate = 0.4f;
  float base_layer_frame_rate = config.frame_rate_max;
  base_layer_frame_rate /= (1 << (config.num_temporal_layers - 1));
  DCHECK_GT(base_layer_frame_rate, kMinimumFrameRate);

  for (size_t tid = 0; tid < config.num_temporal_layers; ++tid) {
    DCHECK_GT(config.layer_settings[tid].avg_bitrate, 0u);
    DCHECK_GT(config.layer_settings[tid].peak_bitrate, 0u);
    DCHECK_GT(config.layer_settings[tid].hrd_buffer_size, 0u);
    DCHECK_GE(config.layer_settings[tid].min_qp,
              h264_rate_control_util::kQPMin);
    DCHECK_LE(config.layer_settings[tid].max_qp,
              h264_rate_control_util::kQPMax);
    DCHECK_GT(config.layer_settings[tid].max_qp,
              config.layer_settings[tid].min_qp);
    DCHECK_GT(config.layer_settings[tid].frame_rate, 0.0f);
    if (tid > 0) {
      DCHECK_GT(config.layer_settings[tid].avg_bitrate,
                config.layer_settings[tid - 1].avg_bitrate);
    }
  }
}

std::string CreateRateControlConfigLogMessage(
    const H264RateControlConfigRTC& config) {
  std::stringstream log_message;
  log_message << "width: " << config.frame_size.width()
              << ", height: " << config.frame_size.height()
              << ", gop_max_duration: "
              << config.gop_max_duration.InMilliseconds()
              << ", frame_rate_max: " << config.frame_rate_max
              << ", num_temporal_layers: " << config.num_temporal_layers
              << ", content_type: "
              << (config.content_type ==
                          VideoEncodeAccelerator::Config::ContentType::kCamera
                      ? "camera"
                      : "display")
              << ", fixed_delta_qp: "
              << (config.fixed_delta_qp ? "true" : "false")
              << ", ease_hrd_reduction: "
              << (config.ease_hrd_reduction ? "true" : "false");
  for (size_t tl = 0; tl < config.num_temporal_layers; tl++) {
    log_message << ", [ temporal_layer_id: " << tl
                << ", avg_bitrate: " << config.layer_settings[tl].avg_bitrate
                << ", peak_bitrate: " << config.layer_settings[tl].peak_bitrate
                << ", hrd_buffer_size: "
                << config.layer_settings[tl].hrd_buffer_size
                << ", min_qp: " << config.layer_settings[tl].min_qp
                << ", max_qp: " << config.layer_settings[tl].max_qp
                << ", frame_rate: " << config.layer_settings[tl].frame_rate
                << " ]";
  }

  return log_message.str();
}

}  // namespace

H264RateCtrlRTC::H264RateCtrlRTC(const H264RateControlConfigRTC& config)
    : config_(config), rate_controller_(config) {
  CheckRateControlConfig(config);

  DVLOG(1) << "Create H264RateCtrlRTC - "
           << CreateRateControlConfigLogMessage(config);

  // Initialize rate controller.
  rate_controller_.EstimateIntraFrameQP(base::Milliseconds(0));
  rate_controller_.reset_frame_number();
}

H264RateCtrlRTC::~H264RateCtrlRTC() = default;

std::unique_ptr<H264RateCtrlRTC> H264RateCtrlRTC::Create(
    const H264RateControlConfigRTC& config) {
  std::unique_ptr<H264RateCtrlRTC> rate_ctrl(new (std::nothrow)
                                                 H264RateCtrlRTC(config));
  return rate_ctrl;
}

void H264RateCtrlRTC::UpdateRateControl(
    const H264RateControlConfigRTC& config) {
  CheckRateControlConfig(config);

  DVLOG(1) << "Update H264RateCtrlRTC - "
           << CreateRateControlConfigLogMessage(config);

  // New settings are applied on ComputeQP() method call.
  new_config_ = config;
  config_changed_ = true;
}

H264RateCtrlRTC::FrameDropDecision H264RateCtrlRTC::ComputeQP(
    const H264FrameParamsRTC& frame_params) {
  DVLOG(3) << "Compute QP - "
           << "temporal_layer_id: " << frame_params.temporal_layer_id
           << ", timestamp: " << frame_params.timestamp.InMilliseconds()
           << ", frame_type: " << (frame_params.keyframe ? "I" : "P");

  if (config_changed_) {
    // Apply new config.
    rate_controller_.UpdateFrameSize(new_config_.frame_size);

    for (size_t tid = 0; tid < new_config_.num_temporal_layers; ++tid) {
      const H264RateControllerLayerSettings& new_layer_settings =
          new_config_.layer_settings[tid];

      rate_controller_.temporal_layers(tid).SetBufferParameters(
          new_layer_settings.hrd_buffer_size, new_layer_settings.avg_bitrate,
          new_layer_settings.peak_bitrate, new_config_.ease_hrd_reduction);
    }

    config_ = new_config_;
    config_changed_ = false;
  } else {
    // Shrink HRD buffer.
    for (size_t tid = 0; tid < config_.num_temporal_layers; ++tid) {
      rate_controller_.temporal_layers(tid).ShrinkHRDBuffer(
          frame_params.timestamp);
    }
  }

  if (!frame_params.keyframe) {
    rate_controller_.EstimateInterFrameQP(frame_params.temporal_layer_id,
                                          frame_params.timestamp);
  } else {
    rate_controller_.EstimateIntraFrameQP(frame_params.timestamp);
  }

  int starting_layer_id =
      config_.fixed_delta_qp
          ? config_.num_temporal_layers - 1
          : frame_params.temporal_layer_id;  // In fixed_delta_qp mode, take the
                                             // topmost layer.

  bool buffer_empty = true;
  int buffer_left = INT32_MAX;
  for (size_t tid = starting_layer_id; tid < config_.num_temporal_layers;
       ++tid) {
    int buffer_left_layer =
        rate_controller_.temporal_layers(tid).GetBufferBytesRemainingAtTime(
            frame_params.timestamp);
    if (buffer_left > buffer_left_layer) {
      buffer_left = buffer_left_layer;
    }
    if (buffer_left_layer <
        static_cast<int>(config_.layer_settings[tid].hrd_buffer_size)) {
      buffer_empty = false;
    }
  }

  int frame_qp =
      rate_controller_.temporal_layers(frame_params.temporal_layer_id)
          .curr_frame_qp();

  // Force encode a frame if we're already at max_qp and HRD is empty.
  bool allow_drop =
      !(buffer_empty &&
        frame_qp >=
            static_cast<int>(
                rate_controller_.temporal_layers(frame_params.temporal_layer_id)
                    .max_qp()));

  // Don't drop IDR.
  if (frame_params.keyframe) {
    allow_drop = false;
  }

  if (allow_drop && buffer_left == 0) {
    frame_qp_ = -1;  // Drop frame.
    DVLOG(3) << "Rate controller estimated QP: " << frame_qp_
             << " - frame drop";
    return FrameDropDecision::kDrop;
  }

  frame_qp_ =
      std::clamp(frame_qp, static_cast<int>(h264_rate_control_util::kQPMin),
                 static_cast<int>(h264_rate_control_util::kQPMax));

  DVLOG(3) << "Rate controller estimated QP: " << frame_qp_;
  return FrameDropDecision::kOk;
}

int H264RateCtrlRTC::GetQP() {
  return frame_qp_;
}

int H264RateCtrlRTC::GetLoopfilterLevel() const {
  return -1;
}

void H264RateCtrlRTC::PostEncodeUpdate(uint64_t encoded_frame_size,
                                       const H264FrameParamsRTC& frame_params) {
  DVLOG(3) << "Post encode update - "
           << "temporal_layer_id: " << frame_params.temporal_layer_id
           << ", timestamp: " << frame_params.timestamp.InMilliseconds()
           << ", frame_type: " << (frame_params.keyframe ? "I" : "P")
           << ", encoded_frame_size: " << encoded_frame_size;

  if (encoded_frame_size == 0) {
    return;
  }

  if (frame_params.keyframe) {
    rate_controller_.FinishIntraFrame(encoded_frame_size,
                                      frame_params.timestamp);
  } else {
    rate_controller_.FinishInterFrame(frame_params.temporal_layer_id,
                                      encoded_frame_size,
                                      frame_params.timestamp);
  }
}

void H264RateCtrlRTC::GetBufferFullness(base::span<int> buffer_fullness,
                                        base::TimeDelta timestamp) {
  rate_controller_.GetHRDBufferFullness(buffer_fullness, timestamp);
}

}  // namespace media
