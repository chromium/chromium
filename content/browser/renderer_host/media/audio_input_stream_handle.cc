// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_stream_handle.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"

namespace content {

namespace {

mojo::PendingRemote<media::mojom::AudioInputStreamClient>
CreateRemoteAndStoreReceiver(
    mojo::PendingReceiver<media::mojom::AudioInputStreamClient>* receiver_out) {
  mojo::PendingRemote<media::mojom::AudioInputStreamClient> remote;
  *receiver_out = remote.InitWithNewPipeAndPassReceiver();
  return remote;
}

}  // namespace

AudioInputStreamHandle::AudioInputStreamHandle(
    mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient>
        client_pending_remote,
    media::MojoAudioInputStream::CreateDelegateCallback
        create_delegate_callback,
    DeleterCallback deleter_callback)
    : stream_id_(base::UnguessableToken::Create()),
      deleter_callback_(std::move(deleter_callback)),
      client_remote_(std::move(client_pending_remote)),
      stream_(pending_stream_.InitWithNewPipeAndPassReceiver(),
              CreateRemoteAndStoreReceiver(&pending_stream_client_),
              std::move(create_delegate_callback),
              base::BindOnce(&AudioInputStreamHandle::OnCreated,
                             base::Unretained(this)),
              base::BindOnce(&AudioInputStreamHandle::CallDeleter,
                             base::Unretained(this))) {
  // Unretained is safe since |this| owns |stream_| and |client_remote_|.
  DCHECK(client_remote_);
  DCHECK(deleter_callback_);
  client_remote_.set_disconnect_handler(base::BindOnce(
      &AudioInputStreamHandle::CallDeleter, base::Unretained(this)));
}

AudioInputStreamHandle::~AudioInputStreamHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AudioInputStreamHandle::SetOutputDeviceForAec(
    const std::string& raw_output_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_.SetOutputDeviceForAec(raw_output_device_id);
}

void AudioInputStreamHandle::OnCreated(
    media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
    bool initially_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_remote_);
  DCHECK(deleter_callback_)
      << "|deleter_callback_| was called, but |this| hasn't been destructed!";
  client_remote_->StreamCreated(
      std::move(pending_stream_), std::move(pending_stream_client_),
      std::move(data_pipe), initially_muted, stream_id_);
}

void AudioInputStreamHandle::CallDeleter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(deleter_callback_);
  std::move(deleter_callback_).Run(this);
}

}  // namespace content
