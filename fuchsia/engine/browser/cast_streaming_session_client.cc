// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/cast_streaming_session_client.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/cast/message_port/message_port_fuchsia.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/mojom/media_types.mojom.h"

CastStreamingSessionClient::CastStreamingSessionClient(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request)
    : message_port_request_(std::move(message_port_request)) {}

CastStreamingSessionClient::~CastStreamingSessionClient() = default;

void CastStreamingSessionClient::StartMojoConnection(
    mojo::AssociatedRemote<mojom::CastStreamingReceiver>
        cast_streaming_receiver) {
  DVLOG(1) << __func__;
  cast_streaming_receiver_ = std::move(cast_streaming_receiver);

  // It is fine to use an unretained pointer to |this| here as the
  // AssociatedRemote, is owned by |this| and will be torn-down at the same time
  // as |this|.
  cast_streaming_receiver_->EnableReceiver(base::BindOnce(
      &CastStreamingSessionClient::OnReceiverEnabled, base::Unretained(this)));
  cast_streaming_receiver_.set_disconnect_handler(base::BindOnce(
      &CastStreamingSessionClient::OnMojoDisconnect, base::Unretained(this)));
}

void CastStreamingSessionClient::OnReceiverEnabled() {
  DVLOG(1) << __func__;
  DCHECK(message_port_request_);
  cast_streaming_session_.Start(this,
                                cast_api_bindings::MessagePortFuchsia::Create(
                                    std::move(message_port_request_)),
                                base::SequencedTaskRunnerHandle::Get());
}

void CastStreamingSessionClient::OnSessionInitialization(
    base::Optional<cast_streaming::CastStreamingSession::AudioStreamInfo>
        audio_stream_info,
    base::Optional<cast_streaming::CastStreamingSession::VideoStreamInfo>
        video_stream_info) {
  DVLOG(1) << __func__;
  DCHECK(audio_stream_info || video_stream_info);

  mojom::AudioStreamInfoPtr mojo_audio_stream_info;
  if (audio_stream_info) {
    mojo_audio_stream_info =
        mojom::AudioStreamInfo::New(audio_stream_info->decoder_config,
                                    audio_remote_.BindNewPipeAndPassReceiver(),
                                    std::move(audio_stream_info->data_pipe));
  }

  mojom::VideoStreamInfoPtr mojo_video_stream_info;
  if (video_stream_info) {
    mojo_video_stream_info =
        mojom::VideoStreamInfo::New(video_stream_info->decoder_config,
                                    video_remote_.BindNewPipeAndPassReceiver(),
                                    std::move(video_stream_info->data_pipe));
  }

  cast_streaming_receiver_->OnStreamsInitialized(
      std::move(mojo_audio_stream_info), std::move(mojo_video_stream_info));
}

void CastStreamingSessionClient::OnAudioBufferReceived(
    media::mojom::DecoderBufferPtr buffer) {
  DVLOG(3) << __func__;
  DCHECK(audio_remote_);
  audio_remote_->ProvideBuffer(std::move(buffer));
}

void CastStreamingSessionClient::OnVideoBufferReceived(
    media::mojom::DecoderBufferPtr buffer) {
  DVLOG(3) << __func__;
  DCHECK(video_remote_);
  video_remote_->ProvideBuffer(std::move(buffer));
}

void CastStreamingSessionClient::OnSessionReinitialization(
    base::Optional<cast_streaming::CastStreamingSession::AudioStreamInfo>
        audio_stream_info,
    base::Optional<cast_streaming::CastStreamingSession::VideoStreamInfo>
        video_stream_info) {
  DVLOG(1) << __func__;
  DCHECK(audio_stream_info || video_stream_info);

  if (audio_stream_info) {
    audio_remote_->OnNewAudioConfig(audio_stream_info->decoder_config,
                                    std::move(audio_stream_info->data_pipe));
  }

  if (video_stream_info) {
    video_remote_->OnNewVideoConfig(video_stream_info->decoder_config,
                                    std::move(video_stream_info->data_pipe));
  }
}

void CastStreamingSessionClient::OnSessionEnded() {
  DVLOG(1) << __func__;

  // Tear down the Mojo connection.
  cast_streaming_receiver_.reset();

  // Tear down all remaining Mojo objects if needed. This is necessary if the
  // Cast Streaming Session ending was initiated by the receiver component.
  if (audio_remote_)
    audio_remote_.reset();
  if (video_remote_)
    video_remote_.reset();
}

void CastStreamingSessionClient::OnMojoDisconnect() {
  DVLOG(1) << __func__;

  if (message_port_request_) {
    // Close the MessagePort if the Cast Streaming Session was never started.
    message_port_request_.Close(ZX_ERR_PEER_CLOSED);
    cast_streaming_receiver_.reset();
    return;
  }

  // Close the Cast Streaming Session. OnSessionEnded() will be called as part
  // of the Session shutdown, which will tear down the Mojo connection.
  cast_streaming_session_.Stop();

  // Tear down all remaining Mojo objects.
  audio_remote_.reset();
  video_remote_.reset();
}
