// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_MOJO_AUDIO_INPUT_IPC_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_MOJO_AUDIO_INPUT_IPC_H_

#include <string>

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/media/renderer_audio_input_stream_factory.mojom.h"
#include "media/audio/audio_input_ipc.h"
#include "media/audio/audio_source_parameters.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "media/webrtc/audio_processor_controls.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/audio_processing.mojom.h"

namespace content {

// MojoAudioInputIPC is a renderer-side class for handling creation,
// initialization and control of an input stream. May only be used on a single
// thread.
class CONTENT_EXPORT MojoAudioInputIPC
    : public media::AudioInputIPC,
      public media::AudioProcessorControls,
      public mojom::RendererAudioInputStreamFactoryClient,
      public media::mojom::AudioInputStreamClient {
 public:
  // This callback is used by MojoAudioInputIPC to create streams.
  // It is expected that after calling, either client->StreamCreated() is
  // called or |client| is destructed.
  using StreamCreatorCB = base::RepeatingCallback<void(
      const media::AudioSourceParameters& source_params,
      mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient> client,
      mojo::PendingReceiver<audio::mojom::AudioProcessorControls>
          controls_receiver,
      const media::AudioParameters& params,
      bool automatic_gain_control,
      uint32_t total_segments)>;

  using StreamAssociatorCB =
      base::RepeatingCallback<void(const base::UnguessableToken& stream_id,
                                   const std::string& output_device_id)>;

  MojoAudioInputIPC(const media::AudioSourceParameters& source_params,
                    StreamCreatorCB stream_creator,
                    StreamAssociatorCB stream_associator);
  ~MojoAudioInputIPC() override;

  // AudioInputIPC implementation
  void CreateStream(media::AudioInputIPCDelegate* delegate,
                    const media::AudioParameters& params,
                    bool automatic_gain_control,
                    uint32_t total_segments) override;

  void RecordStream() override;
  void SetVolume(double volume) override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;
  AudioProcessorControls* GetProcessorControls() override;
  void CloseStream() override;

  // AudioProcessorControls implementation
  void GetStats(GetStatsCB callback) override;
  void StartEchoCancellationDump(base::File file) override;
  void StopEchoCancellationDump() override;

 private:
  void StreamCreated(
      mojo::PendingRemote<media::mojom::AudioInputStream> stream,
      mojo::PendingReceiver<media::mojom::AudioInputStreamClient>
          stream_client_receiver,
      media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
      bool initially_muted,
      const base::Optional<base::UnguessableToken>& stream_id) override;
  void OnError() override;
  void OnMutedStateChanged(bool is_muted) override;

  SEQUENCE_CHECKER(sequence_checker_);

  const media::AudioSourceParameters source_params_;

  StreamCreatorCB stream_creator_;
  StreamAssociatorCB stream_associator_;

  mojo::Remote<media::mojom::AudioInputStream> stream_;
  mojo::Remote<audio::mojom::AudioProcessorControls> processor_controls_;
  // Initialized on StreamCreated.
  base::Optional<base::UnguessableToken> stream_id_;
  mojo::Receiver<AudioInputStreamClient> stream_client_receiver_{this};
  mojo::Receiver<RendererAudioInputStreamFactoryClient>
      factory_client_receiver_{this};
  media::AudioInputIPCDelegate* delegate_ = nullptr;

  base::TimeTicks stream_creation_start_time_;

  base::WeakPtrFactory<MojoAudioInputIPC> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MojoAudioInputIPC);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_MOJO_AUDIO_INPUT_IPC_H_
