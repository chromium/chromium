// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_AV1_VIDEO_RATE_CONTROL_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_AV1_VIDEO_RATE_CONTROL_WRAPPER_H_

#include "video_rate_control_wrapper.h"

namespace aom {
struct AV1RateControlRtcConfig;
struct AV1FrameParamsRTC;
class AV1RateControlRTC;
}  // namespace aom

namespace media {

using AV1RateControl =
    VideoRateControlWrapperInternal<aom::AV1RateControlRtcConfig,
                                    aom::AV1RateControlRTC,
                                    aom::AV1FrameParamsRTC>;
template <>
int AV1RateControl::GetLoopfilterLevel() const;

template <>
void AV1RateControl::PostEncodeUpdate(uint64_t encoded_frame_size,
                                      const FrameParams& frame_params);

template <>
aom::AV1RateControlRtcConfig AV1RateControl::ConvertControlConfig(
    const RateControlConfig& config);

template <>
aom::AV1FrameParamsRTC AV1RateControl::ConvertFrameParams(
    const FrameParams& frame_params);

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_AV1_VIDEO_RATE_CONTROL_WRAPPER_H_
