// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_decoding_info_handler.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "media/video/gpu_video_accelerator_factories.h"
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
    : WebrtcDecodingInfoHandler(Platform::Current()
                                    ? Platform::Current()->GetGpuFactories()
                                    : nullptr) {}

WebrtcDecodingInfoHandler::WebrtcDecodingInfoHandler(
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : WebrtcDecodingInfoHandler(
          blink::CreateWebrtcVideoDecoderFactory(
              gpu_factories,
              Platform::Current()
                  ? Platform::Current()->GetRenderingColorSpace()
                  : gfx::ColorSpace(),
              base::DoNothing()),
          blink::CreateWebrtcAudioDecoderFactory(),
          gpu_factories) {}

WebrtcDecodingInfoHandler::WebrtcDecodingInfoHandler(
    std::unique_ptr<webrtc::VideoDecoderFactory> video_decoder_factory,
    webrtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory,
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : video_decoder_factory_(std::move(video_decoder_factory)),
      audio_decoder_factory_(std::move(audio_decoder_factory)),
      gpu_factories_(gpu_factories) {
  std::vector<webrtc::AudioCodecSpec> supported_audio_specs =
      audio_decoder_factory_->GetSupportedDecoders();
  for (const auto& audio_spec : supported_audio_specs) {
    supported_audio_codecs_.insert(
        String::FromUtf8(audio_spec.format.name).ToAsciiLower());
  }
}

WebrtcDecodingInfoHandler::~WebrtcDecodingInfoHandler() = default;

void WebrtcDecodingInfoHandler::DecodingInfo(
    const std::optional<webrtc::SdpAudioFormat>& sdp_audio_format,
    const std::optional<webrtc::SdpVideoFormat>& sdp_video_format,
    bool video_spatial_scalability,
    std::optional<gfx::Size> video_resolution,
    OnMediaCapabilitiesDecodingInfoCallback callback) const {
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

  if (gpu_factories_ && !gpu_factories_->IsDecoderSupportKnown()) {
    // Avoid making a blocking call to QueryCodecSupport() if decoder support is
    // not known yet.
    // Unretained(this) is safe because WebrtcDecodingInfoHandler is a leaky
    // singleton in production. Tests must ensure the stack-allocated handler
    // outlives the mocked gpu_factories.
    gpu_factories_->NotifyDecoderSupportKnown(base::BindOnce(
        &WebrtcDecodingInfoHandler::ContinueVideoSupportCheck,
        base::Unretained(this), sdp_video_format, video_spatial_scalability,
        video_resolution, std::move(callback)));
    return;
  }

  ContinueVideoSupportCheck(sdp_video_format, video_spatial_scalability,
                            video_resolution, std::move(callback));
}

void WebrtcDecodingInfoHandler::ContinueVideoSupportCheck(
    const std::optional<webrtc::SdpVideoFormat>& sdp_video_format,
    bool video_spatial_scalability,
    std::optional<gfx::Size> video_resolution,
    OnMediaCapabilitiesDecodingInfoCallback callback) const {
  DCHECK(sdp_video_format);
  std::optional<webrtc::Resolution> resolution;
  if (video_resolution) {
    resolution = {video_resolution->width(), video_resolution->height()};
  }
  webrtc::VideoDecoderFactory::CodecSupport support =
      video_decoder_factory_->QueryCodecSupport(
          *sdp_video_format, video_spatial_scalability, resolution);

  DVLOG(1) << "Video:" << sdp_video_format->name
           << " supported:" << support.is_supported
           << " power_efficient:" << support.is_power_efficient;

  std::move(callback).Run(support.is_supported, support.is_power_efficient);
}

}  // namespace blink
