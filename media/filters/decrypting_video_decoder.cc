// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decrypting_video_decoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"

namespace media {

const char DecryptingVideoDecoder::kDecoderName[] = "DecryptingVideoDecoder";

DecryptingVideoDecoder::DecryptingVideoDecoder(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    MediaLog* media_log)
    : task_runner_(task_runner), media_log_(media_log) {}

std::string DecryptingVideoDecoder::GetDisplayName() const {
  return kDecoderName;
}

void DecryptingVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                        bool /* low_delay */,
                                        CdmContext* cdm_context,
                                        InitCB init_cb,
                                        const OutputCB& output_cb,
                                        const WaitingCB& waiting_cb) {
  DVLOG(2) << __func__ << ": " << config.AsHumanReadableString();

  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kUninitialized || state_ == kIdle ||
         state_ == kDecodeFinished)
      << state_;
  DCHECK(!decode_cb_);
  DCHECK(!reset_cb_);
  DCHECK(config.IsValidConfig());

  init_cb_ = BindToCurrentLoop(std::move(init_cb));
  if (!cdm_context) {
    // Once we have a CDM context, one should always be present.
    DCHECK(!support_clear_content_);
    std::move(init_cb_).Run(false);
    return;
  }

  if (!config.is_encrypted() && !support_clear_content_) {
    std::move(init_cb_).Run(false);
    return;
  }

  // Once initialized with encryption support, the value is sticky, so we'll use
  // the decryptor for clear content as well.
  support_clear_content_ = true;

  output_cb_ = BindToCurrentLoop(output_cb);
  weak_this_ = weak_factory_.GetWeakPtr();
  config_ = config;

  DCHECK(waiting_cb);
  waiting_cb_ = waiting_cb;

  if (state_ == kUninitialized) {
    if (!cdm_context->GetDecryptor()) {
      DVLOG(1) << __func__ << ": no decryptor";
      std::move(init_cb_).Run(false);
      return;
    }

    decryptor_ = cdm_context->GetDecryptor();
  } else {
    // Reinitialization (i.e. upon a config change). The new config can be
    // encrypted or clear.
    decryptor_->DeinitializeDecoder(Decryptor::kVideo);
  }

  state_ = kPendingDecoderInit;
  decryptor_->InitializeVideoDecoder(
      config_, BindToCurrentLoop(base::Bind(
                   &DecryptingVideoDecoder::FinishInitialization, weak_this_)));
}

void DecryptingVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                    DecodeCB decode_cb) {
  DVLOG(3) << "Decode()";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kIdle || state_ == kDecodeFinished || state_ == kError)
      << state_;
  DCHECK(decode_cb);
  CHECK(!decode_cb_) << "Overlapping decodes are not supported.";

  decode_cb_ = BindToCurrentLoop(std::move(decode_cb));

  if (state_ == kError) {
    std::move(decode_cb_).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  // Return empty frames if decoding has finished.
  if (state_ == kDecodeFinished) {
    std::move(decode_cb_).Run(DecodeStatus::OK);
    return;
  }

  pending_buffer_to_decode_ = std::move(buffer);
  state_ = kPendingDecode;
  DecodePendingBuffer();
}

void DecryptingVideoDecoder::Reset(base::OnceClosure closure) {
  DVLOG(2) << "Reset() - state: " << state_;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kIdle || state_ == kPendingDecode ||
         state_ == kWaitingForKey || state_ == kDecodeFinished ||
         state_ == kError)
      << state_;
  DCHECK(!init_cb_);  // No Reset() during pending initialization.
  DCHECK(!reset_cb_);

  reset_cb_ = BindToCurrentLoop(std::move(closure));

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
    std::move(decode_cb_).Run(DecodeStatus::ABORTED);
  }

  DCHECK(!decode_cb_);
  DoReset();
}

DecryptingVideoDecoder::~DecryptingVideoDecoder() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == kUninitialized)
    return;

  if (state_ == kWaitingForKey)
    CompleteWaitingForDecryptionKey();
  if (state_ == kPendingDecode)
    CompletePendingDecode(Decryptor::kError);

  if (decryptor_) {
    decryptor_->DeinitializeDecoder(Decryptor::kVideo);
    decryptor_ = NULL;
  }
  pending_buffer_to_decode_.reset();
  if (init_cb_)
    std::move(init_cb_).Run(false);
  if (decode_cb_)
    std::move(decode_cb_).Run(DecodeStatus::ABORTED);
  if (reset_cb_)
    std::move(reset_cb_).Run();
}

void DecryptingVideoDecoder::FinishInitialization(bool success) {
  DVLOG(2) << "FinishInitialization()";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecoderInit) << state_;
  DCHECK(init_cb_);
  DCHECK(!reset_cb_);   // No Reset() before initialization finished.
  DCHECK(!decode_cb_);  // No Decode() before initialization finished.

  if (!success) {
    DVLOG(1) << __func__ << ": failed to init video decoder on decryptor";
    std::move(init_cb_).Run(false);
    decryptor_ = NULL;
    state_ = kError;
    return;
  }

  decryptor_->RegisterNewKeyCB(
      Decryptor::kVideo, BindToCurrentLoop(base::Bind(
                             &DecryptingVideoDecoder::OnKeyAdded, weak_this_)));

  // Success!
  state_ = kIdle;
  std::move(init_cb_).Run(true);
}

