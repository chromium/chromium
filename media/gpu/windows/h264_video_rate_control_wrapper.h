// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_H264_VIDEO_RATE_CONTROL_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_H264_VIDEO_RATE_CONTROL_WRAPPER_H_

#include "media/gpu/windows/video_rate_control_wrapper.h"

namespace media {
struct H264RateControllerSettings;
typedef H264RateControllerSettings H264RateControlConfigRTC;
struct H264FrameParamsRTC;
class H264RateCtrlRTC;

using H264RateControl =
    VideoRateControlWrapperInternal<H264RateControlConfigRTC,
                                    H264RateCtrlRTC,
                                    H264FrameParamsRTC>;

template <>
int H264RateControl::GetLoopfilterLevel() const;

template <>
void H264RateControl::PostEncodeUpdate(uint64_t encoded_frame_size,
                                       const FrameParams& frame_params);
template <>
H264RateControlConfigRTC H264RateControl::ConvertControlConfig(
    const RateControlConfig& config);

template <>
H264FrameParamsRTC H264RateControl::ConvertFrameParams(
    const FrameParams& frame_params);

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_H264_VIDEO_RATE_CONTROL_WRAPPER_H_
