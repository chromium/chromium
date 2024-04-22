// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/receiver_session_client.h"

#include "base/functional/bind.h"
#include "components/cast/message_port/fuchsia/message_port_fuchsia.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast_streaming/browser/public/receiver_config.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "components/cast_streaming/common/public/mojom/demuxer_connector.mojom.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

namespace {
constexpr size_t kMaxFrameRate = 30;
}  // namespace

ReceiverSessionClient::ReceiverSessionClient(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request,
    bool video_only_receiver)
    : message_port_request_(std::move(message_port_request)),
      video_only_receiver_(video_only_receiver) {
  DCHECK(message_port_request_);
}

ReceiverSessionClient::~ReceiverSessionClient() = default;

void ReceiverSessionClient::SetMojoEndpoints(
    mojo::AssociatedRemote<cast_streaming::mojom::DemuxerConnector>
        demuxer_connector,
    mojo::AssociatedRemote<cast_streaming::mojom::RendererController>
        renderer_controller) {
  DCHECK(message_port_request_);

  // TODO: Add streaming session Constraints based on system capabilities
  // (see crbug.com/1013412) and DisplayDescription (see crbug.com/1087520).
  // TODO(crbug.com/40185682): Only populate codecs corresponding to those
  // called out by build flags.
  std::vector<media::VideoCodec> video_codecs;
  std::vector<media::AudioCodec> audio_codecs;

  // Codecs are set in order of preference for the receiver, i.e. H264 is
  // preferred above VP8 in the below code.
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  video_codecs.push_back(media::VideoCodec::kH264);
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  video_codecs.push_back(media::VideoCodec::kVP8);

  if (!video_only_receiver_) {
    audio_codecs.push_back(media::AudioCodec::kOpus);
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    audio_codecs.push_back(media::AudioCodec::kAAC);
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  }

  // TODO(crbug.com/40241121): Set config.remoting to expose the device as a
  // valid remoting endpoint when |renderer_controller| is set.
  cast_streaming::ReceiverConfig config(std::move(video_codecs),
                                        std::move(audio_codecs));

  gfx::Size display_resolution =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().size();
  config.video_limits.push_back(cast_streaming::ReceiverConfig::VideoLimits{
      .max_pixels_per_second =
          static_cast<int>(display_resolution.width() *
                           display_resolution.height() * kMaxFrameRate),
      .max_dimensions = gfx::Rect(display_resolution),
      .max_frame_rate = kMaxFrameRate});
  config.display_description = cast_streaming::ReceiverConfig::Display{
      .dimensions = gfx::Rect(display_resolution),
      .max_frame_rate = kMaxFrameRate,
      .can_scale_content = true};

  receiver_session_ = cast_streaming::ReceiverSession::Create(
      config,
      base::BindOnce(
          [](fidl::InterfaceRequest<fuchsia::web::MessagePort> port)
              -> std::unique_ptr<cast_api_bindings::MessagePort> {
            return cast_api_bindings::MessagePortFuchsia::Create(
                std::move(port));
          },
          std::move(message_port_request_)));
  if (renderer_controller) {
    receiver_session_->StartStreamingAsync(std::move(demuxer_connector),
                                           std::move(renderer_controller));
  } else {
    receiver_session_->StartStreamingAsync(std::move(demuxer_connector));
  }
}

bool ReceiverSessionClient::HasReceiverSession() {
  return !!receiver_session_;
}
