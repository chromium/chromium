// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decrypting_demuxer_stream.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"

namespace media {

static bool IsStreamValid(DemuxerStream* stream) {
  return ((stream->type() == DemuxerStream::AUDIO &&
           stream->audio_decoder_config().IsValidConfig()) ||
          (stream->type() == DemuxerStream::VIDEO &&
           stream->video_decoder_config().IsValidConfig()));
}

DecryptingDemuxerStream::DecryptingDemuxerStream(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    MediaLog* media_log,
    const WaitingCB& waiting_cb)
    : task_runner_(task_runner),
      media_log_(media_log),
      waiting_cb_(waiting_cb) {}

std::string DecryptingDemuxerStream::GetDisplayName() const {
  return "DecryptingDemuxerStream";
}

void DecryptingDemuxerStream::Initialize(DemuxerStream* stream,
                                         CdmContext* cdm_context,
                                         const PipelineStatusCB& status_cb) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kUninitialized) << state_;
  DCHECK(stream);
  DCHECK(cdm_context);
  DCHECK(!demuxer_stream_);

  weak_this_ = weak_factory_.GetWeakPtr();
  demuxer_stream_ = stream;
  init_cb_ = BindToCurrentLoop(status_cb);

  InitializeDecoderConfig();

  if (!cdm_context->GetDecryptor()) {
    DVLOG(1) << __func__ << ": no decryptor";
    state_ = kUninitialized;
    std::move(init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  decryptor_ = cdm_context->GetDecryptor();

  decryptor_->RegisterNewKeyCB(
      GetDecryptorStreamType(),
      BindToCurrentLoop(
          base::Bind(&DecryptingDemuxerStream::OnKeyAdded, weak_this_)));

  state_ = kIdle;
  std::move(init_cb_).Run(PIPELINE_OK);
}

void DecryptingDemuxerStream::Read(ReadCB read_cb) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kIdle) << state_;
  DCHECK(read_cb);
  CHECK(!read_cb_) << "Overlapping reads are not supported.";

  read_cb_ = BindToCurrentLoop(std::move(read_cb));
  state_ = kPendingDemuxerRead;
  demuxer_stream_->Read(base::BindOnce(
      &DecryptingDemuxerStream::OnBufferReadFromDemuxerStream, weak_this_));
}

bool DecryptingDemuxerStream::IsReadPending() const {
  return !read_cb_.is_null();
}

void DecryptingDemuxerStream::Reset(const base::Closure& closure) {
  DVLOG(2) << __func__ << " - state: " << state_;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ != kUninitialized) << state_;
  DCHECK(!reset_cb_);

  reset_cb_ = BindToCurrentLoop(closure);

  decryptor_->CancelDecrypt(GetDecryptorStreamType());

  // Reset() cannot complete if the read callback is still pending.
  // Defer the resetting process in this case. The |reset_cb_| will be fired
  // after the read callback is fired - see OnBufferReadFromDemuxerStream() and
  // OnBufferDecrypted().
  if (state_ == kPendingDemuxerRead || state_ == kPendingDecrypt) {
    DCHECK(read_cb_);
    return;
  }

  if (state_ == kWaitingForKey) {
    CompleteWaitingForDecryptionKey();
    DCHECK(read_cb_);
    pending_buffer_to_decrypt_ = nullptr;
    std::move(read_cb_).Run(kAborted, nullptr);
  }

  DCHECK(!read_cb_);
  DoReset();
}

AudioDecoderConfig DecryptingDemuxerStream::audio_decoder_config() {
  DCHECK(state_ != kUninitialized) << state_;
  CHECK_EQ(demuxer_stream_->type(), AUDIO);
  return audio_config_;
}

VideoDecoderConfig DecryptingDemuxerStream::video_decoder_config() {
  DCHECK(state_ != kUninitialized) << state_;
  CHECK_EQ(demuxer_stream_->type(), VIDEO);
  return video_config_;
}

DemuxerStream::Type DecryptingDemuxerStream::type() const {
  DCHECK(state_ != kUninitialized) << state_;
  return demuxer_stream_->type();
}

DemuxerStream::Liveness DecryptingDemuxerStream::liveness() const {
  DCHECK(state_ != kUninitialized) << state_;
  return demuxer_stream_->liveness();
}

void DecryptingDemuxerStream::EnableBitstreamConverter() {
  demuxer_stream_->EnableBitstreamConverter();
}

bool DecryptingDemuxerStream::SupportsConfigChanges() {
  return demuxer_stream_->SupportsConfigChanges();
}

