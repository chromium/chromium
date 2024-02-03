// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decrypting_video_decoder.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"

namespace media {

const char DecryptingVideoDecoder::kDecoderName[] = "DecryptingVideoDecoder";

DecryptingVideoDecoder::DecryptingVideoDecoder(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    MediaLog* media_log)
    : task_runner_(task_runner), media_log_(media_log) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VideoDecoderType DecryptingVideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kDecrypting;
}

void DecryptingVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                        bool /* low_delay */,
                                        CdmContext* cdm_context,
                                        InitCB init_cb,
                                        const OutputCB& output_cb,
                                        const WaitingCB& waiting_cb) {
  DVLOG(2) << __func__ << ": " << config.AsHumanReadableString();

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == kUninitialized || state_ == kIdle ||
         state_ == kDecodeFinished)
      << state_;
  DCHECK(!decode_cb_);
  DCHECK(!reset_cb_);
  DCHECK(config.IsValidConfig());

  init_cb_ = base::BindPostTaskToCurrentDefault(std::move(init_cb));

  if (!cdm_context) {
    // Once we have a CDM context, one should always be present.
    DCHECK(!support_clear_content_);
    std::move(init_cb_).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  if (!config.is_encrypted() && !support_clear_content_) {
    std::move(init_cb_).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  // Once initialized with encryption support, the value is sticky, so we'll use
  // the decryptor for clear content as well.
  support_clear_content_ = true;

  output_cb_ = base::BindPostTaskToCurrentDefault(output_cb);
  config_ = config;

  DCHECK(waiting_cb);
  waiting_cb_ = waiting_cb;

  if (state_ == kUninitialized) {
    if (!cdm_context->GetDecryptor()) {
      DVLOG(1) << __func__ << ": no decryptor";
      std::move(init_cb_).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
      return;
    }

    decryptor_ = cdm_context->GetDecryptor();
    event_cb_registration_ = cdm_context->RegisterEventCB(
        base::BindRepeating(&DecryptingVideoDecoder::OnCdmContextEvent,
                            weak_factory_.GetWeakPtr()));
  } else {
    // Reinitialization (i.e. upon a config change). The new config can be
    // encrypted or clear.
    decryptor_->DeinitializeDecoder(Decryptor::kVideo);
  }

  state_ = kPendingDecoderInit;
  decryptor_->InitializeVideoDecoder(
      config_, base::BindPostTaskToCurrentDefault(
                   base::BindOnce(&DecryptingVideoDecoder::FinishInitialization,
                                  weak_factory_.GetWeakPtr())));
}

bool DecryptingVideoDecoder::SupportsDecryption() const {
  return true;
}

void DecryptingVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                    DecodeCB decode_cb) {
  DVLOG(3) << "Decode()";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == kIdle || state_ == kDecodeFinished || state_ == kError)
      << state_;
  DCHECK(decode_cb);
  CHECK(!decode_cb_) << "Overlapping decodes are not supported.";

  decode_cb_ = base::BindPostTaskToCurrentDefault(std::move(decode_cb));

  if (state_ == kError) {
    std::move(decode_cb_).Run(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }

  // Return empty frames if decoding has finished.
  if (state_ == kDecodeFinished) {
    std::move(decode_cb_).Run(DecoderStatus::Codes::kOk);
    return;
  }

  // One time set of `has_clear_lead_`.
  if (!has_clear_lead_.has_value()) {
    has_clear_lead_ = !buffer->end_of_stream() && !buffer->decrypt_config();
  }

  // Although the stream may switch from clear to encrypted to clear multiple
  // times (e.g ad-insertions), we only log to the Media log the first switch
  // from clear to encrypted.
  if (HasClearLead() && !switched_clear_to_encrypted_ &&
      !buffer->end_of_stream() && buffer->is_encrypted()) {
    MEDIA_LOG(INFO, media_log_)
        << "First switch from clear to encrypted buffers.";
    switched_clear_to_encrypted_ = true;
  }

  pending_buffer_to_decode_ = std::move(buffer);
  state_ = kPendingDecode;
  DecodePendingBuffer();
}

