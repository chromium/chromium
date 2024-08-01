// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "av1_video_rate_control_wrapper.h"

#include "third_party/libaom/source/libaom/av1/ratectrl_rtc.h"

namespace media {

template <>
int AV1RateControl::GetLoopfilterLevel() const {
  return -1;
}

template <>
void AV1RateControl::PostEncodeUpdate(uint64_t encoded_frame_size,
                                      const FrameParams& frame_params) {
  DCHECK(impl_);
  return impl_->PostEncodeUpdate(encoded_frame_size);
}

template <>
aom::AV1RateControlRtcConfig AV1RateControl::ConvertControlConfig(
    const RateControlConfig& config) {
  aom::AV1RateControlRtcConfig rc_config;
  rc_config.width = config.width;
  rc_config.height = config.height;
  rc_config.target_bandwidth = config.target_bandwidth;
  rc_config.framerate = config.framerate;
  rc_config.max_quantizer = config.max_quantizer;
  rc_config.min_quantizer = config.min_quantizer;
  // Default value from
  // //third_party/webrtc/modules/video_coding/codecs/av1/libaom_av1_encoder.cc.
  rc_config.buf_initial_sz = 600;
  rc_config.buf_optimal_sz = 600;
  rc_config.buf_sz = 1000;
  rc_config.undershoot_pct = 50;
  rc_config.overshoot_pct = 50;
  rc_config.aq_mode = 0;
  rc_config.max_intra_bitrate_pct = 50;
  rc_config.max_inter_bitrate_pct = 0;
  rc_config.ss_number_layers = config.ss_number_layers;
  rc_config.ts_number_layers = config.ts_number_layers;
  for (int tid = 0; tid < config.ts_number_layers; ++tid) {
    rc_config.ts_rate_decimator[tid] = config.ts_rate_decimator[tid];
  }
  for (int sid = 0; sid < config.ss_number_layers; ++sid) {
    rc_config.scaling_factor_num[sid] = config.scaling_factor_num[sid];
    rc_config.scaling_factor_den[sid] = config.scaling_factor_den[sid];
    for (int tid = 0; tid < config.ts_number_layers; ++tid) {
      const int i = sid * config.ts_number_layers + tid;
      rc_config.max_quantizers[i] = config.max_quantizers[i];
      rc_config.min_quantizers[i] = config.min_quantizers[i];
      rc_config.layer_target_bitrate[i] = config.layer_target_bitrate[i];
    }
  }
  return rc_config;
}

template <>
aom::AV1FrameParamsRTC AV1RateControl::ConvertFrameParams(
    const FrameParams& frame_params) {
  aom::AV1FrameParamsRTC rc_params;
  rc_params.spatial_layer_id = frame_params.spatial_layer_id;
  rc_params.temporal_layer_id = frame_params.temporal_layer_id;
  rc_params.frame_type =
      frame_params.frame_type == FrameParams::FrameType::kKeyFrame
          ? aom::kKeyFrame
          : aom::kInterFrame;
  return rc_params;
}

}  // namespace media
