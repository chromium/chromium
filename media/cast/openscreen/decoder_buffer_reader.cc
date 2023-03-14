// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/openscreen/decoder_buffer_reader.h"

#include "base/functional/bind.h"

namespace media::cast {

DecoderBufferReader::DecoderBufferReader(
    NewBufferCb new_buffer_cb,
    mojo::ScopedDataPipeConsumerHandle data_pipe)
    : new_buffer_cb_(std::move(new_buffer_cb)),
      mojo_buffer_reader_(std::move(data_pipe)),
      weak_factory_(this) {
  DCHECK(new_buffer_cb_);
}

DecoderBufferReader::DecoderBufferReader(
    DecoderBufferReader&& other,
    mojo::ScopedDataPipeConsumerHandle data_pipe)
    : DecoderBufferReader(std::move(other.new_buffer_cb_),
                          std::move(data_pipe)) {
  is_read_pending_ = other.is_read_pending_;
}

DecoderBufferReader::~DecoderBufferReader() = default;

void DecoderBufferReader::ProvideBuffer(media::mojom::DecoderBufferPtr buffer) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_buffer_metadata_.push_back(std::move(buffer));
  TryGetNextBuffer();
}

void DecoderBufferReader::ClearReadPending() {
  is_read_pending_ = false;
}

void DecoderBufferReader::ReadBufferAsync() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_read_pending_);

  is_read_pending_ = true;
  CompletePendingRead();
}

void DecoderBufferReader::CompletePendingRead() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_read_pending_ || !current_buffer_) {
    return;
  }

  is_read_pending_ = false;
  const bool is_eos = current_buffer_->end_of_stream();
  new_buffer_cb_.Run(std::move(current_buffer_));
  if (!is_eos) {
    TryGetNextBuffer();
  }
}

void DecoderBufferReader::TryGetNextBuffer() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (current_buffer_ || pending_buffer_metadata_.empty()) {
    return;
  }

  media::mojom::DecoderBufferPtr buffer =
      std::move(pending_buffer_metadata_.front());
  pending_buffer_metadata_.pop_front();
  mojo_buffer_reader_.ReadDecoderBuffer(
      std::move(buffer),
      base::BindOnce(&DecoderBufferReader::OnBufferReadFromDataPipe,
                     weak_factory_.GetWeakPtr()));
}

void DecoderBufferReader::OnBufferReadFromDataPipe(
    scoped_refptr<media::DecoderBuffer> buffer) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!current_buffer_);
  current_buffer_ = buffer;
  CompletePendingRead();
}

}  // namespace media::cast
