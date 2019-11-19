// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_AUDIO_OUTPUT_STREAM_H_
#define MEDIA_MOJO_SERVICES_MOJO_AUDIO_OUTPUT_STREAM_H_

#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "media/audio/audio_output_delegate.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {

// This class handles IPC for single audio output stream by delegating method
// calls to its AudioOutputDelegate.
class MEDIA_MOJO_EXPORT MojoAudioOutputStream
    : public mojom::AudioOutputStream,
      public AudioOutputDelegate::EventHandler {
 public:
  using StreamCreatedCallback =
      base::OnceCallback<void(mojo::PendingRemote<mojom::AudioOutputStream>,
                              media::mojom::ReadWriteAudioDataPipePtr)>;
  using CreateDelegateCallback =
      base::OnceCallback<std::unique_ptr<AudioOutputDelegate>(
          AudioOutputDelegate::EventHandler*)>;
  using DeleterCallback = base::OnceCallback<void(bool)>;

  // |create_delegate_callback| is used to obtain an AudioOutputDelegate for the
  // stream in the constructor. |stream_created_callback| is called when the
  // stream has been initialized. |deleter_callback| is called when this class
  // should be removed (stream ended/error). Its argument indicates if an error
  // was encountered (false indicates that the remote end closed the stream).
  // |deleter_callback| is required to destroy |this| synchronously.
  MojoAudioOutputStream(CreateDelegateCallback create_delegate_callback,
                        StreamCreatedCallback stream_created_callback,
                        DeleterCallback deleter_callback);

  ~MojoAudioOutputStream() override;

 private:
  // mojom::AudioOutputStream implementation.
  void Play() override;
  void Pause() override;
  void Flush() override;
  void SetVolume(double volume) override;

  // AudioOutputDelegate::EventHandler implementation.
  void OnStreamCreated(
      int stream_id,
      base::UnsafeSharedMemoryRegion shared_memory_region,
      std::unique_ptr<base::CancelableSyncSocket> foreign_socket) override;
  void OnStreamError(int stream_id) override;

  void StreamConnectionLost();

  SEQUENCE_CHECKER(sequence_checker_);

  StreamCreatedCallback stream_created_callback_;
  DeleterCallback deleter_callback_;
  mojo::Receiver<AudioOutputStream> receiver_{this};
  std::unique_ptr<AudioOutputDelegate> delegate_;
  base::WeakPtrFactory<MojoAudioOutputStream> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MojoAudioOutputStream);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_AUDIO_OUTPUT_STREAM_H_
