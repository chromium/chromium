// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/ipc_fifo_buffer.h"

#include <limits>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/logging.h"

namespace remoting {

// =============================================================================
// IpcFifoBufferWriter
// =============================================================================

IpcFifoBufferWriter::IpcFifoBufferWriter(
    mojo::ScopedDataPipeProducerHandle producer_handle)
    : producer_handle_(std::move(producer_handle)) {
  CHECK(producer_handle_.is_valid());
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IpcFifoBufferWriter::~IpcFifoBufferWriter() = default;

FifoBufferWriter::Result IpcFifoBufferWriter::Write(
    base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (data.empty()) {
    return FifoBufferWriter::Result::kSuccess;
  }

  size_t bytes_written = 0;
  MojoResult result = producer_handle_->WriteData(
      data, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE, bytes_written);

  if (result == MOJO_RESULT_OK) {
    return FifoBufferWriter::Result::kSuccess;
  }

  if (result == MOJO_RESULT_SHOULD_WAIT || result == MOJO_RESULT_OUT_OF_RANGE) {
    return FifoBufferWriter::Result::kFull;
  }

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // Peer closed. Fail gracefully.
    return FifoBufferWriter::Result::kFailed;
  }

  LOG(ERROR) << "Failed to write to Mojo Data Pipe: " << result;
  return FifoBufferWriter::Result::kFailed;
}

// =============================================================================
// IpcFifoBufferReader
// =============================================================================

IpcFifoBufferReader::IpcFifoBufferReader(
    mojo::ScopedDataPipeConsumerHandle consumer_handle)
    : consumer_handle_(std::move(consumer_handle)) {
  CHECK(consumer_handle_.is_valid());
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IpcFifoBufferReader::~IpcFifoBufferReader() = default;

std::optional<size_t> IpcFifoBufferReader::Read(
    base::span<uint8_t> destination) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (destination.empty()) {
    return 0;
  }

  size_t bytes_read = 0;
  MojoResult result = consumer_handle_->ReadData(MOJO_READ_DATA_FLAG_NONE,
                                                 destination, bytes_read);

  if (result == MOJO_RESULT_OK) {
    return bytes_read;
  }

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    return 0;
  }

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // Peer closed. Fail gracefully.
    return std::nullopt;
  }

  LOG(ERROR) << "Failed to read from Mojo Data Pipe: " << result;
  return std::nullopt;
}

std::optional<size_t> IpcFifoBufferReader::Skip(size_t bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (bytes == 0) {
    return 0;
  }

  size_t bytes_discarded = 0;
  MojoResult result = consumer_handle_->DiscardData(bytes, bytes_discarded);

  if (result == MOJO_RESULT_OK) {
    return bytes_discarded;
  }

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    return 0;
  }

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // Peer closed. Fail gracefully.
    return std::nullopt;
  }

  LOG(ERROR) << "Failed to discard data from Mojo Data Pipe: " << result;
  return std::nullopt;
}

void IpcFifoBufferReader::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [[maybe_unused]] size_t discarded = 0;
  consumer_handle_->DiscardData(std::numeric_limits<size_t>::max(), discarded);
}

std::optional<size_t> IpcFifoBufferReader::GetBufferedBytes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t bytes_available = 0;
  MojoResult result = consumer_handle_->ReadData(
      MOJO_READ_DATA_FLAG_QUERY, base::span<uint8_t>(), bytes_available);

  if (result == MOJO_RESULT_OK) {
    if (bytes_available > 0) {
      return bytes_available;
    }
    // If 0 bytes are available, the pipe might be empty or closed.
    MojoHandleSignalsState signals = consumer_handle_->QuerySignalsState();
    if (signals.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED) {
      return std::nullopt;
    }
    return 0;
  }

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // Pipe is empty.
    return 0;
  }

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // Peer closed.
    return std::nullopt;
  }

  LOG(ERROR) << "Failed to query Mojo Data Pipe: " << result;
  return std::nullopt;
}

bool CreateIpcFifoBuffer(size_t capacity,
                         std::unique_ptr<IpcFifoBufferWriter>& writer,
                         std::unique_ptr<IpcFifoBufferReader>& reader) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoResult result = mojo::CreateDataPipe(capacity, producer, consumer);
  if (result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Failed to create Mojo Data Pipe: " << result;
    return false;
  }

  writer = std::make_unique<IpcFifoBufferWriter>(std::move(producer));
  reader = std::make_unique<IpcFifoBufferReader>(std::move(consumer));
  return true;
}

}  // namespace remoting
