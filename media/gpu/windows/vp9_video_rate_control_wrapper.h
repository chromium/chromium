// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_VP9_VIDEO_RATE_CONTROL_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_VP9_VIDEO_RATE_CONTROL_WRAPPER_H_

#include "video_rate_control_wrapper.h"

namespace libvpx {
struct VP9FrameParamsQpRTC;
class VP9RateControlRTC;
struct VP9RateControlRtcConfig;
}  // namespace libvpx

namespace media {

using VP9RateControl =
    VideoRateControlWrapperInternal<libvpx::VP9RateControlRtcConfig,
                                    libvpx::VP9RateControlRTC,
                                    libvpx::VP9FrameParamsQpRTC>;

template <>
int VP9RateControl::GetLoopfilterLevel() const;

template <>
void VP9RateControl::PostEncodeUpdate(uint64_t encoded_frame_size,
                                      const FrameParams& frame_params);

template <>
libvpx::VP9RateControlRtcConfig VP9RateControl::ConvertControlConfig(
    const RateControlConfig& config);

template <>
libvpx::VP9FrameParamsQpRTC VP9RateControl::ConvertFrameParams(
    const FrameParams& frame_params);

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_VP9_VIDEO_RATE_CONTROL_WRAPPER_H_