void DecryptingVideoDecoder::Reset(base::OnceClosure closure) {
  DVLOG(2) << "Reset() - state: " << state_;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == kIdle || state_ == kPendingDecode ||
         state_ == kWaitingForKey || state_ == kDecodeFinished ||
         state_ == kError)
      << state_;
  DCHECK(!init_cb_);  // No Reset() during pending initialization.
  DCHECK(!reset_cb_);

  reset_cb_ = base::BindPostTaskToCurrentDefault(std::move(closure));

  decryptor_->ResetDecoder(Decryptor::kVideo);

  // Reset() cannot complete if the decode callback is still pending.
  // Defer the resetting process in this case. The |reset_cb_| will be fired
  // after the decode callback is fired - see DecryptAndDecodeBuffer() and
  // DeliverFrame().
  if (state_ == kPendingDecode) {
    DCHECK(decode_cb_);
    return;
  }

  if (state_ == kWaitingForKey) {
    CompleteWaitingForDecryptionKey();
    DCHECK(decode_cb_);
    pending_buffer_to_decode_.reset();
    std::move(decode_cb_).Run(DecoderStatus::Codes::kAborted);
  }

  DCHECK(!decode_cb_);
  DoReset();
}

DecryptingVideoDecoder::~DecryptingVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == kUninitialized)
    return;

  if (state_ == kWaitingForKey)
    CompleteWaitingForDecryptionKey();
  if (state_ == kPendingDecode)
    CompletePendingDecode(Decryptor::kError);

  if (decryptor_) {
    decryptor_->DeinitializeDecoder(Decryptor::kVideo);
    decryptor_ = nullptr;
  }
  pending_buffer_to_decode_.reset();
  if (init_cb_)
    std::move(init_cb_).Run(DecoderStatus::Codes::kInterrupted);
  if (decode_cb_)
    std::move(decode_cb_).Run(DecoderStatus::Codes::kAborted);
  if (reset_cb_)
    std::move(reset_cb_).Run();
}

void DecryptingVideoDecoder::FinishInitialization(bool success) {
  DVLOG(2) << "FinishInitialization()";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, kPendingDecoderInit) << state_;
  DCHECK(init_cb_);
  DCHECK(!reset_cb_);   // No Reset() before initialization finished.
  DCHECK(!decode_cb_);  // No Decode() before initialization finished.

  if (!success) {
    DVLOG(1) << __func__ << ": failed to init video decoder on decryptor";
    // TODO(*) Is there a better reason? Should this method itself take a
    // status?
    std::move(init_cb_).Run(DecoderStatus::Codes::kFailed);
    decryptor_ = nullptr;
    event_cb_registration_.reset();
    state_ = kError;
    return;
  }

  // Success!
  state_ = kIdle;
  std::move(init_cb_).Run(DecoderStatus::Codes::kOk);
}

void DecryptingVideoDecoder::DecodePendingBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, kPendingDecode) << state_;

  auto& buffer = pending_buffer_to_decode_;

  // Note: Traces require a unique ID per decode, if we ever support multiple
  // in flight decodes, the trace begin+end macros need the same unique id.
  DCHECK_EQ(GetMaxDecodeRequests(), 1);
  const bool is_end_of_stream = buffer->end_of_stream();
  const bool is_encrypted = !is_end_of_stream && buffer->decrypt_config();
  const auto timestamp_us =
      is_end_of_stream ? 0 : buffer->timestamp().InMicroseconds();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
      "media", "DecryptingVideoDecoder::DecodePendingBuffer", this,
      "is_encrypted", is_encrypted, "timestamp_us", timestamp_us);

  if (!DecoderBuffer::DoSubsamplesMatch(*buffer)) {
    MEDIA_LOG(ERROR, media_log_)
        << "DecryptingVideoDecoder: Subsamples for Buffer do not match";
    state_ = kError;
    std::move(decode_cb_).Run(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }

  decryptor_->DecryptAndDecodeVideo(
      buffer,
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &DecryptingVideoDecoder::DeliverFrame, weak_factory_.GetWeakPtr())));
}

