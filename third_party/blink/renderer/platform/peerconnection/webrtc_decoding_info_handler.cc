// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_decoding_info_handler.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/peerconnection/audio_codec_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/video_codec_factory.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/webrtc/api/audio_codecs/audio_decoder_factory.h"
#include "third_party/webrtc/api/audio_codecs/audio_format.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_factory.h"
#include "ui/gfx/color_space.h"

namespace blink {
WebrtcDecodingInfoHandler* WebrtcDecodingInfoHandler::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(WebrtcDecodingInfoHandler, instance, ());
  return &instance;
}

WebrtcDecodingInfoHandler::WebrtcDecodingInfoHandler()
    : WebrtcDecodingInfoHandler(
          blink::CreateWebrtcVideoDecoderFactory(
              Platform::Current()->GetGpuFactories(),
              Platform::Current()->GetRenderingColorSpace(),
              base::DoNothing()),
          blink::CreateWebrtcAudioDecoderFactory()) {}

WebrtcDecodingInfoHandler::WebrtcDecodingInfoHandler(
    std::unique_ptr<webrtc::VideoDecoderFactory> video_decoder_factory,
    rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory)
    : video_decoder_factory_(std::move(video_decoder_factory)),
      audio_decoder_factory_(std::move(audio_decoder_factory)) {
  std::vector<webrtc::AudioCodecSpec> supported_audio_specs =
      audio_decoder_factory_->GetSupportedDecoders();
  for (const auto& audio_spec : supported_audio_specs) {
    supported_audio_codecs_.insert(
        String::FromUTF8(audio_spec.format.name).LowerASCII());
  }
}

WebrtcDecodingInfoHandler::~WebrtcDecodingInfoHandler() = default;

void WebrtcDecodingInfoHandler::DecodingInfo(
    const std::optional<webrtc::SdpAudioFormat> sdp_audio_format,
    const std::optional<webrtc::SdpVideoFormat> sdp_video_format,
    const bool video_spatial_scalability,
    OnMediaCapabilitiesDecodingInfoCallback callback) const {
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
    webrtc::VideoDecoderFactory::CodecSupport support =
        video_decoder_factory_->QueryCodecSupport(*sdp_video_format,
                                                  video_spatial_scalability);
    supported = support.is_supported;
    power_efficient = support.is_power_efficient;
    DVLOG(1) << "Video:" << sdp_video_format->name << " supported:" << supported
             << " power_efficient:" << power_efficient;
  }
  std::move(callback).Run(supported, power_efficient);
}

}  // namespace blink
