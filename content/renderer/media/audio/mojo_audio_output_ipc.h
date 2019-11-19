// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_MOJO_AUDIO_OUTPUT_IPC_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_MOJO_AUDIO_OUTPUT_IPC_H_

#include <string>

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/media/renderer_audio_output_stream_factory.mojom.h"
#include "media/audio/audio_output_ipc.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// MojoAudioOutputIPC is a renderer-side class for handling creation,
// initialization and control of an output stream. May only be used on a single
// thread.
class CONTENT_EXPORT MojoAudioOutputIPC
    : public media::AudioOutputIPC,
      public media::mojom::AudioOutputStreamProviderClient {
 public:
  using FactoryAccessorCB =
      base::RepeatingCallback<mojom::RendererAudioOutputStreamFactory*()>;

  // |factory_accessor| is required to provide a
  // RendererAudioOutputStreamFactory* if IPC is possible.
  MojoAudioOutputIPC(
      FactoryAccessorCB factory_accessor,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ~MojoAudioOutputIPC() override;

  // AudioOutputIPC implementation.
  void RequestDeviceAuthorization(media::AudioOutputIPCDelegate* delegate,
                                  const base::UnguessableToken& session_id,
                                  const std::string& device_id) override;
  void CreateStream(
      media::AudioOutputIPCDelegate* delegate,
      const media::AudioParameters& params,
      const base::Optional<base::UnguessableToken>& processing_id) override;
  void PlayStream() override;
  void PauseStream() override;
  void FlushStream() override;
  void CloseStream() override;
  void SetVolume(double volume) override;

  // media::mojom::AudioOutputStreamProviderClient implementation.
  void Created(mojo::PendingRemote<media::mojom::AudioOutputStream> stream,
               media::mojom::ReadWriteAudioDataPipePtr data_pipe) override;

 private:
  static constexpr double kDefaultVolume = 1.0;

  using AuthorizationCB = mojom::RendererAudioOutputStreamFactory::
      RequestDeviceAuthorizationCallback;

  bool AuthorizationRequested() const;
  bool StreamCreationRequested() const;

  void ProviderClientBindingDisconnected(uint32_t disconnect_reason,
                                         const std::string& description);

  mojo::PendingReceiver<media::mojom::AudioOutputStreamProvider>
  MakeProviderReceiver();

  // Tries to acquire a RendererAudioOutputStreamFactory and requests device
  // authorization. On failure to aquire a factory, |callback| is destructed
  // asynchronously.
  void DoRequestDeviceAuthorization(const base::UnguessableToken& session_id,
                                    const std::string& device_id,
                                    AuthorizationCB callback);

  void ReceivedDeviceAuthorization(base::TimeTicks auth_start_time,
                                   media::OutputDeviceStatus status,
                                   const media::AudioParameters& params,
                                   const std::string& device_id) const;

  const FactoryAccessorCB factory_accessor_;

  // This is the state that |delegate_| expects the stream to be in. It is
  // maintained for when the stream is created.
  enum { kPaused, kPlaying } expected_state_ = kPaused;
  base::Optional<double> volume_;

  mojo::Receiver<media::mojom::AudioOutputStreamProviderClient> receiver_{this};
  mojo::Remote<media::mojom::AudioOutputStreamProvider> stream_provider_;
  mojo::Remote<media::mojom::AudioOutputStream> stream_;
  media::AudioOutputIPCDelegate* delegate_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  base::TimeTicks stream_creation_start_time_;

  // To make sure we don't send an "authorization completed" callback for a
  // stream after it's closed, we use this weak factory.
  base::WeakPtrFactory<MojoAudioOutputIPC> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MojoAudioOutputIPC);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_MOJO_AUDIO_OUTPUT_IPC_H_