DecryptingDemuxerStream::~DecryptingDemuxerStream() {
  DVLOG(2) << __func__ << " : state_ = " << state_;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == kUninitialized)
    return;

  if (state_ == kWaitingForKey)
    CompleteWaitingForDecryptionKey();
  if (state_ == kPendingDecrypt)
    CompletePendingDecrypt(Decryptor::kError);

  if (decryptor_) {
    decryptor_->CancelDecrypt(GetDecryptorStreamType());
    decryptor_ = nullptr;
  }
  if (init_cb_)
    std::move(init_cb_).Run(PIPELINE_ERROR_ABORT);
  if (read_cb_)
    std::move(read_cb_).Run(kAborted, nullptr);
  if (reset_cb_)
    std::move(reset_cb_).Run();
  pending_buffer_to_decrypt_ = nullptr;
}

void DecryptingDemuxerStream::OnBufferReadFromDemuxerStream(
    DemuxerStream::Status status,
    scoped_refptr<DecoderBuffer> buffer) {
  DVLOG(3) << __func__ << ": status = " << status;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDemuxerRead) << state_;
  DCHECK(read_cb_);
  DCHECK_EQ(buffer.get() != nullptr, status == kOk) << status;

  // Even when |reset_cb_|, we need to pass |kConfigChanged| back to
  // the caller so that the downstream decoder can be properly reinitialized.
  if (status == kConfigChanged) {
    DVLOG(2) << __func__ << ": config change";
    DCHECK_EQ(demuxer_stream_->type() == AUDIO, audio_config_.IsValidConfig());
    DCHECK_EQ(demuxer_stream_->type() == VIDEO, video_config_.IsValidConfig());

    // Update the decoder config, which the decoder will use when it is notified
    // of kConfigChanged.
    InitializeDecoderConfig();

    state_ = kIdle;
    std::move(read_cb_).Run(kConfigChanged, nullptr);
    if (reset_cb_)
      DoReset();
    return;
  }

  if (reset_cb_) {
    std::move(read_cb_).Run(kAborted, nullptr);
    DoReset();
    return;
  }

  if (status == kAborted || status == kError) {
    if (status == kError) {
      MEDIA_LOG(ERROR, media_log_)
          << GetDisplayName() << ": demuxer stream read error.";
    }
    state_ = kIdle;
    std::move(read_cb_).Run(status, nullptr);
    return;
  }

  DCHECK_EQ(kOk, status);

  if (buffer->end_of_stream()) {
    DVLOG(2) << __func__ << ": EOS buffer";
    state_ = kIdle;
    std::move(read_cb_).Run(kOk, std::move(buffer));
    return;
  }

  if (!buffer->decrypt_config()) {
    DVLOG(2) << __func__ << ": clear buffer";
    state_ = kIdle;
    std::move(read_cb_).Run(kOk, std::move(buffer));
    return;
  }

  pending_buffer_to_decrypt_ = std::move(buffer);
  state_ = kPendingDecrypt;
  DecryptPendingBuffer();
}

void DecryptingDemuxerStream::DecryptPendingBuffer() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecrypt) << state_;
  DCHECK(!pending_buffer_to_decrypt_->end_of_stream());
  TRACE_EVENT_ASYNC_BEGIN2(
      "media", "DecryptingDemuxerStream::DecryptPendingBuffer", this, "type",
      DemuxerStream::GetTypeName(demuxer_stream_->type()), "timestamp_us",
      pending_buffer_to_decrypt_->timestamp().InMicroseconds());
  decryptor_->Decrypt(
      GetDecryptorStreamType(), pending_buffer_to_decrypt_,
      BindToCurrentLoop(
          base::Bind(&DecryptingDemuxerStream::OnBufferDecrypted, weak_this_)));
}

