// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_DECODER_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_DECODER_FACTORY_H_

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/platform/peerconnection/gpu_codec_support_waiter.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_factory.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"
#include "ui/gfx/color_space.h"

namespace webrtc {
class VideoDecoder;
}  // namespace webrtc

namespace media {
class DecoderFactory;
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace blink {

class PLATFORM_EXPORT RTCVideoDecoderFactory
    : public webrtc::VideoDecoderFactory {
 public:
  // The `decoder_factory` and `media_task_runner` are only needed if the
  // experiment `media::kUseDecoderStreamForWebRTC` is enabled. If the
  // RTCVideoDecoderFactory instance is only used to query supported codec
  // configurations (i.e., by calling GetSupportedFormats() and
  // QueryCodecSupport()), it may be created with `decoder_factory` and
  // `media_task_runner` being null pointers. See https://crbug.com/1349423.
  // TODO(crbug.com/1157227): Delete `decoder_factory` and `media_task_runner`
  // arguments if the RTCVideoDecoderStreamAdapter is deleted.
  explicit RTCVideoDecoderFactory(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      base::WeakPtr<media::DecoderFactory> decoder_factory,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      const gfx::ColorSpace& render_color_space);
  RTCVideoDecoderFactory(const RTCVideoDecoderFactory&) = delete;
  RTCVideoDecoderFactory& operator=(const RTCVideoDecoderFactory&) = delete;
  ~RTCVideoDecoderFactory() override;

  // Runs on Chrome_libJingle_WorkerThread. The child thread is blocked while
  // this runs.
  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& format) override;

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;

  webrtc::VideoDecoderFactory::CodecSupport QueryCodecSupport(
      const webrtc::SdpVideoFormat& format,
      bool reference_scaling) const override;

 private:
  void CheckAndWaitDecoderSupportStatusIfNeeded() const;
  media::GpuVideoAcceleratorFactories* gpu_factories_;
  base::WeakPtr<media::DecoderFactory> decoder_factory_;

  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  gfx::ColorSpace render_color_space_;

  std::unique_ptr<GpuCodecSupportWaiter> gpu_codec_support_waiter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_DECODER_FACTORY_H_
