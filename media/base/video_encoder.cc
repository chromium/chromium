// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_encoder.h"

#include "base/cxx17_backports.h"
#include "base/numerics/clamped_math.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"

namespace media {

uint32_t GetDefaultVideoEncodeBitrate(gfx::Size frame_size,
                                      uint32_t framerate) {
  // Let's default to 2M bps for HD at 30 fps.
  const uint32_t kDefaultBitrateForHD30fps = 2'000'000u;
  const uint32_t kHDArea = 1280u * 720u;
  const int kMaxArea = 8000 * 8000;
  const uint64_t kMinBitrate = 10000;
  const uint64_t kMaxBitrate = std::numeric_limits<uint32_t>::max();

  // Scale default bitrate to the given frame size and fps
  base::ClampedNumeric<uint64_t> result = kDefaultBitrateForHD30fps;
  result *= base::clamp(framerate, 1u, 300u);
  result *= base::clamp(frame_size.GetArea(), 1, kMaxArea);
  result /= kHDArea * 30u;  // HD resolution, 30 fps
  return base::clamp(result.RawValue(), kMinBitrate, kMaxBitrate);
}

VideoEncoderOutput::VideoEncoderOutput() = default;
VideoEncoderOutput::VideoEncoderOutput(VideoEncoderOutput&&) = default;
VideoEncoderOutput::~VideoEncoderOutput() = default;

VideoEncoder::VideoEncoder() = default;
VideoEncoder::~VideoEncoder() = default;

VideoEncoder::Options::Options() = default;
VideoEncoder::Options::Options(const Options&) = default;
VideoEncoder::Options::~Options() = default;

VideoEncoder::PendingEncode::PendingEncode() = default;
VideoEncoder::PendingEncode::PendingEncode(PendingEncode&&) = default;
VideoEncoder::PendingEncode::~PendingEncode() = default;

void VideoEncoder::DisablePostedCallbacks() {
  post_callbacks_ = false;
}

VideoEncoder::OutputCB VideoEncoder::BindCallbackToCurrentLoopIfNeeded(
    OutputCB&& callback) {
  return post_callbacks_ ? BindToCurrentLoop(std::move(callback))
                         : std::move(callback);
}

VideoEncoder::EncoderStatusCB VideoEncoder::BindCallbackToCurrentLoopIfNeeded(
    EncoderStatusCB&& callback) {
  return post_callbacks_ ? BindToCurrentLoop(std::move(callback))
                         : std::move(callback);
}

}  // namespace media
