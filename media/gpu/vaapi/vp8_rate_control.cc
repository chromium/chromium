// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp8_rate_control.h"

#include "base/logging.h"
#include "third_party/libvpx/source/libvpx/vp8/vp8_ratectrl_rtc.h"

namespace media {
namespace {
class LibvpxVP8RateControl : public VP8RateControl {
 public:
  explicit LibvpxVP8RateControl(std::unique_ptr<libvpx::VP8RateControlRTC> impl)
      : impl_(std::move(impl)) {}

  ~LibvpxVP8RateControl() override = default;
  LibvpxVP8RateControl(const LibvpxVP8RateControl&) = delete;
  LibvpxVP8RateControl& operator=(const LibvpxVP8RateControl&) = delete;

  void UpdateRateControl(
      const libvpx::VP8RateControlRtcConfig& rate_control_config) override {
    impl_->UpdateRateControl(rate_control_config);
  }
  int GetQP() const override { return impl_->GetQP(); }
  void ComputeQP(const libvpx::VP8FrameParamsQpRTC& frame_params) override {
    impl_->ComputeQP(frame_params);
  }
  void PostEncodeUpdate(uint64_t encoded_frame_size) override {
    impl_->PostEncodeUpdate(encoded_frame_size);
  }

 private:
  const std::unique_ptr<libvpx::VP8RateControlRTC> impl_;
};

}  // namespace

// static
std::unique_ptr<VP8RateControl> VP8RateControl::Create(
    const libvpx::VP8RateControlRtcConfig& config) {
  auto impl = libvpx::VP8RateControlRTC::Create(config);
  if (!impl) {
    DLOG(ERROR) << "Failed creating libvpx::VP8RateControlRTC";
    return nullptr;
  }
  return std::make_unique<LibvpxVP8RateControl>(std::move(impl));
}
}  // namespace media
