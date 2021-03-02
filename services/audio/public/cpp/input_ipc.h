// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_INPUT_IPC_H_
#define SERVICES_AUDIO_PUBLIC_CPP_INPUT_IPC_H_

#include <memory>
#include <string>

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/audio/audio_input_ipc.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace audio {

// InputIPC is a client-side class for handling creation,
// initialization and control of an input stream. May only be used on a single
// thread.
class InputIPC : public media::AudioInputIPC,
                 public media::mojom::AudioInputStreamClient {
 public:
  InputIPC(mojo::PendingRemote<audio::mojom::StreamFactory> stream_factory,
           const std::string& device_id,
           mojo::PendingRemote<media::mojom::AudioLog> log);
  ~InputIPC() override;

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
  // AudioInputStreamClient implementation.
  void OnError() override;
  void OnMutedStateChanged(bool is_muted) override;

  void StreamCreated(media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
                     bool is_muted,
                     const base::Optional<base::UnguessableToken>& stream_id);

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Remote<media::mojom::AudioInputStream> stream_;
  mojo::Receiver<AudioInputStreamClient> stream_client_receiver_{this};
  media::AudioInputIPCDelegate* delegate_ = nullptr;

  std::string device_id_;
  base::Optional<base::UnguessableToken> stream_id_;

  // |pending_stream_factory_| is initialized in the constructor, and later
  // bound to |stream_factory_|. This is done because the constructor may be
  // called from a different sequence than the other functions and
  // |stream_factory_| must be bound on the sequence which uses it.
  mojo::Remote<audio::mojom::StreamFactory> stream_factory_;
  mojo::PendingRemote<audio::mojom::StreamFactory> pending_stream_factory_;

  mojo::Remote<media::mojom::AudioLog> log_;

  base::WeakPtrFactory<InputIPC> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InputIPC);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_INPUT_IPC_H_
