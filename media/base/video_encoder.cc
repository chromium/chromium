// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_encoder.h"

#include <algorithm>

#include "base/numerics/checked_math.h"
#include "base/numerics/clamped_math.h"
#include "base/system/sys_info.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"

namespace media {

namespace {
const char* GetContentHintName(VideoEncoder::ContentHint content_hint) {
  switch (content_hint) {
    case VideoEncoder::ContentHint::Camera:
      return "camera";
    case VideoEncoder::ContentHint::Screen:
      return "screen";
  }
}

}  // namespace

uint8_t GetDefaultVideoEncoderDropFrameThreshold() {
  // This function is to be invoked only in WebCodecs usage.
  // The drop frame threshold is the same as WebRTC.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/codecs/vp9/libvpx_vp9_encoder.cc
  return base::FeatureList::IsEnabled(kWebCodecsVideoEncoderFrameDrop) ? 30 : 0;
}

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
  result *= std::clamp(framerate, 1u, 300u);
  result *= std::clamp(frame_size.GetArea(), 1, kMaxArea);
  result /= kHDArea * 30u;  // HD resolution, 30 fps
  return std::clamp(result.RawValue(), kMinBitrate, kMaxBitrate);
}

int GetNumberOfThreadsForSoftwareEncoding(gfx::Size frame_size) {
  int area = frame_size.GetCheckedArea().ValueOrDefault(1);
  // Default to 1 thread for less than VGA.
  int desired_threads = 1;

  if (area >= 3840 * 2160) {
    desired_threads = 16;
  } else if (area >= 1920 * 1080) {
    desired_threads = 8;
  } else if (area >= 1280 * 720) {
    desired_threads = 4;
  } else if (area >= 640 * 480) {
    desired_threads = 2;
  }

  // Clamp to the number of available logical processors/cores.
  desired_threads =
      std::min(desired_threads, base::SysInfo::NumberOfProcessors());

  return desired_threads;
}

VideoEncoderOutput::VideoEncoderOutput() = default;
VideoEncoderOutput::VideoEncoderOutput(VideoEncoderOutput&&) = default;
VideoEncoderOutput::~VideoEncoderOutput() = default;

VideoEncoder::VideoEncoder() = default;
VideoEncoder::~VideoEncoder() = default;

VideoEncoder::Options::Options() = default;
VideoEncoder::Options::Options(const Options&) = default;
VideoEncoder::Options::~Options() = default;

std::string VideoEncoder::Options::ToString() {
  std::vector<std::string> keys;
  if (bitrate) {
    keys.push_back("bitrate: " + bitrate->ToString());
  }
  if (framerate) {
    keys.push_back(base::StringPrintf("framerate: %f", *framerate));
  }
  keys.push_back("frame_size: " + frame_size.ToString());
  if (keyframe_interval) {
    keys.push_back(
        base::StringPrintf("keyframe_interval: %d", *keyframe_interval));
  }
  keys.push_back(base::StringPrintf(
      "latency_mode: %s",
      latency_mode == LatencyMode::Quality ? "quality" : "realtime"));
  if (scalability_mode) {
    keys.push_back(base::StringPrintf(
        "scalability_mode: %s", GetScalabilityModeName(*scalability_mode)));
  }
  if (content_hint) {
    keys.push_back(base::StringPrintf("content_hint: %s",
                                      GetContentHintName(*content_hint)));
  }
  if (subsampling) {
    keys.push_back("subsampling: " + VideoChromaSamplingToString(*subsampling));
  }
  if (bit_depth) {
    keys.push_back(base::StringPrintf("bit_depth: %d", *bit_depth));
  }
  keys.push_back(base::StringPrintf(
      "produce_annexb: %s",
      avc.produce_annexb || hevc.produce_annexb ? "true" : "false"));
  return base::JoinString(keys, ",  ");
}

VideoEncoder::PendingEncode::PendingEncode() = default;
VideoEncoder::PendingEncode::PendingEncode(PendingEncode&&) = default;
VideoEncoder::PendingEncode::~PendingEncode() = default;

VideoEncoder::EncodeOptions::EncodeOptions(bool key_frame)
    : key_frame(key_frame) {}
VideoEncoder::EncodeOptions::EncodeOptions() = default;
VideoEncoder::EncodeOptions::EncodeOptions(const EncodeOptions&) = default;
VideoEncoder::EncodeOptions::~EncodeOptions() = default;

void VideoEncoder::DisablePostedCallbacks() {
  post_callbacks_ = false;
}

}  // namespace media
