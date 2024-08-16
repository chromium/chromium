// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/chunked_data_pipe_upload_data_stream.h"

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/c/system/types.h"
#include "net/base/io_buffer.h"

namespace network {

ChunkedDataPipeUploadDataStream::ChunkedDataPipeUploadDataStream(
    scoped_refptr<ResourceRequestBody> resource_request_body,
    mojo::PendingRemote<mojom::ChunkedDataPipeGetter> chunked_data_pipe_getter,
    bool has_null_source)
    : net::UploadDataStream(/*is_chunked=*/true,
                            /*has_null_source=*/has_null_source,
                            resource_request_body->identifier()),
      resource_request_body_(std::move(resource_request_body)),
      chunked_data_pipe_getter_(std::move(chunked_data_pipe_getter)),
      handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                      base::SequencedTaskRunner::GetCurrentDefault()) {
  // TODO(yhirano): Turn this to a DCHECK once we find the root cause of
  // https://crbug.com/1156550.
  CHECK(chunked_data_pipe_getter_.is_bound());
  chunked_data_pipe_getter_.set_disconnect_handler(
      base::BindOnce(&ChunkedDataPipeUploadDataStream::OnDataPipeGetterClosed,
                     base::Unretained(this)));
  chunked_data_pipe_getter_->GetSize(
      base::BindOnce(&ChunkedDataPipeUploadDataStream::OnSizeReceived,
                     base::Unretained(this)));
}

ChunkedDataPipeUploadDataStream::~ChunkedDataPipeUploadDataStream() {}

bool ChunkedDataPipeUploadDataStream::AllowHTTP1() const {
  return resource_request_body_->AllowHTTP1ForStreamingUpload();
}

int ChunkedDataPipeUploadDataStream::InitInternal(
    const net::NetLogWithSource& net_log) {
  // If there was an error either passed to the ReadCallback or as a result of
  // closing the DataPipeGetter pipe, fail the read.
  if (status_ != net::OK)
    return status_;

  // If the data pipe was closed, just fail initialization.
  if (!chunked_data_pipe_getter_.is_connected())
    return net::ERR_FAILED;

  switch (cache_state_) {
    case CacheState::kActive:
      if (data_pipe_.is_valid())
        return net::OK;
      else
        break;
    case CacheState::kExhausted:
      return net::ERR_FAILED;
    case CacheState::kDisabled:
      break;
  }

  // Get a new data pipe and start.
  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  MojoResult result =
      mojo::CreateDataPipe(nullptr, data_pipe_producer, data_pipe_consumer);
  if (result != MOJO_RESULT_OK)
    return net::ERR_INSUFFICIENT_RESOURCES;
  chunked_data_pipe_getter_->StartReading(std::move(data_pipe_producer));
  data_pipe_ = std::move(data_pipe_consumer);

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

  int cache_read_len = ReadFromCacheIfNeeded(buf, buf_len);
  if (cache_read_len > 0)
    return cache_read_len;

  // Only start watching once a read starts. This is because OnHandleReadable()
  // uses |buf_| implicitly assuming that this method has already been called.
  if (!handle_watcher_.IsWatching()) {
    handle_watcher_.Watch(
        data_pipe_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        base::BindRepeating(&ChunkedDataPipeUploadDataStream::OnHandleReadable,
                            base::Unretained(this)));
  }

  size_t num_bytes = base::checked_cast<size_t>(buf_len);
  if (size_ && num_bytes > *size_ - bytes_read_)
    num_bytes = *size_ - bytes_read_;
  MojoResult rv = data_pipe_->ReadData(MOJO_READ_DATA_FLAG_NONE,
                                       buf->span().first(num_bytes), num_bytes);
  if (rv == MOJO_RESULT_OK) {
    bytes_read_ += num_bytes;
    // Not needed for correctness, but this allows the consumer to send the
    // final chunk and the end of stream message together, for protocols that
    // allow it.
    if (size_ && *size_ == bytes_read_)
      SetIsFinalChunk();
    WriteToCacheIfNeeded(buf, num_bytes);
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
  buf_ = nullptr;
  buf_len_ = 0;
  handle_watcher_.Cancel();
  bytes_read_ = 0;
  if (cache_state_ != CacheState::kDisabled)
    return;
  // Init rewinds the stream. Throw away current state, other than |size_| and
  // |status_|.
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
    chunked_data_pipe_getter_.reset();

    if (status_ < net::ERR_IO_PENDING) {
      LOG(ERROR) << "OnSizeReceived failed with Error: " << status_;
    }
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

  if (rv != net::ERR_IO_PENDING) {
    if (rv < net::ERR_IO_PENDING) {
      LOG(ERROR) << "OnHandleReadable failed with Error: " << rv;
    }
    OnReadCompleted(rv);
  }

  // |this| may have been deleted at this point.
}

void ChunkedDataPipeUploadDataStream::OnDataPipeGetterClosed() {
  // If the size hasn't been received yet, treat this as receiving an error.
  // Otherwise, this will only be a problem if/when InitInternal() tries to
  // start reading again, so do nothing.
  if (status_ == net::OK && !size_)
    OnSizeReceived(net::ERR_FAILED, 0);
}

void ChunkedDataPipeUploadDataStream::EnableCache(size_t dst_window_size) {
  DCHECK_EQ(bytes_read_, 0u);
  DCHECK_EQ(cache_state_, CacheState::kDisabled);
  DCHECK(cache_.empty());
  cache_state_ = CacheState::kActive;
  dst_window_size_ = dst_window_size;
}

void ChunkedDataPipeUploadDataStream::WriteToCacheIfNeeded(net::IOBuffer* buf,
                                                           size_t num_bytes) {
  if (cache_state_ != CacheState::kActive)
    return;

  // |cache_state_ == CacheState::kActive| and |cache_.size() >= bytes_read_|
  // means we're reading from the cache.
  if (cache_.size() >= bytes_read_)
    return;

  if (cache_.size() >= dst_window_size_) {
    // Attempted to write over the max size. Replay must be failed.
    // Notes: CDPUDS caches chunks from the date pipe until whole size gets over
    // the max size. For example, if the date pipe sends [60k, 60k, 60k] chunks,
    // CDPUDS caches the first 2 60ks. If it is [120k, 1k,], CDPUDS caches the
    // 120k chunk or any size if it is the first chunk.
    cache_state_ = CacheState::kExhausted;
    return;
  }
  cache_.insert(cache_.end(), buf->data(), buf->data() + num_bytes);
}

int ChunkedDataPipeUploadDataStream::ReadFromCacheIfNeeded(net::IOBuffer* buf,
                                                           int buf_len) {
  if (cache_state_ != CacheState::kActive)
    return 0;
  if (cache_.size() <= bytes_read_)
    return 0;

  int read_size =
      std::min(static_cast<int>(cache_.size() - bytes_read_), buf_len);
  DCHECK_GT(read_size, 0);
  memcpy(buf->data(), &cache_[bytes_read_], read_size);
  bytes_read_ += read_size;
  return read_size;
}

}  // namespace network
