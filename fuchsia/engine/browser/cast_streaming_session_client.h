// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_CAST_STREAMING_SESSION_CLIENT_H_
#define FUCHSIA_ENGINE_BROWSER_CAST_STREAMING_SESSION_CLIENT_H_

#include <fuchsia/web/cpp/fidl.h>

#include "components/cast_streaming/browser/public/cast_streaming_session.h"
#include "components/cast_streaming/mojo/cast_streaming_session.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

// Owns the CastStreamingSession and sends buffers to the renderer process via
// a Mojo service.
//
// TODO(crbug.com/1208194): Move this class to //components/cast_streaming,
class CastStreamingSessionClient
    : public cast_streaming::CastStreamingSession::Client {
 public:
  explicit CastStreamingSessionClient(
      fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request);
  ~CastStreamingSessionClient() final;

  CastStreamingSessionClient(const CastStreamingSessionClient&) = delete;
  CastStreamingSessionClient& operator=(const CastStreamingSessionClient&) =
      delete;

  void StartMojoConnection(mojo::AssociatedRemote<mojom::CastStreamingReceiver>
                               cast_streaming_receiver);

 private:
  // Handler for |cast_streaming_receiver_| disconnect.
  void OnMojoDisconnect();

  // Callback for mojom::CastStreamingReceiver::EnableReceiver()
  void OnReceiverEnabled();

  // cast_streaming::CastStreamingSession::Client implementation.
  void OnSessionInitialization(
      absl::optional<cast_streaming::CastStreamingSession::AudioStreamInfo>
          audio_stream_info,
      absl::optional<cast_streaming::CastStreamingSession::VideoStreamInfo>
          video_stream_info) final;
  void OnAudioBufferReceived(media::mojom::DecoderBufferPtr buffer) final;
  void OnVideoBufferReceived(media::mojom::DecoderBufferPtr buffer) final;
  void OnSessionReinitialization(
      absl::optional<cast_streaming::CastStreamingSession::AudioStreamInfo>
          audio_stream_info,
      absl::optional<cast_streaming::CastStreamingSession::VideoStreamInfo>
          video_stream_info) final;
  void OnSessionEnded() final;

  fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request_;
  mojo::AssociatedRemote<mojom::CastStreamingReceiver> cast_streaming_receiver_;
  cast_streaming::CastStreamingSession cast_streaming_session_;

  mojo::Remote<mojom::CastStreamingBufferReceiver> audio_remote_;
  mojo::Remote<mojom::CastStreamingBufferReceiver> video_remote_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_CAST_STREAMING_SESSION_CLIENT_H_
