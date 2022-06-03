// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_output_stream.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/sync_socket.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

MojoAudioOutputStream::MojoAudioOutputStream(
    CreateDelegateCallback create_delegate_callback,
    StreamCreatedCallback stream_created_callback,
    DeleterCallback deleter_callback)
    : stream_created_callback_(std::move(stream_created_callback)),
      deleter_callback_(std::move(deleter_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_created_callback_);
  DCHECK(deleter_callback_);
  delegate_ = std::move(create_delegate_callback).Run(this);
  if (!delegate_) {
    // Failed to initialize the stream. We cannot call |deleter_callback_| yet,
    // since construction isn't done.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MojoAudioOutputStream::OnStreamError,
                       weak_factory_.GetWeakPtr(), /* not used */ 0));
  }
}

MojoAudioOutputStream::~MojoAudioOutputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoAudioOutputStream::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnPlayStream();
}

void MojoAudioOutputStream::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnPauseStream();
}

void MojoAudioOutputStream::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnFlushStream();
}

void MojoAudioOutputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (volume < 0 || volume > 1) {
    LOG(ERROR) << "MojoAudioOutputStream::SetVolume(" << volume
               << ") out of range.";
    OnStreamError(/*not used*/ 0);
    return;
  }
  delegate_->OnSetVolume(volume);
}

void MojoAudioOutputStream::OnStreamCreated(
    int stream_id,
    base::UnsafeSharedMemoryRegion shared_memory_region,
    std::unique_ptr<base::CancelableSyncSocket> foreign_socket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_created_callback_);
  DCHECK(foreign_socket);

  if (!shared_memory_region.IsValid()) {
    OnStreamError(/*not used*/ 0);
    return;
  }

  mojo::PlatformHandle socket_handle(foreign_socket->Take());
  DCHECK(socket_handle.is_valid());

  mojo::PendingRemote<mojom::AudioOutputStream> pending_stream;
  receiver_.Bind(pending_stream.InitWithNewPipeAndPassReceiver());
  // |this| owns |receiver_| so unretained is safe.
  receiver_.set_disconnect_handler(base::BindOnce(
      &MojoAudioOutputStream::StreamConnectionLost, base::Unretained(this)));

  std::move(stream_created_callback_)
      .Run(std::move(pending_stream),
           {base::in_place, std::move(shared_memory_region),
            std::move(socket_handle)});
}

void MojoAudioOutputStream::OnStreamError(int stream_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(deleter_callback_);
  std::move(deleter_callback_).Run(/*had_error*/ true);  // Deletes |this|.
}

void MojoAudioOutputStream::StreamConnectionLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(deleter_callback_);
  std::move(deleter_callback_).Run(/*had_error*/ false);  // Deletes |this|.
}

}  // namespace media
