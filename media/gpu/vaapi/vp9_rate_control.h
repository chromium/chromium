// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_GPU_VAAPI_VP9_RATE_CONTROL_H_
#define MEDIA_GPU_VAAPI_VP9_RATE_CONTROL_H_

#include <memory>

#include "base/callback.h"

namespace libvpx {
struct VP9FrameParamsQpRTC;
struct VP9RateControlRtcConfig;
}  // namespace libvpx

namespace media {
// VP9RateControl is an interface to compute proper quantization
// parameter and loop filter level for vp9.
class VP9RateControl {
 public:
  // Creates VP9RateControl using libvpx implementation.
  static std::unique_ptr<VP9RateControl> Create(
      const libvpx::VP9RateControlRtcConfig& config);

  virtual ~VP9RateControl() = default;

  virtual void UpdateRateControl(
      const libvpx::VP9RateControlRtcConfig& rate_control_config) = 0;
  // libvpx::VP9FrameParamsQpRTC takes 0-63 quantization parameter.
  virtual void ComputeQP(const libvpx::VP9FrameParamsQpRTC& frame_params) = 0;
  // GetQP() returns vp9 ac/dc table index. The range is 0-255.
  virtual int GetQP() const = 0;
  virtual int GetLoopfilterLevel() const = 0;
  virtual void PostEncodeUpdate(uint64_t encoded_frame_size) = 0;
};
}  // namespace media
#endif  // MEDIA_GPU_VAAPI_VP9_RATE_CONTROL_H_
