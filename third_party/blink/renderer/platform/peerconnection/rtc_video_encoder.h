// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/video_decoder_config.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/webrtc/api/video/video_bitrate_allocation.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {
class GpuVideoAcceleratorFactories;
class MojoVideoEncoderMetricsProviderFactory;
struct VideoEncoderInfo;
}  // namespace media

namespace blink {

namespace features {
PLATFORM_EXPORT BASE_DECLARE_FEATURE(kWebRtcScreenshareSwEncoding);
PLATFORM_EXPORT BASE_DECLARE_FEATURE(kForcingSoftwareIncludes360);
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
                  bool is_constrained_h264,
                  media::GpuVideoAcceleratorFactories* gpu_factories,
                  scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
                      encoder_metrics_provider_factory);
  RTCVideoEncoder(const RTCVideoEncoder&) = delete;
  RTCVideoEncoder& operator=(const RTCVideoEncoder&) = delete;
  ~RTCVideoEncoder() override;

  // webrtc::VideoEncoder implementation.
  // They run on |webrtc_sequence_checker_|. Tasks are posted to |impl_| using
  // the appropriate VEA methods.
  int InitEncode(const webrtc::VideoCodec* codec_settings,
                 const webrtc::VideoEncoder::Settings& settings) override;
  int32_t Encode(
      const webrtc::VideoFrame& input_image,
      const std::vector<webrtc::VideoFrameType>* frame_types) override;
  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override;
  int32_t Release() override;
  void SetRates(
      const webrtc::VideoEncoder::RateControlParameters& parameters) override;
  EncoderInfo GetEncoderInfo() const override;

  // Returns true if there's VP9 HW support for spatial layers.
  static bool Vp9HwSupportForSpatialLayers();

  void SetErrorCallbackForTesting(
      WTF::CrossThreadOnceClosure error_callback_for_testing) {
    error_callback_for_testing_ = std::move(error_callback_for_testing);
  }

 private:
  class Impl;

  bool IsCodecInitializationPending() const;
  int32_t InitializeEncoder(
      const media::VideoEncodeAccelerator::Config& vea_config);
  void PreInitializeEncoder(
      const std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>&
          spatial_layers,
      media::VideoPixelFormat pixel_format);
  void UpdateEncoderInfo(
      media::VideoEncoderInfo encoder_info,
      std::vector<webrtc::VideoFrameBuffer::Type> preferred_pixel_formats);
  void SetError();

  const media::VideoCodecProfile profile_;

  const bool is_constrained_h264_;

  // Factory for creating VEAs, shared memory buffers, etc.
  media::GpuVideoAcceleratorFactories* const gpu_factories_;

  scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
      encoder_metrics_provider_factory_;

  // Task runner that the video accelerator runs on.
  const scoped_refptr<base::SequencedTaskRunner> gpu_task_runner_;

  // TODO(b/258782303): Remove this lock if |encoder_info_| is called on
  // webrtc sequence.
  mutable base::Lock lock_;
  // Default values are set in RTCVideoEncoder constructor. Some of its
  // variables are updated in UpdateEncoderInfo() in webrtc encoder thread.
  // There is a minor data race that GetEncoderInfo() can be called in a
  // different thread from webrtc encoder thread.
  webrtc::VideoEncoder::EncoderInfo encoder_info_ GUARDED_BY(lock_);

  // The sequence on which the webrtc::VideoEncoder functions are executed.
  SEQUENCE_CHECKER(webrtc_sequence_checker_);

  bool has_error_ GUARDED_BY_CONTEXT(webrtc_sequence_checker_){false};

  // If this has value, the value is VideoEncodeAccelerator::Config to be used
  // in up-coming Initialize().
  absl::optional<media::VideoEncodeAccelerator::Config> vea_config_
      GUARDED_BY_CONTEXT(webrtc_sequence_checker_);
  // This has a value if SetRates() is called between InitEncode() and the first
  // Encode(). The stored value is used for SetRates() after the encoder
  // initialization with |vea_config_|.
  absl::optional<webrtc::VideoEncoder::RateControlParameters>
      pending_rate_params_ GUARDED_BY_CONTEXT(webrtc_sequence_checker_);

  // Execute in SetError(). This can be valid only in testing.
  WTF::CrossThreadOnceClosure error_callback_for_testing_;

  // The RTCVideoEncoder::Impl that does all the work.
  std::unique_ptr<Impl> impl_;

  // This weak pointer is bound to |gpu_task_runner_|.
  base::WeakPtr<Impl> weak_impl_;

  // |weak_this_| is bound to |webrtc_sequence_checker_|.
  base::WeakPtr<RTCVideoEncoder> weak_this_;
  base::WeakPtrFactory<RTCVideoEncoder> weak_this_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_H_
