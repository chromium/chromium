// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/base/video_decoder_config.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/api/video/video_bitrate_allocation.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace blink {

namespace features {
PLATFORM_EXPORT extern const base::Feature kWebRtcScreenshareSwEncoding;
}

// RTCVideoEncoder uses a media::VideoEncodeAccelerator to implement a
// webrtc::VideoEncoder class for WebRTC.  Internally, VEA methods are
// trampolined to a private RTCVideoEncoder::Impl instance.  The Impl class runs
// on the worker thread queried from the |gpu_factories_|, which is presently
// the media thread.  RTCVideoEncoder is sychronized by webrtc::VideoSender.
// webrtc::VideoEncoder methods do not run concurrently. RtcVideoEncoder needs
// to synchronize RegisterEncodeCompleteCallback and encode complete callback.
class PLATFORM_EXPORT RTCVideoEncoder : public webrtc::VideoEncoder {
 public:
  RTCVideoEncoder(media::VideoCodecProfile profile,
                  media::GpuVideoAcceleratorFactories* gpu_factories);
  ~RTCVideoEncoder() override;

  // webrtc::VideoEncoder implementation.  Tasks are posted to |impl_| using the
  // appropriate VEA methods.
  int32_t InitEncode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores,
                     size_t max_payload_size) override;
  int32_t Encode(
      const webrtc::VideoFrame& input_image,
      const std::vector<webrtc::VideoFrameType>* frame_types) override;
  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override;
  int32_t Release() override;
  void SetRates(
      const webrtc::VideoEncoder::RateControlParameters& parameters) override;
  EncoderInfo GetEncoderInfo() const override;

 private:
  class Impl;
  friend class RTCVideoEncoder::Impl;

  const media::VideoCodecProfile profile_;

  // Factory for creating VEAs, shared memory buffers, etc.
  media::GpuVideoAcceleratorFactories* gpu_factories_;

  // Task runner that the video accelerator runs on.
  const scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  // The RTCVideoEncoder::Impl that does all the work.
  scoped_refptr<Impl> impl_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoEncoder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_H_
