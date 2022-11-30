// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_AUDIO_INPUT_STREAM_H_
#define MEDIA_MOJO_SERVICES_MOJO_AUDIO_INPUT_STREAM_H_

#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "media/audio/audio_input_delegate.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// This class handles IPC for single audio input stream by delegating method
// calls to its AudioInputDelegate.
class MEDIA_MOJO_EXPORT MojoAudioInputStream
    : public mojom::AudioInputStream,
      public AudioInputDelegate::EventHandler {
 public:
  using StreamCreatedCallback =
      base::OnceCallback<void(mojom::ReadOnlyAudioDataPipePtr, bool)>;
  using CreateDelegateCallback =
      base::OnceCallback<std::unique_ptr<AudioInputDelegate>(
          AudioInputDelegate::EventHandler*)>;

  // |create_delegate_callback| is used to obtain an AudioInputDelegate for the
  // stream in the constructor. |stream_created_callback| is called when the
  // stream has been initialized. |deleter_callback| is called when this class
  // should be removed (stream ended/error). |deleter_callback| is required to
  // destroy |this| synchronously.
  MojoAudioInputStream(
      mojo::PendingReceiver<mojom::AudioInputStream> receiver,
      mojo::PendingRemote<mojom::AudioInputStreamClient> client,
      CreateDelegateCallback create_delegate_callback,
      StreamCreatedCallback stream_created_callback,
      base::OnceClosure deleter_callback);

  MojoAudioInputStream(const MojoAudioInputStream&) = delete;
  MojoAudioInputStream& operator=(const MojoAudioInputStream&) = delete;

  ~MojoAudioInputStream() override;

  void SetOutputDeviceForAec(const std::string& raw_output_device_id);

 private:
  // mojom::AudioInputStream implementation.
  void Record() override;
  void SetVolume(double volume) override;

  // AudioInputDelegate::EventHandler implementation.
  void OnStreamCreated(
      int stream_id,
      base::ReadOnlySharedMemoryRegion shared_memory_region,
      std::unique_ptr<base::CancelableSyncSocket> foreign_socket,
      bool initially_muted) override;
  void OnStreamError(int stream_id) override;

  // Closes connection to client and notifies owner.
  void OnError();

  SEQUENCE_CHECKER(sequence_checker_);

  StreamCreatedCallback stream_created_callback_;
  base::OnceClosure deleter_callback_;
  mojo::Receiver<AudioInputStream> receiver_;
  mojo::Remote<mojom::AudioInputStreamClient> client_;
  std::unique_ptr<AudioInputDelegate> delegate_;
  base::WeakPtrFactory<MojoAudioInputStream> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_AUDIO_INPUT_STREAM_H_
