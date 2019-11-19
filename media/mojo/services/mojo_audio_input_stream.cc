// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_input_stream.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sync_socket.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

MojoAudioInputStream::MojoAudioInputStream(
    mojo::PendingReceiver<mojom::AudioInputStream> receiver,
    mojo::PendingRemote<mojom::AudioInputStreamClient> client,
    CreateDelegateCallback create_delegate_callback,
    StreamCreatedCallback stream_created_callback,
    base::OnceClosure deleter_callback)
    : stream_created_callback_(std::move(stream_created_callback)),
      deleter_callback_(std::move(deleter_callback)),
      receiver_(this, std::move(receiver)),
      client_(std::move(client)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_created_callback_);
  DCHECK(deleter_callback_);
  // |this| owns |receiver_|, so unretained is safe.
  receiver_.set_disconnect_handler(
      base::BindOnce(&MojoAudioInputStream::OnError, base::Unretained(this)));
  client_.set_disconnect_handler(
      base::BindOnce(&MojoAudioInputStream::OnError, base::Unretained(this)));
  delegate_ = std::move(create_delegate_callback).Run(this);
  if (!delegate_) {
    // Failed to initialize the stream. We cannot call |deleter_callback_| yet,
    // since construction isn't done.
    receiver_.reset();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MojoAudioInputStream::OnStreamError,
                       weak_factory_.GetWeakPtr(), /* not used */ 0));
  }
}

MojoAudioInputStream::~MojoAudioInputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoAudioInputStream::SetOutputDeviceForAec(
    const std::string& raw_output_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnSetOutputDeviceForAec(raw_output_device_id);
}

void MojoAudioInputStream::Record() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnRecordStream();
}

void MojoAudioInputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (volume < 0 || volume > 1) {
    LOG(ERROR) << "MojoAudioInputStream::SetVolume(" << volume
               << ") out of range.";
    OnStreamError(/*not used*/ 0);
    return;
  }
  delegate_->OnSetVolume(volume);
}

void MojoAudioInputStream::OnStreamCreated(
    int stream_id,
    base::ReadOnlySharedMemoryRegion shared_memory_region,
    std::unique_ptr<base::CancelableSyncSocket> foreign_socket,
    bool initially_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_created_callback_);
  DCHECK(foreign_socket);

  if (!shared_memory_region.IsValid()) {
    OnStreamError(/*not used*/ 0);
    return;
  }

  mojo::ScopedHandle socket_handle =
      mojo::WrapPlatformFile(foreign_socket->Release());

  DCHECK(socket_handle.is_valid());

  std::move(stream_created_callback_)
      .Run({base::in_place, std::move(shared_memory_region),
            std::move(socket_handle)},
           initially_muted);
}

void MojoAudioInputStream::OnMuted(int stream_id, bool is_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_->OnMutedStateChanged(is_muted);
}

void MojoAudioInputStream::OnStreamError(int stream_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_->OnError();
  OnError();
}

void MojoAudioInputStream::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(deleter_callback_);
  std::move(deleter_callback_).Run();  // Deletes |this|.
}

}  // namespace media
