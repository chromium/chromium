// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "h264_video_rate_control_wrapper.h"

#include "media/gpu/h264_ratectrl_rtc.h"

namespace media {

template <>
int H264RateControl::GetLoopfilterLevel() const {
  DCHECK(impl_);
  return impl_->GetLoopfilterLevel();
}

template <>
void H264RateControl::PostEncodeUpdate(uint64_t encoded_frame_size,
                                       const FrameParams& frame_params) {
  DCHECK(impl_);
  return impl_->PostEncodeUpdate(encoded_frame_size,
                                 ConvertFrameParams(frame_params));
}

template <>
H264RateControlConfigRTC H264RateControl::ConvertControlConfig(
    const RateControlConfig& config) {
  // Limit max delay for intra frame with HRD buffer size (500ms-1s for camera
  // video, 1s-10s for desktop sharing).
  constexpr base::TimeDelta kHRDBufferDelayCamera = base::Milliseconds(1000);
  constexpr base::TimeDelta kHRDBufferDelayDisplay = base::Milliseconds(3000);
  H264RateControlConfigRTC rc_config;

  // Coded width and heght.
  rc_config.frame_size.SetSize(config.width, config.height);
  // Maximum GOP duration in milliseconds. It is set to maximum value.
  rc_config.gop_max_duration = base::TimeDelta::Max();
  // Source frame rate.
  rc_config.frame_rate_max = static_cast<float>(config.framerate);
  // Number of temopral layers.
  rc_config.num_temporal_layers = config.ts_number_layers;
  // Type of the video content (camera or display).
  rc_config.content_type = config.content_type;
  rc_config.fixed_delta_qp = true;
  rc_config.ease_hrd_reduction = true;
  for (int tid = 0; tid < config.ts_number_layers; ++tid) {
    rc_config.layer_settings.emplace_back();
    rc_config.layer_settings[tid].avg_bitrate =
        config.layer_target_bitrate[tid] * 1000;
    // Peak bitrate is set to 1.5 times the average bitrate.
    rc_config.layer_settings[tid].peak_bitrate =
        config.layer_target_bitrate[tid] * 1000 * 3 / 2;
    base::TimeDelta buffer_delay;
    if (config.content_type ==
        VideoEncodeAccelerator::Config::ContentType::kCamera) {
      buffer_delay = kHRDBufferDelayCamera;
    } else {
      buffer_delay = kHRDBufferDelayDisplay;
    }
    size_t buffer_size = static_cast<size_t>(
        rc_config.layer_settings[tid].avg_bitrate *
        buffer_delay.InMilliseconds() / base::Time::kMillisecondsPerSecond / 8);
    rc_config.layer_settings[tid].hrd_buffer_size = buffer_size;
    rc_config.layer_settings[tid].min_qp = config.min_quantizers[tid];
    rc_config.layer_settings[tid].max_qp = config.max_quantizers[tid];
    rc_config.layer_settings[tid].frame_rate = static_cast<float>(
        config.framerate / (1 << (config.ts_number_layers - tid - 1)));

    if (tid > 0) {
      DCHECK_GT(rc_config.layer_settings[tid].avg_bitrate,
                rc_config.layer_settings[tid - 1].avg_bitrate);
    }

  }
  return rc_config;
}

template <>
H264FrameParamsRTC H264RateControl::ConvertFrameParams(
    const FrameParams& frame_params) {
  H264FrameParamsRTC rc_params;
  rc_params.temporal_layer_id = frame_params.temporal_layer_id;
  rc_params.keyframe =
      frame_params.frame_type == FrameParams::FrameType::kKeyFrame;
  rc_params.timestamp = base::Milliseconds(frame_params.timestamp);
  return rc_params;
}

}  // namespace media
