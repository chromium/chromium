// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/video_codec_factory.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder_factory.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_software_fallback_wrapper.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_software_fallback_wrapper.h"
#include "third_party/webrtc/media/base/codec.h"
#include "third_party/webrtc/media/engine/encoder_simulcast_proxy.h"
#include "third_party/webrtc/media/engine/internal_decoder_factory.h"
#include "third_party/webrtc/media/engine/internal_encoder_factory.h"
#include "third_party/webrtc/media/engine/simulcast_encoder_adapter.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

namespace blink {

namespace {

bool IsFormatSupported(
    const std::vector<webrtc::SdpVideoFormat>& supported_formats,
    const webrtc::SdpVideoFormat& format) {
  for (const webrtc::SdpVideoFormat& supported_format : supported_formats) {
    if (cricket::IsSameCodec(format.name, format.parameters,
                             supported_format.name,
                             supported_format.parameters)) {
      return true;
    }
  }
  return false;
}

template <typename Factory>
bool IsFormatSupported(const Factory* factory,
                       const webrtc::SdpVideoFormat& format) {
  return factory && IsFormatSupported(factory->GetSupportedFormats(), format);
}

// Merge |formats1| and |formats2|, but avoid adding duplicate formats.
std::vector<webrtc::SdpVideoFormat> MergeFormats(
    std::vector<webrtc::SdpVideoFormat> formats1,
    const std::vector<webrtc::SdpVideoFormat>& formats2) {
  for (const webrtc::SdpVideoFormat& format : formats2) {
    // Don't add same format twice.
    if (!IsFormatSupported(formats1, format))
      formats1.push_back(format);
  }
  return formats1;
}

std::unique_ptr<webrtc::VideoDecoder> CreateDecoder(
    webrtc::VideoDecoderFactory* factory,
    const webrtc::SdpVideoFormat& format) {
  return factory ? factory->CreateVideoDecoder(format) : nullptr;
}

std::unique_ptr<webrtc::VideoDecoder> Wrap(
    std::unique_ptr<webrtc::VideoDecoder> software_decoder,
    std::unique_ptr<webrtc::VideoDecoder> hardware_decoder) {
  if (software_decoder && hardware_decoder) {
    return webrtc::CreateVideoDecoderSoftwareFallbackWrapper(
        std::move(software_decoder), std::move(hardware_decoder));
  }
  return hardware_decoder ? std::move(hardware_decoder)
                          : std::move(software_decoder);
}

std::unique_ptr<webrtc::VideoEncoder> Wrap(
    std::unique_ptr<webrtc::VideoEncoder> software_encoder,
    std::unique_ptr<webrtc::VideoEncoder> hardware_encoder) {
  if (software_encoder && hardware_encoder) {
    return webrtc::CreateVideoEncoderSoftwareFallbackWrapper(
        std::move(software_encoder), std::move(hardware_encoder));
  }
  return hardware_encoder ? std::move(hardware_encoder)
                          : std::move(software_encoder);
}

// This class combines a hardware factory with the internal factory and adds
// internal SW codecs, simulcast, and SW fallback wrappers.
class EncoderAdapter : public webrtc::VideoEncoderFactory {
 public:
  explicit EncoderAdapter(
      std::unique_ptr<webrtc::VideoEncoderFactory> hardware_encoder_factory)
      : hardware_encoder_factory_(std::move(hardware_encoder_factory)) {}

  webrtc::VideoEncoderFactory::CodecInfo QueryVideoEncoder(
      const webrtc::SdpVideoFormat& format) const override {
    const webrtc::VideoEncoderFactory* factory =
        IsFormatSupported(hardware_encoder_factory_.get(), format)
            ? hardware_encoder_factory_.get()
            : &software_encoder_factory_;
    return factory->QueryVideoEncoder(format);
  }

  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override {
    std::unique_ptr<webrtc::VideoEncoder> software_encoder;
    if (IsFormatSupported(&software_encoder_factory_, format)) {
      software_encoder = std::make_unique<webrtc::EncoderSimulcastProxy>(
          &software_encoder_factory_, format);
    }

    std::unique_ptr<webrtc::VideoEncoder> hardware_encoder;
    if (IsFormatSupported(hardware_encoder_factory_.get(), format)) {
      hardware_encoder =
          base::EqualsCaseInsensitiveASCII(format.name.c_str(),
                                           cricket::kVp9CodecName)
              ? hardware_encoder_factory_->CreateVideoEncoder(format)
              : std::make_unique<webrtc::SimulcastEncoderAdapter>(
                    hardware_encoder_factory_.get(), format);
    }

    return Wrap(std::move(software_encoder), std::move(hardware_encoder));
  }

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    std::vector<webrtc::SdpVideoFormat> software_formats =
        software_encoder_factory_.GetSupportedFormats();
    return hardware_encoder_factory_
               ? MergeFormats(software_formats,
                              hardware_encoder_factory_->GetSupportedFormats())
               : software_formats;
  }

 private:
  webrtc::InternalEncoderFactory software_encoder_factory_;
  const std::unique_ptr<webrtc::VideoEncoderFactory> hardware_encoder_factory_;
};

// This class combines a hardware codec factory with the internal factory and
// adds internal SW codecs and SW fallback wrappers.
class DecoderAdapter : public webrtc::VideoDecoderFactory {
 public:
  explicit DecoderAdapter(
      std::unique_ptr<webrtc::VideoDecoderFactory> hardware_decoder_factory)
      : hardware_decoder_factory_(std::move(hardware_decoder_factory)) {}

  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& format) override {
    std::unique_ptr<webrtc::VideoDecoder> software_decoder =
        CreateDecoder(&software_decoder_factory_, format);

    std::unique_ptr<webrtc::VideoDecoder> hardware_decoder =
        CreateDecoder(hardware_decoder_factory_.get(), format);

    return Wrap(std::move(software_decoder), std::move(hardware_decoder));
  }

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    std::vector<webrtc::SdpVideoFormat> software_formats =
        software_decoder_factory_.GetSupportedFormats();
    return hardware_decoder_factory_
               ? MergeFormats(software_formats,
                              hardware_decoder_factory_->GetSupportedFormats())
               : software_formats;
  }

 private:
  webrtc::InternalDecoderFactory software_decoder_factory_;
  const std::unique_ptr<webrtc::VideoDecoderFactory> hardware_decoder_factory_;
};

}  // namespace

std::unique_ptr<webrtc::VideoEncoderFactory> CreateWebrtcVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory;

  if (gpu_factories && gpu_factories->IsGpuVideoAcceleratorEnabled() &&
      Platform::Current()->IsWebRtcHWEncodingEnabled()) {
    encoder_factory = std::make_unique<RTCVideoEncoderFactory>(gpu_factories);
  }

#if defined(OS_ANDROID)
  if (!media::MediaCodecUtil::SupportsSetParameters())
    encoder_factory.reset();
#endif

  return std::make_unique<EncoderAdapter>(std::move(encoder_factory));
}

std::unique_ptr<webrtc::VideoDecoderFactory> CreateWebrtcVideoDecoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  std::unique_ptr<webrtc::VideoDecoderFactory> decoder_factory;

  if (gpu_factories && gpu_factories->IsGpuVideoAcceleratorEnabled() &&
      Platform::Current()->IsWebRtcHWDecodingEnabled()) {
    decoder_factory = std::make_unique<RTCVideoDecoderFactory>(gpu_factories);
  }

  return std::make_unique<DecoderAdapter>(std::move(decoder_factory));
}

}  // namespace blink
