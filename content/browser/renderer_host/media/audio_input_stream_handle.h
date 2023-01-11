// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_STREAM_HANDLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_STREAM_HANDLE_H_

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "media/mojo/services/mojo_audio_input_stream.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/handle.h"
#include "third_party/blink/public/mojom/media/renderer_audio_input_stream_factory.mojom.h"

namespace content {

// This class creates a MojoAudioInputStream and forwards the OnCreated event
// to a RendererAudioInputStreamFactoryClient.
class CONTENT_EXPORT AudioInputStreamHandle {
 public:
  using DeleterCallback = base::OnceCallback<void(AudioInputStreamHandle*)>;

  // |deleter_callback| will be called when encountering an error, in which
  // case |this| should be synchronously destructed by its owner.
  AudioInputStreamHandle(
      mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
          client_pending_remote,
      media::MojoAudioInputStream::CreateDelegateCallback
          create_delegate_callback,
      DeleterCallback deleter_callback);

  AudioInputStreamHandle(const AudioInputStreamHandle&) = delete;
  AudioInputStreamHandle& operator=(const AudioInputStreamHandle&) = delete;

  ~AudioInputStreamHandle();

  const base::UnguessableToken& id() const { return stream_id_; }
  void SetOutputDeviceForAec(const std::string& raw_output_device_id);

 private:
  void OnCreated(media::mojom::ReadOnlyAudioDataPipePtr, bool initially_muted);

  void CallDeleter();

  SEQUENCE_CHECKER(sequence_checker_);
  const base::UnguessableToken stream_id_;
  DeleterCallback deleter_callback_;
  mojo::Remote<blink::mojom::RendererAudioInputStreamFactoryClient>
      client_remote_;
  mojo::PendingRemote<media::mojom::AudioInputStream> pending_stream_;
  mojo::PendingReceiver<media::mojom::AudioInputStreamClient>
      pending_stream_client_;
  media::MojoAudioInputStream stream_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_STREAM_HANDLE_H_
