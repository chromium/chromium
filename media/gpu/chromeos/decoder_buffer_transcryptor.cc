// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/decoder_buffer_transcryptor.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/parsers/vp9_parser.h"

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
    VideoDecoderMixin& decoder,
    bool needs_vp9_superframe_splitting,
    OnBufferTranscryptedCB transcrypt_callback,
    WaitingCB waiting_callback)
    : decoder_(decoder),
      transcrypt_callback_(std::move(transcrypt_callback)),
      waiting_callback_(std::move(waiting_callback)),
      needs_vp9_superframe_splitting_(needs_vp9_superframe_splitting) {
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

void DecoderBufferTranscryptor::SecureBuffersMayBeAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Re-post this so we don't need to worry about re-entrancy issues.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DecoderBufferTranscryptor::DecryptPendingBuffer,
                     weak_this_));
}

void DecoderBufferTranscryptor::EnqueueBuffer(
    scoped_refptr<DecoderBuffer> buffer,
    VideoDecoder::DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  transcrypt_task_queue_.emplace_back(std::move(buffer), std::move(decode_cb));
  DecryptPendingBuffer();
}

void DecoderBufferTranscryptor::Reset(DecoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (current_transcrypt_task_) {
    std::move(current_transcrypt_task_->decode_done_cb).Run(status);
    current_transcrypt_task_ = std::nullopt;
  }

  while (!transcrypt_task_queue_.empty()) {
    std::move(transcrypt_task_queue_.front().decode_done_cb).Run(status);
    transcrypt_task_queue_.pop_front();
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
    transcrypt_task_queue_.pop_front();
  }

  DecoderBuffer* curr_buffer = current_transcrypt_task_->buffer.get();
  if (curr_buffer->end_of_stream()) {
    OnBufferTranscrypted(Decryptor::kSuccess, current_transcrypt_task_->buffer);
    return;
  }

  // Check if we need to split VP9 superframes.
  if (needs_vp9_superframe_splitting_ &&
      Vp9Parser::IsSuperframe(curr_buffer->data(),
                              base::checked_cast<off_t>(curr_buffer->size()),
                              curr_buffer->decrypt_config())) {
    base::circular_deque<Vp9Parser::FrameInfo> frames =
        Vp9Parser::ExtractFrames(curr_buffer->data(),
                                 base::checked_cast<off_t>(curr_buffer->size()),
                                 curr_buffer->decrypt_config());
    if (frames.empty()) {
      LOG(ERROR) << "Failure in Vp9 superframe splitting";
      OnBufferTranscrypted(Decryptor::kError, nullptr);
      return;
    }

    // Save the original DecoderBuffer for the rest of our operations that has
    // the whole superframe to keep its data valid until we are done and also to
    // use for metadata reference.
    scoped_refptr<DecoderBuffer> superframe =
        std::move(current_transcrypt_task_->buffer);

    // Put the first frame in place of the |current_transcrypt_task_|'s buffer,
    // then add the rest to the queue.
    //
    // TODO(crbug.com/40284755): Use `base::span` in `Vp9Parser::FrameInfo`.
    current_transcrypt_task_->buffer = DecoderBuffer::CopyFrom(UNSAFE_TODO(
        base::span(frames.front().ptr.get(),
                   base::checked_cast<size_t>(frames.front().size))));
    curr_buffer = current_transcrypt_task_->buffer.get();

    // We only copy this limited set of fields to match what we do in the
    // corresponding Decryptor implementation in:
    // chromeos::ContentDecryptionModuleAdapter::OnDecrypt
    current_transcrypt_task_->buffer->set_timestamp(superframe->timestamp());
    current_transcrypt_task_->buffer->set_duration(superframe->duration());
    current_transcrypt_task_->buffer->set_is_key_frame(
        superframe->is_key_frame());
    current_transcrypt_task_->buffer->set_side_data(superframe->side_data());
    if (frames.front().decrypt_config) {
      current_transcrypt_task_->buffer->set_decrypt_config(
          std::move(frames.front().decrypt_config));
    }
    frames.pop_front();

    // The last one in the queue should have the decode done callback and the
    // rest should be DoNothing.
    VideoDecoder::DecodeCB next_decode_done_cb;
    if (!frames.empty()) {
      next_decode_done_cb = std::move(current_transcrypt_task_->decode_done_cb);
      current_transcrypt_task_->decode_done_cb = base::DoNothing();
    }
    while (!frames.empty()) {
      // The |frames| are in decode order, so we take from the back of |frames|
      // and append to the front of |transcrypt_task_queue_|.
      scoped_refptr<DecoderBuffer> buffer = DecoderBuffer::CopyFrom(UNSAFE_TODO(
          base::span(frames.back().ptr.get(),
                     base::checked_cast<size_t>(frames.back().size))));
      buffer->set_timestamp(superframe->timestamp());
      buffer->set_duration(superframe->duration());
      buffer->set_is_key_frame(superframe->is_key_frame());
      buffer->set_side_data(superframe->side_data());
      if (frames.back().decrypt_config) {
        buffer->set_decrypt_config(std::move(frames.back().decrypt_config));
      }
      frames.pop_back();
      transcrypt_task_queue_.emplace_front(std::move(buffer),
                                           std::move(next_decode_done_cb));
      next_decode_done_cb = base::DoNothing();
    }
  }

  // If we've already attached a secure buffer, don't do it again.
  if (!curr_buffer->has_side_data() ||
      !curr_buffer->side_data()->secure_handle) {
    auto status =
        decoder_->AttachSecureBuffer(current_transcrypt_task_->buffer);
    if (status == CroStatus::Codes::kSecureBufferPoolEmpty) {
      // We are currently out of secure buffers, so wait until this gets invoked
      // again.
      return;
    } else if (!status.is_ok()) {
      LOG(ERROR) << "Failure in attaching secure buffer";
      OnBufferTranscrypted(Decryptor::kError, nullptr);
      return;
    }

    if (curr_buffer->has_side_data() &&
        curr_buffer->side_data()->secure_handle) {
      // Wrap the callback so we can release the secure buffer when decoding is
      // done.
      current_transcrypt_task_->decode_done_cb =
          base::BindOnce(&DecoderBufferTranscryptor::OnSecureBufferRelease,
                         weak_this_, curr_buffer->side_data()->secure_handle,
                         std::move(current_transcrypt_task_->decode_done_cb));
    }
  }
  transcrypt_pending_ = true;
  cdm_context_ref_->GetCdmContext()->GetDecryptor()->Decrypt(
      Decryptor::kVideo, current_transcrypt_task_->buffer,
      base::BindPostTaskToCurrentDefault(base::BindOnce(
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
    std::optional<TranscryptTask> temp_task =
        std::move(current_transcrypt_task_);
    current_transcrypt_task_ = std::nullopt;
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
  std::optional<TranscryptTask> temp_task = std::move(current_transcrypt_task_);
  current_transcrypt_task_ = std::nullopt;
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

void DecoderBufferTranscryptor::OnSecureBufferRelease(
    uint64_t secure_handle,
    VideoDecoder::DecodeCB decode_cb,
    DecoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  decoder_->ReleaseSecureBuffer(secure_handle);
  std::move(decode_cb).Run(status);
}

}  // namespace media
