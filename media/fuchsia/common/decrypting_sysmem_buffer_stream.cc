// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/common/decrypting_sysmem_buffer_stream.h"

#include "media/base/callback_registry.h"
#include "media/base/decoder_buffer.h"

namespace media {

DecryptingSysmemBufferStream::DecryptingSysmemBufferStream(
    SysmemAllocatorClient* sysmem_allocator,
    CdmContext* cdm_context,
    Decryptor::StreamType stream_type)
    : passthrough_stream_(sysmem_allocator),
      decryptor_(cdm_context->GetDecryptor()),
      stream_type_(stream_type) {
  DCHECK(decryptor_);

  event_cb_registration_ = cdm_context->RegisterEventCB(
      base::BindRepeating(&DecryptingSysmemBufferStream::OnCdmContextEvent,
                          weak_factory_.GetWeakPtr()));
}

DecryptingSysmemBufferStream::~DecryptingSysmemBufferStream() = default;

void DecryptingSysmemBufferStream::Initialize(Sink* sink,
                                              size_t min_buffer_size,
                                              size_t min_buffer_count) {
  sink_ = sink;
  passthrough_stream_.Initialize(sink, min_buffer_size, min_buffer_count);
}

void DecryptingSysmemBufferStream::EnqueueBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  buffer_queue_.push_back(std::move(buffer));
  DecryptNextBuffer();
}

void DecryptingSysmemBufferStream::Reset() {
  buffer_queue_.clear();

  if (state_ == State::kDecryptPending) {
    decryptor_->CancelDecrypt(stream_type_);
  }

  state_ = State::kIdle;
  retry_on_no_key_ = false;
}

void DecryptingSysmemBufferStream::OnCdmContextEvent(CdmContext::Event event) {
  if (event != CdmContext::Event::kHasAdditionalUsableKey)
    return;

  switch (state_) {
    case State::kIdle:
      break;

    case State::kDecryptPending:
      retry_on_no_key_ = true;
      break;

    case State::kWaitingKey:
      state_ = State::kIdle;
      DecryptNextBuffer();
      break;
  }
}

void DecryptingSysmemBufferStream::DecryptNextBuffer() {
  if (buffer_queue_.empty() || state_ != State::kIdle)
    return;

  if (buffer_queue_.front()->end_of_stream()) {
    scoped_refptr<DecoderBuffer> buffer = std::move(buffer_queue_.front());
    buffer_queue_.pop_front();
    DCHECK(buffer_queue_.empty());
    passthrough_stream_.EnqueueBuffer(std::move(buffer));
    return;
  }

  state_ = State::kDecryptPending;
  decryptor_->Decrypt(
      stream_type_, buffer_queue_.front(),
      base::BindOnce(&DecryptingSysmemBufferStream::OnBufferDecrypted,
                     weak_factory_.GetWeakPtr()));
}

void DecryptingSysmemBufferStream::OnBufferDecrypted(
    Decryptor::Status status,
    scoped_refptr<DecoderBuffer> decrypted_buffer) {
  DCHECK(state_ == State::kDecryptPending);
  state_ = State::kIdle;

  switch (status) {
    case Decryptor::kError:
      sink_->OnSysmemBufferStreamError();
      return;

    case Decryptor::kNoKey:
      if (retry_on_no_key_) {
        retry_on_no_key_ = false;
        DecryptNextBuffer();
      } else {
        state_ = State::kWaitingKey;
        sink_->OnSysmemBufferStreamNoKey();
      }
      return;

    case Decryptor::kNeedMoreData:
      break;

    case Decryptor::kSuccess:
      passthrough_stream_.EnqueueBuffer(std::move(decrypted_buffer));
  }

  buffer_queue_.pop_front();

  DecryptNextBuffer();
}

}  // namespace media