void DecryptingVideoDecoder::DecodePendingBuffer() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecode) << state_;

  // Note: Traces require a unique ID per decode, if we ever support multiple
  // in flight decodes, the trace begin+end macros need the same unique id.
  DCHECK_EQ(GetMaxDecodeRequests(), 1);
  TRACE_EVENT_ASYNC_BEGIN1(
      "media", "DecryptingVideoDecoder::DecodePendingBuffer", this,
      "timestamp_us",
      pending_buffer_to_decode_->end_of_stream()
          ? 0
          : pending_buffer_to_decode_->timestamp().InMicroseconds());

  decryptor_->DecryptAndDecodeVideo(
      pending_buffer_to_decode_,
      BindToCurrentLoop(base::BindRepeating(
          &DecryptingVideoDecoder::DeliverFrame, weak_this_)));
}

void DecryptingVideoDecoder::DeliverFrame(Decryptor::Status status,
                                          scoped_refptr<VideoFrame> frame) {
  DVLOG(3) << "DeliverFrame() - status: " << status;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecode) << state_;
  DCHECK(decode_cb_);
  DCHECK(pending_buffer_to_decode_.get());
  CompletePendingDecode(status);

  bool need_to_try_again_if_nokey_is_returned = key_added_while_decode_pending_;
  key_added_while_decode_pending_ = false;

  scoped_refptr<DecoderBuffer> scoped_pending_buffer_to_decode =
      std::move(pending_buffer_to_decode_);

  if (reset_cb_) {
    std::move(decode_cb_).Run(DecodeStatus::ABORTED);
    DoReset();
    return;
  }

  DCHECK_EQ(status == Decryptor::kSuccess, frame.get() != NULL);

  if (status == Decryptor::kError) {
    DVLOG(2) << "DeliverFrame() - kError";
    MEDIA_LOG(ERROR, media_log_) << GetDisplayName() << ": decode error";
    state_ = kError;
    std::move(decode_cb_).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  if (status == Decryptor::kNoKey) {
    std::string key_id =
        scoped_pending_buffer_to_decode->decrypt_config()->key_id();
    std::string log_message =
        "no key for key ID " + base::HexEncode(key_id.data(), key_id.size()) +
        "; will resume decoding after new usable key is available";
    DVLOG(1) << __func__ << ": " << log_message;
    MEDIA_LOG(INFO, media_log_) << GetDisplayName() << ": " << log_message;

    // Set |pending_buffer_to_decode_| back as we need to try decoding the
    // pending buffer again when new key is added to the decryptor.
    pending_buffer_to_decode_ = std::move(scoped_pending_buffer_to_decode);

    if (need_to_try_again_if_nokey_is_returned) {
      // The |state_| is still kPendingDecode.
      MEDIA_LOG(INFO, media_log_)
          << GetDisplayName() << ": key was added, resuming decode";
      DecodePendingBuffer();
      return;
    }

    TRACE_EVENT_ASYNC_BEGIN0(
        "media", "DecryptingVideoDecoder::WaitingForDecryptionKey", this);
    state_ = kWaitingForKey;
    waiting_cb_.Run(WaitingReason::kNoDecryptionKey);
    return;
  }

  if (status == Decryptor::kNeedMoreData) {
    DVLOG(2) << "DeliverFrame() - kNeedMoreData";
    state_ = scoped_pending_buffer_to_decode->end_of_stream() ? kDecodeFinished
                                                              : kIdle;
    std::move(decode_cb_).Run(DecodeStatus::OK);
    return;
  }

  DCHECK_EQ(status, Decryptor::kSuccess);
  CHECK(frame);

  // Frame returned with kSuccess should not be an end-of-stream frame.
  DCHECK(!frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));

  // If color space is not set, use the color space in the |config_|.
  if (!frame->ColorSpace().IsValid()) {
    DVLOG(3) << "Setting color space using information in the config.";
    if (config_.color_space_info().IsSpecified())
      frame->set_color_space(config_.color_space_info().ToGfxColorSpace());
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
  std::move(decode_cb_).Run(DecodeStatus::OK);
}

void DecryptingVideoDecoder::OnKeyAdded() {
  DVLOG(2) << "OnKeyAdded()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == kPendingDecode) {
    key_added_while_decode_pending_ = true;
    return;
  }

  if (state_ == kWaitingForKey) {
    CompleteWaitingForDecryptionKey();
    MEDIA_LOG(INFO, media_log_)
        << GetDisplayName() << ": key added, resuming decode";
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
  TRACE_EVENT_ASYNC_END1("media", "DecryptingVideoDecoder::DecodePendingBuffer",
                         this, "status", Decryptor::GetStatusName(status));
}

void DecryptingVideoDecoder::CompleteWaitingForDecryptionKey() {
  DCHECK_EQ(state_, kWaitingForKey);
  TRACE_EVENT_ASYNC_END0(
      "media", "DecryptingVideoDecoder::WaitingForDecryptionKey", this);
}

}  // namespace media
