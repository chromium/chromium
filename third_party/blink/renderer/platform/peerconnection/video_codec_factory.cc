// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/video_codec_factory.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/stats_collecting_decoder.h"
#include "third_party/blink/renderer/platform/peerconnection/stats_collecting_encoder.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_software_fallback_wrapper.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_software_fallback_wrapper.h"
#include "third_party/webrtc/media/base/codec.h"
#include "third_party/webrtc/media/engine/internal_decoder_factory.h"
#include "third_party/webrtc/media/engine/internal_encoder_factory.h"
#include "third_party/webrtc/media/engine/simulcast_encoder_adapter.h"
#include "third_party/webrtc/modules/video_coding/codecs/h264/include/h264.h"

#if BUILDFLAG(IS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

namespace blink {

namespace {

template <typename Factory>
bool IsFormatSupported(const Factory* factory,
                       const webrtc::SdpVideoFormat& format) {
  return factory && format.IsCodecInList(factory->GetSupportedFormats());
}

// Merge |formats1| and |formats2|, but avoid adding duplicate formats.
std::vector<webrtc::SdpVideoFormat> MergeFormats(
    std::vector<webrtc::SdpVideoFormat> formats1,
    const std::vector<webrtc::SdpVideoFormat>& formats2) {
  for (const webrtc::SdpVideoFormat& format : formats2) {
    // Don't add same format twice.
    if (!format.IsCodecInList(formats1))
      formats1.push_back(format);
  }
  return formats1;
}

std::unique_ptr<webrtc::VideoDecoder> CreateDecoder(
    webrtc::VideoDecoderFactory* factory,
    const webrtc::Environment& env,
    const webrtc::SdpVideoFormat& format) {
  if (!IsFormatSupported(factory, format))
    return nullptr;
  return factory->Create(env, format);
}

std::unique_ptr<webrtc::VideoDecoder> Wrap(
    const webrtc::Environment& env,
    std::unique_ptr<webrtc::VideoDecoder> software_decoder,
    std::unique_ptr<webrtc::VideoDecoder> hardware_decoder) {
  if (software_decoder && hardware_decoder) {
    return webrtc::CreateVideoDecoderSoftwareFallbackWrapper(
        env, std::move(software_decoder), std::move(hardware_decoder));
  }
  return hardware_decoder ? std::move(hardware_decoder)
                          : std::move(software_decoder);
}

// This class combines a hardware factory with the internal factory and adds
// internal SW codecs, simulcast, and SW fallback wrappers.
class EncoderAdapter : public webrtc::VideoEncoderFactory {
 public:
  explicit EncoderAdapter(
      std::unique_ptr<webrtc::VideoEncoderFactory> hardware_encoder_factory,
      StatsCollector::StoreProcessingStatsCB stats_callback)
      : hardware_encoder_factory_(std::move(hardware_encoder_factory)),
        stats_callback_(stats_callback) {}

  std::unique_ptr<webrtc::VideoEncoder> Create(
      const webrtc::Environment& env,
      const webrtc::SdpVideoFormat& format) override {
    if (!WebRTCFormatToCodecProfile(format)) {
      LOG(ERROR) << "Unsupported SDP format: " << format.name;
      return nullptr;
    }
    const bool supported_in_hardware =
        IsFormatSupported(hardware_encoder_factory_.get(), format);
    bool allow_h264_profile_fallback = false;
    // Special handling of H264 hardware encoder fallback during encoding when
    // high profile is requested. However if hardware encoding is not supported,
    // trust supported formats reported by |software_encoder_factory_| and do
    // not allow profile mismatch when only software encoder factory is used for
    // creating the simulcast encoder adapter.
    if (base::EqualsCaseInsensitiveASCII(format.name.c_str(),
                                         cricket::kH264CodecName) &&
        supported_in_hardware) {
      allow_h264_profile_fallback = IsFormatSupported(
          &software_encoder_factory_,
          webrtc::CreateH264Format(
              webrtc::H264Profile::kProfileConstrainedBaseline,
              webrtc::H264Level::kLevel1_1, "1"));
    }
    const bool supported_in_software =
        allow_h264_profile_fallback ||
        IsFormatSupported(&software_encoder_factory_, format);

    if (!supported_in_software && !supported_in_hardware)
      return nullptr;

    VideoEncoderFactory* primary_factory = supported_in_hardware
                                               ? hardware_encoder_factory_.get()
                                               : &software_encoder_factory_;
    VideoEncoderFactory* fallback_factory =
        supported_in_hardware && supported_in_software
            ? &software_encoder_factory_
            : nullptr;
    std::unique_ptr<webrtc::VideoEncoder> encoder =
        std::make_unique<webrtc::SimulcastEncoderAdapter>(
            env, primary_factory, fallback_factory, format);

    return std::make_unique<StatsCollectingEncoder>(format, std::move(encoder),
                                                    stats_callback_);
  }

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    std::vector<webrtc::SdpVideoFormat> software_formats =
        software_encoder_factory_.GetSupportedFormats();
    return hardware_encoder_factory_
               ? MergeFormats(software_formats,
                              hardware_encoder_factory_->GetSupportedFormats())
               : software_formats;
  }

