// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_FAKE_STREAM_FACTORY_H_
#define SERVICES_AUDIO_PUBLIC_CPP_FAKE_STREAM_FACTORY_H_

#include <string>

#include "base/optional.h"
#include "base/run_loop.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace audio {

class FakeStreamFactory : public mojom::StreamFactory {
 public:
  FakeStreamFactory();
  ~FakeStreamFactory() override;

  mojo::PendingRemote<mojom::StreamFactory> MakeRemote() {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(base::BindOnce(
        &FakeStreamFactory::ResetReceiver, base::Unretained(this)));
    return remote;
  }

  void ResetReceiver() {
    receiver_.reset();
    if (disconnect_loop_)
      disconnect_loop_->Quit();
  }

  void WaitForDisconnect() {
    disconnect_loop_.emplace();
    disconnect_loop_->Run();
  }

  void CreateInputStream(
      mojo::PendingReceiver<::media::mojom::AudioInputStream> stream_receiver,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<::media::mojom::AudioInputStreamObserver> observer,
      mojo::PendingRemote<::media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool enable_agc,
      base::ReadOnlySharedMemoryRegion key_press_count_buffer,
      CreateInputStreamCallback callback) override {}

  void AssociateInputAndOutputForAec(
      const base::UnguessableToken& input_stream_id,
      const std::string& output_device_id) override {}

  void CreateOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateOutputStreamCallback created_callback) override {}
  void BindMuter(mojo::PendingAssociatedReceiver<mojom::LocalMuter> receiver,
                 const base::UnguessableToken& group_id) override {}
  void CreateLoopbackStream(
      mojo::PendingReceiver<media::mojom::AudioInputStream> receiver,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      const base::UnguessableToken& group_id,
      CreateLoopbackStreamCallback created_callback) override {}

  mojo::Receiver<mojom::StreamFactory> receiver_{this};

 private:
  base::Optional<base::RunLoop> disconnect_loop_;

  DISALLOW_COPY_AND_ASSIGN(FakeStreamFactory);
};

static_assert(
    !std::is_abstract<FakeStreamFactory>(),
    "FakeStreamFactory should implement all of the StreamFactory interface.");

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_FAKE_STREAM_FACTORY_H_
