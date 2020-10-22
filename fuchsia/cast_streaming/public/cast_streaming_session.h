// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_CAST_STREAMING_PUBLIC_CAST_STREAMING_SESSION_H_
#define FUCHSIA_CAST_STREAMING_PUBLIC_CAST_STREAMING_SESSION_H_

#include <fuchsia/web/cpp/fidl.h>

#include <memory>

#include "base/callback.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace cast_streaming {

// Entry point for the Cast Streaming Receiver implementation. Used to start a
// Cast Streaming Session for a provided FIDL MessagePort request.
class CastStreamingSession {
 public:
  using NetworkContextGetter =
      base::RepeatingCallback<network::mojom::NetworkContext*()>;

  // Sets the NetworkContextGetter. This must be called before any call to
  // Start() and must only be called once. If the NetworkContext crashes, any
  // existing Cast Streaming Session will eventually terminate and call
  // OnSessionEnded().
  static void SetNetworkContextGetter(NetworkContextGetter getter);

  template <class T>
  struct StreamInfo {
    T decoder_config;
    mojo::ScopedDataPipeConsumerHandle data_pipe;
  };
  using AudioStreamInfo = StreamInfo<media::AudioDecoderConfig>;
  using VideoStreamInfo = StreamInfo<media::VideoDecoderConfig>;

  class Client {
   public:
    // Called when the Cast Streaming Session has been successfully initialized.
    // It is guaranteed that at least one of |audio_stream_info| or
    // |video_stream_info| will be set.
    virtual void OnSessionInitialization(
        base::Optional<AudioStreamInfo> audio_stream_info,
        base::Optional<VideoStreamInfo> video_stream_info) = 0;

    // Called on every new audio buffer after OnSessionInitialization(). The
    // frame data must be accessed via the |data_pipe| property in StreamInfo.
    virtual void OnAudioBufferReceived(
        media::mojom::DecoderBufferPtr buffer) = 0;

    // Called on every new video buffer after OnSessionInitialization(). The
    // frame data must be accessed via the |data_pipe| property in StreamInfo.
    virtual void OnVideoBufferReceived(
        media::mojom::DecoderBufferPtr buffer) = 0;

    // Called on receiver session reinitialization. It is guaranteed that at
    // least one of |audio_stream_info| or |video_stream_info| will be set.
    virtual void OnSessionReinitialization(
        base::Optional<AudioStreamInfo> audio_stream_info,
        base::Optional<VideoStreamInfo> video_stream_info) = 0;

    // Called when the Cast Streaming Session has ended.
    virtual void OnSessionEnded() = 0;

   protected:
    virtual ~Client();
  };

  CastStreamingSession();
  ~CastStreamingSession();

  CastStreamingSession(const CastStreamingSession&) = delete;
  CastStreamingSession& operator=(const CastStreamingSession&) = delete;

  // Starts the Cast Streaming Session. This can only be called once during the
  // lifespan of this object. |client| must not be null and must outlive this
  // object.
  // * On success, OnSessionInitialization() will be called and
  //   OnAudioFrameReceived() and/or OnVideoFrameReceived() will be called on
  //   every subsequent Frame.
  // * On failure, OnSessionEnded() will be called.
  // * When a new offer is sent by the Cast Streaming Sender,
  //   OnSessionReinitialization() will be called.
  void Start(
      Client* client,
      fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Stops the Cast Streaming Session. This can only be called once during the
  // lifespan of this object and only after a call to Start().
  void Stop();

 private:
  class Internal;
  std::unique_ptr<Internal> internal_;
};

}  // namespace cast_streaming

#endif  // FUCHSIA_CAST_STREAMING_PUBLIC_CAST_STREAMING_SESSION_H_
