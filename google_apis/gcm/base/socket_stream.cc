// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "google_apis/gcm/base/socket_stream.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"

namespace gcm {

namespace {

// TODO(zea): consider having dynamically-sized buffers if this becomes too
// expensive.
const size_t kDefaultBufferSize = 8*1024;

}  // namespace

SocketInputStream::SocketInputStream(mojo::ScopedDataPipeConsumerHandle stream)
    : stream_(std::move(stream)),
      stream_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      read_size_(0),
      io_buffer_(
          base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize)),
      read_buffer_(
          base::MakeRefCounted<net::DrainableIOBuffer>(io_buffer_,
                                                       kDefaultBufferSize)),
      next_pos_(0),
      last_error_(net::OK) {
  stream_watcher_.Watch(
      stream_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SocketInputStream::ReadMore,
                          base::Unretained(this)));
}

SocketInputStream::~SocketInputStream() {
}

bool SocketInputStream::Next(const void** data, int* size) {
  if (GetState() != EMPTY && GetState() != READY) {
    NOTREACHED() << "Invalid input stream read attempt.";
  }

  if (GetState() == EMPTY) {
    DVLOG(1) << "No unread data remaining, ending read.";
    return false;
  }

  DCHECK_EQ(GetState(), READY)
      << " Input stream must have pending data before reading.";
  DCHECK_LT(next_pos_, read_buffer_->BytesConsumed());
  *data = io_buffer_->data() + next_pos_;
  *size = UnreadByteCount();
  next_pos_ = read_buffer_->BytesConsumed();
  DVLOG(1) << "Consuming " << *size << " bytes in input buffer.";
  return true;
}

void SocketInputStream::BackUp(int count) {
  DCHECK(GetState() == READY || GetState() == EMPTY);
  // TODO(zea): investigating crbug.com/409985
  CHECK_GT(count, 0);
  CHECK_LE(count, next_pos_);

  next_pos_ -= count;
  DVLOG(1) << "Backing up " << count << " bytes in input buffer. "
           << "Current position now at " << next_pos_
           << " of " << read_buffer_->BytesConsumed();
}

bool SocketInputStream::Skip(int count) {
  NOTIMPLEMENTED();
  return false;
}

int64_t SocketInputStream::ByteCount() const {
  DCHECK_NE(GetState(), CLOSED);
  DCHECK_NE(GetState(), READING);
  return next_pos_;
}

int SocketInputStream::UnreadByteCount() const {
  DCHECK_NE(GetState(), CLOSED);
  DCHECK_NE(GetState(), READING);
  return read_buffer_->BytesConsumed() - next_pos_;
}

net::Error SocketInputStream::Refresh(base::OnceClosure callback,
                                      int byte_limit) {
  DCHECK(!read_callback_);
  DCHECK_NE(GetState(), CLOSED);
  DCHECK_NE(GetState(), READING);
  DCHECK_GT(byte_limit, 0);

  if (byte_limit > read_buffer_->BytesRemaining()) {
    LOG(ERROR) << "Out of buffer space, closing input stream.";
    CloseStream(net::ERR_FILE_TOO_BIG);
    return net::OK;
  }

  read_size_ = base::checked_cast<size_t>(byte_limit);
  read_callback_ = std::move(callback);
  stream_watcher_.ArmOrNotify();
  last_error_ = net::ERR_IO_PENDING;
  return net::ERR_IO_PENDING;
}

void SocketInputStream::ReadMore(
    MojoResult result,
    const mojo::HandleSignalsState& /* ignored */) {
  DCHECK(read_callback_);
  DCHECK_NE(0u, read_size_);

  size_t num_bytes = read_size_;
  if (result == MOJO_RESULT_OK) {
    DVLOG(1) << "Refreshing input stream, limit of " << num_bytes << " bytes.";
    result =
        stream_->ReadData(MOJO_READ_DATA_FLAG_NONE,
                          read_buffer_->span().first(num_bytes), num_bytes);
    DVLOG(1) << "Read returned mojo result" << result;
  }

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    stream_watcher_.ArmOrNotify();
    return;
  }

  read_size_ = 0;
  if (result != MOJO_RESULT_OK) {
    CloseStream(net::ERR_FAILED);
    std::move(read_callback_).Run();
    return;
  }

  // If an EOF has been received, close the stream.
  if (result == MOJO_RESULT_OK && num_bytes == 0) {
    CloseStream(net::ERR_CONNECTION_CLOSED);
    std::move(read_callback_).Run();
    return;
  }

  // If an error occurred before the completion callback could complete, ignore
  // the result.
  if (GetState() == CLOSED)
    return;

  last_error_ = net::OK;
  read_buffer_->DidConsume(base::checked_cast<uint32_t>(num_bytes));
  // TODO(zea): investigating crbug.com/409985
  CHECK_GT(UnreadByteCount(), 0);

  DVLOG(1) << "Refresh complete with " << num_bytes << " new bytes. "
           << "Current position " << next_pos_ << " of "
           << read_buffer_->BytesConsumed() << ".";

  std::move(read_callback_).Run();
}