void DecryptingVideoDecoder::DeliverFrame(Decryptor::Status status,
                                          scoped_refptr<VideoFrame> frame) {
  DVLOG(3) << "DeliverFrame() - status: " << status;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, kPendingDecode) << state_;
  DCHECK(decode_cb_);
  DCHECK(pending_buffer_to_decode_.get());
  CompletePendingDecode(status);

  bool need_to_try_again_if_nokey_is_returned = key_added_while_decode_pending_;
  key_added_while_decode_pending_ = false;

  scoped_refptr<DecoderBuffer> scoped_pending_buffer_to_decode =
      std::move(pending_buffer_to_decode_);

  if (reset_cb_) {
    std::move(decode_cb_).Run(DecoderStatus::Codes::kAborted);
    DoReset();
    return;
  }

  DCHECK_EQ(status == Decryptor::kSuccess, frame.get() != nullptr);

  if (status == Decryptor::kError) {
    DVLOG(2) << "DeliverFrame() - kError";
    MEDIA_LOG(ERROR, media_log_) << GetDecoderType() << ": decode error";
    state_ = kError;
    std::move(decode_cb_).Run(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }

  if (status == Decryptor::kNoKey) {
    std::string key_id =
        scoped_pending_buffer_to_decode->decrypt_config()->key_id();
    std::string log_message =
        "no key for key ID " + base::HexEncode(key_id) +
        "; will resume decoding after new usable key is available";
    DVLOG(1) << __func__ << ": " << log_message;
    MEDIA_LOG(INFO, media_log_) << GetDecoderType() << ": " << log_message;

    // Set |pending_buffer_to_decode_| back as we need to try decoding the
    // pending buffer again when new key is added to the decryptor.
    pending_buffer_to_decode_ = std::move(scoped_pending_buffer_to_decode);

    if (need_to_try_again_if_nokey_is_returned) {
      // The |state_| is still kPendingDecode.
      MEDIA_LOG(INFO, media_log_)
          << GetDecoderType() << ": key was added, resuming decode";
      DecodePendingBuffer();
      return;
    }

    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        "media", "DecryptingVideoDecoder::WaitingForDecryptionKey", this);
    state_ = kWaitingForKey;
    waiting_cb_.Run(WaitingReason::kNoDecryptionKey);
    return;
  }

  if (status == Decryptor::kNeedMoreData) {
    DVLOG(2) << "DeliverFrame() - kNeedMoreData";
    state_ = scoped_pending_buffer_to_decode->end_of_stream() ? kDecodeFinished
                                                              : kIdle;
    std::move(decode_cb_).Run(DecoderStatus::Codes::kOk);
    return;
  }

  DCHECK_EQ(status, Decryptor::kSuccess);
  CHECK(frame);

  // Frame returned with kSuccess should not be an end-of-stream frame.
  DCHECK(!frame->metadata().end_of_stream);

  // If color space is not set, use the color space in the |config_|.
  if (!frame->ColorSpace().IsValid()) {
    DVLOG(3) << "Setting color space using information in the config.";
    if (config_.color_space_info().IsSpecified())
      frame->set_color_space(config_.color_space_info().ToGfxColorSpace());
  }

  // Attach the HDR metadata from the `config_` if it's not set on the `frame`.
  if (!frame->hdr_metadata() && config_.hdr_metadata()) {
    frame->set_hdr_metadata(config_.hdr_metadata());
  }

  output_cb_.Run(std::move(frame));

  if (scoped_pending_buffer_to_decode->end_of_stream()) {
    // Set |pending_buffer_to_decode_| back as we need to keep flushing the
    // decryptor.
    pending_buffer_to_decode_ = std::move(scoped_pending_buffer_to_decode);
    DecodePendingBuffer();
    return;
  }

  state_ = kIdle;
  std::move(decode_cb_).Run(DecoderStatus::Codes::kOk);
}

void DecryptingVideoDecoder::OnCdmContextEvent(CdmContext::Event event) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (event != CdmContext::Event::kHasAdditionalUsableKey)
    return;

  if (state_ == kPendingDecode) {
    key_added_while_decode_pending_ = true;
    return;
  }

  if (state_ == kWaitingForKey) {
    CompleteWaitingForDecryptionKey();
    MEDIA_LOG(INFO, media_log_)
        << GetDecoderType() << ": key added, resuming decode";
    state_ = kPendingDecode;
    DecodePendingBuffer();
  }
}

void DecryptingVideoDecoder::DoReset() {
  DCHECK(!init_cb_);
  DCHECK(!decode_cb_);
  state_ = kIdle;
  std::move(reset_cb_).Run();
}

void DecryptingVideoDecoder::CompletePendingDecode(Decryptor::Status status) {
  DCHECK_EQ(state_, kPendingDecode);
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "media", "DecryptingVideoDecoder::DecodePendingBuffer", this, "status",
      Decryptor::GetStatusName(status));
}

void DecryptingVideoDecoder::CompleteWaitingForDecryptionKey() {
  DCHECK_EQ(state_, kWaitingForKey);
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "media", "DecryptingVideoDecoder::WaitingForDecryptionKey", this);
}

}  // namespace media
