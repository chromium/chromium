// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_decoding_info_handler.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/cpu.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/peerconnection/audio_codec_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/video_codec_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/webrtc/api/audio_codecs/audio_decoder_factory.h"
#include "third_party/webrtc/api/audio_codecs/audio_format.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_factory.h"

namespace blink {
WebrtcDecodingInfoHandler* WebrtcDecodingInfoHandler::Instance() {
  DEFINE_STATIC_LOCAL(WebrtcDecodingInfoHandler, instance, ());
  return &instance;
}

WebrtcDecodingInfoHandler::WebrtcDecodingInfoHandler()
    : WebrtcDecodingInfoHandler(
          blink::CreateWebrtcVideoDecoderFactory(
              Platform::Current()->GetGpuFactories(),
              Platform::Current()->GetMediaDecoderFactory(),
              Platform::Current()->MediaThreadTaskRunner(),
              Platform::Current()->GetRenderingColorSpace()),
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
    const absl::optional<String> audio_mime_type,
    const absl::optional<String> video_mime_type,
    const absl::optional<String> video_scalability_mode,
    OnMediaCapabilitiesDecodingInfoCallback callback) const {
  DCHECK(audio_mime_type || video_mime_type);

  // Set default values to true in case an audio configuration is not specified.
  bool supported = true;
  bool power_efficient = true;
  if (audio_mime_type) {
    ParsedContentType audio_content_type(audio_mime_type->LowerASCII());
    DCHECK(audio_content_type.IsValid());
    const String codec_name =
        WebrtcCodecNameFromMimeType(audio_content_type.MimeType(), "audio");
    supported = base::Contains(supported_audio_codecs_, codec_name);
    // Audio is always assumed to be power efficient whenever it is
    // supported.
    power_efficient = supported;
    DVLOG(1) << "Audio MIME type:" << codec_name << " supported:" << supported
             << " power_efficient:" << power_efficient;
  }

  // Only check video configuration if the audio configuration was supported (or
  // not specified).
  if (video_mime_type && supported) {
    // Convert video_configuration to SdpVideoFormat.
    ParsedContentType video_content_type(video_mime_type->LowerASCII());
    DCHECK(video_content_type.IsValid());
    const String codec_name =
        WebrtcCodecNameFromMimeType(video_content_type.MimeType(), "video");
    const webrtc::SdpVideoFormat::Parameters parameters =
        ConvertToSdpVideoFormatParameters(video_content_type.GetParameters());
    webrtc::SdpVideoFormat sdp_video_format(codec_name.Utf8(), parameters);
    bool reference_scaling = false;
    if (video_scalability_mode) {
      absl::optional<int> dependent_spatial_layers =
          WebRtcScalabilityModeDependentSpatialLayers(
              video_scalability_mode->Utf8());
      if (dependent_spatial_layers) {
        // Reference scaling must be supported to be able to decode a stream
        // with more than one dependent spatial layers.
        reference_scaling = *dependent_spatial_layers > 1;
      } else {
        // We were not able to decode the scalability mode string. Return
        // unsupported.
        supported = false;
        power_efficient = false;
      }
    }

    if (supported) {
      webrtc::VideoDecoderFactory::CodecSupport support =
          video_decoder_factory_->QueryCodecSupport(sdp_video_format,
                                                    reference_scaling);
      supported = support.is_supported;
      power_efficient = support.is_power_efficient;
    }
    DVLOG(1) << "Video MIME type:" << codec_name << " supported:" << supported
             << " power_efficient:" << power_efficient;
  }
  std::move(callback).Run(supported, power_efficient);
}

}  // namespace blink