  webrtc::VideoEncoderFactory::CodecSupport QueryCodecSupport(
      const webrtc::SdpVideoFormat& format,
      std::optional<std::string> scalability_mode) const override {
    webrtc::VideoEncoderFactory::CodecSupport codec_support =
        hardware_encoder_factory_
            ? hardware_encoder_factory_->QueryCodecSupport(format,
                                                           scalability_mode)
            : webrtc::VideoEncoderFactory::CodecSupport();
    if (!codec_support.is_supported) {
      codec_support =
          software_encoder_factory_.QueryCodecSupport(format, scalability_mode);
    }
    return codec_support;
  }

 private:
  webrtc::InternalEncoderFactory software_encoder_factory_;
  const std::unique_ptr<webrtc::VideoEncoderFactory> hardware_encoder_factory_;
  StatsCollector::StoreProcessingStatsCB stats_callback_;
};

// This class combines a hardware codec factory with the internal factory and
// adds internal SW codecs and SW fallback wrappers.
class DecoderAdapter : public webrtc::VideoDecoderFactory {
 public:
  explicit DecoderAdapter(
      std::unique_ptr<webrtc::VideoDecoderFactory> hardware_decoder_factory,
      StatsCollector::StoreProcessingStatsCB stats_callback)
      : hardware_decoder_factory_(std::move(hardware_decoder_factory)),
        stats_callback_(stats_callback) {}

  std::unique_ptr<webrtc::VideoDecoder> Create(
      const webrtc::Environment& env,
      const webrtc::SdpVideoFormat& format) override {
    std::unique_ptr<webrtc::VideoDecoder> software_decoder =
        CreateDecoder(&software_decoder_factory_, env, format);

    std::unique_ptr<webrtc::VideoDecoder> hardware_decoder =
        CreateDecoder(hardware_decoder_factory_.get(), env, format);

    if (!software_decoder && !hardware_decoder)
      return nullptr;

    return std::make_unique<StatsCollectingDecoder>(
        format,
        Wrap(env, std::move(software_decoder), std::move(hardware_decoder)),
        stats_callback_);
  }

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    std::vector<webrtc::SdpVideoFormat> software_formats =
        software_decoder_factory_.GetSupportedFormats();
    return hardware_decoder_factory_
               ? MergeFormats(software_formats,
                              hardware_decoder_factory_->GetSupportedFormats())
               : software_formats;
  }

  webrtc::VideoDecoderFactory::CodecSupport QueryCodecSupport(
      const webrtc::SdpVideoFormat& format,
      bool reference_scaling) const override {
    webrtc::VideoDecoderFactory::CodecSupport codec_support =
        hardware_decoder_factory_
            ? hardware_decoder_factory_->QueryCodecSupport(format,
                                                           reference_scaling)
            : webrtc::VideoDecoderFactory::CodecSupport();
    if (!codec_support.is_supported) {
      codec_support = software_decoder_factory_.QueryCodecSupport(
          format, reference_scaling);
    }
    return codec_support;
  }

 private:
  webrtc::InternalDecoderFactory software_decoder_factory_;
  const std::unique_ptr<webrtc::VideoDecoderFactory> hardware_decoder_factory_;
  StatsCollector::StoreProcessingStatsCB stats_callback_;
};

}  // namespace

std::unique_ptr<webrtc::VideoEncoderFactory> CreateHWVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
        encoder_metrics_provider_factory) {
  std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory;

  if (gpu_factories && gpu_factories->IsGpuVideoEncodeAcceleratorEnabled() &&
      Platform::Current()->IsWebRtcHWEncodingEnabled()) {
    encoder_factory = std::make_unique<RTCVideoEncoderFactory>(
        gpu_factories, std::move(encoder_metrics_provider_factory));
  }

  return encoder_factory;
}

std::unique_ptr<webrtc::VideoEncoderFactory> CreateWebrtcVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
        encoder_metrics_provider_factory,
    StatsCollector::StoreProcessingStatsCB stats_callback) {
  return std::make_unique<EncoderAdapter>(
      CreateHWVideoEncoderFactory(gpu_factories,
                                  std::move(encoder_metrics_provider_factory)),
      stats_callback);
}

std::unique_ptr<webrtc::VideoDecoderFactory> CreateWebrtcVideoDecoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    const gfx::ColorSpace& render_color_space,
    StatsCollector::StoreProcessingStatsCB stats_callback) {
  const bool use_hw_decoding =
      gpu_factories != nullptr &&
      gpu_factories->IsGpuVideoDecodeAcceleratorEnabled() &&
      Platform::Current()->IsWebRtcHWDecodingEnabled();

  std::unique_ptr<RTCVideoDecoderFactory> decoder_factory;
  if (use_hw_decoding) {
    decoder_factory = std::make_unique<RTCVideoDecoderFactory>(
        use_hw_decoding ? gpu_factories : nullptr, render_color_space);
  }

  return std::make_unique<DecoderAdapter>(std::move(decoder_factory),
                                          stats_callback);
}

}  // namespace blink
