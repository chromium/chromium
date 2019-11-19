// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/chunked_data_pipe_upload_data_stream.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "mojo/public/c/system/types.h"
#include "net/base/io_buffer.h"

namespace network {

ChunkedDataPipeUploadDataStream::ChunkedDataPipeUploadDataStream(
    scoped_refptr<ResourceRequestBody> resource_request_body,
    mojo::PendingRemote<mojom::ChunkedDataPipeGetter> chunked_data_pipe_getter)
    : net::UploadDataStream(true /* is_chunked */,
                            resource_request_body->identifier()),
      resource_request_body_(std::move(resource_request_body)),
      chunked_data_pipe_getter_(std::move(chunked_data_pipe_getter)),
      handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                      base::SequencedTaskRunnerHandle::Get()) {
  chunked_data_pipe_getter_.set_disconnect_handler(
      base::BindOnce(&ChunkedDataPipeUploadDataStream::OnDataPipeGetterClosed,
                     base::Unretained(this)));
  chunked_data_pipe_getter_->GetSize(
      base::BindOnce(&ChunkedDataPipeUploadDataStream::OnSizeReceived,
                     base::Unretained(this)));
}

ChunkedDataPipeUploadDataStream::~ChunkedDataPipeUploadDataStream() {}

int ChunkedDataPipeUploadDataStream::InitInternal(
    const net::NetLogWithSource& net_log) {
  // If there was an error either passed to the ReadCallback or as a result of
  // closing the DataPipeGetter pipe, fail the read.
  if (status_ != net::OK)
    return status_;

  // If the data pipe was closed, just fail initialization.
  if (!chunked_data_pipe_getter_.is_connected())
    return net::ERR_FAILED;

  // Get a new data pipe and start.
  mojo::DataPipe data_pipe;
  chunked_data_pipe_getter_->StartReading(std::move(data_pipe.producer_handle));
  data_pipe_ = std::move(data_pipe.consumer_handle);

  return net::OK;
}

int ChunkedDataPipeUploadDataStream::ReadInternal(net::IOBuffer* buf,
                                                  int buf_len) {
  DCHECK(!buf_);
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);

  // If there was an error either passed to the ReadCallback or as a result of
  // closing the DataPipeGetter pipe, fail the read.
  if (status_ != net::OK)
    return status_;

  // Nothing else to do, if the entire body was read.
  if (size_ && bytes_read_ == *size_) {
    // This shouldn't be called if the stream was already completed.
    DCHECK(!IsEOF());

    SetIsFinalChunk();
    return net::OK;
  }

  // Only start watching once a read starts. This is because OnHandleReadable()
  // uses |buf_| implicitly assuming that this method has already been called.
  if (!handle_watcher_.IsWatching()) {
    handle_watcher_.Watch(
        data_pipe_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        base::BindRepeating(&ChunkedDataPipeUploadDataStream::OnHandleReadable,
                            base::Unretained(this)));
  }

  uint32_t num_bytes = buf_len;
  if (size_ && num_bytes > *size_ - bytes_read_)
    num_bytes = *size_ - bytes_read_;
  MojoResult rv =
      data_pipe_->ReadData(buf->data(), &num_bytes, MOJO_READ_DATA_FLAG_NONE);
  if (rv == MOJO_RESULT_OK) {
    bytes_read_ += num_bytes;
    // Not needed for correctness, but this allows the consumer to send the
    // final chunk and the end of stream message together, for protocols that
    // allow it.
    if (size_ && *size_ == bytes_read_)
      SetIsFinalChunk();
    return num_bytes;
  }

  if (rv == MOJO_RESULT_SHOULD_WAIT) {
    handle_watcher_.ArmOrNotify();
    buf_ = buf;
    buf_len_ = buf_len;
    return net::ERR_IO_PENDING;
  }

  // The pipe was closed. If the size isn't known yet, could be a success or a
  // failure.
  if (!size_) {
    // Need to keep the buffer around because its presence is used to indicate
    // that there's a pending UploadDataStream read.
    buf_ = buf;
    buf_len_ = buf_len;

    handle_watcher_.Cancel();
    data_pipe_.reset();
    return net::ERR_IO_PENDING;
  }

  // |size_| was checked earlier, so if this point is reached, the pipe was
  // closed before receiving all bytes.
  DCHECK_LT(bytes_read_, *size_);

  return net::ERR_FAILED;
}

void ChunkedDataPipeUploadDataStream::ResetInternal() {
  // Init rewinds the stream. Throw away current state, other than |size_| and
  // |status_|.
  buf_ = nullptr;
  buf_len_ = 0;
  handle_watcher_.Cancel();
  bytes_read_ = 0;
  data_pipe_.reset();
}

void ChunkedDataPipeUploadDataStream::OnSizeReceived(int32_t status,
                                                     uint64_t size) {
  DCHECK(!size_);
  DCHECK_EQ(net::OK, status_);

  status_ = status;
  if (status == net::OK) {
    size_ = size;
    if (size == bytes_read_) {
      // Only set this as a final chunk if there's a read in progress. Setting
      // it asynchronously could result in confusing consumers.
      if (buf_)
        SetIsFinalChunk();
    } else if (size < bytes_read_ || (buf_ && !data_pipe_.is_valid())) {
      // If more data was received than was expected, or there's a pending read
      // and data pipe was closed without passing in as many bytes as expected,
      // the upload can't continue.  If there's no pending read but the pipe was
      // closed, the closure and size difference will be noticed on the next
      // read attempt.
      status_ = net::ERR_FAILED;
    }
  }

  // If this is done, and there's a pending read, complete the pending read.
  // If there's not a pending read, either |status_| will be reported on the
  // next read, the file will be marked as done, so ReadInternal() won't be
  // called again.
  if (buf_ && (IsEOF() || status_ != net::OK)) {
    // |data_pipe_| isn't needed any more, and if it's still open, a close pipe
    // message would cause issues, since this class normally only watches the
    // pipe when there's a pending read.
    handle_watcher_.Cancel();
    data_pipe_.reset();
    // Clear |buf_| as well, so it's only non-null while there's a pending read.
    buf_ = nullptr;
    buf_len_ = 0;

    OnReadCompleted(status_);

    // |this| may have been deleted at this point.
  }
}

void ChunkedDataPipeUploadDataStream::OnHandleReadable(MojoResult result) {
  DCHECK(buf_);

  // Final result of the Read() call, to be passed to the consumer.
  // Swap out |buf_| and |buf_len_|
  scoped_refptr<net::IOBuffer> buf(std::move(buf_));
  int buf_len = buf_len_;
  buf_len_ = 0;

  int rv = ReadInternal(buf.get(), buf_len);

  if (rv != net::ERR_IO_PENDING)
    OnReadCompleted(rv);

  // |this| may have been deleted at this point.
}

void ChunkedDataPipeUploadDataStream::OnDataPipeGetterClosed() {
  // If the size hasn't been received yet, treat this as receiving an error.
  // Otherwise, this will only be a problem if/when InitInternal() tries to
  // start reading again, so do nothing.
  if (!size_)
    OnSizeReceived(net::ERR_FAILED, 0);
}

}  // namespace network
