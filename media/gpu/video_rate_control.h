// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VIDEO_RATE_CONTROL_H_
#define MEDIA_GPU_VIDEO_RATE_CONTROL_H_

#include <memory>

#include "base/logging.h"

namespace media {
// VideoRateControl is an interface to compute proper quantization
// parameter and loop filter level for vp8 and vp9.
// T is a libvpx::VP(8|9)RateControlRtcConfig
// S is a libvpx::VP(8|9)RateControlRTC
// U is a libvpx::VP(8|9)FrameParamsQpRTC
template <typename T, typename S, typename U>
class VideoRateControl {
 public:
  // Creates VideoRateControl using libvpx implementation.
  static std::unique_ptr<VideoRateControl> Create(const T& config) {
    auto impl = S::Create(config);
    if (!impl) {
      DLOG(ERROR) << "Failed creating video RateControlRTC";
      return nullptr;
    }
    return std::make_unique<VideoRateControl>(std::move(impl));
  }

  VideoRateControl() = default;
  explicit VideoRateControl(std::unique_ptr<S> impl) : impl_(std::move(impl)) {}
  virtual ~VideoRateControl() = default;

  virtual void UpdateRateControl(const T& rate_control_config) {
    impl_->UpdateRateControl(rate_control_config);
  }
  // libvpx::VP(8|9)FrameParamsQpRTC take 0-63 quantization parameter.
  // ComputeQP() returns vp8/9 ac/dc table index. The range is 0-255.
  virtual int ComputeQP(const U& frame_params) {
    impl_->ComputeQP(frame_params);
    return impl_->GetQP();
  }
  // GetLoopfilterLevel() is only available for VP9 -- see .cc file.
  virtual int GetLoopfilterLevel() const { return -1; }
  virtual void PostEncodeUpdate(uint64_t encoded_frame_size) {
    impl_->PostEncodeUpdate(encoded_frame_size);
  }

 private:
  const std::unique_ptr<S> impl_;
};

}  // namespace media
#endif  // MEDIA_GPU_VIDEO_RATE_CONTROL_H_
