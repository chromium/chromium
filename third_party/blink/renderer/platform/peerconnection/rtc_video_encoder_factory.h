// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_FACTORY_H_

#include <vector>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/peerconnection/gpu_codec_support_waiter.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace blink {

// This class creates RTCVideoEncoder instances (each wrapping a
// media::VideoEncodeAccelerator) on behalf of the WebRTC stack.
class RTCVideoEncoderFactory : public webrtc::VideoEncoderFactory {
 public:
  explicit RTCVideoEncoderFactory(
      media::GpuVideoAcceleratorFactories* gpu_factories);
  ~RTCVideoEncoderFactory() override;

  // webrtc::VideoEncoderFactory implementation.
  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override;
  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;

 private:
  void CheckAndWaitEncoderSupportStatusIfNeeded() const;

  media::GpuVideoAcceleratorFactories* gpu_factories_;

  GpuCodecSupportWaiter gpu_codec_support_waiter_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoEncoderFactory);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_FACTORY_H_
