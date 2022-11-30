// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/decoder_buffer_transcryptor.h"

#include "base/callback.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"
#include "media/base/bind_to_current_loop.h"

namespace media {
DecoderBufferTranscryptor::TranscryptTask::TranscryptTask(
    scoped_refptr<DecoderBuffer> buffer,
    VideoDecoder::DecodeCB decode_done_cb)
    : buffer(std::move(buffer)), decode_done_cb(std::move(decode_done_cb)) {}

DecoderBufferTranscryptor::TranscryptTask::~TranscryptTask() = default;

DecoderBufferTranscryptor::TranscryptTask::TranscryptTask(TranscryptTask&&) =
    default;

DecoderBufferTranscryptor::DecoderBufferTranscryptor(
    CdmContext* cdm_context,
    OnBufferTranscryptedCB transcrypt_callback,
    WaitingCB waiting_callback)
    : transcrypt_callback_(std::move(transcrypt_callback)),
      waiting_callback_(std::move(waiting_callback)) {
  weak_this_ = weak_this_factory_.GetWeakPtr();

  DCHECK(cdm_context);
  cdm_event_cb_registration_ = cdm_context->RegisterEventCB(base::BindRepeating(
      &DecoderBufferTranscryptor::OnCdmContextEvent, weak_this_));
  cdm_context_ref_ = cdm_context->GetChromeOsCdmContext()->GetCdmContextRef();
  DCHECK(cdm_context->GetDecryptor());
}

DecoderBufferTranscryptor::~DecoderBufferTranscryptor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Reset(DecoderStatus::Codes::kAborted);
}

void DecoderBufferTranscryptor::EnqueueBuffer(
    scoped_refptr<DecoderBuffer> buffer,
    VideoDecoder::DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  transcrypt_task_queue_.emplace(std::move(buffer), std::move(decode_cb));
  DecryptPendingBuffer();
}

void DecoderBufferTranscryptor::Reset(DecoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (current_transcrypt_task_) {
    std::move(current_transcrypt_task_->decode_done_cb).Run(status);
    current_transcrypt_task_ = absl::nullopt;
  }

  while (!transcrypt_task_queue_.empty()) {
    std::move(transcrypt_task_queue_.front().decode_done_cb).Run(status);
    transcrypt_task_queue_.pop();
  }
}

void DecoderBufferTranscryptor::OnCdmContextEvent(CdmContext::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event != CdmContext::Event::kHasAdditionalUsableKey)
    return;

  if (transcrypt_pending_)
    key_added_while_decrypting_ = true;
  else
    DecryptPendingBuffer();
}

void DecoderBufferTranscryptor::DecryptPendingBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (transcrypt_pending_)
    return;

  if (!current_transcrypt_task_) {
    if (transcrypt_task_queue_.empty())
      return;
    current_transcrypt_task_ = std::move(transcrypt_task_queue_.front());
    transcrypt_task_queue_.pop();
  }

  if (current_transcrypt_task_->buffer->end_of_stream()) {
    OnBufferTranscrypted(Decryptor::kSuccess, current_transcrypt_task_->buffer);
    return;
  }
  transcrypt_pending_ = true;
  cdm_context_ref_->GetCdmContext()->GetDecryptor()->Decrypt(
      Decryptor::kVideo, current_transcrypt_task_->buffer,
      BindToCurrentLoop(base::BindOnce(
          &DecoderBufferTranscryptor::OnBufferTranscrypted, weak_this_)));
}

void DecoderBufferTranscryptor::OnBufferTranscrypted(
    Decryptor::Status status,
    scoped_refptr<DecoderBuffer> transcrypted_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  transcrypt_pending_ = false;

  // If we've cleared the task then drop this one.
  if (!current_transcrypt_task_) {
    DecryptPendingBuffer();
    return;
  }

  const bool need_to_try_again_if_nokey = key_added_while_decrypting_;
  key_added_while_decrypting_ = false;
  // This should never happen w/our decryptor.
  DCHECK_NE(status, Decryptor::kNeedMoreData);
  if (status == Decryptor::kError) {
    // Clear |current_transcrypt_task_| now so when the pipeline invokes Reset
    // on us we don't try to invoke the move'd callback.
    absl::optional<TranscryptTask> temp_task =
        std::move(current_transcrypt_task_);
    current_transcrypt_task_ = absl::nullopt;
    transcrypt_callback_.Run(nullptr, std::move(temp_task->decode_done_cb));
    return;
  }

  if (status == Decryptor::kNoKey) {
    if (need_to_try_again_if_nokey) {
      DecryptPendingBuffer();
      return;
    }

    waiting_callback_.Run(WaitingReason::kNoDecryptionKey);
    return;
  }

  DCHECK_EQ(status, Decryptor::kSuccess);
  DCHECK(transcrypted_buffer);

  const bool eos_buffer = transcrypted_buffer->end_of_stream();
  absl::optional<TranscryptTask> temp_task =
      std::move(current_transcrypt_task_);
  current_transcrypt_task_ = absl::nullopt;
  transcrypt_callback_.Run(std::move(transcrypted_buffer),
                           std::move(temp_task->decode_done_cb));

  // Do not post this as another task, execute it immediately instead. Otherwise
  // we will not be parallelizing decrypt and decode fully. We want to have the
  // Mojo IPC call for decrypt active whenever we are processing a decode task,
  // and since the decoder probably just put a decode task in the queue...if we
  // hand control back to the task runner it'll do decode now even though we
  // have no decrypt task in flight.
  if (!eos_buffer)
    DecryptPendingBuffer();
}

}  // namespace media
