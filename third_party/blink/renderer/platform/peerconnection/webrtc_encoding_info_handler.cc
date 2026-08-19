// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_encoding_info_handler.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "media/video/gpu_video_accelerator_factories.h"
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

WebrtcEncodingInfoHandler::WebrtcEncodingInfoHandler()
    : WebrtcEncodingInfoHandler(Platform::Current()
                                    ? Platform::Current()->GetGpuFactories()
                                    : nullptr) {}

// |encoder_metrics_provider_factory| is not used unless
// RTCVideoEncoder::InitEncode() is called.
WebrtcEncodingInfoHandler::WebrtcEncodingInfoHandler(
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : WebrtcEncodingInfoHandler(
          blink::CreateWebrtcVideoEncoderFactory(
              gpu_factories,
              /*encoder_metrics_provider_factory=*/nullptr,
              base::DoNothing()),
          blink::CreateWebrtcAudioEncoderFactory(),
          gpu_factories) {}

WebrtcEncodingInfoHandler::WebrtcEncodingInfoHandler(
    std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory,
    webrtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory,
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : video_encoder_factory_(std::move(video_encoder_factory)),
      audio_encoder_factory_(std::move(audio_encoder_factory)),
      gpu_factories_(gpu_factories) {
  std::vector<webrtc::AudioCodecSpec> supported_audio_specs =
      audio_encoder_factory_->GetSupportedEncoders();
  for (const auto& audio_spec : supported_audio_specs) {
    supported_audio_codecs_.insert(
        String::FromUtf8(audio_spec.format.name).ToAsciiLower());
  }
}

WebrtcEncodingInfoHandler::~WebrtcEncodingInfoHandler() = default;

void WebrtcEncodingInfoHandler::EncodingInfo(
    const std::optional<webrtc::SdpAudioFormat>& sdp_audio_format,
    const std::optional<webrtc::SdpVideoFormat>& sdp_video_format,
    const String& video_scalability_mode,
    std::optional<gfx::Size> video_resolution,
    OnMediaCapabilitiesEncodingInfoCallback callback) const {
  DCHECK(sdp_audio_format || sdp_video_format);

  // Set default values to true in case an audio configuration is not specified.
  bool supported = true;
  bool power_efficient = true;
  if (sdp_audio_format) {
    const String codec_name =
        String::FromUtf8(sdp_audio_format->name).ToAsciiLower();
    supported = supported_audio_codecs_.Contains(codec_name);
    // Audio is always assumed to be power efficient whenever it is
    // supported.
    power_efficient = supported;
    DVLOG(1) << "Audio:" << sdp_audio_format->name << " supported:" << supported
             << " power_efficient:" << power_efficient;
  }

  if (!supported || !sdp_video_format) {
    std::move(callback).Run(supported, power_efficient);
    return;
  }

  std::optional<std::string> scalability_mode =
      !video_scalability_mode.IsNull()
          ? std::make_optional(video_scalability_mode.Utf8())
          : std::nullopt;

  if (gpu_factories_ && !gpu_factories_->IsEncoderSupportKnown()) {
    // Avoid making a blocking call to QueryCodecSupport() if encoder support is
    // not known yet.
    // Unretained(this) is safe because WebrtcEncodingInfoHandler is a leaky
    // singleton in production. Tests must ensure the stack-allocated handler
    // outlives the mocked gpu_factories.
    gpu_factories_->NotifyEncoderSupportKnown(base::BindOnce(
        &WebrtcEncodingInfoHandler::ContinueVideoSupportCheck,
        base::Unretained(this), sdp_video_format, std::move(scalability_mode),
        video_resolution, std::move(callback)));
    return;
  }

  ContinueVideoSupportCheck(sdp_video_format, std::move(scalability_mode),
                            video_resolution, std::move(callback));
}

void WebrtcEncodingInfoHandler::ContinueVideoSupportCheck(
    const std::optional<webrtc::SdpVideoFormat>& sdp_video_format,
    std::optional<std::string> scalability_mode,
    std::optional<gfx::Size> video_resolution,
    OnMediaCapabilitiesEncodingInfoCallback callback) const {
  DCHECK(sdp_video_format);
  std::optional<webrtc::Resolution> resolution;
  if (video_resolution) {
    resolution = {video_resolution->width(), video_resolution->height()};
  }
  webrtc::VideoEncoderFactory::CodecSupport support =
      video_encoder_factory_->QueryCodecSupport(
          *sdp_video_format, std::move(scalability_mode), resolution);

  DVLOG(1) << "Video:" << sdp_video_format->name
           << " supported:" << support.is_supported
           << " power_efficient:" << support.is_power_efficient;

  std::move(callback).Run(support.is_supported, support.is_power_efficient);
}

}  // namespace blink
