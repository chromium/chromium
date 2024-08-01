// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "vp9_video_rate_control_wrapper.h"

#include "third_party/libvpx/source/libvpx/vp9/ratectrl_rtc.h"

namespace {

// The return value is expressed as a percentage of the average. For example,
// to allocate no more than 4.5 frames worth of bitrate to a keyframe, the
// return value is 450.
uint32_t MaxSizeOfKeyframeAsPercentage(uint32_t optimal_buffer_size,
                                       uint32_t max_framerate) {
  // Set max to the optimal buffer level (normalized by target BR),
  // and scaled by a scale_par.
  // Max target size = scale_par * optimal_buffer_size * targetBR[Kbps].
  // This value is presented in percentage of perFrameBw:
  // perFrameBw = targetBR[Kbps] * 1000 / framerate.
  // The target in % is as follows:
  const double target_size_byte_per_frame = optimal_buffer_size * 0.5;
  const uint32_t target_size_kbyte =
      target_size_byte_per_frame * max_framerate / 1000;
  const uint32_t target_size_kbyte_as_percent = target_size_kbyte * 100;

  // Don't go below 3 times the per frame bandwidth.
  constexpr uint32_t kMinIntraSizePercentage = 300u;
  return kMinIntraSizePercentage > target_size_kbyte_as_percent
             ? kMinIntraSizePercentage
             : target_size_kbyte_as_percent;
}

}  // namespace

namespace media {

template <>
int VP9RateControl::GetLoopfilterLevel() const {
  DCHECK(impl_);
  return impl_->GetLoopfilterLevel();
}

template <>
void VP9RateControl::PostEncodeUpdate(uint64_t encoded_frame_size,
                                      const FrameParams& frame_params) {
  DCHECK(impl_);
  return impl_->PostEncodeUpdate(encoded_frame_size,
                                 ConvertFrameParams(frame_params));
}

template <>
libvpx::VP9RateControlRtcConfig VP9RateControl::ConvertControlConfig(
    const RateControlConfig& config) {
  libvpx::VP9RateControlRtcConfig rc_config;
  rc_config.width = config.width;
  rc_config.height = config.height;
  rc_config.target_bandwidth = config.target_bandwidth;
  rc_config.framerate = config.framerate;
  rc_config.max_quantizer = config.max_quantizer;
  rc_config.min_quantizer = config.min_quantizer;
  // These default values come from
  // //third_party/webrtc/modules/video_coding/codecs/vp9/libvpx_vp9_encoder.cc.
  rc_config.buf_initial_sz = 500;
  rc_config.buf_optimal_sz = 600;
  rc_config.buf_sz = 1000;
  rc_config.undershoot_pct = 50;
  rc_config.overshoot_pct = 50;
  rc_config.aq_mode = 0;
  rc_config.rc_mode = VPX_CBR;
  rc_config.max_intra_bitrate_pct = MaxSizeOfKeyframeAsPercentage(
      rc_config.buf_optimal_sz, rc_config.framerate);
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
libvpx::VP9FrameParamsQpRTC VP9RateControl::ConvertFrameParams(
    const FrameParams& frame_params) {
  libvpx::VP9FrameParamsQpRTC rc_params;
  rc_params.spatial_layer_id = frame_params.spatial_layer_id;
  rc_params.temporal_layer_id = frame_params.temporal_layer_id;
  rc_params.frame_type =
      frame_params.frame_type == FrameParams::FrameType::kKeyFrame
          ? libvpx::RcFrameType::kKeyFrame
          : libvpx::RcFrameType::kInterFrame;
  return rc_params;
}

}  // namespace media
