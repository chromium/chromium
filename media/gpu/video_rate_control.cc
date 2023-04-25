// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/video_rate_control.h"

#include "third_party/libaom/source/libaom/av1/ratectrl_rtc.h"
#include "third_party/libvpx/source/libvpx/vp8/vp8_ratectrl_rtc.h"
#include "third_party/libvpx/source/libvpx/vp9/ratectrl_rtc.h"

namespace media {

// Template method specialization for VP9.
// TODO(mcasas): Remove when VP8 also has a GetLoopfilterLevel() method.
template <>
int VideoRateControl<libvpx::VP9RateControlRtcConfig,
                     libvpx::VP9RateControlRTC,
                     libvpx::VP9FrameParamsQpRTC>::GetLoopfilterLevel() const {
  return impl_->GetLoopfilterLevel();
}

template <>
void VideoRateControl<libvpx::VP9RateControlRtcConfig,
                      libvpx::VP9RateControlRTC,
                      libvpx::VP9FrameParamsQpRTC>::
    PostEncodeUpdate(uint64_t encoded_frame_size,
                     const libvpx::VP9FrameParamsQpRTC& frame_params) {
  impl_->PostEncodeUpdate(encoded_frame_size, frame_params);
}

template <>
void VideoRateControl<libvpx::VP8RateControlRtcConfig,
                      libvpx::VP8RateControlRTC,
                      libvpx::VP8FrameParamsQpRTC>::
    PostEncodeUpdate(uint64_t encoded_frame_size,
                     const libvpx::VP8FrameParamsQpRTC& frame_params) {
  impl_->PostEncodeUpdate(encoded_frame_size);
}

template <>
void VideoRateControl<aom::AV1RateControlRtcConfig,
                      aom::AV1RateControlRTC,
                      aom::AV1FrameParamsRTC>::
    PostEncodeUpdate(uint64_t encoded_frame_size,
                     const aom::AV1FrameParamsRTC& frame_params) {
  impl_->PostEncodeUpdate(encoded_frame_size);
}

}  // namespace media
