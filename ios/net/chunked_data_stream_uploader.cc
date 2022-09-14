// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/chunked_data_stream_uploader.h"

#include "base/check_op.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

ChunkedDataStreamUploader::ChunkedDataStreamUploader(Delegate* delegate)
    : UploadDataStream(true, 0),
      delegate_(delegate),
      pending_read_buffer_(nullptr),
      pending_read_buffer_length_(0),
      pending_internal_read_(false),
      is_final_chunk_(false),
      is_front_of_stream_(true),
      weak_factory_(this) {
  DCHECK(delegate_);
}

ChunkedDataStreamUploader::~ChunkedDataStreamUploader() {}

int ChunkedDataStreamUploader::InitInternal(const NetLogWithSource& net_log) {
  if (is_front_of_stream_)
    return OK;
  else
    return ERR_FAILED;
}

int ChunkedDataStreamUploader::ReadInternal(net::IOBuffer* buffer,
                                            int buffer_length) {
  DCHECK(buffer);
  DCHECK_GT(buffer_length, 0);
  DCHECK(!pending_read_buffer_);

  pending_read_buffer_ = buffer;
  pending_read_buffer_length_ = buffer_length;

  // Read the stream if input data comes first.
  return Upload();
}

void ChunkedDataStreamUploader::ResetInternal() {
  pending_read_buffer_ = nullptr;
  pending_read_buffer_length_ = 0;
  pending_internal_read_ = false;
  // Internal reset will not affect the external stream data state.
  is_final_chunk_ = false;
}

void ChunkedDataStreamUploader::UploadWhenReady(bool is_final_chunk) {
  is_final_chunk_ = is_final_chunk;

  // Put the data if internal read comes first.
  if (pending_internal_read_) {
    Upload();
  }
}

int ChunkedDataStreamUploader::Upload() {
  DCHECK(pending_read_buffer_);

  is_front_of_stream_ = false;
  int bytes_read = 0;

  if (is_final_chunk_) {
    SetIsFinalChunk();
  } else {
    bytes_read = delegate_->OnRead(pending_read_buffer_->data(),
                                   pending_read_buffer_length_);

    // NSInputStream can read 0 bytes when hasBytesAvailable is true, so ignore
    // this piece and let this internal read remain pending.
    // Still returns ERR_IO_PENDING for other errors because currently it is not
    // supported to return failure in UploadDataStream::Read(). Handle the
    // failure in the delegate level.
    if (bytes_read <= 0) {
      pending_internal_read_ = true;
      return ERR_IO_PENDING;
    }
  }

  pending_read_buffer_ = nullptr;
  pending_read_buffer_length_ = 0;

  // When there is a Read() pending, call OnReadCompleted to notify read
  // completed.
  if (pending_internal_read_) {
    pending_internal_read_ = false;
    OnReadCompleted(bytes_read);
  }
  return bytes_read;
}

}  // namespace net
