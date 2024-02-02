// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_encoding_info_handler.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/peerconnection/audio_codec_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/video_codec_factory.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/webrtc/api/audio_codecs/audio_encoder_factory.h"
#include "third_party/webrtc/api/audio_codecs/audio_format.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"

namespace blink {

WebrtcEncodingInfoHandler* WebrtcEncodingInfoHandler::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(WebrtcEncodingInfoHandler, instance, ());
  return &instance;
}

// |encoder_metrics_provider_factory| is not used unless
// RTCVideoEncoder::InitEncode() is called.
WebrtcEncodingInfoHandler::WebrtcEncodingInfoHandler()
    : WebrtcEncodingInfoHandler(
          blink::CreateWebrtcVideoEncoderFactory(
              Platform::Current()->GetGpuFactories(),
              /*encoder_metrics_provider_factory=*/nullptr,
              base::DoNothing()),
          blink::CreateWebrtcAudioEncoderFactory()) {}

WebrtcEncodingInfoHandler::WebrtcEncodingInfoHandler(
    std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory,
    rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory)
    : video_encoder_factory_(std::move(video_encoder_factory)),
      audio_encoder_factory_(std::move(audio_encoder_factory)) {
  std::vector<webrtc::AudioCodecSpec> supported_audio_specs =
      audio_encoder_factory_->GetSupportedEncoders();
  for (const auto& audio_spec : supported_audio_specs) {
    supported_audio_codecs_.insert(
        String::FromUTF8(audio_spec.format.name).LowerASCII());
  }
}

WebrtcEncodingInfoHandler::~WebrtcEncodingInfoHandler() = default;

void WebrtcEncodingInfoHandler::EncodingInfo(
    const std::optional<webrtc::SdpAudioFormat> sdp_audio_format,
    const std::optional<webrtc::SdpVideoFormat> sdp_video_format,
    const std::optional<String> video_scalability_mode,
    OnMediaCapabilitiesEncodingInfoCallback callback) const {
  DCHECK(sdp_audio_format || sdp_video_format);

  // Set default values to true in case an audio configuration is not specified.
  bool supported = true;
  bool power_efficient = true;
  if (sdp_audio_format) {
    const String codec_name =
        String::FromUTF8(sdp_audio_format->name).LowerASCII();
    supported = base::Contains(supported_audio_codecs_, codec_name);
    // Audio is always assumed to be power efficient whenever it is
    // supported.
    power_efficient = supported;
    DVLOG(1) << "Audio:" << sdp_audio_format->name << " supported:" << supported
             << " power_efficient:" << power_efficient;
  }

  // Only check video configuration if the audio configuration was supported (or
  // not specified).
  if (sdp_video_format && supported) {
    std::optional<std::string> scalability_mode =
        video_scalability_mode
            ? std::make_optional(video_scalability_mode->Utf8())
            : std::nullopt;
    webrtc::VideoEncoderFactory::CodecSupport support =
        video_encoder_factory_->QueryCodecSupport(*sdp_video_format,
                                                  scalability_mode);

    supported = support.is_supported;
    power_efficient = support.is_power_efficient;

    DVLOG(1) << "Video:" << sdp_video_format->name << " supported:" << supported
             << " power_efficient:" << power_efficient;
  }
  std::move(callback).Run(supported, power_efficient);
}

}  // namespace blink