void SocketInputStream::RebuildBuffer() {
  DVLOG(1) << "Rebuilding input stream, consumed "
           << next_pos_ << " bytes.";
  DCHECK_NE(GetState(), READING);
  DCHECK_NE(GetState(), CLOSED);

  int unread_data_size = 0;
  const void* unread_data_ptr = nullptr;
  Next(&unread_data_ptr, &unread_data_size);
  ResetInternal();

  if (unread_data_ptr != io_buffer_->data()) {
    DVLOG(1) << "Have " << unread_data_size
             << " unread bytes remaining, shifting.";
    // Move any remaining unread data to the start of the buffer;
    std::copy(static_cast<const char*>(unread_data_ptr),
              static_cast<const char*>(unread_data_ptr) + unread_data_size,
              io_buffer_->data());
  } else {
    DVLOG(1) << "Have " << unread_data_size << " unread bytes remaining.";
  }
  read_buffer_->DidConsume(unread_data_size);
  // TODO(zea): investigating crbug.com/409985
  CHECK_GE(UnreadByteCount(), 0);
}

net::Error SocketInputStream::last_error() const {
  return last_error_;
}

SocketInputStream::State SocketInputStream::GetState() const {
  if (last_error_ < net::ERR_IO_PENDING)
    return CLOSED;

  if (last_error_ == net::ERR_IO_PENDING)
    return READING;

  DCHECK_EQ(last_error_, net::OK);
  if (read_buffer_->BytesConsumed() == next_pos_)
    return EMPTY;

  return READY;
}

void SocketInputStream::ResetInternal() {
  read_buffer_->SetOffset(0);
  next_pos_ = 0;
  last_error_ = net::OK;
  weak_ptr_factory_.InvalidateWeakPtrs();  // Invalidate any callbacks.
}

void SocketInputStream::CloseStream(net::Error error) {
  DCHECK_LT(error, net::ERR_IO_PENDING);
  ResetInternal();
  last_error_ = error;
}

SocketOutputStream::SocketOutputStream(
    mojo::ScopedDataPipeProducerHandle stream)
    : stream_(std::move(stream)),
      stream_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      io_buffer_(
          base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize)),
      next_pos_(0),
      last_error_(net::OK) {
  stream_watcher_.Watch(
      stream_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SocketOutputStream::WriteMore,
                          base::Unretained(this)));
}

SocketOutputStream::~SocketOutputStream() {
}

bool SocketOutputStream::Next(void** data, int* size) {
  DCHECK_NE(GetState(), CLOSED);
  DCHECK_NE(GetState(), FLUSHING);
  if (next_pos_ == io_buffer_->size())
    return false;

  *data = io_buffer_->data() + next_pos_;
  *size = io_buffer_->size() - next_pos_;
  next_pos_ = io_buffer_->size();
  return true;
}

void SocketOutputStream::BackUp(int count) {
  DCHECK_GE(count, 0);
  if (count > next_pos_)
    next_pos_ = 0;
  next_pos_ -= count;
  DVLOG(1) << "Backing up " << count << " bytes in output buffer. "
           << next_pos_ << " bytes used.";
}

int64_t SocketOutputStream::ByteCount() const {
  DCHECK_NE(GetState(), CLOSED);
  DCHECK_NE(GetState(), FLUSHING);
  return next_pos_;
}

net::Error SocketOutputStream::Flush(base::OnceClosure callback) {
  DCHECK(!write_callback_);
  DCHECK_EQ(GetState(), READY);

  if (!write_buffer_) {
    write_buffer_ = base::MakeRefCounted<net::DrainableIOBuffer>(
        io_buffer_.get(), next_pos_);
  }

  last_error_ = net::ERR_IO_PENDING;
  stream_watcher_.ArmOrNotify();
  write_callback_ = std::move(callback);
  return net::ERR_IO_PENDING;
}

void SocketOutputStream::WriteMore(MojoResult result,
                                   const mojo::HandleSignalsState& state) {
  DCHECK(write_callback_);
  DCHECK(write_buffer_);

  const base::span<const uint8_t> bytes = write_buffer_->span().first(
      base::checked_cast<size_t>(write_buffer_->BytesRemaining()));
  DVLOG(1) << "Flushing " << bytes.size() << " bytes into socket.";

  size_t bytes_written = 0;
  if (result == MOJO_RESULT_OK) {
    result =
        stream_->WriteData(bytes, MOJO_WRITE_DATA_FLAG_NONE, bytes_written);
  }
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    stream_watcher_.ArmOrNotify();
    return;
  }
  if (result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Failed to flush socket.";
    last_error_ = net::ERR_FAILED;
    std::move(write_callback_).Run();
    return;
  }
  DVLOG(1) << "Wrote  " << bytes_written;
  // If an error occurred before the completion callback could complete, ignore
  // the result.
  if (GetState() == CLOSED)
    return;

  DCHECK_GE(bytes_written, 0u);
  last_error_ = net::OK;
  write_buffer_->DidConsume(base::checked_cast<uint32_t>(bytes_written));
  if (write_buffer_->BytesRemaining() > 0) {
    DVLOG(1) << "Partial flush complete. Retrying.";
    // Only a partial write was completed. Flush again to finish the write.
    Flush(std::move(write_callback_));
    return;
  }
  DVLOG(1) << "Socket flush complete.";
  write_buffer_ = nullptr;
  next_pos_ = 0;
  std::move(write_callback_).Run();
}

SocketOutputStream::State SocketOutputStream::GetState() const{
  if (last_error_ < net::ERR_IO_PENDING)
    return CLOSED;

  if (last_error_ == net::ERR_IO_PENDING)
    return FLUSHING;

  DCHECK_EQ(last_error_, net::OK);
  if (next_pos_ == 0)
    return EMPTY;

  return READY;
}

net::Error SocketOutputStream::last_error() const {
  return last_error_;
}

}  // namespace gcm