void DecryptingDemuxerStream::OnBufferDecrypted(
    Decryptor::Status status,
    scoped_refptr<DecoderBuffer> decrypted_buffer) {
  DVLOG(3) << __func__ << " - status: " << status;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecrypt) << state_;
  DCHECK(read_cb_);
  DCHECK(pending_buffer_to_decrypt_);
  CompletePendingDecrypt(status);

  bool need_to_try_again_if_nokey = key_added_while_decrypt_pending_;
  key_added_while_decrypt_pending_ = false;

  if (reset_cb_) {
    pending_buffer_to_decrypt_ = nullptr;
    std::move(read_cb_).Run(kAborted, nullptr);
    DoReset();
    return;
  }

  DCHECK_EQ(status == Decryptor::kSuccess, decrypted_buffer.get() != nullptr);

  if (status == Decryptor::kError || status == Decryptor::kNeedMoreData) {
    DVLOG(2) << __func__ << ": Error with status " << status;
    MEDIA_LOG(ERROR, media_log_)
        << GetDisplayName() << ": decrypt error " << status;
    pending_buffer_to_decrypt_ = nullptr;
    state_ = kIdle;
    std::move(read_cb_).Run(kError, nullptr);
    return;
  }

  if (status == Decryptor::kNoKey) {
    std::string key_id = pending_buffer_to_decrypt_->decrypt_config()->key_id();

    std::string log_message =
        "no key for key ID " + base::HexEncode(key_id.data(), key_id.size()) +
        "; will resume decrypting after new usable key is available";
    DVLOG(1) << __func__ << ": " << log_message;
    MEDIA_LOG(INFO, media_log_) << GetDisplayName() << ": " << log_message;

    if (need_to_try_again_if_nokey) {
      // The |state_| is still kPendingDecrypt.
      MEDIA_LOG(INFO, media_log_)
          << GetDisplayName() << ": key was added, resuming decrypt";
      DecryptPendingBuffer();
      return;
    }

    state_ = kWaitingForKey;

    TRACE_EVENT_ASYNC_BEGIN0(
        "media", "DecryptingDemuxerStream::WaitingForDecryptionKey", this);
    waiting_cb_.Run(WaitingReason::kNoDecryptionKey);
    return;
  }

  DCHECK_EQ(status, Decryptor::kSuccess);

  // Copy the key frame flag from the encrypted to decrypted buffer, assuming
  // that the decryptor initialized the flag to false.
  if (pending_buffer_to_decrypt_->is_key_frame())
    decrypted_buffer->set_is_key_frame(true);

  pending_buffer_to_decrypt_ = nullptr;
  state_ = kIdle;
  std::move(read_cb_).Run(kOk, std::move(decrypted_buffer));
}

void DecryptingDemuxerStream::OnKeyAdded() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == kPendingDecrypt) {
    key_added_while_decrypt_pending_ = true;
    return;
  }

  // Nothing to do.
  if (state_ != kWaitingForKey)
    return;

  CompleteWaitingForDecryptionKey();
  MEDIA_LOG(INFO, media_log_)
      << GetDisplayName() << ": key was added, resuming decrypt";
  state_ = kPendingDecrypt;
  DecryptPendingBuffer();
}

void DecryptingDemuxerStream::DoReset() {
  DCHECK(state_ != kUninitialized);
  DCHECK(!init_cb_);
  DCHECK(!read_cb_);

  state_ = kIdle;
  std::move(reset_cb_).Run();
}

Decryptor::StreamType DecryptingDemuxerStream::GetDecryptorStreamType() const {
  if (demuxer_stream_->type() == AUDIO)
    return Decryptor::kAudio;

  DCHECK_EQ(demuxer_stream_->type(), VIDEO);
  return Decryptor::kVideo;
}

void DecryptingDemuxerStream::InitializeDecoderConfig() {
  // The decoder selector or upstream demuxer make sure the stream is valid.
  DCHECK(IsStreamValid(demuxer_stream_));

  // Since |this| is a decrypted version of |demuxer_stream_|, the decoder
  // config of |this| should always be a decrypted version of |demuxer_stream_|
  // configs.
  switch (demuxer_stream_->type()) {
    case AUDIO: {
      audio_config_ = demuxer_stream_->audio_decoder_config();
      if (audio_config_.is_encrypted())
        audio_config_.SetIsEncrypted(false);
      break;
    }

    case VIDEO: {
      video_config_ = demuxer_stream_->video_decoder_config();
      if (video_config_.is_encrypted())
        video_config_.SetIsEncrypted(false);
      break;
    }

    default:
      NOTREACHED();
      return;
  }
}

void DecryptingDemuxerStream::CompletePendingDecrypt(Decryptor::Status status) {
  DCHECK_EQ(state_, kPendingDecrypt);
  TRACE_EVENT_ASYNC_END1("media",
                         "DecryptingDemuxerStream::DecryptPendingBuffer", this,
                         "status", Decryptor::GetStatusName(status));
}

void DecryptingDemuxerStream::CompleteWaitingForDecryptionKey() {
  DCHECK_EQ(state_, kWaitingForKey);
  TRACE_EVENT_ASYNC_END0(
      "media", "DecryptingDemuxerStream::WaitingForDecryptionKey", this);
}

}  // namespace media
