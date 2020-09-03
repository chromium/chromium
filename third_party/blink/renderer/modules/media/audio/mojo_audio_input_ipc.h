// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_AUDIO_MOJO_AUDIO_INPUT_IPC_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_AUDIO_MOJO_AUDIO_INPUT_IPC_H_

#include <string>

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/audio/audio_input_ipc.h"
#include "media/audio/audio_source_parameters.h"
#include "media/mojo/mojom/audio_input_stream.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/media/renderer_audio_input_stream_factory.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

// MojoAudioInputIPC is a renderer-side class for handling creation,
// initialization and control of an input stream. May only be used on a single
// thread.
class MODULES_EXPORT MojoAudioInputIPC
    : public media::AudioInputIPC,
      public mojom::blink::RendererAudioInputStreamFactoryClient,
      public media::mojom::blink::AudioInputStreamClient {
 public:
  // This callback is used by MojoAudioInputIPC to create streams.
  // It is expected that after calling, either client->StreamCreated() is
  // called or |client| is destructed.
  using StreamCreatorCB = base::RepeatingCallback<void(
      const media::AudioSourceParameters& source_params,
      mojo::PendingRemote<mojom::blink::RendererAudioInputStreamFactoryClient>
          client,
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
  void CloseStream() override;

 private:
  void StreamCreated(
      mojo::PendingRemote<media::mojom::blink::AudioInputStream> stream,
      mojo::PendingReceiver<media::mojom::blink::AudioInputStreamClient>
          stream_client_receiver,
      media::mojom::blink::ReadOnlyAudioDataPipePtr data_pipe,
      bool initially_muted,
      const base::Optional<base::UnguessableToken>& stream_id) override;
  void OnError() override;
  void OnMutedStateChanged(bool is_muted) override;

  SEQUENCE_CHECKER(sequence_checker_);

  const media::AudioSourceParameters source_params_;

  StreamCreatorCB stream_creator_;
  StreamAssociatorCB stream_associator_;

  mojo::Remote<media::mojom::blink::AudioInputStream> stream_;
  // Initialized on StreamCreated.
  base::Optional<base::UnguessableToken> stream_id_;
  mojo::Receiver<AudioInputStreamClient> stream_client_receiver_{this};
  mojo::Receiver<mojom::blink::RendererAudioInputStreamFactoryClient>
      factory_client_receiver_{this};
  media::AudioInputIPCDelegate* delegate_ = nullptr;

  base::WeakPtrFactory<MojoAudioInputIPC> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MojoAudioInputIPC);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_AUDIO_MOJO_AUDIO_INPUT_IPC_H_
