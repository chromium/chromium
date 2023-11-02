// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/receiver_session_client.h"

#include "base/bind.h"
#include "components/cast/message_port/fuchsia/message_port_fuchsia.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "components/cast_streaming/public/config_conversions.h"
#include "components/cast_streaming/public/mojom/demuxer_connector.mojom.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"

ReceiverSessionClient::ReceiverSessionClient(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request,
    bool video_only_receiver)
    : message_port_request_(std::move(message_port_request)),
      video_only_receiver_(video_only_receiver) {
  DCHECK(message_port_request_);
}

ReceiverSessionClient::~ReceiverSessionClient() = default;

void ReceiverSessionClient::SetDemuxerConnector(
    mojo::AssociatedRemote<cast_streaming::mojom::DemuxerConnector>
        demuxer_connector) {
  DCHECK(message_port_request_);

  // TODO: Add streaming session Constraints based on system capabilities
  // (see crbug.com/1013412) and DisplayDescription (see crbug.com/1087520).
  // TODO(crbug.com/1218498): Only populate codecs corresponding to those called
  // out by build flags.
  auto stream_config =
      std::make_unique<cast_streaming::ReceiverSession::AVConstraints>(
          cast_streaming::ToVideoCaptureConfigCodecs(
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
              media::VideoCodec::kH264,
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
              media::VideoCodec::kVP8),
          video_only_receiver_ ? cast_streaming::ToAudioCaptureConfigCodecs()
                               : cast_streaming::ToAudioCaptureConfigCodecs(
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
                                     media::AudioCodec::kAAC,
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
                                     media::AudioCodec::kOpus));

  receiver_session_ = cast_streaming::ReceiverSession::Create(
      std::move(stream_config),
      base::BindOnce(
          [](fidl::InterfaceRequest<fuchsia::web::MessagePort> port)
              -> std::unique_ptr<cast_api_bindings::MessagePort> {
            return cast_api_bindings::MessagePortFuchsia::Create(
                std::move(port));
          },
          std::move(message_port_request_)));
  receiver_session_->StartStreamingAsync(std::move(demuxer_connector));
}

bool ReceiverSessionClient::HasReceiverSession() {
  return !!receiver_session_;
}
