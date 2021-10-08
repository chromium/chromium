// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VP8_RATE_CONTROL_H_
#define MEDIA_GPU_VAAPI_VP8_RATE_CONTROL_H_

#include <cstdint>

#include <memory>

namespace libvpx {
struct VP8FrameParamsQpRTC;
struct VP8RateControlRtcConfig;
}  // namespace libvpx

namespace media {
// VP8RateControl is an interface to compute proper quantization
// parameter and loop filter level for VP8.
class VP8RateControl {
 public:
  // Creates VP8RateControl using libvpx implementation.
  static std::unique_ptr<VP8RateControl> Create(
      const libvpx::VP8RateControlRtcConfig& config);

  virtual ~VP8RateControl() = default;

  virtual void UpdateRateControl(
      const libvpx::VP8RateControlRtcConfig& rate_control_config) = 0;
  // libvpx::VP8FrameParamsQpRTC takes 0-63 quantization parameter.
  virtual void ComputeQP(const libvpx::VP8FrameParamsQpRTC& frame_params) = 0;
  // GetQP() returns VP8 ac/dc table index. The range is 0-255.
  virtual int GetQP() const = 0;
  virtual void PostEncodeUpdate(uint64_t encoded_frame_size) = 0;
};
}  // namespace media
#endif  // MEDIA_GPU_VAAPI_VP8_RATE_CONTROL_H_
