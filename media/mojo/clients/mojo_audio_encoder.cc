// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_audio_encoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/audio_buffer.h"
#include "media/mojo/common/media_type_converters.h"

namespace media {

MojoAudioEncoder::MojoAudioEncoder(
    mojo::PendingRemote<mojom::AudioEncoder> remote_encoder)
    : pending_remote_encoder_(std::move(remote_encoder)),
      buffer_pool_(new AudioBufferMemoryPool()),
      runner_(base::SequencedTaskRunnerHandle::Get()) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoAudioEncoder::~MojoAudioEncoder() = default;

void MojoAudioEncoder::Initialize(const Options& options,
                                  OutputCB output_cb,
                                  StatusCB done_cb) {
  DCHECK(output_cb);
  DCHECK(done_cb);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (remote_encoder_.is_bound() || client_receiver_.is_bound()) {
    PostStatusCallback(std::move(done_cb), StatusCode::kEncoderInitializeTwice);
    return;
  }

  BindRemote();

  if (!remote_encoder_.is_bound() || !remote_encoder_.is_connected()) {
    PostStatusCallback(std::move(done_cb),
                       StatusCode::kEncoderInitializationError);
    return;
  }

  output_cb_ = std::move(output_cb);
  options_ = options;
  remote_encoder_->Initialize(client_receiver_.BindNewEndpointAndPassRemote(),
                              options,
                              WrapCallbackAsPending(std::move(done_cb)));
}

void MojoAudioEncoder::Encode(std::unique_ptr<AudioBus> audio_bus,
                              base::TimeTicks capture_time,
                              StatusCB done_cb) {
  DCHECK(audio_bus);
  DCHECK(done_cb);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!remote_encoder_.is_bound() || !remote_encoder_.is_connected()) {
    PostStatusCallback(std::move(done_cb), StatusCode::kEncoderFailedEncode);
    return;
  }

  auto buffer = AudioBuffer::CopyFrom(options_.sample_rate,
                                      capture_time - base::TimeTicks(),
                                      audio_bus.get(), buffer_pool_);

  remote_encoder_->Encode(mojom::AudioBuffer::From(*buffer),
                          WrapCallbackAsPending(std::move(done_cb)));
}

void MojoAudioEncoder::Flush(StatusCB done_cb) {
  DCHECK(done_cb);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!remote_encoder_.is_bound() || !remote_encoder_.is_connected()) {
    PostStatusCallback(std::move(done_cb), StatusCode::kEncoderFailedFlush);
    return;
  }

  remote_encoder_->Flush(WrapCallbackAsPending(std::move(done_cb)));
}

void MojoAudioEncoder::OnEncodedBufferReady(media::EncodedAudioBuffer buffer,
                                            const CodecDescription& desc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  absl::optional<CodecDescription> opt_desc;
  if (desc.size() > 0)
    opt_desc = desc;
  output_cb_.Run(std::move(buffer), std::move(opt_desc));
}

void MojoAudioEncoder::CallAndReleaseCallback(PendingCallbackHandle handle,
                                              const Status& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_callbacks_.empty()) {
    StatusCB callback = std::move(*handle);
    pending_callbacks_.erase(handle);
    std::move(callback).Run(status);
  }
}

MojoAudioEncoder::WrappedStatusCB MojoAudioEncoder::WrapCallbackAsPending(
    StatusCB callback) {
  PendingCallbackHandle handle =
      pending_callbacks_.insert(pending_callbacks_.end(), std::move(callback));
  return base::BindOnce(&MojoAudioEncoder::CallAndReleaseCallback, weak_this_,
                        handle);
}

void MojoAudioEncoder::CallAndReleaseAllPendingCallbacks(Status status) {
  for (auto& callback : pending_callbacks_)
    PostStatusCallback(std::move(callback), status);
  pending_callbacks_.clear();
}

void MojoAudioEncoder::BindRemote() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_encoder_.Bind(std::move(pending_remote_encoder_));
  remote_encoder_.set_disconnect_handler(
      base::BindOnce(&MojoAudioEncoder::OnConnectionError, weak_this_));
}

void MojoAudioEncoder::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!remote_encoder_.is_connected());
  CallAndReleaseAllPendingCallbacks(StatusCode::kEncoderMojoConnectionError);
  weak_factory_.InvalidateWeakPtrs();
  remote_encoder_.reset();
}

void MojoAudioEncoder::PostStatusCallback(StatusCB callback, Status status) {
  runner_->PostTask(FROM_HERE,
                    base::BindOnce(std::move(callback), std::move(status)));
}

}  // namespace media
